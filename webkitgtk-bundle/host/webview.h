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

/* webkitgtk-bundle/host/webview.h
 *
 * Window creation/destruction and the live-webview registry, ported from
 * ProtonSource/wine/dlls/webview2loader/unixlib.c as of commit ac3634ea6
 * (see webview.c for the per-function mapping). This is Task 4 of the
 * webview2loader-host-process plan
 * (docs/superpowers/plans/2026-08-14-webview2loader-host-process.md) --
 * geometry sync (Task 5) and navigation/cookies (Task 6) build on top of
 * struct native_webview and webview_lookup() defined here.
 */
#ifndef WV2L_HOST_WEBVIEW_H
#define WV2L_HOST_WEBVIEW_H

#include <gtk/gtk.h>
#include <webkit/webkit.h>
#include <stdint.h>

/* Task 7 crash fix, round 15 (original unixlib.c comment): which real X11
 * parent window (if any) nv->window has already been XReparentWindow'd
 * into. 0 means "not reparented yet" (still an independent top-level, or
 * no valid parent_xid has ever been supplied). Re-checked against the
 * current params->parent_xid on every call so a change (including
 * recovering from Wine recreating the parent's whole_window) re-reparents
 * on the next call rather than silently going stale. Not used by this
 * task -- Task 5's geometry_sync() is the only reader/writer -- kept here
 * because it belongs on the object's own lifetime, not because Task 4
 * itself touches it. */
struct native_webview
{
    /* ICoreWebView2Settings::AreDefaultContextMenusEnabled; consulted by
     * on_context_menu. TRUE (WebView2's default) until Studio says otherwise. */
    gboolean default_context_menus_enabled;
    GtkWidget *window;
    WebKitWebView *view;
    unsigned long reparented_into;

    /* Cosmetic WebKitWebView::web-process-terminated mitigation (see
     * navigate.c's own comment on on_web_process_terminated for the full
     * real-signal/enum verification and rationale) -- both fields are
     * zero-initialized by webview_create's calloc, no explicit init needed.
     *
     * process_terminated: diagnostic-only record of "has this webview's
     * WebProcess ever terminated abnormally at least once" -- NOT a
     * permanent failure gate. A real WebKitWebView lazily relaunches its
     * own WebProcess on the next navigation that needs one (confirmed
     * against WebPageProxy::loadRequest -> launchProcess when
     * !hasRunningProcess(), the same real WebKit source this mitigation was
     * verified against), exactly like real WebView2 recovers into a fresh
     * content process -- so nothing here should ever refuse a later
     * Navigate()/cookie call just because this flag is set. Presently
     * write-only outside of its own set site (on_web_process_terminated) --
     * nothing else in this codebase reads it yet, it exists purely so a
     * future reader/debugger (or a future feature) has a real, honest
     * record of this rather than having to infer it; not a sign it's dead
     * code to remove.
     *
     * active_wait_loop: a borrowed (non-owning) pointer to whichever bounded
     * GMainLoop wait (navigate_and_wait/cookies_delete_all/cookies_count/
     * cookies_get in navigate.c) is currently blocked on THIS webview's
     * WebProcess, or NULL if none is in flight. on_web_process_terminated
     * uses this to wake that wait immediately when the WebProcess it's
     * waiting on dies mid-wait, rather than leaving it to spin out its full
     * 10s/30s timeout for something that can now never arrive -- same
     * "return a real, honest failure" outcome those timeouts already
     * produce, just prompt instead of delayed. */
    gboolean process_terminated;
    GMainLoop *active_wait_loop;
};

/* Creates a new native window + WebKitWebView pair, undecorated, with the
 * close-request signal wired to refuse self-destruction (see webview.c's
 * on_close_request comment). Shows the window unless is_message_only is
 * set (HWND_MESSAGE-parented controllers, e.g. the CookieManager flow,
 * never get a visible top-level). Returns NULL on allocation failure --
 * callers must treat that exactly like any other unix-side "operation
 * failed" case, never dereference it. */
struct native_webview *webview_create(int is_message_only);

/* Destroys nv->window (which tears down nv->view along with it, still its
 * child at that point) and frees the bookkeeping struct itself. No-op if
 * nv is NULL. Unregisters from the live-webview registry first, so any
 * handler racing behind this one on the same dispatch queue sees the
 * handle as no-longer-live rather than dereferencing freed memory. */
void webview_destroy(struct native_webview *nv);

/* Resolves an opaque uint64_t handle (as received over the wire from
 * unixlib.c -- see webview2loader_ipc_protocol.h) back into a real
 * struct native_webview*, but only if it's still a live, registered
 * entry -- protects against a stale/dangling handle the same way the
 * original live_webview_is_valid() did, except this returns the usable
 * pointer instead of just a yes/no answer, since every later dispatch
 * handler (Tasks 5-6) needs the pointer itself, not just a boolean.
 * Returns NULL for handle == 0 or any handle not currently registered. */
struct native_webview *webview_lookup(uint64_t handle);

/* Installs `script_utf8` as a real WebKitUserScript on `nv`, injected at
 * document start in all frames -- the same mechanism the F-08 probe shim
 * already uses, pointed at Studio's own script instead of ours.
 *
 * This is the host half of ICoreWebView2::AddScriptToExecuteOnDocumentCreated,
 * which the Wine side used to accept and then discard. See
 * wv2l_add_user_script_params in webview2loader_ipc_protocol.h for the Toolbox
 * hang that produced.
 *
 * Returns FALSE (and logs) if nv is NULL/stale or the webview has no user
 * content manager; never fatal, matching every other entry point here. */
gboolean webview_add_user_script(struct native_webview *nv, const char *script_utf8);

/* ICoreWebView2Settings2::put_UserAgent. See the definition in webview.c for
 * why the agent decides whether the Toolbox page renders embedded or as the
 * ordinary website. */
gboolean webview_set_user_agent(struct native_webview *nv, const char *user_agent_utf8);

/* Applies the ICoreWebView2Settings properties that map onto real WebKitSettings. */
gboolean webview_apply_settings(struct native_webview *nv, gboolean script_enabled,
                                 gboolean dev_tools_enabled, gboolean context_menus_enabled);

/* Studio -> page half of the web-message channel: delivers `message_utf8` to
 * the page's own window.chrome.webview 'message' listeners. `is_string` picks
 * which postMessage overload it arrives as (a parsed value vs a plain string),
 * mirroring PostWebMessageAsJson / PostWebMessageAsString. */
gboolean webview_post_web_message(struct native_webview *nv, const char *message_utf8, gboolean is_string);

/* Page -> Studio: thin wrapper that stamps the posting page's current URI as
 * the event's source before handing off to navigate.c's event sender. Lives
 * here because only this file knows the WebKitWebView. */
void webview_send_web_message_event(struct native_webview *nv, const char *payload_utf8, gboolean is_string);

#endif /* WV2L_HOST_WEBVIEW_H */
