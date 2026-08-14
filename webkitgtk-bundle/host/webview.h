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
    GtkWidget *window;
    WebKitWebView *view;
    unsigned long reparented_into;
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

#endif /* WV2L_HOST_WEBVIEW_H */
