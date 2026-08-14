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

/* webkitgtk-bundle/host/geometry.h
 *
 * X11 window geometry sync + reparenting, ported from
 * ProtonSource/wine/dlls/webview2loader/unixlib.c as of commit ac3634ea6
 * (see git show ac3634ea6:ProtonSource/wine/dlls/webview2loader/unixlib.c),
 * the last commit before Task 3's rewrite deleted that file's in-process
 * dlmopen/GTK-thread machinery. This is Task 5 of the
 * webview2loader-host-process plan
 * (docs/superpowers/plans/2026-08-14-webview2loader-host-process.md).
 *
 * sync_window_geometry_on_gtk_thread (-> geometry_sync below) is the
 * product of an 18-round, real, hard-won debugging investigation (see
 * .superpowers/sdd/2026-08-13-webview2-window-docking-messaging/progress.md)
 * -- every guard/branch in geometry.c's implementation exists because of a
 * specific, previously-observed crash or visual defect (UAF/TOCTOU races on
 * the shared X11 Display connection, a structurally-impossible >32-bit xid,
 * a real BadWindow X protocol error from a stale parent_xid, and the
 * round-19 ICCCM synthetic-ConfigureNotify fix for GDK's cached surface
 * size never updating after a raw XReparentWindow). It was ported
 * mechanically (p_-prefix dropped, void *data wrapper dropped, hand-declared
 * externs replaced by the real <X11/Xlib.h>/<gdk/x11/gdkx.h> this file now
 * links against), not rewritten -- see geometry.c's own top-of-file comment
 * for the exact real-header substitutions made.
 *
 * Windows BOOL and GLib gboolean are both plain `int` with identical
 * TRUE(1)/FALSE(0) values. This host process has no windef.h (it is not
 * Wine/PE code) -- gboolean, already the established boolean type
 * throughout webview.c/main.c (see e.g. on_close_request, webview_create),
 * stands in for the plan's "BOOL" return type below; this is a type-name
 * substitution only, not a behavior change.
 */
#ifndef WV2L_HOST_GEOMETRY_H
#define WV2L_HOST_GEOMETRY_H

#include "webview.h"
#include "webview2loader_ipc_protocol.h"
#include <X11/Xlib.h>

/* Real position/size/visibility sync for a native webview window -- called
 * (via unixlib.c's IPC client, WV2L_OP_SYNC_WINDOW_GEOMETRY) from
 * put_Bounds/put_IsVisible. `bounds` is already an absolute, root-relative
 * on-screen rect (PE side has already composed ClientToScreen with the
 * client-relative Bounds -- see controller.c's own
 * controller_push_geometry_to_native); `visible` is applied unconditionally
 * before any size/position work; `parent_xid` is the real X11 window ID of
 * the parent HWND's own "whole window" (0 if unavailable), which -- when
 * non-zero -- causes this to reparent the native webview as a genuine X11
 * child of that window instead of leaving it an independent floating
 * top-level. `nv` must already be a resolved, live pointer (or NULL for a
 * stale/unregistered handle -- see webview_lookup) -- same calling
 * convention Task 4 established for webview_destroy.
 *
 * Every real code path in this function's original implementation resolves
 * to TRUE ("never fatal to the controller" -- degrade gracefully instead:
 * no surface yet, not an X11 surface/Wayland, a structurally-impossible
 * xid, no live GdkDisplay, etc. all just skip the position/size work for
 * this one call rather than failing it). That is preserved verbatim here,
 * not narrowed or tightened. */
gboolean geometry_sync(struct native_webview *nv, struct wv2l_rect bounds, gboolean visible, uint64_t parent_xid);

/* Crash fix -- a second, distinct trigger for the same fatal GDK assertion
 * watchdog.h documents (real, reproduced coredump found during this task's
 * own verification: a normal, single-webview Close()/Release() teardown
 * while still X11-reparented into a perfectly alive parent, no external
 * destroy or second controller involved at all). Un-reparents nv->window
 * back to the real X11 root window if (and only if) it is currently
 * reparented into something else (nv->reparented_into != 0), so GDK's own
 * gtk_window_destroy() teardown sequence -- called from webview_destroy()
 * immediately after this -- operates on a surface whose real X11 parent
 * once again matches what GDK's own internal bookkeeping (never updated by
 * geometry_sync's own raw XReparentWindow call in the first place) still
 * believes it to be. See geometry.c's own implementation comment for the
 * full real backtrace and root-cause reasoning. Always returns TRUE
 * (never fatal to the controller) -- a no-op return here just means the
 * caller proceeds straight to its own destroy call exactly as it would
 * without this fix, not a new failure mode. */
gboolean geometry_unreparent(struct native_webview *nv);

/* Test-support only (matches the original's own comment): real GdkSurface
 * width/height readback so a test can confirm geometry_sync actually
 * changed the on-screen window, not just that the call returned success.
 * Returns FALSE for a stale/unregistered nv (NULL) or if no GdkSurface
 * exists yet for the window. */
gboolean geometry_get(struct native_webview *nv, struct wv2l_rect *out);

/* Task 7 crash fix, round 17/18 (original unixlib.c comment): a real,
 * reproducible crash -- XReparentWindow (round 15) against a stale/invalid
 * parent_xid produces a genuine X11 protocol error (BadWindow), and Xlib's
 * own default error handler treats ANY X protocol error as FATAL
 * (exit()/abort() on the whole process). parent_xid is a value this
 * process does NOT own or control the lifecycle of (Wine's own winex11.drv,
 * reading a value that could be stale) -- exactly the kind of
 * externally-sourced, not-fully-trustworthy value real, defensive Xlib code
 * brackets with a custom, non-fatal error handler. Defined in geometry.c
 * (colocated with x11_error_seen_during_call, its only reader, inside
 * geometry_sync); installed via XSetErrorHandler from main.c's own startup
 * sequence, right after gtk_init_check() succeeds -- so it overrides
 * whatever handler GDK's own X11 backend init installed for itself, rather
 * than being silently clobbered BY it (same ordering rationale the original
 * gtk_thread_proc used). */
int x11_error_handler(Display *display, XErrorEvent *event);

#endif /* WV2L_HOST_GEOMETRY_H */
