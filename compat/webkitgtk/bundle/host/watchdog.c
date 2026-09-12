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

/* webkitgtk-bundle/host/watchdog.c
 *
 * See watchdog.h for the full crash mechanism this defends against and the
 * rationale for the dedicated-second-Xlib-connection approach. This file
 * only ever runs on this process's single serialized GTK/GLib main-loop
 * thread (same reasoning already established throughout webview.c/
 * geometry.c/main.c) -- no locking is needed around `watched[]` or
 * `watchdog_display` below, and no locking is needed for individual calls
 * against watchdog_display either: it is a connection no other thread in
 * this process ever touches (WebKitGTK's own auxiliary compositing/GL
 * threads, the leading suspect throughout geometry.c's own locking
 * comments, only ever touch GDK's OWN display connection, never this
 * private one).
 */
#include "watchdog.h"
#include <X11/Xlib.h>
#include <glib-unix.h>
#include <stdio.h>

#define MAX_WATCHED_PARENTS 64

struct watched_parent
{
    Window parent_xid;
    struct native_webview *nv;
};

static Display *watchdog_display;
static struct watched_parent watched[MAX_WATCHED_PARENTS];

/* Forces this process's own controlled teardown (webview_destroy(), which
 * unregisters nv from the live_webviews registry -- see webview.c's own
 * Task 7 UAF-guard comment -- then calls gtk_window_destroy(), GDK's own
 * real, cooperative destroy entrypoint, then frees nv) ahead of GDK's own
 * async discovery of the same underlying event on its own connection. See
 * watchdog.h's own top-of-file comment for why this has a real, favorable
 * chance of winning that race, and for the honest caveat that it is not a
 * guaranteed win of every possible timing. */
static void handle_parent_gone(struct native_webview *nv, const char *event_name, Window parent_xid)
{
    fprintf(stderr, "webview2loader-host: watchdog: %s observed for watched parent_xid=0x%lx -- "
                    "proactively tearing down reparented webview nv=%p through this process's own "
                    "controlled destroy path (webview_destroy -> gtk_window_destroy) before GDK's own "
                    "async X11 event processing on its own connection can discover the same thing and "
                    "hit a fatal internal-consistency assertion (\"GdkSurface ... unexpectedly "
                    "destroyed\" / \"assertion failed: (priv->egl_native_window == NULL)\")\n",
                    event_name, (unsigned long)parent_xid, (void *)nv);
    webview_destroy(nv);
}

/* Drains every pending event on watchdog_display. Runs as a GSource
 * callback on this process's single main-loop thread, same integration
 * pattern main.c's own on_ipc_readable already uses for the IPC socket --
 * see that function's own comment for why no two GSource callbacks in this
 * process can ever be interleaved with each other. */
static gboolean on_watchdog_readable(gint fd, GIOCondition condition, gpointer user_data)
{
    (void)fd;
    (void)condition;
    (void)user_data;

    while (XPending(watchdog_display) > 0)
    {
        XEvent ev;
        int i;

        XNextEvent(watchdog_display, &ev);

        if (ev.type == DestroyNotify)
        {
            Window w = ev.xdestroywindow.window;
            for (i = 0; i < MAX_WATCHED_PARENTS; i++)
            {
                if (watched[i].nv && watched[i].parent_xid == w)
                {
                    struct native_webview *nv = watched[i].nv;
                    watched[i].nv = NULL;
                    watched[i].parent_xid = 0;
                    handle_parent_gone(nv, "DestroyNotify", w);
                }
            }
        }
        else if (ev.type == ReparentNotify)
        {
            /* The watched parent itself just got reparented somewhere
             * else -- not a destroy, so the reparented webview is not
             * torn down here, but the parent/child relationship this
             * watch entry was tracking (and geometry_sync's own
             * nv->reparented_into cache) no longer reflects reality.
             * Reset it so the next geometry_sync call for this nv treats
             * whatever parent_xid it's given next as a genuinely new
             * target (real re-reparent, or gracefully falling back to
             * floating-window behavior if parent_xid comes back 0) rather
             * than trusting a stale cached value -- same "recovering from
             * Wine recreating the parent's whole_window" spirit
             * geometry.c's own reparented_into comparison already
             * documents, just triggered here instead of lazily on the
             * next put_Bounds call. Drops this watch entry; a fresh
             * XReparentWindow success in geometry_sync re-adds one via
             * watchdog_track_parent. */
            Window w = ev.xreparent.window;
            for (i = 0; i < MAX_WATCHED_PARENTS; i++)
            {
                if (watched[i].nv && watched[i].parent_xid == w)
                {
                    fprintf(stderr, "webview2loader-host: watchdog: ReparentNotify observed for watched "
                                    "parent_xid=0x%lx -- nv=%p's reparented_into cache reset, next "
                                    "geometry_sync call re-establishes the real relationship\n",
                                    (unsigned long)w, (void *)watched[i].nv);
                    watched[i].nv->reparented_into = 0;
                    watched[i].nv = NULL;
                    watched[i].parent_xid = 0;
                }
            }
        }
        /* Every other StructureNotifyMask event type (ConfigureNotify,
         * MapNotify, UnmapNotify, GravityNotify, CirculateNotify) is
         * expected noise from selecting a whole mask rather than
         * individual event types -- Xlib has no per-event-type select
         * granularity finer than the mask bits themselves, and
         * StructureNotifyMask is the coarsest mask that includes
         * DestroyNotify/ReparentNotify at all. Deliberately ignored, not
         * logged -- these are not evidence of anything wrong, just this
         * watch's own necessarily-broad selection picking up neighboring
         * event types for free. */
    }
    return G_SOURCE_CONTINUE;
}

void watchdog_init(void)
{
    int fd;

    watchdog_display = XOpenDisplay(NULL);
    if (!watchdog_display)
    {
        fprintf(stderr, "webview2loader-host: watchdog: XOpenDisplay failed -- the reparented-parent-"
                        "destroyed crash mitigation is disabled for this process's whole lifetime; "
                        "geometry_sync's own X11 reparenting still works without it, just without this "
                        "specific defense\n");
        return;
    }

    fd = ConnectionNumber(watchdog_display);
    /* G_PRIORITY_HIGH, not g_unix_fd_add's default G_PRIORITY_DEFAULT: GDK's own
     * X11 event source runs at GDK_PRIORITY_EVENTS, which gdkevents.h defines as
     * exactly G_PRIORITY_DEFAULT. At equal priority the two sources are a coin
     * flip, and losing the flip is what this watchdog exists to prevent -- a real
     * crash log shows GDK winning it, with no watchdog line logged at all.
     * Dispatching first whenever both are ready in the same main-loop iteration
     * turns that coin flip into the intended ordering.
     *
     * This narrows the race, it does not close it: if GDK already dispatched the
     * DestroyNotify in an EARLIER iteration, no priority helps. The actual crash
     * is fixed in the gtk4 build itself (see build-in-container.sh's own
     * gdk_x11_surface_destroy EGL-teardown patch) -- this ordering just means the
     * clean, cooperative teardown path is the one normally taken. */
    g_unix_fd_add_full(G_PRIORITY_HIGH, fd, G_IO_IN, on_watchdog_readable, NULL, NULL);
    fprintf(stderr, "webview2loader-host: watchdog: dedicated X11 connection opened for reparented-"
                    "parent-destroyed crash mitigation (fd=%d)\n", fd);
}

void watchdog_track_parent(struct native_webview *nv, unsigned long parent_xid)
{
    int i, free_slot = -1;

    if (!watchdog_display || !nv || !parent_xid) return;

    for (i = 0; i < MAX_WATCHED_PARENTS; i++)
    {
        if (watched[i].nv == nv)
        {
            /* Already tracking this nv -- update in place (a real
             * re-reparent, e.g. recovering from Wine recreating the
             * parent's whole_window, per geometry.c's own comment) rather
             * than growing a second, stale entry for the same nv. */
            watched[i].parent_xid = (Window)parent_xid;
            XSelectInput(watchdog_display, (Window)parent_xid, StructureNotifyMask);
            XFlush(watchdog_display);
            return;
        }
        if (free_slot < 0 && !watched[i].nv) free_slot = i;
    }

    if (free_slot < 0)
    {
        fprintf(stderr, "webview2loader-host: watchdog: watched-parents table full (%d entries) -- "
                        "nv=%p's parent 0x%lx will not get the crash-mitigation watch\n",
                        MAX_WATCHED_PARENTS, (void *)nv, parent_xid);
        return;
    }

    watched[free_slot].nv = nv;
    watched[free_slot].parent_xid = (Window)parent_xid;
    /* XSelectInput can raise a real X11 protocol error (e.g. BadWindow if
     * parent_xid is already stale by the time this runs -- the exact same
     * class of not-fully-trustworthy externally-sourced value geometry.c's
     * own round-17 x11_error_handler comment documents for XReparentWindow)
     * -- caught by that same already-installed, process-wide, non-fatal
     * handler (XSetErrorHandler has no per-Display scoping in Xlib, so the
     * handler main.c installs for GDK's own display covers this second
     * connection too, with no extra wiring needed here). */
    XSelectInput(watchdog_display, (Window)parent_xid, StructureNotifyMask);
    XFlush(watchdog_display);
}

void watchdog_untrack(struct native_webview *nv)
{
    int i;
    if (!nv) return;
    for (i = 0; i < MAX_WATCHED_PARENTS; i++)
        if (watched[i].nv == nv) { watched[i].nv = NULL; watched[i].parent_xid = 0; }
}
