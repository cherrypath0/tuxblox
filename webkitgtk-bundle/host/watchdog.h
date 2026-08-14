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

/* webkitgtk-bundle/host/watchdog.h
 *
 * Crash fix for a real, reproduced fatal GDK assertion, found identically in
 * two independent real login-flow crash sessions (one predating this
 * process's own out-of-process migration, proving this is a pre-existing
 * bug the migration carried forward faithfully, not something the migration
 * introduced):
 *
 *   Gdk-WARNING **: GdkSurface 0x... unexpectedly destroyed
 *   Gdk:ERROR:../gdk/gdksurface.c:1011:_gdk_surface_destroy_hierarchy:
 *       assertion failed: (priv->egl_native_window == NULL)
 *
 * Root cause: once geometry_sync's own XReparentWindow (round 15, see
 * geometry.c) makes a webview's own GdkSurface/X11 window a genuine X11
 * CHILD of parent_xid (Roblox Studio's own top-level HWND's own "whole
 * window"), that relationship is entirely outside GDK's own control. Per
 * the X11 core protocol, destroying a window destroys its whole subtree --
 * so whenever something else (Wine's own winex11.drv, e.g. while recreating
 * a HWND's own whole_window in response to a style change -- see
 * geometry.c's own "recovering from Wine recreating the parent's
 * whole_window" comment) destroys parent_xid, the X server immediately and
 * unconditionally destroys the reparented webview's own X window right
 * along with it, with no cooperation from GDK at all. GDK only finds out
 * afterwards, asynchronously, when its own X11 event processing (on its
 * OWN separate connection to the X server, gdk_x11_display_get_xdisplay)
 * eventually dequeues the resulting DestroyNotify for a GdkSurface it still
 * considers live -- and because that surface never went through GDK's own
 * CONTROLLED destroy path (gdk_surface_destroy(), which nulls
 * priv->egl_native_window as one of ITS OWN teardown steps before doing the
 * real, low-level X-level destroy itself), GDK's internal consistency check
 * fires and hard-aborts the whole process.
 *
 * GDK4 (unlike GDK3) exposes no public event-filter API
 * (gdk_window_add_filter and equivalents were removed entirely from the
 * public API surface -- confirmed by grepping every installed GDK4 header
 * in this bundle's own prefix; nothing anywhere under gdk/x11/ declares a
 * FilterFunc-suffixed type or an add_filter-named symbol) -- so there is
 * no supported way to
 * intercept/consume the real DestroyNotify event on GDK's OWN connection
 * before GDK's own internal dispatch processes it and hits the assertion.
 * PE-side coordination (Wine telling this process ahead of time that a
 * parent HWND is about to be torn down) is also not an option here -- see
 * this task's own scope discipline notes; no protocol/PE-side file may be
 * touched for this fix.
 *
 * The fix implemented here (approach (b) from this task's own brief: watch
 * parent_xid for DestroyNotify/ReparentNotify and react gracefully before
 * GDK's own async processing gets there) works around the missing public
 * filter API by opening a SECOND, entirely independent Xlib connection to
 * the same X server (same pattern main.c's own log_gl_dispatch_info already
 * uses via its own XOpenDisplay() call for EGL), dedicated solely to
 * selecting StructureNotifyMask on every parent_xid a webview is currently
 * reparented into. Because this is a wholly separate socket from GDK's own,
 * the X server delivers the same DestroyNotify to both connections at
 * essentially the same time, but this connection's own processing here is
 * a single, minimal XPending()/XNextEvent() loop with nothing else to do --
 * far less work than GDK's own multi-stage translate-then-dispatch
 * pipeline on its own connection -- so reacting here and immediately
 * forcing this process's own CONTROLLED teardown (webview_destroy(), same
 * function the normal WV2L_OP_DESTROY_WEBVIEW path already uses, which
 * calls gtk_window_destroy() -- GDK's own real, cooperative destroy
 * entrypoint) has a real, favorable chance of running before GDK's own
 * connection has finished dispatching the same underlying event on its own
 * side. Once webview_destroy() has already removed the surface from GDK's
 * own internal bookkeeping through its OWN controlled path, GDK's later
 * processing of the real (by-then-stale) DestroyNotify on its own
 * connection simply finds no matching live GdkSurface and drops it --
 * never reaching the assertion at all.
 *
 * This is not a mathematically provable win of every possible race -- the
 * X11 protocol gives no cross-connection event-ordering guarantee, and
 * nothing here can un-destroy the reparented window's own X-level resource
 * once the server has already destroyed the whole subtree in one atomic
 * request. What this closes is the STRUCTURAL gap: before this fix, this
 * process had zero code watching for an externally-triggered destroy of a
 * reparented window at all, so the crash was effectively guaranteed
 * whenever the trigger pattern (Wine recreating/destroying a
 * currently-reparented-into parent window) occurred -- confirmed twice in
 * real production trace logs, "reparent check: call #5 ... will reparent:
 * no" immediately followed by the fatal assertion (see this task's own
 * final report for the full trace excerpts). After this fix, that same
 * trigger has a real, structurally-grounded path to a graceful, non-fatal
 * teardown instead of an unconditional crash.
 */
#ifndef WV2L_HOST_WATCHDOG_H
#define WV2L_HOST_WATCHDOG_H

#include "webview.h"

/* Opens the dedicated watcher connection and wires its fd into this
 * process's own GMainLoop (g_unix_fd_add, same integration pattern
 * main.c's own IPC socket already uses). Call once at startup, after
 * gtk_init_check() and XSetErrorHandler() have both already run (same
 * ordering rationale x11_error_handler's own installation already
 * documents -- this connection's own XSelectInput/XNextEvent calls can
 * raise real X11 protocol errors, e.g. a BadWindow if a parent_xid handed
 * to watchdog_track_parent is already stale by the time this runs, and
 * those need the same already-installed non-fatal handler). Degrades
 * gracefully (logs once, leaves every watchdog_track_parent/
 * watchdog_untrack call below a silent no-op) if the second connection
 * can't be opened at all -- geometry_sync's own reparenting still works
 * without this, just without this specific crash mitigation, matching
 * this whole codebase's established never-fatal-to-the-controller
 * pattern. */
void watchdog_init(void);

/* Registers (or re-registers, if parent_xid genuinely changed since the
 * last call for this same nv) a live watch on parent_xid for nv. Called by
 * geometry_sync (geometry.c) right after a real XReparentWindow(...) call
 * actually took nv's own window into a new parent -- i.e. from exactly the
 * same call site that already updates nv->reparented_into. No-op if
 * watchdog_init() never succeeded, or if nv/parent_xid is NULL/0. */
void watchdog_track_parent(struct native_webview *nv, unsigned long parent_xid);

/* Removes any watch entry for nv. Called from webview_destroy() (webview.c)
 * before nv is freed through the NORMAL (WV2L_OP_DESTROY_WEBVIEW-triggered,
 * or this file's own crash-mitigation-triggered) teardown path, so a later
 * DestroyNotify/ReparentNotify for the same (by-then-recycled) parent_xid
 * value can never dereference a dangling nv pointer. Safe to call even if
 * nv was never tracked (e.g. an HWND_MESSAGE-only webview that was never
 * reparented at all) -- a plain, silent no-op in that case. */
void watchdog_untrack(struct native_webview *nv);

#endif /* WV2L_HOST_WATCHDOG_H */
