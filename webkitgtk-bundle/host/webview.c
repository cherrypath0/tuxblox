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
#include "watchdog.h"
#include "geometry.h"
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

/* --- F-08 spike: does the page actually use window.chrome.webview? ---
 *
 * See webview_create's own call-site comment for why this exists. Diagnostic
 * only: nothing is forwarded to the Wine side, so no PE-side event plumbing,
 * no protocol change, and no behaviour change for anything that already works.
 *
 * The JS deliberately mirrors real WebView2's shape rather than inventing one,
 * so that if the answer is "yes, the page posts messages", this same script is
 * the starting point for the real bridge instead of throwaway code:
 * postMessage(string) and postMessage(object) are distinguished, because real
 * WebView2 surfaces those differently (TryGetWebMessageAsString returns the
 * string only for the former, while get_WebMessageAsJson always returns JSON).
 * addEventListener is accepted and recorded so a page that registers for
 * host->page messages doesn't throw; nothing dispatches to it yet. */
static const char *const WV2_MESSAGE_SHIM_JS =
    "(function(){\n"
    "  if (window.chrome && window.chrome.webview) return;\n"
    "  var listeners = [];\n"
    "  function send(kind, payload) {\n"
    "    try {\n"
    "      window.webkit.messageHandlers.tuxblox.postMessage(\n"
    "        JSON.stringify({kind: kind, payload: payload}));\n"
    "    } catch (e) { /* handler not registered -- nothing to do */ }\n"
    "  }\n"
    "  window.chrome = window.chrome || {};\n"
    "  window.chrome.webview = {\n"
    "    postMessage: function(msg) {\n"
    "      send(typeof msg === 'string' ? 'string' : 'json',\n"
    "           typeof msg === 'string' ? msg : JSON.stringify(msg));\n"
    "    },\n"
    "    addEventListener: function(t, f) { if (t === 'message') listeners.push(f); },\n"
    "    removeEventListener: function(t, f) {\n"
    "      var i = listeners.indexOf(f); if (i >= 0) listeners.splice(i, 1);\n"
    "    }\n"
    "  };\n"
    "})();\n";

static void on_script_message(WebKitUserContentManager *manager, JSCValue *value, void *user_data)
{
    char *json = jsc_value_to_string(value);

    (void)manager;
    fprintf(stderr, "webview2loader-host: F-08 PROBE: page called window.chrome.webview.postMessage "
                    "on nv=%p -- payload: %s\n", user_data, json ? json : "(unconvertible)");
    g_free(json);
}

static void webview_install_message_probe(struct native_webview *nv)
{
    WebKitUserContentManager *manager = webkit_web_view_get_user_content_manager(nv->view);
    WebKitUserScript *script;

    if (!manager)
    {
        fprintf(stderr, "webview2loader-host: F-08 PROBE: no WebKitUserContentManager for nv=%p -- "
                        "probe not installed\n", (void *)nv);
        return;
    }

    /* NULL world_name == the page's own main script world, which is where
     * window.chrome has to exist for the page to see it. */
    if (!webkit_user_content_manager_register_script_message_handler(manager, "tuxblox", NULL))
    {
        fprintf(stderr, "webview2loader-host: F-08 PROBE: could not register the 'tuxblox' script "
                        "message handler for nv=%p -- probe not installed\n", (void *)nv);
        return;
    }
    g_signal_connect_data(manager, "script-message-received::tuxblox",
                           (GCallback)on_script_message, nv, NULL, 0);

    /* DOCUMENT_START so window.chrome.webview exists before any page script
     * runs -- a page that feature-detects it at load time must see it. ALL
     * FRAMES because an OAuth flow can run in an iframe. */
    script = webkit_user_script_new(WV2_MESSAGE_SHIM_JS,
                                     WEBKIT_USER_CONTENT_INJECT_ALL_FRAMES,
                                     WEBKIT_USER_SCRIPT_INJECT_AT_DOCUMENT_START,
                                     NULL, NULL);
    webkit_user_content_manager_add_script(manager, script);
    webkit_user_script_unref(script);
    fprintf(stderr, "webview2loader-host: F-08 PROBE: window.chrome.webview shim installed for nv=%p\n",
            (void *)nv);
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
    /* F-08 spike (diagnostic only -- no message is delivered to the Wine side
     * yet). Roblox Studio registers BOTH add_WebMessageReceived and
     * add_NavigationStarting before it navigates, and this shim implements
     * neither, so we do not know which one its login flow actually depends on.
     * Real WebView2 exposes window.chrome.webview.postMessage to the page; if
     * Studio's login page uses it to hand back the auth result, that is the
     * mechanism to build, and if the page never calls it then the redirect to
     * roblox-studio-auth: (i.e. NavigationStarting) is, and the whole message
     * bridge would be the wrong thing to build.
     *
     * This installs just enough to answer that: a document-start user script
     * defining window.chrome.webview.postMessage on top of WebKitGTK's own
     * webkit.messageHandlers channel, plus a handler that logs whatever
     * arrives. Cheap, reversible, and it makes the next login attempt decide
     * the design instead of a guess. */
    webview_install_message_probe(nv);
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
    /* user_data is nv (was NULL): on_decide_policy now needs to know WHICH
     * webview a navigation belongs to, so it can name it in the
     * NavigationStarting event it sends to Studio. */
    g_signal_connect_data(nv->view, "decide-policy", (GCallback)on_decide_policy,
                           nv, NULL, 0);
    /* 2026-08-15 cosmetic mitigation for a real, understood WebKitGTK/NVIDIA
     * Skia GPU-teardown race in WebKitWebProcess's own shutdown path (a
     * genuine upstream WebKitGTK/NVIDIA bug -- not anything in this file or
     * this process -- see navigate.c's own comment on
     * on_web_process_terminated for the full real-signal/enum verification
     * against this bundle's actual built WebKitGTK 2.52.5 headers/source,
     * and .superpowers/sdd/2026-08-14-webview2loader-host-process/
     * webprocess-terminated-mitigation-report.md for the full writeup).
     * Connected unconditionally, same reasoning and same lifetime as the
     * decide-policy connection just above (torn down together by
     * webview_destroy's own gtk_window_destroy call, no separate
     * disconnect/cleanup needed) -- user_data is nv itself (unlike
     * decide-policy, which doesn't need it), so the handler can mark this
     * specific webview's state and wake any in-flight wait on it; see
     * webview.h's own comment on struct native_webview's process_terminated/
     * active_wait_loop fields. */
    g_signal_connect_data(nv->view, "web-process-terminated", (GCallback)on_web_process_terminated,
                           nv, NULL, 0);
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
    /* If a bounded wait in navigate.c is currently blocked on this webview
     * (nested GMainLoop -- see wait_ended_with_nv_alive there), wake it now.
     * Its own guard already keeps it from touching nv after this point, so
     * this is purely about not making it sit out the rest of its 10s/30s
     * timeout waiting for a completion that can no longer arrive. Read before
     * the free below, obviously; g_main_loop_quit only sets a flag, so the
     * waiter unwinds when the current dispatch returns, not re-entrantly from
     * inside this call. */
    if (nv->active_wait_loop)
    {
        fprintf(stderr, "webview2loader-host: destroying native_webview %p with a wait in flight -- "
                        "waking it so it fails now instead of at its timeout\n", (void *)nv);
        g_main_loop_quit(nv->active_wait_loop);
        nv->active_wait_loop = NULL;
    }
    /* Crash fix -- see watchdog.h's own top-of-file comment: drop any live
     * watch this nv has on a reparented-into parent_xid before nv is freed
     * below, so a later DestroyNotify/ReparentNotify for that (by-then-
     * recycled) parent_xid value can never dereference a dangling nv
     * pointer. Also reached when watchdog.c itself is the one calling this
     * function (its own crash-mitigation teardown path) -- a harmless,
     * idempotent no-op there, since that path already cleared its own
     * watched[] entry for nv immediately before calling here. */
    watchdog_untrack(nv);
    /* Crash fix -- see geometry.h's own comment on geometry_unreparent for
     * the full real-coredump evidence and root-cause reasoning: a still-
     * X11-reparented nv->window can make GTK4's own gtk_window_destroy()
     * below hit the same fatal GDK internal-consistency assertion
     * watchdog.c defends against for an externally-triggered destroy --
     * except this trigger is entirely self-inflicted (a completely normal
     * Close()/Release() while still reparented, no external actor, no
     * second controller, no watchdog event involved at all). Restoring the
     * X11 hierarchy to what GDK's own bookkeeping still believes it is
     * (an ordinary top-level, effectively parented to the real root
     * window) before the real destroy call closes that gap. No-op (and
     * never fatal) if nv was never reparented in the first place. */
    geometry_unreparent(nv);
    /* Destroying nv->window tears down nv->view along with it (still its
     * child at this point), so this is the one native GTK/WebKit call
     * needed per webview. */
    gtk_window_destroy(GTK_WINDOW(nv->window));
    free(nv);
}
