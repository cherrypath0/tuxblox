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

/* webkitgtk-bundle/host/main.c
 *
 * Entry point for webview2loader-host: a separate GTK4/WebKitGTK process
 * that Task 3's unixlib.c (Wine side) talks to over a socket, using the
 * wire protocol in webview2loader_ipc_protocol.h. This task only wires up
 * the dispatch loop skeleton -- WV2L_OP_INIT is handled for real (there's
 * nothing more to initialize yet beyond GTK itself, done in main() below).
 * Every other opcode defined in the protocol header has its own named case
 * that fully reads its specific request struct and replies with an honest
 * all-zero/failure response, so the wire always stays correctly framed no
 * matter which of the 10 opcodes Task 3 sends first -- `default:` is
 * reserved for a genuinely out-of-range opcode value (real protocol
 * corruption), not for "not implemented yet". Tasks 4-6 replace each stub
 * with the real ported handler, one case at a time.
 *
 * Deviation from the plan brief's example, found while actually compiling
 * this against the real GTK4 headers this bundle builds (build-in-container.sh
 * links against pkg-config's "gtk4", not a GTK3 gtk+-3.0): GTK4 removed
 * gtk_main()/gtk_main_quit() entirely (no declaration anywhere in
 * $PREFIX/include/gtk-4.0/gtk/gtkmain.h -- confirmed directly against the
 * installed headers, `-Werror` caught this immediately as
 * implicit-function-declaration) and changed gtk_init_check() to take no
 * arguments (it used to take argc/argv in GTK3; GTK4 apps get their command
 * line from GApplication instead, which this helper doesn't use). The
 * standard GTK4-era replacement for a bare, non-GApplication event loop is a
 * plain GLib GMainLoop, used below -- functionally identical to what
 * gtk_main()/gtk_main_quit() did, just GLib's own API instead of GTK's now
 * removed wrapper around it.
 */
#include <gtk/gtk.h>
#include <glib-unix.h>
#include <X11/Xlib.h>
#include <dlfcn.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include "ipc.h"
#include "webview2loader_ipc_protocol.h"
#include "webview.h"
#include "geometry.h"
#include "navigate.h"
#include "watchdog.h"

static int g_ipc_fd = -1;
static GMainLoop *g_loop = NULL;

/* Task 8 (2026-08-14-webview2loader-host-process plan, real end-to-end
 * verification): the whole point of this plan was to move GTK/WebKit
 * hosting out-of-process so real GPU-accelerated (glvnd/NVIDIA) rendering
 * becomes possible -- unixlib.c's host_has_usable_egl()/spawn_helper()
 * already decide host-vs-bundled EGL and print a WARN() on the FAILURE
 * paths (Task 7), but nothing anywhere prints a positive confirmation of
 * which EGL actually got resolved once this process is alive. Without
 * that, "is the fix actually working" could only be inferred negatively
 * (absence of "nouveau: driver missing" -- style warnings), not confirmed
 * directly. This queries the real, live EGL_VENDOR/EGL_VERSION strings
 * from whichever libEGL.so.1 this process's own dynamic linker actually
 * resolved (governed by spawn_helper()'s LD_LIBRARY_PATH decision, made
 * before this process's execl()) and prints them ONCE, at startup --
 * mirroring geometry.c's geometry_debug_on() precedent of never letting a
 * diagnostic print on every call/frame, just scoped here to "once per
 * process" rather than behind an opt-in env var, since a once-per-process
 * print is inherently not the repeated-per-call spam that precedent
 * guards against.
 *
 * Plain dlopen()+dlsym() against libEGL.so.1, not a real
 * `#include <EGL/egl.h>` + `-lEGL` link dependency: this process already
 * pulls in libEGL indirectly at runtime (WebKitGTK's own real GL/EGL use
 * once a webview is created), so a probe here observes the same resolved
 * library WebKit itself will use -- but adding a *build-time* link
 * dependency on EGL would require touching build-in-container.sh's
 * pkg-config invocation for a value only ever read as a one-line startup
 * log, which isn't worth a new build dependency. The EGL_VENDOR/
 * EGL_VERSION integer constants below are stable, public values from the
 * Khronos EGL registry (egl.h's own #defines never renumber), same
 * "well-known constant instead of a header" tradeoff unixlib.c's own
 * host_has_usable_egl() already makes for eglGetDisplay/dlsym. */
#define WV2L_EGL_VENDOR  0x3053
#define WV2L_EGL_VERSION 0x3054

/* 2026-08-15 (software-only-llvmpipe plan) update: unixlib.c's spawn_helper()
 * no longer conditionally chooses host-vs-bundled EGL at all -- it now
 * ALWAYS sets LD_LIBRARY_PATH to this bundle's own gl-fallback/ directory
 * before exec'ing this process (see spawn_helper()'s WV2L_ALWAYS_USE_BUNDLE_GL,
 * a permanent workaround for a confirmed upstream WebKitGTK/NVIDIA driver
 * crash, not the host-preference logic this comment originally described).
 * That means this function's output is no longer just an informational log
 * of "which EGL got picked" -- it is now the real, positive verification that
 * the fix actually took effect on a given launch: EGL_VENDOR should always
 * read as Mesa's own vendor string ("Mesa Project" for llvmpipe -- there is
 * no other Mesa gallium software driver left in this bundle as of the
 * softpipe->llvmpipe switch, see build-in-container.sh's Mesa comment) and
 * should NEVER show an NVIDIA-identifying vendor string on any launch, on any
 * host, regardless of what GPU driver that host has installed. A real relaunch
 * showing an NVIDIA vendor string here would mean WV2L_ALWAYS_USE_BUNDLE_GL
 * somehow isn't taking effect -- a regression, not an expected outcome on any
 * machine. */
static void log_gl_dispatch_info(void)
{
    void *h;
    void *(*p_eglGetDisplay)(void *);
    unsigned int (*p_eglInitialize)(void *, int *, int *);
    const char *(*p_eglQueryString)(void *, int);
    void *dpy = NULL;
    int major = 0, minor = 0;
    Display *x11_dpy;

    h = dlopen("libEGL.so.1", RTLD_LAZY);
    if (!h)
    {
        fprintf(stderr, "webview2loader-host: GL dispatch: dlopen(\"libEGL.so.1\") failed (%s) -- "
                        "no EGL available at all in this process\n", dlerror());
        return;
    }

    p_eglGetDisplay = dlsym(h, "eglGetDisplay");
    p_eglInitialize = dlsym(h, "eglInitialize");
    p_eglQueryString = dlsym(h, "eglQueryString");
    if (!p_eglGetDisplay || !p_eglInitialize || !p_eglQueryString)
    {
        fprintf(stderr, "webview2loader-host: GL dispatch: libEGL.so.1 loaded but missing expected symbols\n");
        dlclose(h);
        return;
    }

    /* Task 8 real-launch verification finding, round 2 -- this is now
     * confirmed against a real standalone repro (a tiny dlopen/dlsym test
     * program run directly against this dev machine's real glvnd+NVIDIA
     * EGL, DISPLAY=:0), not just theorized: eglGetDisplay(NULL) i.e.
     * EGL_DEFAULT_DISPLAY fails outright on this glvnd setup -- it has no
     * way to guess which of several installed EGL platform backends (X11,
     * GBM, surfaceless) to hand back a display for with no hint at all.
     * unixlib.c's own host_has_usable_egl() never hits this because it
     * only presence-checks dlopen+dlsym, never actually calls
     * eglGetDisplay/eglInitialize -- so it silently chose host EGL (no
     * WARN() logged) even while THIS probe's original EGL_DEFAULT_DISPLAY
     * call failed, which is exactly why the repo owner's own "lag is
     * fixed" hands-on confirmation of real hardware compositing and this
     * probe's startup log used to disagree.
     *
     * The repro also ruled out the fancier fix tried first here
     * (eglGetPlatformDisplayEXT() with EGL_PLATFORM_X11_KHR): that
     * function is part of the EGL_EXT_platform_base *extension*, which on
     * this system's libEGL.so.1 is genuinely not resolvable via plain
     * dlsym() at all (confirmed: dlsym() returned NULL) -- EGL extension
     * entry points are only guaranteed reachable through
     * eglGetProcAddress(), the same rule OpenGL extensions follow, not
     * through the dynamic symbol table the way core EGL 1.x entry points
     * (eglGetDisplay itself included) are.
     *
     * The actual, repro-confirmed fix needs neither of those: plain,
     * always-present eglGetDisplay() works fine and returns a real,
     * initializable NVIDIA display -- it just needs a genuine X11
     * Display*, not NULL, as its argument. Opened via this file's own
     * XOpenDisplay() (already linked via -lX11 for x11_error_handler
     * above); NULL is kept as a last-resort fallback only for the case
     * XOpenDisplay() itself fails, not because it's expected to help once
     * eglGetDisplay(x11_dpy) has already failed. */
    x11_dpy = XOpenDisplay(NULL);
    if (x11_dpy)
        dpy = p_eglGetDisplay(x11_dpy);
    if (!dpy)
        dpy = p_eglGetDisplay(NULL /* EGL_DEFAULT_DISPLAY */);
    if (!dpy || !p_eglInitialize(dpy, &major, &minor))
    {
        fprintf(stderr, "webview2loader-host: GL dispatch: eglGetDisplay/eglInitialize failed -- "
                        "EGL present but no usable display\n");
        dlclose(h);
        return;
    }

    fprintf(stderr, "webview2loader-host: GL dispatch: EGL_VENDOR=\"%s\" EGL_VERSION=\"%s\" (eglInitialize %d.%d)\n",
            p_eglQueryString(dpy, WV2L_EGL_VENDOR), p_eglQueryString(dpy, WV2L_EGL_VERSION), major, minor);

    /* Deliberately no eglTerminate()/dlclose() on this success path: this
     * display handle is process-global per EGL's own spec (repeated
     * eglGetDisplay(EGL_DEFAULT_DISPLAY) calls return the same handle),
     * and WebKitGTK's own later real EGL use in this same process will
     * re-initialize/use it. Unlike the plain presence-check dlopen()/
     * dlsym()/dlclose() pattern unixlib.c's host_has_usable_egl() uses
     * (which never calls eglInitialize, so has no driver state to worry
     * about), eglInitialize() here really does create live driver state
     * (device fds, mmaps) tied to this specific dlopen handle -- calling
     * dlclose() afterward risks unmapping libEGL.so.1 out from under that
     * state if this was the only holder of the refcount, corrupting
     * WebKitGTK's later real EGL use for the sake of a value that only
     * exists to be logged once. A one-time intentional "leak" of this
     * dlopen handle for the lifetime of the process is the safe choice
     * here, not an oversight. */
}

/* Reads one opcode + its struct, dispatches, writes the struct back.
 * Returns FALSE (stopping the GSource / ending the process) once the
 * socket is gone -- matches unixlib.c's own "helper died" detection on
 * the other end (Task 3). */
static gboolean on_ipc_readable(gint fd, GIOCondition condition, gpointer user_data)
{
    uint32_t opcode;
    if (ipc_read_full(fd, &opcode, sizeof(opcode)) < 0) { g_main_loop_quit(g_loop); return G_SOURCE_REMOVE; }

    switch (opcode)
    {
    case WV2L_OP_INIT:
    {
        struct wv2l_init_params p;
        if (ipc_read_full(fd, &p, sizeof(p)) < 0) { g_main_loop_quit(g_loop); return G_SOURCE_REMOVE; }
        p.success = 1;
        if (ipc_write_full(fd, &p, sizeof(p)) < 0) { g_main_loop_quit(g_loop); return G_SOURCE_REMOVE; }
        return G_SOURCE_CONTINUE;
    }
    /* Every opcode below: fully read its own request struct (still need to
     * consume the exact byte count so the stream stays framed for whatever
     * request comes next, whatever its size -- these structs are NOT all
     * the same size), respond with an all-zero/failure struct of the same
     * type. Tasks 4-6 replace each case with the real ported handler; until
     * then this is an honest "not implemented yet" for that one call, never
     * a hang, a crash, or a protocol desync that would take down the whole
     * IPC channel for every other opcode too. */
    case WV2L_OP_CREATE_WEBVIEW:
    {
        struct wv2l_create_webview_params p;
        struct native_webview *nv;
        if (ipc_read_full(fd, &p, sizeof(p)) < 0) { g_main_loop_quit(g_loop); return G_SOURCE_REMOVE; }
        nv = webview_create(p.is_message_only);
        p.handle = nv ? (uint64_t)(uintptr_t)nv : 0;
        if (ipc_write_full(fd, &p, sizeof(p)) < 0) { g_main_loop_quit(g_loop); return G_SOURCE_REMOVE; }
        return G_SOURCE_CONTINUE;
    }
    case WV2L_OP_DESTROY_WEBVIEW:
    {
        struct wv2l_destroy_webview_params p;
        if (ipc_read_full(fd, &p, sizeof(p)) < 0) { g_main_loop_quit(g_loop); return G_SOURCE_REMOVE; }
        webview_destroy(webview_lookup(p.handle));
        /* No `out` fields defined for this opcode (see the protocol header) --
         * write the struct back unchanged, same framing contract as every
         * other opcode: caller always gets exactly one response struct back. */
        if (ipc_write_full(fd, &p, sizeof(p)) < 0) { g_main_loop_quit(g_loop); return G_SOURCE_REMOVE; }
        return G_SOURCE_CONTINUE;
    }
    case WV2L_OP_NAVIGATE_AND_WAIT:
    {
        struct wv2l_navigate_params p;
        struct native_webview *nv;
        char *uri_utf8;
        gboolean success = FALSE;
        uint64_t nav_id = 0;
        if (ipc_read_full(fd, &p, sizeof(p)) < 0) { g_main_loop_quit(g_loop); return G_SOURCE_REMOVE; }
        nv = webview_lookup(p.handle);
        uri_utf8 = wire_uri_to_utf8(p.uri);
        navigate_and_wait(nv, uri_utf8 ? uri_utf8 : "", &success, &nav_id);
        g_free(uri_utf8);
        p.is_success = success ? 1 : 0;
        p.navigation_id = nav_id;
        if (ipc_write_full(fd, &p, sizeof(p)) < 0) { g_main_loop_quit(g_loop); return G_SOURCE_REMOVE; }
        return G_SOURCE_CONTINUE;
    }
    case WV2L_OP_DELETE_ALL_COOKIES:
    {
        struct wv2l_delete_all_cookies_params p;
        if (ipc_read_full(fd, &p, sizeof(p)) < 0) { g_main_loop_quit(g_loop); return G_SOURCE_REMOVE; }
        /* No `out` fields defined for this opcode -- cookies_delete_all's
         * own gboolean return is only useful for this file's own stderr
         * diagnostics (a stale handle or an allocation failure), see that
         * function's own comment; nothing in the wire struct to carry it
         * back in even if a caller wanted it. */
        cookies_delete_all(webview_lookup(p.handle));
        if (ipc_write_full(fd, &p, sizeof(p)) < 0) { g_main_loop_quit(g_loop); return G_SOURCE_REMOVE; }
        return G_SOURCE_CONTINUE;
    }
    case WV2L_OP_COUNT_COOKIES:
    {
        struct wv2l_count_cookies_params p;
        uint32_t count = 0;
        if (ipc_read_full(fd, &p, sizeof(p)) < 0) { g_main_loop_quit(g_loop); return G_SOURCE_REMOVE; }
        cookies_count(webview_lookup(p.handle), &count);
        p.count = count;
        if (ipc_write_full(fd, &p, sizeof(p)) < 0) { g_main_loop_quit(g_loop); return G_SOURCE_REMOVE; }
        return G_SOURCE_CONTINUE;
    }
    case WV2L_OP_GET_COOKIES:
    {
        struct wv2l_get_cookies_params p;
        struct native_webview *nv;
        char *uri_utf8;
        if (ipc_read_full(fd, &p, sizeof(p)) < 0) { g_main_loop_quit(g_loop); return G_SOURCE_REMOVE; }
        nv = webview_lookup(p.handle);
        uri_utf8 = wire_uri_to_utf8(p.uri);
        cookies_get(nv, uri_utf8, &p); /* writes p.success/p.count/p.cookies directly */
        g_free(uri_utf8);
        if (ipc_write_full(fd, &p, sizeof(p)) < 0) { g_main_loop_quit(g_loop); return G_SOURCE_REMOVE; }
        return G_SOURCE_CONTINUE;
    }
    case WV2L_OP_GET_WINDOW_VISIBLE:
    {
        /* Task 4 finding: no task in the plan (4, 5, or 6) explicitly lists
         * this opcode's dispatch case, even though unixlib.c's
         * unix_get_window_visible_impl (already committed, Task 3) sends it
         * for real -- grepped the whole plan document to confirm. Left as a
         * stub, this permanently fails
         * tests/webview2loader.c's test_hwnd_message_never_shows_window
         * (__wine_test_webview2loader_window_is_visible always reading back
         * FALSE) even after this task's real webview_create -- a real,
         * observed regression once CREATE_WEBVIEW stopped being a stub,
         * not a hang/crash. Query is a trivial, direct one-line read on the
         * same struct native_webview* this task's webview_lookup already
         * resolves -- no new state, no new file -- so closing this gap here
         * rather than leaving a real test permanently red. */
        struct wv2l_get_window_visible_params p;
        struct native_webview *nv;
        if (ipc_read_full(fd, &p, sizeof(p)) < 0) { g_main_loop_quit(g_loop); return G_SOURCE_REMOVE; }
        nv = webview_lookup(p.handle);
        p.visible = (nv && gtk_widget_get_visible(nv->window)) ? 1 : 0;
        if (ipc_write_full(fd, &p, sizeof(p)) < 0) { g_main_loop_quit(g_loop); return G_SOURCE_REMOVE; }
        return G_SOURCE_CONTINUE;
    }
    case WV2L_OP_SYNC_WINDOW_GEOMETRY:
    {
        struct wv2l_sync_window_geometry_params p;
        struct native_webview *nv;
        if (ipc_read_full(fd, &p, sizeof(p)) < 0) { g_main_loop_quit(g_loop); return G_SOURCE_REMOVE; }
        nv = webview_lookup(p.handle);
        p.success = geometry_sync(nv, p.screen_bounds, p.visible, p.parent_xid) ? 1 : 0;
        if (ipc_write_full(fd, &p, sizeof(p)) < 0) { g_main_loop_quit(g_loop); return G_SOURCE_REMOVE; }
        return G_SOURCE_CONTINUE;
    }
    case WV2L_OP_GET_WINDOW_GEOMETRY:
    {
        struct wv2l_get_window_geometry_params p;
        struct native_webview *nv;
        if (ipc_read_full(fd, &p, sizeof(p)) < 0) { g_main_loop_quit(g_loop); return G_SOURCE_REMOVE; }
        nv = webview_lookup(p.handle);
        p.success = geometry_get(nv, &p.screen_bounds) ? 1 : 0;
        if (!p.success)
        {
            p.screen_bounds.left = 0;
            p.screen_bounds.top = 0;
            p.screen_bounds.right = 0;
            p.screen_bounds.bottom = 0;
        }
        if (ipc_write_full(fd, &p, sizeof(p)) < 0) { g_main_loop_quit(g_loop); return G_SOURCE_REMOVE; }
        return G_SOURCE_CONTINUE;
    }
    /* Reserved for a truly out-of-range/garbage opcode value that doesn't
     * match any of the 10 real opcodes above -- genuine protocol corruption,
     * so terminating the helper here (rather than trying to guess a framing)
     * is still the right call. Every opcode wv2l_opcode actually defines has
     * its own case above and must never fall through to here. */
    default:
        fprintf(stderr, "webview2loader-host: unknown/unimplemented opcode %u\n", opcode);
        g_main_loop_quit(g_loop);
        return G_SOURCE_REMOVE;
    }
}

int main(int argc, char **argv)
{
    gboolean gtk_ok;
    const char *fd_env;

    (void)argc;
    (void)argv;

    /* Task 7 UAF/Xlib-locking fix round 3 (original unixlib.c comment on
     * XInitThreads' own extern declaration) -- see geometry.c's own
     * XLockDisplay/XUnlockDisplay bracketing in geometry_sync for the full
     * crash evidence this addresses. Must be the very first Xlib call made
     * anywhere in this process (Xlib's own documented requirement);
     * gtk_init_check() below is what actually opens the X11 display
     * connection geometry_sync later issues raw XMoveResizeWindow/
     * XReparentWindow/XSendEvent calls against, so this has to run
     * strictly before it. GDK likely also calls this internally as part of
     * opening its own X11 backend -- calling it again there is a
     * documented no-op/safe, so there's no harm doing it twice; what
     * matters is that it happens before ANY Xlib activity, which this
     * guarantees regardless of GDK's own internal ordering. WebKitGTK is
     * known to touch the shared X11 connection from auxiliary threads of
     * its own (compositing/GL) outside this process's own main-loop
     * thread, so without this, geometry_sync's later raw Xlib calls would
     * be exactly the kind of unguarded access that corrupts Xlib's
     * per-Display request buffers and segfaults deep inside _XGetRequest.
     *
     * Real return value (a real Xlib Status, non-zero on success) is
     * checked -- if this ever legitimately fails (rare, but real -- e.g. a
     * libX11 build without thread support), Xlib's own documented behavior
     * is for XLockDisplay/XUnlockDisplay to silently become no-ops, which
     * would quietly reopen the exact TOCTOU race geometry.c's own locking
     * exists to close, with nothing anywhere indicating the locking had
     * become inert. Loud diagnostic rather than a hard init failure: this
     * process can still do useful work (visibility/size sync, cookies,
     * navigation) even if X11 position sync specifically degrades. */
    if (!XInitThreads())
        fprintf(stderr, "webview2loader-host: XInitThreads() failed -- XLockDisplay/XUnlockDisplay will "
                        "silently no-op per Xlib's own documented behavior, reopening the exact TOCTOU race "
                        "geometry.c's own locking exists to close\n");

    /* Task 7 crash fix, round 12 (original unixlib.c comment, ported here
     * since this process's own main() -- specifically the gtk_init_check()
     * call below -- is now the ordering anchor; there is no separate
     * gtk_thread_proc anymore, this whole process's main() *is* that
     * thread): force GTK4's own backend selection to X11, not Wayland.
     * This environment is a genuine Wayland compositor session
     * (WAYLAND_DISPLAY set) and GDK_BACKEND is never set anywhere else in
     * this codebase (confirmed: unixlib.c's own spawn_helper does not set
     * it on this helper's environment either) -- GTK4's own documented
     * behavior is to auto-detect and PREFER native Wayland over X11
     * whenever both are available, unless an app explicitly forces
     * otherwise. Without this, every gdk_x11_surface_get_xid(surface) call
     * geometry_sync/geometry_get make is a real type mismatch (surface
     * would be a GdkWaylandSurface*, not a GdkX11Surface*) -- the exact
     * round-9-through-11 bug this fix resolved in the original
     * implementation. Must be set via setenv() here, before
     * gtk_init_check() below -- that's the one call that actually opens
     * GDK's display connection and locks in its backend choice. */
    setenv("GDK_BACKEND", "x11", 1);

    /* GTK 4.18 removed the old, separate "gl" renderer outright -- it's now
     * just a deprecated alias for "ngl" (confirmed directly in gtk's own
     * gsk/gskrenderer.c: GSK_RENDERER=gl warns and then returns the exact
     * same GSK_TYPE_GL_RENDERER "ngl" already uses), so there's no simpler
     * GL renderer left to switch to. full-redraw forces every frame to
     * union in the surface's full rectangle instead of computing a damage
     * region via gsk_render_node_diff() -- tested against a real repro
     * (reparented webview painting solid white when the pointer leaves its
     * area) and it did NOT fix it, ruling out damage-region diffing as the
     * cause. Left enabled anyway (real, official GTK4 flag, negligible
     * cost for a single webview-sized surface) since it's a plausible
     * contributor even if not the whole story, and there's no evidence it
     * hurts. See GDK_DEBUG below for the next diagnostic step. */
    setenv("GSK_DEBUG", "full-redraw", 1);

    /* GDK_DEBUG=events,frames,opengl was tried here as diagnostic tracing
     * for the flicker investigation and REMOVED again: both real crash
     * repros seen during that round (a real SIGSEGV inside libgtk-4.so.1's
     * own signal-emission code, from a nested g_main_loop_run() during the
     * login/OAuth flow -- see the 2026-08-15 crash report) happened while
     * this verbose tracing was active. Confirmed the tracing itself was a
     * real contributing factor, not just correlation: a same-day repro
     * WITHOUT it reached the same login flow with no SIGSEGV and no crash
     * dialog at all (Studio just closed on its own -- a separate, still
     * open issue, but not this crash). Re-add temporarily (same setenv()
     * call, before gtk_init_check() below) only if actively investigating
     * that SIGSEGV again -- don't leave it on by default. */

    /* Forced-active/visible override, read by three separate WebKit source
     * patches (WindowIsActive, isInMonitor, isSuspended -- see
     * ProtonSource/webkitgtk/README-TUXBLOX-PATCHES.md for the full
     * rationale on each): this webview's GdkSurface gets XReparentWindow'd
     * into Roblox Studio's own window, so it can never receive real
     * WM-mediated active/focus/monitor state again, which left WebKit
     * believing the window was inactive/off-monitor/suspended and reducing
     * or suspending its own compositing -- the leading suspect for the
     * reparented webview intermittently painting solid white or blank.
     * Always "yes": it is never correct for this specific webview to be
     * treated as backgrounded. Confirmed as of 2026-08-15 that this alone
     * does not fully explain every real repro (a geometry_sync-level
     * diagnostic below ruled out a separate IsVisible-toggle theory) --
     * investigation ongoing. */
    setenv("WEBVIEW2LOADER_FORCE_WINDOW_ACTIVE", "yes", 1);

    fd_env = getenv("WEBVIEW2LOADER_IPC_FD");
    if (!fd_env) { fprintf(stderr, "webview2loader-host: WEBVIEW2LOADER_IPC_FD not set\n"); return 1; }
    g_ipc_fd = atoi(fd_env);

    /* Task 4 finding: this used to be prctl(PR_SET_PDEATHSIG, SIGTERM) here,
     * matching the design spec's Lifecycle section ("if Wine dies without
     * cleanly closing the socket, don't become an orphan spinning
     * forever"). Removed -- real, reproduced bug, not a style choice.
     *
     * PR_SET_PDEATHSIG's signal fires when the specific THREAD that called
     * fork() (the "parent" the kernel actually tracks for this purpose)
     * terminates, not when the wider process exits (a well-documented Linux
     * gotcha, confirmed here with an isolated repro: a pthread that fork()s
     * then returns kills the child within the same instant, even though the
     * process itself keeps running for hours afterward). Wine's own
     * unixlib.c spawns this helper (spawn_helper(), called from
     * webview2loader_unix_init()) from exactly that shape of thread --
     * ProtonSource/wine/dlls/webview2loader/main.c's
     * create_environment_worker(), a one-shot CreateThread worker that
     * returns (and so terminates) immediately after the WV2L_OP_INIT
     * round-trip completes. With PDEATHSIG set, this helper was being
     * SIGTERM'd within roughly 100ms of spawning -- confirmed via a process-
     * lifetime monitor during a real Wine test run -- long before any real
     * WV2L_OP_CREATE_WEBVIEW call could reach it, making every webview
     * creation fail with a transport error that looked identical to "helper
     * never started."
     *
     * No replacement mechanism is needed: the IPC socket itself already
     * provides correct, thread-lifetime-independent death detection.
     * Closing a process closes every fd it holds, including this end's peer
     * -- when the real Wine PROCESS dies (crash, normal exit, kill), for
     * any reason, on any thread, this helper's next read on the socket
     * returns EOF, ipc_read_full() below returns -1, and on_ipc_readable()
     * already calls g_main_loop_quit() for exactly that case. That path
     * does not depend on which OS thread happened to call fork() on the
     * Wine side, so it has none of PDEATHSIG's race window. */

    gtk_ok = gtk_init_check();

    /* Task 7 crash fix, round 17 (see geometry.h's own x11_error_handler
     * comment for the full rationale): install AFTER gtk_init_check() (not
     * before), specifically so this overrides whatever error handler GDK's
     * own X11 backend init installed for itself while opening the display
     * connection above, rather than being silently clobbered BY it -- same
     * "must come after the call that locks in GDK's own X11 setup"
     * ordering GDK_BACKEND above already has, just on the other side of
     * that call instead of before it. Installed unconditionally (not
     * gated on gtk_ok): even a failed gtk_init_check() may have partially
     * opened an X11 display connection this process could still receive a
     * real protocol error against before the `return 1` below runs, and a
     * missing handler here is exactly the fatal-abort gap this round
     * fixes, so there's no safe case to skip it in. */
    XSetErrorHandler(x11_error_handler);

    if (!gtk_ok)
    {
        fprintf(stderr, "webview2loader-host: gtk_init_check failed\n");
        return 1;
    }

    log_gl_dispatch_info();

    /* Crash fix (see watchdog.h's own top-of-file comment for the full
     * mechanism this defends against): opens the dedicated second Xlib
     * connection used to detect a reparented-into parent window being
     * destroyed out from under a webview, and react before GDK's own
     * async event processing on its own connection can hit a fatal
     * internal-consistency assertion over it. Called after
     * XSetErrorHandler() above (same ordering rationale that call's own
     * comment documents -- this connection's own XSelectInput/XNextEvent
     * calls can raise real X11 protocol errors too, and need that same
     * already-installed non-fatal handler in place first) and after
     * gtk_ok is confirmed (no point opening a second X11 connection if
     * GDK's own X11 backend init already failed above). */
    watchdog_init();

    g_loop = g_main_loop_new(NULL, FALSE);
    g_unix_fd_add(g_ipc_fd, G_IO_IN, on_ipc_readable, NULL);
    g_main_loop_run(g_loop);
    g_main_loop_unref(g_loop);
    return 0;
}
