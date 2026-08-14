/* TuxBlox - Linux Compatibility Layer for the Roblox Engine
 * Copyright (C) 2026 TuxBlox Developers
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <https://www.gnu.org/licenses/>.
 */

/* webkitgtk-bundle/host/webview.c
 *
 * Window creation/destruction and the live-webview registry -- ported from
 * ProtonSource/wine/dlls/webview2loader/unixlib.c as of commit ac3634ea6
 * (see git show ac3634ea6:ProtonSource/wine/dlls/webview2loader/unixlib.c),
 * the last commit before Task 3's rewrite deleted that file's in-process
 * dlmopen/GTK-thread machinery. This is Task 4 of the
 * webview2loader-host-process plan
 * (docs/superpowers/plans/2026-08-14-webview2loader-host-process.md).
 *
 * Mapping from the old file (all renames/removals are purely mechanical,
 * per Task 4's brief -- no logic changes beyond what's noted):
 *   - struct native_webview                     -> struct native_webview (webview.h), unchanged
 *   - live_webview_register/_unregister/_is_valid -> folded into webview_create/
 *     webview_destroy/webview_lookup below (same MAX_LIVE_WEBVIEWS array,
 *     extended so lookup-by-handle returns the pointer, not just a bool --
 *     needed now that handles cross a real IPC boundary as opaque uint64_t
 *     instead of already being a live in-process pointer)
 *   - on_close_request                          -> on_close_request, verbatim
 *   - create_webview_on_gtk_thread               -> webview_create, with the
 *     struct create_webview_ctx/void* data marshaling wrapper dropped (this
 *     runs directly as the dispatch handler's own body now, no
 *     gtk_thread_invoke_sync to bridge across)
 *   - destroy_webview_on_gtk_thread              -> webview_destroy, same
 *     wrapper-removal treatment
 *
 * Every p_-prefixed GTK function call from the old file (e.g.
 * p_gtk_window_new()) becomes a direct call against the real GTK4 headers
 * this file links against (gtk_window_new()) -- the old p_ indirection only
 * existed because that code was dlopen()'d at runtime with no real headers
 * available; that constraint doesn't apply here.
 *
 * Task 6 addition (additive only -- this file's Task 4 window creation/
 * destruction/registry logic is otherwise unchanged): the
 * WebKitWebView::decide-policy connection (real OAuth/xdg-open redirect
 * handoff, on_decide_policy -- implemented in navigate.c, Task 6's file)
 * is wired up in webview_create below, alongside the existing
 * "close-request" connection, exactly where it lived at webview-creation
 * time in the original source (see navigate.c's own comment on
 * on_decide_policy for why it must be connected here, not scoped to a
 * single Navigate() call).
 */
#include "webview.h"
#include "navigate.h"
#include <stdio.h>
#include <stdlib.h>

/* Task 7 crash fix, round 14 (original unixlib.c comment, preserved
 * verbatim below on on_close_request itself): a small live-handle registry
 * is enough here -- unlike the original unixlib.c, every dispatch handler
 * in this process already runs serialized on the single GTK main-loop
 * thread (there is no separate "GTK thread" to bridge to anymore, the
 * whole helper process's main() *is* that thread), so no lock is needed
 * around this array: destroy unregisters (and only then frees) strictly
 * before any later-queued handler for the same handle can run. */
#define MAX_LIVE_WEBVIEWS 64
static struct native_webview *live_webviews[MAX_LIVE_WEBVIEWS];

static void live_webview_register(struct native_webview *nv)
{
    int i;
    for (i = 0; i < MAX_LIVE_WEBVIEWS; i++)
        if (!live_webviews[i]) { live_webviews[i] = nv; return; }
    fprintf(stderr, "webview2loader-host: live_webviews registry full (%d entries) -- "
                     "UAF guard won't cover handle %p\n", MAX_LIVE_WEBVIEWS, (void *)nv);
}

static void live_webview_unregister(struct native_webview *nv)
{
    int i;
    for (i = 0; i < MAX_LIVE_WEBVIEWS; i++)
        if (live_webviews[i] == nv) { live_webviews[i] = NULL; return; }
}

struct native_webview *webview_lookup(uint64_t handle)
{
    struct native_webview *nv = (struct native_webview *)(uintptr_t)handle;
    int i;

    if (!nv) return NULL;
    for (i = 0; i < MAX_LIVE_WEBVIEWS; i++)
        if (live_webviews[i] == nv) return nv;
    return NULL;
}

/* Task 7 crash fix, round 14: a real, reproducible double-destroy crash
 * (coredump, RAX == 0xaaaaaaaaaaaaaaaa -- GLib's own "gc-friendly" freed-
 * memory poison fill -- reading through nv->window inside GTK4's own
 * gtk_window_destroy(), called from this file's own webview_destroy)
 * surfaced only once these windows became genuinely X11/WM-backed (a
 * window that's not really window-manager-managed can't receive a real
 * close request from one either). GTK4's own documented default behavior
 * for GtkWindow::close-request is to destroy the window itself unless a
 * connected handler returns TRUE to stop that. Without a connected
 * handler, any window-manager-initiated close (the WM's own close button,
 * Alt+F4, etc.) could trigger GTK4 to self-destroy nv->window out from
 * under this file's own bookkeeping; a later, entirely normal
 * Close()/Release() on the same real WebView2 controller then calls this
 * file's own webview_destroy, which tries to destroy the same (already
 * GTK-internally-destroyed) widget a second time -- a genuine
 * double-destroy. Real WebView2 controllers are never independently
 * closable by the user or window manager at all (only via the real API's
 * own Close()), so unconditionally stopping this signal is the correct
 * semantic fix, not just a crash workaround. */
static gboolean on_close_request(GtkWindow *window, void *user_data)
{
    return TRUE; /* GDK_EVENT_STOP -- stop GTK4's own default handler,
                  * which would otherwise destroy the window itself. */
}

struct native_webview *webview_create(int is_message_only)
{
    struct native_webview *nv = calloc(1, sizeof(*nv));

    /* Code review fix (original unixlib.c): calloc() can fail under real
     * memory pressure -- dereferencing NULL two lines below would kill
     * this process's single GTK main-loop thread (and every future
     * dispatch on it). Returning NULL here is already a real, handled
     * failure path -- main.c's WV2L_OP_CREATE_WEBVIEW dispatch case
     * reports handle = 0, exactly the existing wire "failure" convention,
     * so simply not creating anything here is sufficient, no new
     * signaling needed. */
    if (!nv)
    {
        fprintf(stderr, "webview2loader-host: calloc failed for a new native_webview -- "
                         "out of memory, failing create\n");
        return NULL;
    }

    nv->window = gtk_window_new();
    /* Task 7 crash fix, round 13 (original unixlib.c comment): set before
     * the window is ever shown (below) so the window manager never draws
     * chrome on it even momentarily; unconditional (not gated on
     * is_message_only) since a message-only controller's window is never
     * shown at all, so this is a harmless no-op for that case rather than
     * something that needs its own branch. */
    gtk_window_set_decorated(GTK_WINDOW(nv->window), FALSE);
    /* Task 7 crash fix, round 14 -- see on_close_request's own comment
     * just above for the full crash evidence and reasoning. Connected
     * unconditionally (both is_message_only and real controllers can, in
     * principle, end up window-manager-visible/-addressable), before the
     * window is ever shown, so there's no window in existence yet that
     * could receive a close request before this handler is wired up. No
     * disconnect/cleanup needed -- the connection's lifetime is exactly
     * nv->window's own lifetime, torn down together by webview_destroy's
     * own gtk_window_destroy call. */
    g_signal_connect_data(nv->window, "close-request", (GCallback)on_close_request,
                           NULL, NULL, 0);
    nv->view = WEBKIT_WEB_VIEW(webkit_web_view_new());
    /* Task 6 addition -- see this file's own top-of-file comment and
     * navigate.c's own comment on on_decide_policy for the full rationale.
     * Connected unconditionally, same reasoning as the close-request
     * connection just above -- even a message-only (HWND_MESSAGE/
     * CookieManager) controller has a real, live WebKitWebView that could
     * in principle navigate through this same OAuth flow. No disconnect/
     * cleanup needed -- the connection's lifetime is exactly nv->view's own
     * lifetime, torn down together by webview_destroy's own
     * gtk_window_destroy call (which destroys nv->view along with
     * nv->window, still its child at that point). */
    g_signal_connect_data(nv->view, "decide-policy", (GCallback)on_decide_policy,
                           NULL, NULL, 0);
    gtk_window_set_child(GTK_WINDOW(nv->window), GTK_WIDGET(nv->view));
    /* Plan 3 Task 2 (original unixlib.c comment): HWND_MESSAGE-parented
     * controllers (the CookieManager flow) still need a real, live
     * WebKitWebView -- Navigate/GetCookies/DeleteAllCookies all operate on
     * it -- but must never show a visible top-level window (real
     * WebView2's own HWND_MESSAGE semantics: the browser process runs,
     * there is simply no visible top-level HWND). */
    /* gtk_widget_show() (what the original unixlib.c called through its
     * p_gtk_widget_show function pointer) is deprecated as of GTK 4.10 in
     * favor of gtk_widget_set_visible() -- caught here, not in the old
     * file, because the old p_-prefixed indirection was just a hand-typed
     * function-pointer declaration with no deprecation annotation attached;
     * linking against the real header (which does carry the annotation)
     * turns this into a hard error under this build's -Werror. Using
     * gtk_widget_set_visible(widget, TRUE) rather than gtk_window_present()
     * is the literal behavioral equivalent of the old show() call (no
     * implicit focus-grab/present side effect), not just the first
     * deprecation-fix suggestion in the compiler note. */
    if (!is_message_only)
        gtk_widget_set_visible(nv->window, TRUE);

    live_webview_register(nv); /* Task 7 UAF guard -- see webview_lookup's own comment above */
    return nv;
}

void webview_destroy(struct native_webview *nv)
{
    if (!nv) return;

    /* Task 7 UAF guard -- see webview_lookup's own comment above.
     * Unregister strictly before freeing, and before the GTK/WebKit call
     * below too: every other dispatch handler that checks this registry
     * runs serialized on this same GTK main-loop thread, so once this line
     * has executed, any such handler still queued behind this one for the
     * same (now-dying) handle will see it as no-longer-live. */
    live_webview_unregister(nv);
    /* Destroying nv->window tears down nv->view along with it (still its
     * child at this point), so this is the one native GTK/WebKit call
     * needed per webview. */
    gtk_window_destroy(GTK_WINDOW(nv->window));
    free(nv);
}
