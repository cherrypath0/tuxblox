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

/* webkitgtk-bundle/host/geometry.c
 *
 * X11 window geometry sync + reparenting -- ported from
 * ProtonSource/wine/dlls/webview2loader/unixlib.c as of commit ac3634ea6
 * (see git show ac3634ea6:ProtonSource/wine/dlls/webview2loader/unixlib.c).
 * This is Task 5 of the webview2loader-host-process plan
 * (docs/superpowers/plans/2026-08-14-webview2loader-host-process.md).
 *
 * Mapping from the old file (all renames/removals purely mechanical, per
 * Task 5's brief -- no logic changes beyond what's noted):
 *   - sync_window_geometry_on_gtk_thread -> geometry_sync, struct
 *     sync_window_geometry_params/void *data wrapper dropped (params->handle
 *     becomes an already-resolved struct native_webview *nv, same calling
 *     convention Task 4 established for webview_destroy/webview_lookup;
 *     params->screen_bounds/visible/parent_xid become plain arguments;
 *     params->success becomes the gboolean return value)
 *   - get_window_geometry_on_gtk_thread  -> geometry_get, same treatment
 *   - live_webview_is_valid(nv)          -> nv itself is already the
 *     webview_lookup() result by the time it reaches this file (main.c's
 *     dispatch case resolves the handle first, same pattern as
 *     WV2L_OP_DESTROY_WEBVIEW/WV2L_OP_GET_WINDOW_VISIBLE) -- so the old
 *     "stale handle" guard becomes a plain `if (!nv)` here, structurally
 *     the same check, just performed once by the caller instead of
 *     re-derived from a raw handle inside this file
 *   - x11_error_handler, x11_error_seen_during_call -> ported verbatim,
 *     kept together (the flag is this function's only reader/writer other
 *     than the handler itself); XSetErrorHandler(x11_error_handler) itself
 *     is installed from main.c's startup sequence now (no gtk_thread_proc
 *     to install it from anymore) -- see geometry.h's own comment on
 *     x11_error_handler
 *   - GTK_THREAD_LOG(...)  -> plain fprintf(stderr, "webview2loader-host: "
 *     ...) calls, matching webview.c/main.c's own established diagnostic
 *     convention in this process. GTK_THREAD_LOG existed ONLY because the
 *     original code ran on a raw pthread_create() thread with no
 *     Wine-initialized TEB, which made Wine's own WARN/ERR/TRACE/FIXME
 *     macros unsafe to call there (see that macro's own original comment,
 *     preserved in ac3634ea6's unixlib.c for the full crash forensics).
 *     This helper process is not Wine code at all and has no TEB/thread
 *     model dependency of any kind -- the reason GTK_THREAD_LOG existed
 *     does not apply here, so this substitution is purely mechanical, not a
 *     simplification of any of this function's real, hard-won X11 logic.
 *   - hand-declared X11 externs (XMoveResizeWindow, XReparentWindow,
 *     XTranslateCoordinates, XDefaultRootWindow, XSetErrorHandler, XSync,
 *     XSendEvent, XLockDisplay/XUnlockDisplay, XInitThreads) and the
 *     hand-declared XErrorEvent/XConfigureEvent structs -> replaced by a
 *     real #include <X11/Xlib.h> (geometry.h) -- the whole reason for the
 *     hand-declared-extern convention (no real headers available under
 *     dlmopen) no longer applies once this file links normally. The
 *     synthetic ConfigureNotify (round 19) now builds a real XEvent union
 *     and sets its .xconfigure member (real Xlib client code's own idiom),
 *     rather than casting a hand-rolled struct that merely matched
 *     XConfigureEvent's layout by hand -- same fields, same values, real
 *     union instead of a look-alike struct. TUXBLOX_ConfigureNotify/
 *     TUXBLOX_StructureNotifyMask (hand-declared X11 core protocol
 *     constants) -> the real ConfigureNotify/StructureNotifyMask macros
 *     from <X11/X.h> (pulled in transitively by <X11/Xlib.h>/<gdk/x11/gdkx.h>).
 *   - hand-declared GDK/GTK X11-bridge externs (gtk_widget_get_native,
 *     gtk_native_get_surface, gdk_surface_get_display,
 *     gdk_surface_get_width/height, gdk_x11_surface_get_xid,
 *     gdk_x11_display_get_xdisplay, gdk_display_is_closed,
 *     gtk_widget_set_visible, gtk_window_set_default_size,
 *     gtk_widget_get_realized/get_mapped, g_object_ref/unref) -> real calls
 *     against <gdk/x11/gdkx.h> (the real GTK4/GDK4 X11 backend header) and
 *     <gtk/gtk.h> (already pulled in via webview.h), same as Task 4 already
 *     did for the plain GTK/WebKit calls it ported.
 *
 * Every other guard, comment, and log message below is preserved as
 * closely as a mechanical port allows -- this is 18+ rounds of real,
 * hard-won debugging (UAF/TOCTOU races on the shared X11 Display
 * connection, a structurally-impossible >32-bit xid, a real BadWindow X
 * protocol error from a stale parent_xid, and the round-19 ICCCM
 * synthetic-ConfigureNotify fix), not a place to "clean up" or restructure.
 */
#include "geometry.h"
#include "watchdog.h"
#include <gdk/x11/gdkx.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

/* Task 7 diagnostic (round 7) -- see this counter's own use, further down
 * in geometry_sync, for why. File-scope so it persists/accumulates across
 * every call for the life of the process. Unlike the original (which ran
 * on a single serialized "GTK thread" separate from every PE-calling
 * thread), this whole helper process's main() *is* the single GLib
 * main-loop thread every IPC dispatch handler (including this one) runs
 * serialized on -- see webview.c's own live_webviews comment for the same
 * observation -- so this still needs no lock. */
static unsigned long xmove_call_count;

/* Code review fix (cleanup, round 11): the round 9-11 diagnostic log calls
 * fire on every successful call -- gated behind an explicit opt-in env var
 * instead of firing unconditionally or being deleted outright, still
 * genuinely useful for whoever picks the xid investigation back up.
 * Checked once, lazily, cached here -- this function only ever runs on the
 * single serialized main-loop thread, so no lock is needed for the cache
 * either. */
static int geometry_debug_enabled = -1; /* -1 = not yet checked */

static gboolean geometry_debug_on(void)
{
    if (geometry_debug_enabled < 0)
        geometry_debug_enabled = getenv("TUXBLOX_WEBVIEW_GEOMETRY_DEBUG") ? 1 : 0;
    return geometry_debug_enabled;
}

/* Task 7 crash fix, round 18 -- see x11_error_handler's own comment (below,
 * and in geometry.h) for why this exists. File-scope, same
 * single-serialized-thread reasoning as xmove_call_count above -- no lock
 * needed. */
static gboolean x11_error_seen_during_call;

/* Task 7 crash fix, round 17 (original unixlib.c comment, preserved): a
 * real, reproducible crash found via the fast unit-test pass: XReparentWindow
 * (round 15) against a stale/invalid parent_xid produced a genuine X11
 * protocol error (BadWindow, request code 7 == ReparentWindow), and Xlib's
 * OWN default error handler (installed by GDK during its own init, or
 * Xlib's own built-in default if nothing else claimed it) treats ANY X
 * protocol error as FATAL -- calling exit()/abort() on the whole process.
 * This is a real, serious hazard specifically for round 15's own new
 * XReparentWindow call: unlike XMoveResizeWindow (always operating on a
 * window this process itself created and fully owns the lifecycle of),
 * XReparentWindow's second argument is a window this process does NOT own
 * or control the lifecycle of at all (Wine's own winex11.drv, reading a
 * value that could be stale -- see parent_xid's own comment in geometry.h
 * on whole_window recreation) -- exactly the kind of externally-sourced,
 * not-fully-trustworthy value real, defensive Xlib code brackets with a
 * custom, non-fatal error handler. Installed via XSetErrorHandler from
 * main.c's own startup sequence, AFTER gtk_init_check() succeeds (so it
 * overrides whatever handler GDK's own X11 backend init installed for
 * itself, rather than being overridden BY it), covering every raw Xlib
 * call this process makes from that point on, not just XReparentWindow
 * specifically. Deliberately generic ("an X11 request failed") rather than
 * a NULL-checked display name/request-code table lookup: the goal here is
 * solely "don't let one bad window ID committed by external,
 * not-fully-trustworthy state take down the whole process," not a full
 * diagnostic decoder -- request_code/error_code are still logged raw for
 * anyone reading the log to cross-reference against
 * /usr/include/X11/Xproto.h by hand if needed. */
int x11_error_handler(Display *display, XErrorEvent *event)
{
    (void)display;
    fprintf(stderr, "webview2loader-host: Xlib reported a non-fatal X11 protocol error "
                    "(request_code=%u minor_code=%u error_code=%u serial=%lu) -- ignoring "
                    "instead of letting Xlib's own default handler abort the process\n",
                    event ? (unsigned)event->request_code : 0,
                    event ? (unsigned)event->minor_code : 0,
                    event ? (unsigned)event->error_code : 0,
                    event ? event->serial : 0UL);
    /* Task 7 crash fix, round 18: the repo owner's own reviewer correctly
     * flagged this handler as the prime suspect for turning a real, silent
     * XReparentWindow failure into an invisible no-op -- exactly the
     * tradeoff a non-fatal error handler makes. Setting this flag (checked
     * immediately after XReparentWindow + XSync, see that call site's own
     * comment in geometry_sync) makes that tradeoff observable again
     * instead of genuinely invisible. */
    x11_error_seen_during_call = TRUE;
    return 0;
}

/* Plan 3 Task 3: real position/size/visibility sync, called (via
 * unixlib.c's IPC client) from put_Bounds/put_IsVisible. See geometry.h's
 * own doc comment for the full calling-convention description. */
gboolean geometry_sync(struct native_webview *nv, struct wv2l_rect bounds, gboolean visible, uint64_t parent_xid)
{
    GtkNative *native;
    GdkSurface *surface;
    GdkDisplay *gdisplay;
    unsigned long xid;
    Display *display;
    int width = bounds.right - bounds.left;
    int height = bounds.bottom - bounds.top;

    /* Task 7 UAF guard -- see geometry.c's own top-of-file mapping comment
     * for why this is now a plain NULL check rather than a re-derived
     * registry lookup: main.c's dispatch case already resolved `nv` via
     * webview_lookup() before calling here, same pattern Task 4 established
     * for webview_destroy. This is the exact call site a real Studio
     * relaunch crashed in twice in the original implementation (first via a
     * NULL Display*, then confirmed via coredump/GDB analysis to be a stale
     * handle: XMoveResizeWindow's `xid` argument read back as a
     * heap-pointer-shaped value, not a plausible X11 resource id -- the
     * fingerprint of dereferencing already-freed memory). */
    if (!nv)
    {
        fprintf(stderr, "webview2loader-host: stale/destroyed native window handle -- skipping geometry sync\n");
        return TRUE; /* never fatal to the controller, matches this
                       * function's existing degrade-gracefully pattern */
    }

    /* 2026-08-15 flicker investigation diagnostic: log every real transition
     * of the `visible` flag this function receives from put_Bounds/
     * put_IsVisible (i.e. what Roblox Studio itself is asking for), not
     * every call -- put_Bounds fires very frequently and most calls repeat
     * the same visible value. A real repro (video evidence: the reparented
     * webview goes completely blank for a fraction of a second, correlated
     * with the pointer being outside Roblox Studio's own window, recovering
     * once it's back inside) is consistent with Studio itself toggling
     * IsVisible around hover/focus state, and gtk_widget_set_visible(FALSE)
     * then TRUE again causing a real GTK4 unmap/remap repaint gap. This
     * confirms or refutes that without guessing further. Remove once
     * answered either way. */
    {
        static gboolean last_visible = TRUE;
        static gboolean have_last = FALSE;
        if (!have_last || last_visible != visible)
        {
            fprintf(stderr, "webview2loader-host: geometry_sync: visible transition %s -> %s\n",
                            have_last ? (last_visible ? "TRUE" : "FALSE") : "(none)",
                            visible ? "TRUE" : "FALSE");
            have_last = TRUE;
            last_visible = visible;
        }
    }

    /* Visibility is applied unconditionally, before the `visible` guard
     * below, so a transition to hidden actually hides the window on this
     * same call rather than requiring a separate one -- the guard right
     * after only short-circuits the size/position work that's meaningless
     * once the window is (or is becoming) invisible. */
    gtk_widget_set_visible(nv->window, visible);
    if (!visible)
        return TRUE; /* nothing more to sync while hidden */

    if (width > 0 && height > 0)
        gtk_window_set_default_size(GTK_WINDOW(nv->window), width, height);

    /* Position: X11-only -- GTK4 removed GTK3's gtk_window_move/
     * gtk_window_resize/gdk_window_move_resize entirely and has no
     * cross-backend equivalent, so this resolves the toplevel's real X11
     * XID via gdk_x11_surface_get_xid and moves/resizes it directly via
     * Xlib's XMoveResizeWindow. Degrades gracefully (size/visibility still
     * applied above) under Wayland. */
    native = gtk_widget_get_native(nv->window);
    surface = native ? gtk_native_get_surface(native) : NULL;
    if (!surface)
    {
        fprintf(stderr, "webview2loader-host: no GdkSurface yet for native window %p -- skipping position sync\n",
                (void *)nv->window);
        return TRUE;
    }

    /* Task 7 crash fix round 6: hold a real GObject reference on `surface`
     * (and, below, on `gdisplay`) for the entire span these are used,
     * instead of just re-checking validity right before each use --
     * closes a real TOCTOU race structurally (WebKitGTK's own auxiliary
     * compositing/GL threads remain the leading suspect for what else can
     * touch this same shared X11 connection concurrently -- unchanged in
     * this new process, since it's the same WebKitGTK library). Released
     * at every exit below and at the end of the function. */
    g_object_ref(surface);

    /* Task 7 crash fix, round 11: real-time object-graph diagnostic. */
    if (geometry_debug_on())
        fprintf(stderr, "webview2loader-host: object graph before gdk_x11_surface_get_xid: nv=%p nv->window=%p "
                        "native=%p surface=%p realized=%d mapped=%d\n",
                        (void *)nv, (void *)nv->window, (void *)native, (void *)surface,
                        gtk_widget_get_realized(nv->window), gtk_widget_get_mapped(nv->window));

    /* Real-header build finding (Task 5, analogous to Task 4's own
     * gtk_widget_show()/gtk_widget_set_visible() deprecation substitution):
     * this bundle's real, installed <gdk/x11/gdkx11surface.h> marks
     * gdk_x11_surface_get_xid GDK_DEPRECATED_IN_4_18, hard-erroring under
     * this build's -Werror. Unlike gtk_widget_show, there is no
     * behaviorally-equivalent non-deprecated replacement: GDK4's own public
     * API genuinely has no other way to obtain a real X11 XID at all (see
     * this file's own top-of-file mapping comment, and the original
     * unixlib.c's extensive comment on why this whole feature has to go
     * through raw Xlib in the first place) -- the entire real-embedding
     * feature this function exists for (XReparentWindow/XMoveResizeWindow/
     * the round-19 synthetic ConfigureNotify) is structurally impossible
     * without it. Scoped tightly to just this one call (and
     * gdk_x11_display_get_xdisplay below, same situation) rather than a
     * blanket -Wno-error=deprecated-declarations on the whole file, so any
     * *other* accidental use of deprecated API elsewhere in this file still
     * hard-errors as intended. */
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
    xid = gdk_x11_surface_get_xid(surface);
#pragma GCC diagnostic pop
    if (!xid)
    {
        /* Not an X11 surface (Wayland) -- gdk_x11_surface_get_xid already
         * logged GLib's own non-fatal CRITICAL. */
        g_object_unref(surface);
        return TRUE;
    }

    /* Task 7 crash fix, round 10: X11 resource IDs are a protocol-level
     * 32-bit quantity (core protocol section 2.3, "Every RESOURCE ... is a
     * 32-bit value") -- ANY value here that doesn't fit in 32 bits is
     * proof-positive not a real XID, full stop (surface is structurally
     * guaranteed live here -- ref'd via g_object_ref above before this
     * call -- so this isn't a freed `surface`; whatever's wrong lives
     * inside gdk_x11_surface_get_xid's own read, or in memory it reads
     * through). Degrades the same way the !xid check just above already
     * does for the "not an X11 surface" case -- never fatal to the
     * controller. */
    if (xid > 0xffffffffUL)
    {
        fprintf(stderr, "webview2loader-host: xid %lu (0x%lx) for native window %p exceeds the 32-bit X11 "
                        "resource ID range -- not a real XID, skipping position sync\n",
                        xid, xid, (void *)nv->window);
        g_object_unref(surface);
        return TRUE;
    }

    /* Code review fix (Important 1): mirror the width>0 && height>0 guard
     * gtk_window_set_default_size already gets above -- a non-positive
     * width or height here (e.g. a transient zero-size Bounds update that
     * arrives before the controller is actually hidden) would otherwise be
     * cast to `unsigned int` for XMoveResizeWindow, wrapping a negative
     * value into a huge one; X11's ConfigureWindow protocol requires
     * width/height >= 1 (0 is BadValue), and there is no guarantee the
     * server tolerates the wrapped huge value either. Skipping the move
     * (rather than clamping to 1x1) matches this function's existing
     * degrade-gracefully pattern -- never fatal to the controller.
     *
     * Code review fix (cleanup): the GdkDisplay lookup/liveness-check/ref
     * and the raw Display* lookup below are only ever needed for the
     * actual XMoveResizeWindow call inside this same guard -- kept inside
     * it (wasted work on every no-op zero-size call otherwise). */
    if (width > 0 && height > 0)
    {
        /* Task 7 crash fix round 5: check the GdkDisplay CONNECTION's own
         * liveness before converting it to a raw Display* at all. The
         * existing live_webviews registry only covers the per-webview
         * handle, not this shared, longer-lived connection object -- if
         * something closes it between whenever GDK last touched it and
         * this call, dereferencing it here is a real, structurally-
         * analogous UAF to the one that registry already fixed for `nv`,
         * just one level up the object graph. (Round 6: this check alone
         * wasn't sufficient either, see the g_object_ref comment above --
         * kept anyway as a fast, cheap early-out; the ref taken just below
         * is what actually closes the race.) */
        gdisplay = gdk_surface_get_display(surface);
        if (!gdisplay || gdk_display_is_closed(gdisplay))
        {
            fprintf(stderr, "webview2loader-host: no live GdkDisplay for native window %p -- skipping "
                            "position sync\n", (void *)nv->window);
            g_object_unref(surface);
            return TRUE;
        }
        g_object_ref(gdisplay); /* see the g_object_ref comment on `surface` above */

        /* Task 7 real-launch crash fix: gdk_x11_display_get_xdisplay can
         * return NULL here even though the xid lookup above already
         * succeeded (best-effort root cause points at a stale/UAF'd
         * surface rather than a legitimate GDK_IS_X11_DISPLAY backend
         * mismatch, since a real X11 GdkSurface's display can't validly be
         * a non-X11 backend). Passing a NULL Display* straight into
         * XMoveResizeWindow segfaults inside libX11 (_XGetRequest
         * dereferencing display->request). Guard it the same
         * degrade-gracefully way as the !surface/!xid checks above --
         * never fatal to the controller. */
        /* Same real-header deprecation situation as gdk_x11_surface_get_xid
         * above (GDK_DEPRECATED_IN_4_18, no non-deprecated replacement
         * exists for obtaining a raw Xlib Display* either) -- see that
         * call site's own comment. */
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
        display = gdk_x11_display_get_xdisplay(gdisplay);
#pragma GCC diagnostic pop
        if (!display)
        {
            fprintf(stderr, "webview2loader-host: no Display* for native window %p -- skipping position "
                            "sync\n", (void *)nv->window);
            g_object_unref(gdisplay);
            g_object_unref(surface);
            return TRUE;
        }

        /* Task 7 UAF/Xlib-locking fix round 3: this raw Xlib call bypasses
         * GDK's own request serialization entirely, so it has to bracket
         * itself with XLockDisplay/XUnlockDisplay against whatever else
         * (WebKitGTK's own auxiliary compositing/GL threads are the
         * leading suspect) may be using this same Display* connection
         * concurrently -- otherwise this is exactly the kind of
         * unsynchronized access that corrupts Xlib's per-Display request
         * buffers and segfaults deep inside _XGetRequest. Rounds 4-6
         * (g_object_ref on `surface`/`gdisplay` above) close the
         * underlying TOCTOU race structurally; this locking is still
         * needed for the raw request itself. */
        /* Task 7 diagnostic, round 9: also dumps the first 32 raw bytes at
         * `display` itself -- not because this file knows Xlib's private
         * struct layout (it deliberately doesn't), just as a raw
         * byte-level sanity check: if a future crash's coredump can be
         * compared against a LOGGED "last known good" dump of the same
         * pointer, a mismatch would itself be evidence of corruption
         * between calls, no struct knowledge required to observe that
         * much. */
        ++xmove_call_count;
        if (geometry_debug_on())
        {
            fprintf(stderr, "webview2loader-host: before position sync: call #%lu xid=%lu display=%p "
                            "parent_xid=%lu reparented_into=%lu bytes=%016lx %016lx %016lx %016lx\n",
                            xmove_call_count, xid, (void *)display, (unsigned long)parent_xid,
                            nv->reparented_into,
                            *(unsigned long *)display,
                            *(unsigned long *)((char *)display + 8),
                            *(unsigned long *)((char *)display + 16),
                            *(unsigned long *)((char *)display + 24));
        }
        XLockDisplay(display);
        if (geometry_debug_on())
            fprintf(stderr, "webview2loader-host: after XLockDisplay: call #%lu (locked ok)\n", xmove_call_count);

        /* Task 7 crash fix, round 15: real X11-level reparenting -- see
         * controller.c's own controller_push_geometry_to_native and
         * parent_xid's own comment in geometry.h for the full rationale
         * (repo owner's direct ask: real embedding, not a
         * manually-repositioned floating window). Reparented once per
         * distinct parent_xid value (not every call) -- XReparentWindow
         * itself is a real, visible operation on the X server (briefly
         * unmaps/remaps in some WM implementations), so only doing it when
         * the target actually changes avoids redundant server round-trips
         * on every single put_Bounds call. Comparing against
         * nv->reparented_into (not just "have we ever reparented") also
         * means a genuinely changed parent_xid -- including recovering
         * from Wine recreating the parent's whole_window -- gets a fresh
         * reparent on the very next call. */
        /* Task 7 crash fix, round 18: unconditional (not
         * geometry_debug_on()-gated) logging around the reparent decision
         * and the XReparentWindow call itself -- added after a real
         * relaunch showed ZERO evidence either way with debug logging
         * gated off. This makes all of that directly observable on every
         * run, not just debug-flagged ones -- cheap enough (once per
         * put_Bounds call, not per frame) to always pay for. */
        fprintf(stderr, "webview2loader-host: reparent check: call #%lu parent_xid=0x%lx "
                        "reparented_into=0x%lx (will reparent: %s)\n",
                        xmove_call_count, (unsigned long)parent_xid, nv->reparented_into,
                        (parent_xid && nv->reparented_into != parent_xid) ? "YES" : "no");
        if (parent_xid && nv->reparented_into != parent_xid)
        {
            int reparent_status;
            x11_error_seen_during_call = FALSE;
            fprintf(stderr, "webview2loader-host: XReparentWindow: call #%lu ABOUT TO CALL xid=0x%lx into "
                            "parent_xid=0x%lx\n", xmove_call_count, xid, (unsigned long)parent_xid);
            reparent_status = XReparentWindow(display, xid, parent_xid, 0, 0);
            /* XReparentWindow is asynchronous (like almost all core X11
             * requests) -- a real protocol-level failure (e.g. BadWindow if
             * parent_xid is stale/invalid) is delivered later, out-of-band,
             * to the error handler round 17 installed, not via this call's
             * own return value. XSync forces the request (and any error
             * reply for it) to round-trip to completion before we check
             * the flag below, so "did the error handler fire during this
             * specific call" is actually meaningful instead of racing
             * ahead of the server's real response. */
            XSync(display, 0);
            fprintf(stderr, "webview2loader-host: XReparentWindow: call #%lu RETURNED status=%d "
                            "x11_error_during_call=%s\n", xmove_call_count, reparent_status,
                            x11_error_seen_during_call ? "YES -- see x11_error_handler's own log line above "
                            "for details" : "no");
            nv->reparented_into = parent_xid;

            /* Crash fix (see watchdog.h's own top-of-file comment for the
             * full mechanism this defends against): now that nv->window is
             * a real X11 child of parent_xid, arm the watchdog's own
             * dedicated connection to notice if parent_xid is ever
             * destroyed out from under it (Wine recreating the parent's
             * own whole_window, e.g.) BEFORE GDK's own async event
             * processing on ITS OWN connection discovers the same fait
             * accompli and hits a fatal internal-consistency assertion.
             * Only armed on a REAL reparent (this branch, not every
             * geometry_sync call) -- the watch itself persists at the X
             * server for as long as this process's watchdog connection
             * stays open, it does not need re-arming on every call for the
             * same still-current parent_xid. */
            watchdog_track_parent(nv, parent_xid);
        }

        if (parent_xid)
        {
            /* Once reparented, XMoveResizeWindow's x/y become relative to
             * the NEW parent's own origin, not the root window's -- bounds
             * is still an absolute (root-relative) rect (controller.c's
             * own ClientToScreen-based computation, unchanged), so
             * translate it into parent_xid's own coordinate space via real
             * X11 coordinate translation rather than guessing/hardcoding
             * whatever offset the parent's own window-manager decorations
             * or Wine-drawn chrome introduce between its top-left corner
             * and the root window's origin. Falls back to the untranslated
             * absolute position (the old floating-window-era behavior) if
             * the translation call itself fails -- degrade gracefully,
             * never fatal, matching this function's own established
             * pattern throughout. */
            int dest_x = bounds.left, dest_y = bounds.top;
            unsigned long child_return;

            if (!XTranslateCoordinates(display, XDefaultRootWindow(display), parent_xid,
                                        bounds.left, bounds.top, &dest_x, &dest_y, &child_return))
            {
                fprintf(stderr, "webview2loader-host: XTranslateCoordinates failed for parent_xid=%lu -- "
                                "falling back to untranslated absolute position\n", (unsigned long)parent_xid);
                dest_x = bounds.left;
                dest_y = bounds.top;
            }
            XMoveResizeWindow(display, xid, dest_x, dest_y, (unsigned int)width, (unsigned int)height);

            /* Task 7 crash fix, round 19: the real fix for a genuine,
             * evidence-confirmed bug -- put_Bounds after reparenting
             * visibly changed the real X11 window size (confirmed via a
             * direct XGetGeometry readback: immediately after this exact
             * XMoveResizeWindow call, the real server-side geometry was
             * already correct), but GDK's own cached notion of the
             * surface's size (gdk_surface_get_width/height, which is what
             * both a test's readback AND -- critically -- GTK's own
             * internal widget size-allocate/layout cycle for the embedded
             * WebKitWebView are driven from) never updated, staying at
             * GTK4's original default-window size from realize time no
             * matter how many further resizes were issued. Root cause:
             * ICCCM. Per the ICCCM (the decades-old convention every real
             * X11 window manager and toolkit follows), a client's toplevel
             * only trusts a *synthetic* (XSendEvent-delivered,
             * send_event=True) ConfigureNotify as authoritative for its own
             * on-screen geometry -- a *real*, server-generated one is
             * defined by the ICCCM to report coordinates relative to
             * whatever the window's CURRENT immediate parent happens to be
             * (meaningless for absolute placement once reparented), so
             * well-behaved toolkits, GDK's X11 backend included, are
             * SUPPOSED to ignore a real ConfigureNotify for a toplevel and
             * wait for the WM's synthetic one instead. A real window
             * manager sends that synthetic event whenever it moves/resizes
             * a window it manages -- exactly why the OTHER branch below (no
             * parent_xid, still a WM-managed independent top-level, this
             * function's pre-round-15 behavior) has always worked
             * correctly. Once round 15 reparented nv->window into an
             * arbitrary application HWND instead of leaving it WM-managed,
             * nothing plays the WM's role of sending that synthetic
             * event -- so GDK's cache silently never updates again, even
             * though the real window keeps moving/resizing correctly at
             * the X11 protocol level the whole time. Fix: send it
             * ourselves, exactly like a real window manager would, with
             * ROOT-relative x/y (bounds is already in that space -- an
             * ICCCM synthetic ConfigureNotify's x/y are defined as
             * root-relative regardless of what the window's real immediate
             * parent is) and the real width/height we just set. Built as a
             * real XEvent union (.xconfigure member) now that a real
             * <X11/Xlib.h> is available, rather than the original's
             * hand-rolled XConfigureEvent-shaped struct cast through a
             * void* -- same fields, same values, real union instead of a
             * look-alike. */
            {
                XEvent synthetic_configure;
                memset(&synthetic_configure, 0, sizeof(synthetic_configure));
                synthetic_configure.xconfigure.type = ConfigureNotify;
                synthetic_configure.xconfigure.send_event = True;
                synthetic_configure.xconfigure.display = display;
                synthetic_configure.xconfigure.event = xid;
                synthetic_configure.xconfigure.window = xid;
                synthetic_configure.xconfigure.x = bounds.left;
                synthetic_configure.xconfigure.y = bounds.top;
                synthetic_configure.xconfigure.width = width;
                synthetic_configure.xconfigure.height = height;
                synthetic_configure.xconfigure.border_width = 0;
                synthetic_configure.xconfigure.above = None;
                synthetic_configure.xconfigure.override_redirect = False;
                XSendEvent(display, xid, False, StructureNotifyMask, &synthetic_configure);
                if (geometry_debug_on())
                    fprintf(stderr, "webview2loader-host: sent synthetic ConfigureNotify: call #%lu x=%d "
                                    "y=%d w=%d h=%d\n", xmove_call_count, synthetic_configure.xconfigure.x,
                                    synthetic_configure.xconfigure.y, width, height);
            }
        }
        else
        {
            /* No valid parent_xid this call (message-only controller, no
             * parent_window, or __wine_x11_whole_window not found/set yet)
             * -- degrade to the original floating-window behavior:
             * absolute screen position, no reparenting. Matches this
             * function's own pre-round-15 behavior exactly. */
            XMoveResizeWindow(display, xid, bounds.left, bounds.top, (unsigned int)width, (unsigned int)height);
        }

        if (geometry_debug_on())
            fprintf(stderr, "webview2loader-host: after XMoveResizeWindow: call #%lu returned "
                            "successfully\n", xmove_call_count);
        XUnlockDisplay(display);
        if (geometry_debug_on())
            fprintf(stderr, "webview2loader-host: after XUnlockDisplay: call #%lu (unlocked ok)\n",
                            xmove_call_count);
        g_object_unref(gdisplay);
    }
    g_object_unref(surface);
    return TRUE;
}

/* Crash fix -- a SECOND, distinct trigger for the same fatal GDK assertion
 * watchdog.h/.c defend against (see that file's own top-of-file comment for
 * the full mechanism), found via a real, reproduced coredump during this
 * task's own verification pass, entirely independent of anything the
 * watchdog watches for: test_controller_parent_window's own normal
 * ICoreWebView2Controller_Release(ctrl) at the end of a real reparent ->
 * destroy sequence produced this real backtrace --
 *
 *   #3  abort (libc.so.6)
 *   #4  g_assertion_message_expr (libglib-2.0.so.0)
 *   #5-#9 (libgtk-4.so.1, GtkWindow's own dispose/destroy chain)
 *   #10-#13 g_signal_emit_by_name (libgobject-2.0.so.0)
 *   #14-#16 (libgtk-4.so.1)
 *
 * -- with the real X11 parent (real_parent in the test, Studio's own
 * top-level HWND's whole_window in production) still fully alive and
 * untouched at the moment of the crash. No external destroy, no second
 * controller, no racing geometry_sync call -- this fires purely from THIS
 * process's own, entirely normal WV2L_OP_DESTROY_WEBVIEW -> webview_destroy()
 * -> gtk_window_destroy() call, whenever nv->window is still a real X11
 * child of some other window at that moment (i.e. nv->reparented_into != 0
 * and never reset). Root cause: geometry_sync's own round-15 XReparentWindow
 * (see that function's own comment) is a raw Xlib call, entirely out-of-band
 * from GDK's own object model -- GDK's own internal bookkeeping for this
 * surface's parent/hierarchy relationship is never told about it. GTK4's own
 * gtk_window_destroy() teardown sequence (accelerated-compositing/EGL
 * resource teardown included) still believes and relies on this surface
 * having whatever parent relationship GDK itself last set up (effectively
 * "real top-level, GDK-managed"); once that's silently no longer true, the
 * same internal-consistency assertion watchdog.c defends against for an
 * externally-triggered destroy can just as easily fire from OUR OWN
 * self-triggered one.
 *
 * Fix: restore the X11 hierarchy to what GDK's own bookkeeping still
 * believes it is -- an ordinary top-level, effectively parented to the real
 * X11 root window -- via a plain XReparentWindow back to root BEFORE
 * gtk_window_destroy() runs, so GDK's own teardown sequence operates on a
 * surface whose real, current X11 parent genuinely matches its own internal
 * assumptions again. Called from webview_destroy() (webview.c), immediately
 * before its own gtk_window_destroy() call. Degrades gracefully (never
 * fatal to the controller) through every guard below, matching
 * geometry_sync's own established pattern throughout this file -- a no-op
 * return here just means webview_destroy() proceeds straight to
 * gtk_window_destroy() exactly as it did before this fix existed, not a new
 * failure mode. */
gboolean geometry_unreparent(struct native_webview *nv)
{
    GtkNative *native;
    GdkSurface *surface;
    GdkDisplay *gdisplay;
    Display *display;
    unsigned long xid;
    Window root;

    if (!nv || !nv->reparented_into) return TRUE; /* never reparented -- nothing to undo */

    native = gtk_widget_get_native(nv->window);
    surface = native ? gtk_native_get_surface(native) : NULL;
    if (!surface) return TRUE;

    /* Same TOCTOU-closing ref pattern geometry_sync's own comment documents
     * in detail above -- held for this function's entire span, released at
     * every exit. */
    g_object_ref(surface);

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
    xid = gdk_x11_surface_get_xid(surface);
#pragma GCC diagnostic pop
    if (!xid || xid > 0xffffffffUL)
    {
        /* Not a real X11 surface, or a structurally-impossible xid -- see
         * geometry_sync's own comments on both checks above for the full
         * rationale, identical reasoning applies here. */
        g_object_unref(surface);
        return TRUE;
    }

    gdisplay = gdk_surface_get_display(surface);
    if (!gdisplay || gdk_display_is_closed(gdisplay))
    {
        g_object_unref(surface);
        return TRUE;
    }
    g_object_ref(gdisplay);

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
    display = gdk_x11_display_get_xdisplay(gdisplay);
#pragma GCC diagnostic pop
    if (!display)
    {
        g_object_unref(gdisplay);
        g_object_unref(surface);
        return TRUE;
    }

    XLockDisplay(display); /* same shared-connection locking rationale as geometry_sync's own XLockDisplay */
    root = XDefaultRootWindow(display);
    fprintf(stderr, "webview2loader-host: un-reparenting nv=%p (xid=0x%lx) back to root=0x%lx before "
                    "destroy -- was reparented into 0x%lx\n", (void *)nv, xid, (unsigned long)root,
                    nv->reparented_into);
    x11_error_seen_during_call = FALSE;
    XReparentWindow(display, xid, root, 0, 0);
    /* Same "force the request to complete before trusting the error flag"
     * rationale as geometry_sync's own round-15 XReparentWindow call --
     * see that call site's own comment. A real protocol error here (e.g. if
     * the parent already went away between the last successful geometry_sync
     * call and this destroy, the exact watchdog.c scenario) is caught by the
     * same already-installed, process-wide, non-fatal x11_error_handler --
     * never fatal to the controller, this is purely best-effort. */
    XSync(display, 0);
    if (x11_error_seen_during_call)
        fprintf(stderr, "webview2loader-host: un-reparent-to-root for nv=%p hit a non-fatal X11 protocol "
                        "error (see x11_error_handler's own log line above) -- proceeding to "
                        "gtk_window_destroy() regardless, matching this function's own never-fatal "
                        "pattern\n", (void *)nv);
    XUnlockDisplay(display);

    nv->reparented_into = 0;
    g_object_unref(gdisplay);
    g_object_unref(surface);
    return TRUE;
}

/* Test-support only (Plan 3 Task 3, original comment): real GdkSurface
 * width/height readback so a test can confirm geometry_sync actually
 * changed the on-screen window, not just that the call returned success. */
gboolean geometry_get(struct native_webview *nv, struct wv2l_rect *out)
{
    GtkNative *native;
    GdkSurface *surface;

    /* Task 7 UAF guard -- see geometry_sync's own comment on the same
     * pattern above. */
    if (!nv) return FALSE;

    native = gtk_widget_get_native(nv->window);
    surface = native ? gtk_native_get_surface(native) : NULL;
    if (!surface) return FALSE;

    out->left = 0;
    out->top = 0;
    out->right = gdk_surface_get_width(surface);
    out->bottom = gdk_surface_get_height(surface);
    return TRUE;
}

