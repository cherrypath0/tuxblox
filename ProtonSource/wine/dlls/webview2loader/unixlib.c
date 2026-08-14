#if 0
#pragma makedep unix
#endif

#include "config.h"

/* dlmopen()/Lmid_t/dlinfo()/RTLD_DI_LMID/LM_ID_NEWLM below are GNU
 * extensions, only declared by glibc's <dlfcn.h> when _GNU_SOURCE is
 * defined before it's included. Verified (not assumed) that Wine's own
 * build already provides this: configure.ac's generic Linux case
 * unconditionally does AC_DEFINE(_GNU_SOURCE, 1, ...), and the built
 * ProtonBuild/obj-wine-{x86_64,i386}/include/config.h confirms
 * `#define _GNU_SOURCE 1` is present -- config.h is included first, above,
 * so no separate local #define is needed here. */
#include <dlfcn.h>
#include <errno.h>
#include <limits.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
/* Login-redirect fix: fork()+execvp()+waitpid() for the xdg-open handoff --
 * see xdg_open_handoff's own comment below for why this shape (matching
 * dlls/ntdll/unix/process.c's own established double-fork pattern) rather
 * than system()/posix_spawn(). */
#include <sys/wait.h>
#include <unistd.h>

#include <ntstatus.h>
#define WIN32_NO_STATUS
#include <windef.h>
#include <winbase.h>
#include <winternl.h>

#include <wine/debug.h>
#include <wine/unixlib.h>

#include "unixlib.h"

WINE_DEFAULT_DEBUG_CHANNEL(webview2loader);

/* Task 7 crash fix, round 8: WARN()/ERR()/TRACE()/FIXME() (the macros
 * above route through) are NOT safe to call from gtk_thread (spawned via
 * a raw pthread_create() a few hundred lines down -- see that call's own
 * site) or from anything that runs ON gtk_thread (every *_on_gtk_thread
 * function, and any GLib callback dispatched through the GMainLoop that
 * function runs). Confirmed via a real coredump: __wine_dbg_header
 * (dlls/ntdll/thread.c) derives its scratch-buffer pointer from
 * NtCurrentTeb(), which requires a valid, Wine-initialized TEB --
 * something only Wine's own thread-creation path sets up (a wineserver
 * round-trip + arch_prctl(ARCH_SET_GS, ...), see dlls/ntdll/unix/thread.c
 * and dlls/ntdll/unix/signal_x86_64.c), and which a plain host
 * pthread_create() thread never gets. On this thread, NtCurrentTeb() is
 * garbage: sometimes that crashes cleanly (as this one did, real
 * relaunch, real coredump), and on an unlucky day it instead writes
 * through that garbage pointer into whatever unrelated, mapped, writable
 * memory it happens to land on -- a very plausible unifying explanation
 * for this whole function's own multi-round Xlib corruption saga (rounds
 * 1-6, see sync_window_geometry_on_gtk_thread's own git log), not just
 * this specific crash.
 *
 * No Wine-internal API exists to retrofit a valid TEB onto an
 * already-running foreign thread after the fact -- TEB setup is
 * interleaved into Wine's own thread-creation sequence, not a separable
 * step. Established precedent elsewhere in this exact Wine tree (grep
 * "pthread_create" across every dlls subdirectory's unixlib.c/unix
 * sources): every other raw-pthread thread body either logs nothing at
 * all
 * (dlls/winealsa.drv/alsamidi.c, dlls/wineoss.drv/ossmidi.c,
 * dlls/winebus.sys/bus_udev.c), or uses the underlying native library's
 * own TEB-independent logging instead of Wine's
 * (dlls/winegstreamer/wg_parser.c uses GStreamer's GST_* macros, never
 * Wine's TRACE/WARN/ERR/FIXME). This file already links GLib, but a
 * plain stderr write needs no new symbol resolution and no dependency on
 * GLib's log-handler configuration being sane in this exact runtime --
 * the simplest, lowest-risk TEB-independent option, matching this
 * function's own %04x:%s-style shape closely enough to stay readable
 * next to real WARN/ERR output from the PE-calling-thread call sites
 * elsewhere in this file (those are unaffected -- gtk_thread_invoke_sync
 * can only ever be called from a thread other than gtk_thread itself,
 * since it blocks waiting for gtk_thread to run the posted work, so every
 * WARN/ERR immediately following an `if (!gtk_thread_invoke_sync(...))`
 * check in this file runs on the PE-calling thread, not gtk_thread, and
 * is unaffected by this issue). Use this from gtk_thread_proc and
 * anything reachable from it instead of WARN/ERR/TRACE/FIXME. */
#define GTK_THREAD_LOG(fmt, ...) \
    fprintf(stderr, "%04x:warn:webview2loader:%s " fmt, (unsigned int)(ULONG_PTR)pthread_self(), __func__, ##__VA_ARGS__)

/* Minimal hand-declared subset of GLib/GObject/GTK4/WebKitGTK-6.0 types and
 * function signatures actually used below. Deliberately NOT the real
 * glib.h/gtk/gtk.h/webkit/webkit.h -- those aren't available (or wanted) at
 * Wine's own build time, since bundling WebKitGTK's whole point (Plan 1)
 * was avoiding a host GTK/WebKit *build* dependency. Everything is resolved
 * at runtime via dlopen/dlsym against the Plan-1 bundle instead, exactly
 * like dlls/comdlg32/unixlib.c's SONAME_LIBDBUS_1/DO_FUNC pattern -- except
 * comdlg32 gets its prototypes for free from a real (system) <dbus/dbus.h>,
 * whereas nothing here is ever included, so every prototype below is
 * hand-written from the real headers found inside the committed bundle
 * (ProtonSource/contrib/webkitgtk-2.52.5-x86_64.tar.xz's include/), just so
 * typeof() below has something to compute a correct function-pointer type
 * from -- these extern declarations are never themselves called or
 * referenced by address, only used as typeof() operands (which, like
 * sizeof(), emits no symbol reference), so this never actually links
 * against a real libglib/libgtk/libwebkitgtk. */
typedef void GtkWidget;
typedef void GtkWindow;
typedef void GMainContext;
typedef void GMainLoop;
typedef void WebKitWebView;
typedef void WebKitNetworkSession;
typedef void WebKitCookieManager;
typedef int gboolean;
typedef unsigned long gulong;
typedef gboolean (*GSourceFunc)(void *user_data);
typedef void (*GCallback)(void);

/* Task 8: opaque/transparent types for the real WebKitCookieManager async
 * cookie-enumeration/deletion API (see WEBKIT_FUNCS/SOUP_FUNCS below).
 * Deliberately declared here, alongside every other hand-declared
 * GLib/GObject/GTK/WebKit type in this file, rather than in unixlib.h as
 * the task brief's Step 2 literally says -- unixlib.h is the shared
 * PE<->unix bridge header (struct definitions used by both sides of
 * WINE_UNIX_CALL), and none of these types are ever referenced by PE-side
 * code (cookie_manager.c only touches struct delete_all_cookies_params, a
 * plain UINT64 handle); they exist solely so typeof() below can compute
 * function-pointer types, exactly like GtkWidget/WebKitCookieManager above.
 * unixlib.h also has no pre-existing "type-declaration block" to add to
 * (checked -- it's just struct/enum definitions), confirming this is a
 * brief inaccuracy rather than an intentional new file layout.
 * GObject is added too (not mentioned by the brief at all): it's a genuine
 * omission -- GAsyncReadyCallback's real signature takes a GObject* first
 * argument, and nothing in this file typedef'd GObject before this task,
 * which fails to compile ("unknown type name 'GObject'") without it.
 *
 * GList is declared as its REAL transparent struct (data/next/prev), not
 * opaque `typedef void GList` -- unlike GObject-derived types (GtkWidget,
 * WebKitCookieManager, ...), GList's layout is genuinely stable public
 * GLib ABI (documented and unchanged for decades) and this file needs to
 * actually walk one (see on_get_all_cookies_done below), which an opaque
 * type can't support without additional accessor-function plumbing this
 * task has no other reason to add. */
typedef void GObject;
typedef void *gpointer;
typedef struct _GList GList;
struct _GList { gpointer data; GList *next; GList *prev; };
typedef void GCancellable;
typedef void GAsyncResult;
typedef void GError;
typedef void SoupCookie;
typedef void (*GAsyncReadyCallback)(GObject *source, GAsyncResult *res, void *user_data);
typedef void (*GDestroyNotify)(gpointer data);

extern void g_free(void *mem);
/* Frees a GList AND calls free_func on each element's ->data -- used to
 * release the GList<SoupCookie*> webkit_cookie_manager_get_all_cookies_finish
 * hands back (transfer-full: caller owns both the list and every cookie in
 * it, per webkitgtk.org's own CookieManager.get_all_cookies_finish docs --
 * "should be released with g_list_free_full() and soup_cookie_free()"). */
extern void g_list_free_full(GList *list, GDestroyNotify free_func);
extern GMainContext *g_main_context_default(void);
extern void g_main_context_invoke_full(GMainContext *context, int priority, GSourceFunc function,
                                        void *data, void (*notify)(void *data));
extern GMainLoop *g_main_loop_new(GMainContext *context, gboolean is_running);
extern void g_main_loop_run(GMainLoop *loop);
extern void g_main_loop_quit(GMainLoop *loop);

extern void g_object_unref(void *object);
/* Task 7 crash fix round 6: real signature returns the same pointer
 * (gpointer g_object_ref(gpointer object)) so it can be used inline, but
 * this file only ever calls it for its ref-taking side effect -- see
 * sync_window_geometry_on_gtk_thread's own comment on why. */
extern void *g_object_ref(void *object);
/* g_signal_connect_data lives in libgobject-2.0.so, not libglib-2.0.so --
 * the GObject signal system is part of GObject, not core GLib (confirmed
 * via `nm -D libgobject-2.0.so.0* | grep g_signal_connect_data` against the
 * committed bundle in Task 4 Step 1; it is NOT exported by libglib-2.0). */
extern unsigned long g_signal_connect_data(void *instance, const char *detailed_signal,
                                            GCallback c_handler, void *data,
                                            void (*destroy_data)(void *data, void *closure),
                                            int connect_flags);
/* Needed to tear down the per-navigation "load-changed" connection
 * unix_navigate_and_wait_impl makes below -- without this, the connection
 * outlives the stack-allocated struct navigate_ctx it points at as
 * closure data, and any later "load-changed" emission on the same view
 * (a second Navigate(), or the page's own internal post-login redirect)
 * calls back into freed/reused stack memory. Confirmed real via
 * `nm -D libgobject-2.0.so.0*` against the committed bundle, same as
 * g_signal_connect_data above. */
extern void g_signal_handler_disconnect(void *instance, unsigned long handler_id);

/* NOT gtk_init() -- confirmed via `strings`/`nm -D` against the real
 * committed libgtk-4.so.1.1800.6 that GTK4's gtk_init() is implemented as
 * `if (!gtk_init_check()) { g_warning(...); exit(1); }` (the library
 * contains the "Failed to open display" g_warning string and exports both
 * gtk_init and gtk_init_check as plain functions -- the G_OS_WIN32-only
 * gtk_init()/gtk_init_check() ABI-check macros in gtkmain.h don't apply on
 * Linux). Calling gtk_init() from the GTK thread would exit(1) the whole
 * Roblox process the moment DISPLAY/WAYLAND_DISPLAY isn't usable, with no
 * chance to return STATUS_NOT_SUPPORTED gracefully instead. gtk_init_check()
 * has the identical signature/no-arg contract but returns gboolean instead
 * of aborting. */
extern gboolean gtk_init_check(void);
extern GtkWidget *gtk_window_new(void);
extern void gtk_window_set_child(GtkWindow *window, GtkWidget *child);
/* Task 7 crash fix, round 13: real GTK4 API (docs.gtk.org/gtk4/method.
 * Window.set_decorated.html) -- without this, nv->window is a completely
 * ordinary top-level GtkWindow as far as the window manager (KWin, in
 * this environment) is concerned, so it draws its own full title bar,
 * borders, and min/max/close controls on it, exactly like any other
 * independent app window. Real WebView2 controls on native Windows are
 * borderless child controls with zero window chrome of their own -- the
 * actual visual target here. Fixing round 12's position/xid bug alone
 * (real, kept -- XMoveResizeWindow now genuinely succeeds with a valid
 * xid) still wasn't enough on its own for this to visually read as
 * "docked into Studio" rather than "a second window sitting on top of
 * the first": confirmed via the repo owner's own live screenshots, which
 * show this window's own titled frame even after position sync started
 * genuinely working. */
extern void gtk_window_set_decorated(GtkWindow *window, gboolean setting);
extern void gtk_window_present(GtkWindow *window);
extern void gtk_widget_show(GtkWidget *widget);
extern gboolean gtk_widget_get_visible(GtkWidget *widget);
/* Task 7 crash fix, round 11: diagnostic-only (repo owner wants real
 * docking fixed, not just the crash guarded -- see this file's own git
 * log). Real, documented GTK4 API (docs.gtk.org/gtk4/method.Widget.
 * get_realized.html / get_mapped.html) used here purely to observe the
 * ACTUAL live realize/map state of nv->window at the moment
 * gdk_x11_surface_get_xid returns garbage, rather than relying on GTK4's
 * own documented (but unverified-live) contract that gtk_widget_show()
 * on a toplevel synchronously realizes+maps it. */
extern gboolean gtk_widget_get_realized(GtkWidget *widget);
extern gboolean gtk_widget_get_mapped(GtkWidget *widget);
/* Destroys the window and, since it's still set as the window's child at
 * that point, its WebKitWebView along with it -- GTK4's normal container
 * ownership model tears down children when their parent is destroyed, so
 * no separate g_object_unref() on the view is needed here. Confirmed real
 * (`nm -D libgtk-4.so.1*`) against the committed bundle. */
extern void gtk_window_destroy(GtkWindow *window);

/* Plan 3 Task 3: position-sync support types/externs. GTK4 removed
 * GTK3's gtk_window_move/gtk_window_resize/gdk_window_move_resize entirely
 * -- confirmed by grepping the real bundled gtk-4.0/gdk headers
 * (ProtonBuild/dist/files/lib/tuxblox-webview/include/gtk-4.0/):
 * GdkToplevel's only position-related call is gdk_toplevel_begin_move (an
 * interactive, user-gesture-driven drag), and even the X11-specific
 * gdkx11surface.h has no move/resize entry point. There is no cross-backend,
 * public GTK4 API for programmatic absolute window positioning at all, so
 * this resolves the toplevel's real X11 XID via gdk_x11_surface_get_xid and
 * moves/resizes it directly via Xlib's XMoveResizeWindow (see X11_FUNCS
 * below for how libX11.so.6 gets into this file's namespace without adding
 * a new dependency). */
typedef void GdkSurface;
typedef void GdkDisplay;
typedef void GtkNative;
typedef void Display; /* Xlib's opaque connection handle */

extern GtkNative *gtk_widget_get_native(GtkWidget *widget);
extern GdkSurface *gtk_native_get_surface(GtkNative *self);
extern GdkDisplay *gdk_surface_get_display(GdkSurface *surface);
extern int gdk_surface_get_width(GdkSurface *surface);
extern int gdk_surface_get_height(GdkSurface *surface);
/* Real, exported by libgtk-4.so.1 itself (GTK4 merged GDK into one .so);
 * confirmed via `nm -D libgtk-4.so.1.1800.6` against the bundle. Returns
 * the real X11 Window XID, or 0 (with a non-fatal GLib CRITICAL logged --
 * same "CRITICAL but non-fatal" behavior this file already relies on for
 * webkit_cookie_manager_replace_cookies(NULL), see that call's own comment)
 * if `surface` isn't backed by the X11 GDK backend, i.e. running under
 * Wayland. */
extern unsigned long gdk_x11_surface_get_xid(GdkSurface *surface);
extern Display *gdk_x11_display_get_xdisplay(GdkDisplay *display);
/* Task 7 crash fix round 5: a real GdkDisplay-liveness check. Round 4's
 * investigation (a dedicated subagent verifying the dlmopen/RTLD_NOLOAD
 * namespace join is provably correct, ruling out a cross-namespace
 * libX11 mismatch) narrowed this down to the most concrete remaining
 * lead: nothing between resolving `display` here and actually using it
 * a few lines down re-checks that the underlying X11 connection is still
 * open -- the existing live_webviews registry only covers the per-webview
 * handle, not this shared, singleton-ish connection object. Real, public
 * GDK4 API (gdk4/gdk/gdkdisplay.h: "gdk_display_is_closed: ... Finds out
 * if the display has been closed.") -- exactly the check needed here. */
extern gboolean gdk_display_is_closed(GdkDisplay *display);
extern void gtk_widget_set_visible(GtkWidget *widget, gboolean visible);
extern void gtk_window_set_default_size(GtkWindow *window, int width, int height);

/* Real Xlib entry point, confirmed exported by the HOST's own libX11.so.6
 * (`nm -D /usr/lib/libX11.so.6`) -- this file never dlopen()s libX11
 * itself (see x11_handle below: it is already resident in the same
 * dlmopen namespace as libgtk-4.so.1's own mandatory NEEDED dependency by
 * the time load_bundle_functions finishes loading gtk_handle). Declared
 * here, not via a real <X11/Xlib.h> include, for the same reason every
 * other extern in this file is hand-declared: typeof() needs a real
 * signature to compute a function-pointer type from, without actually
 * linking against libX11 at Wine's own build time. */
extern int XMoveResizeWindow(Display *display, unsigned long w, int x, int y,
                              unsigned int width, unsigned int height);

/* Task 7 crash fix, round 15: real X11-level reparenting -- see this
 * file's own sync_window_geometry_on_gtk_thread and controller.c's
 * controller_push_geometry_to_native for the full rationale (repo
 * owner's direct ask: real embedding, not a manually-repositioned
 * floating window). Real Xlib entry points, same host-resident-library
 * situation as XMoveResizeWindow's own extern comment above -- no new
 * host dependency. */
extern int XReparentWindow(Display *display, unsigned long w, unsigned long parent, int x, int y);
/* Real return type is Xlib's Bool (an int), per the same "hand-declare,
 * no real header" convention as every other extern in this file. Used to
 * translate the already-computed absolute screen position into
 * coordinates relative to the NEW parent (whatever offset the parent's
 * own window-manager decorations/borders/Wine-drawn chrome introduce
 * between its own top-left corner and the root window's origin) --
 * genuine X11 coordinate translation, not a guessed/hardcoded offset. */
extern int XTranslateCoordinates(Display *display, unsigned long src_w, unsigned long dest_w,
                                  int src_x, int src_y, int *dest_x_return, int *dest_y_return,
                                  unsigned long *child_return);
extern unsigned long XDefaultRootWindow(Display *display);

/* Task 7 crash fix, round 17: a real, reproducible crash found via the
 * fast unit-test pass (not yet a real relaunch -- caught before it could
 * become one): XReparentWindow (round 15) against a stale/invalid
 * parent_xid produced a genuine X11 protocol error (BadWindow, request
 * code 7 == ReparentWindow), and Xlib's OWN default error handler
 * (installed by GDK during its own init, or Xlib's own built-in default
 * if nothing else claimed it) treats ANY X protocol error as FATAL --
 * calling exit()/abort() on the whole process. This is a real, serious
 * hazard specifically FOR round 15's own new XReparentWindow call: unlike
 * XMoveResizeWindow (always operating on a window this DLL itself
 * created and fully owns the lifecycle of), XReparentWindow's second
 * argument is a window this DLL does NOT own or control the lifecycle of
 * at all (Wine's own winex11.drv, reading a value that could be stale --
 * see parent_xid's own comment on whole_window recreation) -- exactly
 * the kind of externally-sourced, not-fully-trustworthy value real,
 * defensive Xlib code brackets with a custom, non-fatal error handler.
 * Real Xlib API (XSetErrorHandler, X11/Xlib.h) -- installed in
 * gtk_thread_proc AFTER gtk_init_check() succeeds (so it overrides
 * whatever handler GDK's own X11 backend init installed, rather than
 * being overridden BY it), covering every raw Xlib call this file makes
 * from that point on, not just XReparentWindow specifically. */
typedef struct
{
    int type;
    Display *display;
    unsigned long serial;
    unsigned char error_code;
    unsigned char request_code;
    unsigned char minor_code;
    unsigned long resourceid;
} XErrorEvent;
extern int (*XSetErrorHandler(int (*handler)(Display *, XErrorEvent *)))(Display *, XErrorEvent *);

/* Task 7 crash fix, round 18: forces the just-issued XReparentWindow
 * request (and any error reply the server sends back for it) to actually
 * complete/arrive before the code right after it checks whether
 * x11_error_handler fired -- see that call site's own comment. Real,
 * standard Xlib API (X11/Xlib.h); the `int` second parameter is
 * Xlib's own Bool `discard` flag (0 == don't discard queued events, just
 * flush and wait, which is what's wanted here -- discarding would also
 * throw away legitimate unrelated events other parts of this file's own
 * event handling might still need). */
extern int XSync(Display *display, int discard);

/* Task 7 crash fix, round 19: the actual root-cause fix for the
 * put_Bounds-after-reparent size-readback bug -- see
 * sync_window_geometry_on_gtk_thread's own comment at the XSendEvent call
 * site for the full ICCCM-based explanation. XSendEvent's real signature
 * takes an XEvent* (a union of every core event struct, padded to the
 * largest member's size); this file has no real <X11/Xlib.h> to pull that
 * union in from, so -- same hand-declare convention as XErrorEvent above --
 * only the ConfigureNotify-shaped view of it is declared here
 * (XConfigureEvent, matching real Xlib's exact field layout/order), which is
 * exactly the view XSendEvent's own wire-marshalling code reads through
 * once `.type == ConfigureNotify` selects that branch internally. Passed to
 * XSendEvent via a cast, the same pattern any real X11 client code sending
 * a synthetic ConfigureNotify uses -- safe because XConfigureEvent is
 * smaller than the real XEvent union it's standing in for. */
typedef struct
{
    int type;
    unsigned long serial;
    int send_event;
    Display *display;
    unsigned long event;
    unsigned long window;
    int x, y;
    int width, height;
    int border_width;
    unsigned long above;
    int override_redirect;
} XConfigureEvent;
extern int XSendEvent(Display *display, unsigned long w, int propagate, long event_mask, void *event_send);
/* Real X11 core protocol constants (X11/X.h) -- ConfigureNotify's numeric
 * event-type code and the StructureNotifyMask event-selection bit a client
 * must pass to XSendEvent for the server to actually deliver a synthetic
 * event to a window that didn't itself opt into receiving it via
 * SelectInput (StructureNotifyMask is exactly the mask GDK/GTK always
 * selects on its own toplevel surfaces already, for its own real-event
 * handling -- this just has to match that same bit for XSendEvent's
 * targeted delivery to reach it). Hand-declared, not pulled from a real
 * header, for the same reason every other X11 constant/struct in this file
 * is -- no real X11 headers are available at Wine's own build time here. */
#define TUXBLOX_ConfigureNotify 22
#define TUXBLOX_StructureNotifyMask (1L << 17)

/* Task 7 real-launch crash fix, round 3: a real relaunch still segfaulted
 * at this exact call site (_XGetRequest, deep inside XMoveResizeWindow)
 * even after both the NULL-Display* guard and the stale-handle registry
 * above -- confirmed via coredump/GDB that the handle was genuinely live
 * and every intermediate value (surface, xid, display) was non-NULL right
 * up to the fault. Xlib's per-Display connection state (request buffers,
 * sequence counters, etc.) is not safe to touch from more than one thread
 * at a time unless the caller brackets *every* access -- including raw
 * calls like this one that bypass GDK's own higher-level API entirely --
 * with XLockDisplay/XUnlockDisplay, and that only works at all if
 * XInitThreads was called before any other Xlib activity in the process
 * (see gtk_thread_proc's own call to it, added alongside this). WebKitGTK
 * is known to touch the shared X11 connection from auxiliary threads of
 * its own (compositing/GL) outside GDK's main-loop thread, so this raw
 * call -- previously unguarded -- was exactly the kind of access this
 * class of Xlib bug hits. */
extern int XInitThreads(void); /* real return type is Xlib's Status, itself
                                 * just a typedef for int -- same convention
                                 * XMoveResizeWindow's own declaration above
                                 * already uses for the same reason (no real
                                 * <X11/Xlib.h> include in this file). */
extern void XLockDisplay(Display *display);
extern void XUnlockDisplay(Display *display);

extern GtkWidget *webkit_web_view_new(void);
extern void webkit_web_view_load_uri(WebKitWebView *web_view, const char *uri);
extern const char *webkit_web_view_get_uri(WebKitWebView *web_view);

/* Login-redirect fix (roblox-studio-auth:/... OAuth handoff): real,
 * hand-declared subset of WebKitPolicyDecision/WebKitNavigationAction/
 * WebKitURIRequest, confirmed against the real committed bundle headers
 * (ProtonBuild/dist/files/lib/tuxblox-webview/include/webkitgtk-6.0/webkit/
 * WebKit{PolicyDecision,NavigationPolicyDecision,NavigationAction,
 * URIRequest,WebView}.h) -- not guessed, and re-derived fresh here since
 * round 16's own version of these declarations was fully reverted (see
 * progress.md); nothing of round 16's actually survived to build on.
 * WebKitNavigationPolicyDecision is WebKitPolicyDecision's own concrete
 * subtype for WEBKIT_POLICY_DECISION_TYPE_NAVIGATION_ACTION/
 * _NEW_WINDOW_ACTION (per WebKitWebView.h's own WebKitPolicyDecisionType
 * doc comment) -- a plain opaque pointer, same convention as every other
 * GObject-derived type already in this file (WebKitWebView,
 * WebKitCookieManager, ...), so no extra cast machinery is needed. */
typedef void WebKitPolicyDecision;
typedef void WebKitNavigationPolicyDecision;
typedef void WebKitNavigationAction;
typedef void WebKitURIRequest;
/* Real values, confirmed from the bundle's own WebKitWebView.h enum
 * (NAVIGATION_ACTION first, so numerically 0) -- NAVIGATION_ACTION is
 * requested for main-frame/subframe navigations, which is what WebKit's
 * own internal redirect to a custom roblox-studio-auth: URI triggers;
 * NEW_WINDOW_ACTION is for window.open()-style navigations, RESPONSE for
 * a network response about to be loaded. Only NAVIGATION_ACTION is
 * handled below -- see on_decide_policy's own comment for why. */
typedef enum {
    WEBKIT_POLICY_DECISION_TYPE_NAVIGATION_ACTION,
    WEBKIT_POLICY_DECISION_TYPE_NEW_WINDOW_ACTION,
    WEBKIT_POLICY_DECISION_TYPE_RESPONSE,
} WebKitPolicyDecisionType;

extern WebKitNavigationAction *webkit_navigation_policy_decision_get_navigation_action(
    WebKitNavigationPolicyDecision *decision);
extern WebKitURIRequest *webkit_navigation_action_get_request(WebKitNavigationAction *navigation);
extern const char *webkit_uri_request_get_uri(WebKitURIRequest *request);
extern void webkit_policy_decision_ignore(WebKitPolicyDecision *decision);

extern WebKitNetworkSession *webkit_web_view_get_network_session(WebKitWebView *web_view);
extern WebKitCookieManager *webkit_network_session_get_cookie_manager(WebKitNetworkSession *session);
/* Task 8: real "delete all cookies" implementation -- see the WEBKIT_FUNCS
 * comment below (previously left by Task 4) for why there's no dedicated
 * delete-all entry point in this WebKitGTK version, and
 * unix_delete_all_cookies_impl's own comment for why this ended up as
 * get_all_cookies+delete_cookie rather than the brief's originally
 * specified replace_cookies(NULL) (verified broken against the real
 * bundle -- see that comment for the exact runtime evidence). */
extern void webkit_cookie_manager_get_all_cookies(WebKitCookieManager *cookie_manager, GCancellable *cancellable,
                                                    GAsyncReadyCallback callback, gpointer user_data);
extern GList *webkit_cookie_manager_get_all_cookies_finish(WebKitCookieManager *cookie_manager,
                                                             GAsyncResult *result, GError **error);
/* Task 11 real-bug fix: the real, exported, URI-scoped sibling of
 * get_all_cookies above (confirmed real and exported via `nm -D
 * libwebkitgtk-6.0.so.4* | grep webkit_cookie_manager_get_cookies` against
 * the committed bundle, and its exact signature confirmed against the real
 * committed header, include/webkitgtk-6.0/webkit/WebKitCookieManager.h) --
 * this is what lets unix_get_cookies_impl below satisfy real
 * ICoreWebView2CookieManager::GetCookies's own documented uri-filtering
 * semantics ("Gets a list of cookies matching the specific URI") without
 * hand-rolling domain/path cookie-applicability matching ourselves: WebKit
 * already implements the standard algorithm here. */
extern void webkit_cookie_manager_get_cookies(WebKitCookieManager *cookie_manager, const char *uri,
                                                GCancellable *cancellable, GAsyncReadyCallback callback,
                                                gpointer user_data);
extern GList *webkit_cookie_manager_get_cookies_finish(WebKitCookieManager *cookie_manager,
                                                         GAsyncResult *result, GError **error);
extern void webkit_cookie_manager_delete_cookie(WebKitCookieManager *cookie_manager, SoupCookie *cookie,
                                                 GCancellable *cancellable, GAsyncReadyCallback callback,
                                                 gpointer user_data);
/* libsoup, not glib/gobject/gtk/webkit -- SoupCookie objects returned by
 * get_all_cookies_finish are libsoup's own type (confirmed via `nm -D
 * libwebkitgtk-6.0.so.4 | grep soup_cookie`: webkit only IMPORTS
 * soup_cookie_free/soup_cookie_new, doesn't export them itself). Needs its
 * own dlopen handle -- see soup_handle below.
 *
 * (A webkit_cookie_manager_add_cookie/soup_cookie_new-based test-support
 * "inject an arbitrary cookie" unix call briefly existed here and was
 * removed after code review: it was reachable from webview2loader.dll's own
 * PE export table with zero validation, on the exact same live cookie store
 * the real client's session cookies live in -- a real capability-widening
 * risk, not just test scaffolding, since this Makefile.in produces one
 * unconditional production DLL with no test/production build split. See
 * struct count_cookies_params's own comment in unixlib.h and
 * tests/cookie_test_server.py for how cookie-add verification works now
 * instead, entirely through the already-legitimate Navigate() path.) */
extern void soup_cookie_free(SoupCookie *cookie);
/* Task 11 real-bug fix: real per-cookie field accessors, confirmed real and
 * exported (`nm -D libsoup-3.0.so.0* | grep soup_cookie_get`) and their
 * exact signatures confirmed against the real committed header,
 * include/libsoup-3.0/libsoup/soup-cookie.h -- not guessed. get_expires
 * returns a GDateTime* owned by the cookie (transfer-none per the real
 * libsoup source's own doc comment: "owned by @cookie and should not be
 * modified or freed"), or NULL for a session cookie -- NULL-ness is exactly
 * real ICoreWebView2Cookie::IsSession's own signal (see
 * struct unix_cookie's own comment in unixlib.h). SoupSameSitePolicy's
 * three values are numerically identical to COREWEBVIEW2_COOKIE_SAME_SITE_KIND's
 * (both confirmed from their real headers), so no translation table is
 * needed to pass one straight through as the other's underlying int. */
extern const char *soup_cookie_get_name(SoupCookie *cookie);
extern const char *soup_cookie_get_value(SoupCookie *cookie);
extern const char *soup_cookie_get_domain(SoupCookie *cookie);
extern const char *soup_cookie_get_path(SoupCookie *cookie);
typedef void GDateTime;
extern GDateTime *soup_cookie_get_expires(SoupCookie *cookie);
extern gboolean soup_cookie_get_http_only(SoupCookie *cookie);
extern gboolean soup_cookie_get_secure(SoupCookie *cookie);
typedef enum { SOUP_SAME_SITE_POLICY_NONE, SOUP_SAME_SITE_POLICY_LAX, SOUP_SAME_SITE_POLICY_STRICT } SoupSameSitePolicy;
extern SoupSameSitePolicy soup_cookie_get_same_site_policy(SoupCookie *cookie);
/* GLib, not libsoup -- GDateTime is core GLib, already resident via
 * glib_handle (confirmed real/exported: `nm -D libglib-2.0.so.0* | grep
 * g_date_time_to_unix` against the committed bundle). gint64 is GLib's own
 * 8-byte signed integer typedef; declared here as `long long` (same width,
 * same x86-64 calling-convention register) purely so typeof() below has
 * something to compute a function-pointer type from, matching this file's
 * existing convention for every other hand-declared extern. */
extern long long g_date_time_to_unix(GDateTime *datetime);

/* Task 11 real-bug fix: JavaScriptCore's real, public, exported C API for
 * moving its GC/JIT "safepoint" signal off its Linux default of SIGUSR1 --
 * confirmed exported (`nm -D libjavascriptcoregtk-6.0.so.1 | grep
 * JSConfigureSignalForGC`), confirmed via a standalone dlopen probe (outside
 * Wine entirely) to actually work end-to-end, and returns bool (see below),
 * matching its real declared signature (WebKitGTK's own
 * Source/JavaScriptCore/API/JSBasePrivate.h: "JS_EXPORT bool
 * JSConfigureSignalForGC(int signal);", documented as "Call this function
 * before any of JSC initialization starts. Otherwise, it fails.").
 *
 * The JSC_SIGNAL_FOR_GC env var WebKit's own stderr warning suggests
 * ("Overriding existing handler for signal %d. Set JSC_SIGNAL_FOR_GC if you
 * want WebKit to use a different signal") ALSO actually works -- confirmed
 * with the same kind of standalone probe used below (dummy SIGUSR1 handler
 * installed beforehand survives webview creation with JSC_SIGNAL_FOR_GC=30
 * set, and a subsequent raise(SIGUSR1) no longer crashes). The permanent
 * "ERROR: invalid option: JSC_SIGNAL_FOR_GC=<value>" line it prints on
 * stderr on every single run is a red herring from a separate, unrelated
 * generic scan over all JSC_-prefixed env vars (WebKit's Options.cpp) that
 * doesn't gate whether the variable actually took effect -- it's silently
 * non-fatal by default, but would CRASH() the whole process instead if
 * WebKit's own JSC_validateOptions is ever set (by us or anything else
 * sharing this environment), which the exported API has no exposure to at
 * all. The exported API call below is used instead because it's strictly
 * better on every axis that matters here: no permanent stderr noise, no
 * exposure to validateOptions, and no dependency on an env var actually
 * propagating unmangled through Proton's own environment plumbing -- not
 * because the env var doesn't work.
 *
 * (Earlier investigation here initially misread the "0"/"-1" crash-aborts
 * seen while probing plausible env var values as evidence the var was
 * being ignored. It isn't: sigaction(0, ...)/sigaction(-1, ...)
 * legitimately fail for those signal numbers, which trips a real
 * RELEASE_ASSERT inside WTF's own signal-setup code
 * (Source/WTF/wtf/posix/ThreadingPOSIX.cpp) -- that assert firing is
 * itself proof the env var's value WAS being read and used, the opposite
 * of what was originally concluded.)
 *
 * Verified end-to-end with a standalone probe: calling this with SIGPWR
 * (30) before the first webkit_web_view_new() means JSC never touches
 * SIGUSR1 at all -- no "Overriding existing handler" line, and a dummy
 * SIGUSR1 handler installed beforehand (standing in for ntdll's real
 * usr1_handler, dlls/ntdll/unix/signal_x86_64.c -- "used to signal a thread
 * that it got suspended", the actual mechanism SuspendThread/
 * GetThreadContext/SetThreadContext rely on on Linux) survives completely
 * intact afterward, and a subsequent raise(SIGUSR1) returns normally.
 * WITHOUT calling it, the same probe reproduces the real crash mechanism
 * directly: the dummy handler gets silently replaced, and a plain
 * raise(SIGUSR1) afterward segfaults the whole process outright -- matching
 * this plan's own real Roblox Studio reproduction, where two unrelated
 * threads (ordinary NtWaitForAlertByThreadId/NtWaitForMultipleObjects
 * waiters, not anything of ours) crashed simultaneously inside Roblox's
 * bundled ucrtbase.dll within ~15ms of that same stderr line appearing.
 * Declared here as its own extern (not folded into WEBKIT_FUNCS) because it
 * lives in libjavascriptcoregtk-6.0.so.1, a separate library from
 * libwebkitgtk-6.0.so.4 -- see javascriptcore_handle below. */
extern _Bool JSConfigureSignalForGC(int signalNumber);

#define GLIB_FUNCS \
    DO_FUNC(g_main_context_default); \
    DO_FUNC(g_main_context_invoke_full); \
    DO_FUNC(g_main_loop_new); \
    DO_FUNC(g_main_loop_run); \
    DO_FUNC(g_main_loop_quit); \
    DO_FUNC(g_free); \
    DO_FUNC(g_list_free_full); \
    DO_FUNC(g_date_time_to_unix)

#define GOBJECT_FUNCS \
    DO_FUNC(g_object_unref); \
    DO_FUNC(g_object_ref); \
    DO_FUNC(g_signal_connect_data); \
    DO_FUNC(g_signal_handler_disconnect)

#define GTK_FUNCS \
    DO_FUNC(gtk_init_check); \
    DO_FUNC(gtk_window_new); \
    DO_FUNC(gtk_window_set_decorated); \
    DO_FUNC(gtk_window_set_child); \
    DO_FUNC(gtk_window_present); \
    DO_FUNC(gtk_widget_show); \
    DO_FUNC(gtk_widget_get_visible); \
    DO_FUNC(gtk_widget_get_realized); \
    DO_FUNC(gtk_widget_get_mapped); \
    DO_FUNC(gtk_widget_get_native); \
    DO_FUNC(gtk_native_get_surface); \
    DO_FUNC(gdk_surface_get_display); \
    DO_FUNC(gdk_surface_get_width); \
    DO_FUNC(gdk_surface_get_height); \
    DO_FUNC(gdk_x11_surface_get_xid); \
    DO_FUNC(gdk_x11_display_get_xdisplay); \
    DO_FUNC(gdk_display_is_closed); \
    DO_FUNC(gtk_widget_set_visible); \
    DO_FUNC(gtk_window_set_default_size); \
    DO_FUNC(gtk_window_destroy)

/* Task 8: webkit_cookie_manager_delete_all_cookies does NOT exist in this
 * WebKitGTK version (Task 4 Step 1's `nm -D ... | grep -i cookie` against
 * the real committed bundle confirmed this). The task brief's originally
 * specified "delete everything" equivalent -- webkit_cookie_manager_
 * replace_cookies() called with an empty (NULL) cookie list -- was tried
 * first and found broken against the REAL bundle at runtime: it fires
 * `CRITICAL **: void webkit_cookie_manager_replace_cookies(...): assertion
 * 'cookies' failed` (a glib g_return_if_fail(cookies) inside WebKit's own
 * implementation) and returns without doing anything -- confirmed via the
 * test binary's actual stdout during this task's own verification run,
 * not a guess. GLib's own convention treats a NULL GList as a legitimate
 * empty list everywhere else, but this specific entry point's real
 * implementation rejects it outright, so "replace with nothing" cannot
 * express "delete all" here. The real fix used instead: enumerate every
 * cookie via webkit_cookie_manager_get_all_cookies(_finish) and delete each
 * one individually via webkit_cookie_manager_delete_cookie -- see
 * unix_delete_all_cookies_impl below. */
#define WEBKIT_FUNCS \
    DO_FUNC(webkit_web_view_new); \
    DO_FUNC(webkit_web_view_load_uri); \
    DO_FUNC(webkit_web_view_get_uri); \
    DO_FUNC(webkit_navigation_policy_decision_get_navigation_action); \
    DO_FUNC(webkit_navigation_action_get_request); \
    DO_FUNC(webkit_uri_request_get_uri); \
    DO_FUNC(webkit_policy_decision_ignore); \
    DO_FUNC(webkit_web_view_get_network_session); \
    DO_FUNC(webkit_network_session_get_cookie_manager); \
    DO_FUNC(webkit_cookie_manager_get_all_cookies); \
    DO_FUNC(webkit_cookie_manager_get_all_cookies_finish); \
    DO_FUNC(webkit_cookie_manager_get_cookies); \
    DO_FUNC(webkit_cookie_manager_get_cookies_finish); \
    DO_FUNC(webkit_cookie_manager_delete_cookie)

/* libsoup-3.0 -- needs its own dlopen handle (soup_handle below), separate
 * from webkit_handle; see soup_cookie_free's own extern declaration comment
 * above for why. */
#define SOUP_FUNCS \
    DO_FUNC(soup_cookie_free); \
    DO_FUNC(soup_cookie_get_name); \
    DO_FUNC(soup_cookie_get_value); \
    DO_FUNC(soup_cookie_get_domain); \
    DO_FUNC(soup_cookie_get_path); \
    DO_FUNC(soup_cookie_get_expires); \
    DO_FUNC(soup_cookie_get_http_only); \
    DO_FUNC(soup_cookie_get_secure); \
    DO_FUNC(soup_cookie_get_same_site_policy)

/* libjavascriptcoregtk-6.0.so.1 -- see JSConfigureSignalForGC's own extern
 * declaration comment above. Confirmed a real NEEDED dependency of
 * libwebkitgtk-6.0.so.4 (`readelf -d libwebkitgtk-6.0.so.4 | grep
 * javascriptcore`), same situation as soup_handle: already resident in the
 * shared isolated namespace by the time this runs, so this load_one() call
 * just joins that namespace and hands back a handle scoped to its own
 * exported symbols. */
#define JAVASCRIPTCORE_FUNCS \
    DO_FUNC(JSConfigureSignalForGC)

/* libX11.so.6 -- NOT part of the Plan-1 bundle (Plan 1 only bundles
 * GTK4/WebKitGTK/GLib and friends). Real, confirmed NEEDED dependency of
 * the bundle's own libgtk-4.so.1 (`readelf -d libgtk-4.so.1.1800.6 | grep
 * NEEDED` lists libX11.so.6 -- GTK4 always links X11 backend support, even
 * when the process ultimately runs under Wayland), so it is unconditionally
 * already resident in the SAME isolated dlmopen namespace by the time
 * gtk_handle finishes loading below -- joined here via RTLD_NOLOAD rather
 * than loaded fresh, adding no new host dependency beyond what GTK4 itself
 * already requires to load at all (see load_one's own sibling helper,
 * join_loaded, just below). */
#define X11_FUNCS \
    DO_FUNC(XMoveResizeWindow); \
    DO_FUNC(XInitThreads); \
    DO_FUNC(XLockDisplay); \
    DO_FUNC(XUnlockDisplay); \
    DO_FUNC(XReparentWindow); \
    DO_FUNC(XTranslateCoordinates); \
    DO_FUNC(XDefaultRootWindow); \
    DO_FUNC(XSetErrorHandler); \
    DO_FUNC(XSync); \
    DO_FUNC(XSendEvent)

#define DO_FUNC(f) typeof(f) *p_##f
GLIB_FUNCS; GOBJECT_FUNCS; GTK_FUNCS; WEBKIT_FUNCS; SOUP_FUNCS; JAVASCRIPTCORE_FUNCS; X11_FUNCS;
#undef DO_FUNC

static void *glib_handle, *gobject_handle, *gtk_handle, *webkit_handle, *soup_handle, *javascriptcore_handle, *x11_handle;

/* Loads relpath into the isolated linker namespace identified by *lmid.
 *
 * On the very first call, *lmid must be LM_ID_NEWLM: dlmopen() then creates
 * a brand new, empty linker namespace and loads this library into it. We
 * capture which namespace it actually landed in (via dlinfo()'s
 * RTLD_DI_LMID request) back into *lmid, so every subsequent call --
 * passing that same *lmid instead of LM_ID_NEWLM again -- joins THE SAME
 * namespace rather than each spawning its own brand new one. This matters
 * because these four libraries have real dependencies on each other (gtk
 * needs gobject needs glib) and must be able to resolve each other's
 * symbols/sonames within one shared namespace.
 *
 * Why a namespace instead of plain dlopen()+RTLD_GLOBAL (round 2's
 * attempted fix, which did NOT work): investigation round 3
 * (investigation-glib-collision-round3-report.md) traced the real crash
 * cause with LD_DEBUG=libs,files and found the host's own
 * /usr/lib/libharfbuzz.so.0 gets loaded into the process's DEFAULT
 * namespace by something intrinsic to Wine's own early startup, before our
 * own unix_init ever runs -- and that host harfbuzz pulls in the host's
 * own GLib as ITS transitive dependency. RTLD_GLOBAL only affects whether
 * OUR OWN loaded libraries become visible to OUR OWN later dlopen() calls;
 * it does nothing about a library loaded earlier, by something entirely
 * outside our control, into the shared default namespace. Once the host's
 * libharfbuzz.so.0 is resident in the default namespace, glibc's dynamic
 * linker reuses that exact mapping for ANY later same-soname request in
 * that namespace -- including from the bundle's own libgtk-4.so.1/
 * libpango*, regardless of those libraries' own (correct) RPATH, since
 * RPATH only affects where a library's OWN deps are searched, not whether
 * an already-loaded soname gets reused. dlmopen(LM_ID_NEWLM, ...) sidesteps
 * this entirely: everything loaded into the new namespace resolves its own
 * dependencies (including transitive ones like harfbuzz) only against
 * libraries already in THAT namespace, never reusing anything the default
 * namespace (host or Wine) has already loaded. */
static void *load_one(const char *dir, const char *relpath, Lmid_t *lmid)
{
    char path[PATH_MAX];
    void *h;

    snprintf(path, sizeof(path), "%s/%s", dir, relpath);
    h = dlmopen(*lmid, path, RTLD_NOW);
    if (!h)
    {
        WARN("failed to load %s: %s\n", path, dlerror());
        return NULL;
    }

    if (*lmid == LM_ID_NEWLM)
    {
        if (dlinfo(h, RTLD_DI_LMID, lmid) != 0)
        {
            WARN("dlinfo(RTLD_DI_LMID) failed for %s: %s\n", path, dlerror());
            return NULL;
        }
    }

    return h;
}

/* Joins an ALREADY-LOADED library in the same dlmopen namespace without
 * loading anything new -- distinct from load_one() above, which loads a
 * bundle-relative path fresh. libX11.so.6 isn't a bundle file (see
 * X11_FUNCS's own comment); it only ever gets INTO this namespace as
 * libgtk-4.so.1's own transitive NEEDED dependency, resolved automatically
 * by the dynamic linker (from the host's own /usr/lib/libX11.so.6, per
 * `ldconfig -p`) the moment load_one() dlmopen()s libgtk-4.so.1 itself --
 * shared-object NEEDED dependencies are always mapped in immediately
 * regardless of RTLD_NOW/RTLD_LAZY (only individual SYMBOL resolution is
 * ever lazy). RTLD_NOLOAD hands back a handle to that existing mapping
 * without incrementing dlopen's own "fresh load" bookkeeping incorrectly
 * or risking a second, separate mapping. */
static void *join_loaded(const char *soname, Lmid_t lmid)
{
    void *h = dlmopen(lmid, soname, RTLD_NOW | RTLD_NOLOAD);
    if (!h) WARN("failed to join already-loaded %s: %s\n", soname, dlerror());
    return h;
}

/* Relative paths below were confirmed real against the committed bundle in
 * Task 4 Step 1 -- re-check with `nm -D`/`ls` if this ever stops finding a
 * symbol, rather than guessing a new name.
 *
 * Deviation from the original task brief: libglib-2.0/libgobject-2.0/
 * libgtk-4 all live under lib/x86_64-linux-gnu/ (the Debian multiarch
 * convention meson-built dependencies use), but libwebkitgtk-6.0.so.4
 * itself lives directly under lib/ -- WebKitGTK's own CMake build installs
 * there, not into the multiarch subdir. Confirmed both by extracting the
 * real committed tarball (Task 4 Step 1) and by webkitgtk-bundle/
 * package.sh's own RPATH-rewrite comment, which sets every ELF file's
 * RPATH to "$ORIGIN:$ORIGIN/../lib:$ORIGIN/../lib/x86_64-linux-gnu" for
 * exactly this reason (both directories need to be reachable). Using a
 * single hardcoded "lib/x86_64-linux-gnu/%s" for every soname (as the
 * original brief draft did) would fail to dlopen libwebkitgtk-6.0.so.4 at
 * all. */
static BOOL load_bundle_functions(void)
{
    /* Starts as LM_ID_NEWLM so the first load_one() call creates a fresh
     * isolated namespace; load_one() overwrites this with the namespace it
     * actually landed in, so every later call joins that same namespace
     * instead of each creating its own. */
    Lmid_t lmid = LM_ID_NEWLM;
    const char *dir = getenv("TUXBLOX_WEBVIEW_DIR");
    if (!dir || !dir[0])
    {
        WARN("TUXBLOX_WEBVIEW_DIR not set -- not running under this repo's proton\n");
        return FALSE;
    }

    if (!(glib_handle = load_one(dir, "lib/x86_64-linux-gnu/libglib-2.0.so.0", &lmid))) return FALSE;
    if (!(gobject_handle = load_one(dir, "lib/x86_64-linux-gnu/libgobject-2.0.so.0", &lmid))) return FALSE;
    if (!(gtk_handle = load_one(dir, "lib/x86_64-linux-gnu/libgtk-4.so.1", &lmid))) return FALSE;
    /* Plan 3 Task 3: libX11.so.6 is already resident in this same namespace
     * as libgtk-4.so.1's own mandatory dependency -- see X11_FUNCS's own
     * comment. */
    if (!(x11_handle = join_loaded("libX11.so.6", lmid))) return FALSE;
    if (!(webkit_handle = load_one(dir, "lib/libwebkitgtk-6.0.so.4", &lmid))) return FALSE;
    /* Task 8: libsoup-3.0 is already a real NEEDED dependency of
     * libwebkitgtk-6.0.so.4 (confirmed via `readelf -d`), so it's already
     * resident in the shared isolated namespace by the time this runs --
     * this load_one() call just joins that same namespace (same lmid) and
     * hands back a handle to the already-loaded mapping (standard dlopen
     * refcount-bump behavior for a same-soname, same-namespace request),
     * needed so soup_cookie_free can be dlsym'd from a handle scoped to
     * libsoup's own exported symbols specifically -- matching every other
     * library in this function rather than assuming dlsym(webkit_handle, ...)
     * would also find a transitively-loaded dependency's symbols. */
    if (!(soup_handle = load_one(dir, "lib/x86_64-linux-gnu/libsoup-3.0.so.0", &lmid))) return FALSE;
    /* Task 11: see JAVASCRIPTCORE_FUNCS's own comment above -- already
     * resident in the shared namespace as libwebkitgtk-6.0.so.4's own
     * dependency, same situation as soup_handle just above. */
    if (!(javascriptcore_handle = load_one(dir, "lib/libjavascriptcoregtk-6.0.so.1", &lmid))) return FALSE;

#define DO_FUNC(f) if (!(p_##f = dlsym(glib_handle, #f))) \
    { WARN("failed to load symbol %s\n", #f); return FALSE; }
    GLIB_FUNCS;
#undef DO_FUNC
#define DO_FUNC(f) if (!(p_##f = dlsym(gobject_handle, #f))) \
    { WARN("failed to load symbol %s\n", #f); return FALSE; }
    GOBJECT_FUNCS;
#undef DO_FUNC
#define DO_FUNC(f) if (!(p_##f = dlsym(gtk_handle, #f))) \
    { WARN("failed to load symbol %s\n", #f); return FALSE; }
    GTK_FUNCS;
#undef DO_FUNC
#define DO_FUNC(f) if (!(p_##f = dlsym(webkit_handle, #f))) \
    { WARN("failed to load symbol %s\n", #f); return FALSE; }
    WEBKIT_FUNCS;
#undef DO_FUNC
#define DO_FUNC(f) if (!(p_##f = dlsym(soup_handle, #f))) \
    { WARN("failed to load symbol %s\n", #f); return FALSE; }
    SOUP_FUNCS;
#undef DO_FUNC
#define DO_FUNC(f) if (!(p_##f = dlsym(javascriptcore_handle, #f))) \
    { WARN("failed to load symbol %s\n", #f); return FALSE; }
    JAVASCRIPTCORE_FUNCS;
#undef DO_FUNC
#define DO_FUNC(f) if (!(p_##f = dlsym(x11_handle, #f))) \
    { WARN("failed to load symbol %s\n", #f); return FALSE; }
    X11_FUNCS;
#undef DO_FUNC

    /* Task 7 crash fix, round 12: symbol-resolution sanity check (repo
     * owner wants real docking fixed, not just guarded -- round 11's own
     * live realized=1/mapped=1 data ruled out the "called before realize"
     * theory for real, pointing back at whether p_gdk_x11_surface_get_xid
     * is actually the real GTK4 symbol at all, per the round-9 subagent
     * investigation's own suggested next step). No live debugger is
     * available in this environment (ptrace_scope=1, no passwordless
     * sudo -- see this file's own git log), so this does the equivalent
     * self-inspection from inside the process instead: read this
     * process's own /proc/self/maps (no ptrace needed -- a process can
     * always read its own maps) to find the bundled libgtk-4.so.1's real
     * load base, and log it alongside the actual resolved function
     * pointer. Comparing the two (offset = pointer - base) against the
     * real bundled .so file's own symbol table (`nm -D`/`readelf`, done
     * separately, outside this process) tells whether dlsym resolved the
     * genuine exported symbol or something else. Logged once, at load
     * time, not per-call. */
    {
        FILE *maps = fopen("/proc/self/maps", "r");
        char line[512];
        unsigned long gtk_base = 0;

        if (maps)
        {
            while (fgets(line, sizeof(line), maps))
            {
                if (strstr(line, "libgtk-4.so.1") && !gtk_base)
                {
                    gtk_base = strtoul(line, NULL, 16);
                    break;
                }
            }
            fclose(maps);
        }
        GTK_THREAD_LOG("symbol-resolution check: libgtk-4.so.1 load base=0x%lx "
                        "p_gdk_x11_surface_get_xid=%p (offset=0x%lx) "
                        "p_gtk_native_get_surface=%p p_gdk_surface_get_display=%p\n",
                        gtk_base, (void *)p_gdk_x11_surface_get_xid,
                        gtk_base ? (unsigned long)p_gdk_x11_surface_get_xid - gtk_base : 0,
                        (void *)p_gtk_native_get_surface, (void *)p_gdk_surface_get_display);
    }

    /* Task 11 real-bug fix -- must run here, before this function returns
     * and unix_init_impl goes on to pthread_create() the GTK thread that
     * will eventually call webkit_web_view_new() (create_webview_on_gtk_thread
     * below): JSC's Options/signal-handler setup happens lazily, once, via
     * pthread_once on the FIRST WebKitWebView construction, so this only
     * needs to run once, before that first call, not once per webview. See
     * JSConfigureSignalForGC's own extern declaration comment above for the
     * full evidence trail (why the exported API was chosen over the also-
     * working-but-noisier JSC_SIGNAL_FOR_GC env var, and how this call was
     * verified end-to-end with a standalone dlopen probe outside Wine).
     * SIGPWR (30): confirmed via grep that dlls/ntdll/unix/signal_x86_64.c
     * never installs a handler for it (only SIGINT/FPE/ABRT/QUIT/USR1/
     * TRAP/SEGV/ILL/BUS/SYS), same choice Android's ART runtime made for
     * its own GC-suspend signal for exactly this kind of collision-avoidance
     * reason.
     *
     * The real signature returns bool ("fails" if JSC already started
     * initializing on some other path before this runs) -- MUST be checked,
     * not just called for effect: a silent no-op here would mean the
     * original crash is still fully possible with zero indication anything
     * went wrong, defeating the entire point of this fix. */
    if (!p_JSConfigureSignalForGC(30))
    {
        ERR("JSConfigureSignalForGC(SIGPWR) failed -- JSC already initialized; "
            "SuspendThread/GetThreadContext will be unreliable\n");
        return FALSE;
    }

    return TRUE;
}

/* WEBKIT_EXEC_PATH and friends -- the full env-var relocation contract
 * verified end-to-end in webkitgtk-bundle/README.md, derived here from the
 * single TUXBLOX_WEBVIEW_DIR value rather than being set piecemeal by
 * proton (see Task 2's rationale comment). setenv(), not a one-time cache:
 * safe to call before any dlopen of the bundle's own libraries, since
 * unlike LD_LIBRARY_PATH these are read by WebKitGTK itself via getenv()
 * at actual use time, not cached by ld.so at process start. */
/* Final-review fix (Important 2): prepends `value` onto whatever is already
 * in env var `name` (":"-joined, PATH-style), rather than overwriting it
 * outright via a plain setenv(..., 1). Used specifically for the two vars
 * below that collide with something else already relying on them in this
 * SAME process (Proton's own bundled GStreamer's GST_PLUGIN_SYSTEM_PATH_1_0,
 * and any other in-process consumer of XDG_DATA_DIRS) -- see each call
 * site's own comment. If `name` isn't set yet, this is equivalent to a
 * plain setenv(name, value, 1) (no leading ":" is emitted). */
static void prepend_env(const char *name, const char *value)
{
    const char *existing = getenv(name);
    char *joined;
    size_t len;

    if (!existing || !existing[0])
    {
        setenv(name, value, 1);
        return;
    }

    len = strlen(value) + 1 + strlen(existing) + 1;
    if (!(joined = malloc(len)))
    {
        WARN("out of memory prepending %s to %s -- leaving existing value in place\n", value, name);
        return;
    }
    snprintf(joined, len, "%s:%s", value, existing);
    setenv(name, joined, 1);
    free(joined);
}

static void set_webkit_relocation_env(const char *dir)
{
    char path[PATH_MAX];

    snprintf(path, sizeof(path), "%s/libexec/webkitgtk-6.0", dir);
    setenv("WEBKIT_EXEC_PATH", path, 1);

    snprintf(path, sizeof(path), "%s/lib/webkitgtk-6.0/injected-bundle", dir);
    setenv("WEBKIT_INJECTED_BUNDLE_PATH", path, 1);

    snprintf(path, sizeof(path), "%s/lib/x86_64-linux-gnu/gio/modules", dir);
    setenv("GIO_EXTRA_MODULES", path, 1);

    snprintf(path, sizeof(path), "%s/lib/x86_64-linux-gnu/gbm", dir);
    setenv("GBM_BACKENDS_PATH", path, 1);

    snprintf(path, sizeof(path), "%s/libexec/gstreamer-1.0/gst-plugin-scanner", dir);
    setenv("GST_PLUGIN_SCANNER", path, 1);

    snprintf(path, sizeof(path), "%s/lib/x86_64-linux-gnu/gstreamer-1.0", dir);
    /* GStreamer 1.x reads GST_PLUGIN_SYSTEM_PATH_1_0 first and only falls
     * back to the unsuffixed GST_PLUGIN_SYSTEM_PATH when the _1_0-suffixed
     * one is unset. ProtonSource/proton sets GST_PLUGIN_SYSTEM_PATH_1_0
     * unconditionally for its own bundled GStreamer, so under this repo's
     * Proton the unsuffixed var alone is inert -- the bundle's own
     * gstreamer-1.0 plugins (built specifically for this bundle) would
     * never be found and WebKit's media pipeline would silently fall back
     * to Proton's differently-versioned GStreamer plugins instead. Set
     * both: the suffixed one to actually take effect here, the unsuffixed
     * one kept for any other GStreamer-based consumer that only checks the
     * unsuffixed name.
     *
     * Final-review fix (Important 2): this DLL runs inside the Roblox
     * process, not a child -- Proton's own launcher script already set
     * GST_PLUGIN_SYSTEM_PATH_1_0 for its OWN bundled GStreamer before this
     * ever runs. A plain setenv(..., 1) here overwrote that outright,
     * meaning Wine's own winegstreamer.so (unrelated to WebKit, but living
     * in the same process/environment) would get pointed at this bundle's
     * different-version GStreamer plugins instead of Proton's own.
     * Prepending instead of replacing keeps this bundle's plugins found
     * first (same effective behavior as before for WebKit) while leaving
     * Proton's own path reachable afterward for winegstreamer.so or any
     * other in-process GStreamer consumer. GST_PLUGIN_SYSTEM_PATH
     * (unsuffixed) has no such known collision -- left as a plain setenv,
     * out of scope for this fix. */
    setenv("GST_PLUGIN_SYSTEM_PATH", path, 1);
    prepend_env("GST_PLUGIN_SYSTEM_PATH_1_0", path);

    snprintf(path, sizeof(path), "%s/share", dir);
    /* Final-review fix (Important 2): same reasoning as
     * GST_PLUGIN_SYSTEM_PATH_1_0 above -- a plain setenv(..., 1) replaced
     * XDG_DATA_DIRS outright, dropping whatever the host/Proton already
     * had there for any other in-process consumer of XDG data dirs.
     * Prepending keeps this bundle's share/ directory found first (same
     * effective behavior as before) without discarding the rest. */
    prepend_env("XDG_DATA_DIRS", path);

    snprintf(path, sizeof(path), "%s/share/glib-2.0/schemas", dir);
    setenv("GSETTINGS_SCHEMA_DIR", path, 1);
}

static pthread_t gtk_thread;
static GMainLoop *gtk_main_loop;
static pthread_mutex_t gtk_thread_ready_lock = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t gtk_thread_ready_cond = PTHREAD_COND_INITIALIZER;
static BOOL gtk_thread_ready;
/* Set (under gtk_thread_ready_lock, before gtk_thread_ready is signalled) to
 * whether the GTK thread actually finished initializing successfully --
 * distinct from gtk_thread_ready, which only means "the thread reached the
 * ready-signal point", success or not. unix_init_impl must not report
 * STATUS_SUCCESS unless this is TRUE too. */
static BOOL gtk_thread_init_ok;

/* Task 7 crash fix, round 17 -- see the XSetErrorHandler extern's own
 * comment above (near XDefaultRootWindow) for the full BadWindow crash
 * this addresses. Xlib's documented contract for a custom error handler:
 * it runs synchronously on whichever thread issued the offending
 * request (always gtk_thread here -- every raw Xlib call in this file is
 * made from that single serialized thread), must not call back into
 * Xlib itself, and its return value is ignored by Xlib (only NOT calling
 * exit()/abort() matters). GTK_THREAD_LOG rather than WARN/ERR for the
 * same TEB-safety reason documented on that macro itself. Deliberately
 * generic ("an X11 request failed") rather than a NULL-checked display
 * name / request-code table lookup: the goal here is solely "don't let
 * one bad window ID committed by external, not-fully-trustworthy state
 * (see parent_xid's own comment) take down the whole process," not a
 * full diagnostic decoder -- request_code/error_code are still logged
 * raw for anyone reading the log to cross-reference against
 * /usr/include/X11/Xproto.h by hand if needed. */
/* Task 7 crash fix, round 18 -- see x11_error_handler's own comment just
 * below for why this exists. */
static BOOL x11_error_seen_during_call;

static int x11_error_handler(Display *display, XErrorEvent *event)
{
    GTK_THREAD_LOG("Xlib reported a non-fatal X11 protocol error (request_code=%u "
                    "minor_code=%u error_code=%u serial=%lu) -- ignoring instead of "
                    "letting Xlib's own default handler abort the process\n",
                    event ? (unsigned)event->request_code : 0,
                    event ? (unsigned)event->minor_code : 0,
                    event ? (unsigned)event->error_code : 0,
                    event ? event->serial : 0UL);
    /* Task 7 crash fix, round 18: the repo owner's own reviewer correctly
     * flagged this handler as the prime suspect for turning a real,
     * silent XReparentWindow failure into an invisible no-op -- exactly
     * the tradeoff a non-fatal error handler makes. Setting this flag
     * (checked immediately after XReparentWindow + XSync, see that call
     * site's own comment) makes that tradeoff observable again instead of
     * genuinely invisible: gtk_thread is single-threaded by this whole
     * file's own established invariant (every raw Xlib call happens on
     * this one serialized thread), so a plain static flag needs no
     * locking, same as xmove_call_count just below. */
    x11_error_seen_during_call = TRUE;
    return 0;
}

static void *gtk_thread_proc(void *arg)
{
    BOOL ok;

    /* Task 7 UAF/Xlib-locking fix round 3 -- see XInitThreads' own extern
     * comment above for the full crash evidence this addresses. Must be
     * the first Xlib call made anywhere in the process (Xlib's own
     * documented requirement); gtk_init_check() below is what actually
     * opens the X11 display connection this DLL later issues raw
     * XMoveResizeWindow calls against, so this has to run strictly before
     * it. GDK likely also calls this internally as part of opening its own
     * X11 backend -- calling it again there is a documented no-op/safe, so
     * there's no harm doing it twice; what matters is that it happens
     * before ANY Xlib activity, which this guarantees regardless of GDK's
     * own internal ordering.
     *
     * Code review fix: the real return value (a real Xlib Status, non-zero
     * on success) was previously discarded. If this ever legitimately
     * fails (rare, but real -- e.g. a libX11 build without thread support),
     * Xlib's own documented behavior is for XLockDisplay/XUnlockDisplay to
     * silently become no-ops, which would quietly reopen the exact TOCTOU
     * race rounds 3-6 spent most of their effort closing, with nothing
     * anywhere indicating the locking had become inert. Loud diagnostic
     * (GTK_THREAD_LOG -- see its own comment for why not WARN/ERR here)
     * rather than a hard init failure: this thread can still do useful
     * work (visibility/size sync, cookies, navigation) even if X11 position
     * sync specifically degrades. */
    if (!p_XInitThreads())
        GTK_THREAD_LOG("XInitThreads() failed -- XLockDisplay/XUnlockDisplay will silently "
                        "no-op per Xlib's own documented behavior, reopening the exact TOCTOU "
                        "race this file's own git log documents fixing\n");

    /* Task 7 crash fix, round 12: force GTK4's own backend selection to
     * X11, not Wayland. Round 11's live diagnostic data (realized=1,
     * mapped=1, every single call) conclusively ruled out any lifecycle/
     * ordering theory for the deterministic garbage xid rounds 9-11 kept
     * observing. This environment is a genuine Wayland compositor session
     * (WAYLAND_DISPLAY set, confirmed via the repo's own env at real
     * launch time) and GDK_BACKEND is never set anywhere in this
     * codebase -- GTK4's own documented behavior is to auto-detect and
     * PREFER native Wayland over X11 whenever both are available, unless
     * an app explicitly forces otherwise. If the bundled GTK4 stack this
     * DLL loads actually initialized under the native Wayland backend,
     * every gdk_x11_surface_get_xid(surface) call this file makes is a
     * real type mismatch: `surface` would be a GdkWaylandSurface*, not a
     * GdkX11Surface*, and if this GDK4 build's internal type-check for
     * that cast doesn't cleanly reject it (returning the documented safe
     * 0 the `!xid` guard above already handles), the function instead
     * reads whatever real pointer-sized field a Wayland surface happens
     * to store at the offset an X11 surface would keep its XID at --
     * exactly matching every round-9-through-11 observation: always
     * non-zero (never a clean 0 -- consistent with an unchecked/
     * mis-validated cast, not a failed one), always pointer-shaped (a
     * real, valid Wayland-surface-internal pointer, not corrupted/freed
     * memory -- consistent with nothing rounds 1-6's UAF/locking/
     * ref-counting fixes ever finding a genuinely invalid object), and
     * always the exact same value per surface (a fixed struct offset read
     * deterministically, not a race). Must be set via setenv() here,
     * before gtk_init_check() below -- that's the one call that actually
     * opens GDK's display connection and locks in its backend choice; GDK
     * reads GDK_BACKEND during that same call, so this has to land before
     * it, same ordering requirement XInitThreads() above already has for
     * a different reason. */
    setenv("GDK_BACKEND", "x11", 1);

    ok = p_gtk_init_check();

    /* Task 7 crash fix, round 17: install AFTER gtk_init_check() (not
     * before), specifically so this overrides whatever error handler
     * GDK's own X11 backend init installed for itself while opening the
     * display connection above, rather than being silently clobbered BY
     * it -- same "must come after the call that locks in GDK's own X11
     * setup" ordering already established for GDK_BACKEND above, just on
     * the other side of that call instead of before it. Installed
     * unconditionally (not gated on `ok`): even a failed gtk_init_check()
     * may have partially opened an X11 display connection this thread
     * could still make raw Xlib calls against before exiting, and a
     * missing handler here is exactly the fatal-abort gap this round
     * fixes, so there's no safe case to skip it in. */
    p_XSetErrorHandler(x11_error_handler);

    /* Only build the main loop if init actually succeeded -- calling
     * g_main_loop_new()/g_main_loop_run() after a failed gtk_init_check()
     * would run an event loop nothing can ever usefully post GTK/WebKit
     * work through. */
    if (ok)
    {
        gtk_main_loop = p_g_main_loop_new(NULL, 0);
        if (!gtk_main_loop) ok = FALSE;
    }

    pthread_mutex_lock(&gtk_thread_ready_lock);
    gtk_thread_init_ok = ok;
    gtk_thread_ready = TRUE;
    pthread_cond_signal(&gtk_thread_ready_cond);
    pthread_mutex_unlock(&gtk_thread_ready_lock);

    if (ok) p_g_main_loop_run(gtk_main_loop);
    return NULL;
}

/* Runs fn(data) on the GTK thread's main context and blocks the CALLING
 * thread (a PE-side worker thread, blocked here inside its WINE_UNIX_CALL)
 * until fn has actually run. Every later unix call that touches GTK/WebKit
 * objects (Tasks 5-8) goes through this -- GTK/WebKit objects may only be
 * touched from the thread that owns their GMainContext. */
struct sync_invoke_ctx
{
    void (*fn)(void *data);
    void *data;
    pthread_mutex_t lock;
    pthread_cond_t cond;
    BOOL done;
};

static gboolean sync_invoke_trampoline(void *data)
{
    struct sync_invoke_ctx *ctx = data;

    ctx->fn(ctx->data);

    pthread_mutex_lock(&ctx->lock);
    ctx->done = TRUE;
    pthread_cond_signal(&ctx->cond);
    pthread_mutex_unlock(&ctx->lock);
    return 0; /* G_SOURCE_REMOVE */
}

/* Returns TRUE if fn was actually invoked (on the GTK thread) and this
 * function waited for it to finish, FALSE if it bailed out early without
 * running fn at all (init never succeeded) -- callers whose fn fills an
 * out-param need this to distinguish "ran, filled it" from "didn't run,
 * output untouched" rather than silently getting stale/uninitialized
 * output on the FALSE path. */
BOOL gtk_thread_invoke_sync(void (*fn)(void *data), void *data)
{
    /* struct sync_invoke_ctx is stack-allocated (automatic storage), not
     * static -- PTHREAD_MUTEX_INITIALIZER/PTHREAD_COND_INITIALIZER are only
     * POSIX-sanctioned for statically-allocated objects (works in practice
     * on glibc, but pthread_*_init()/pthread_*_destroy() is the portable,
     * correct way to initialize a lock/condvar living on the stack). The
     * actual synchronization behaviour below (invoke on the GTK thread,
     * block the caller on ctx.done) is unchanged. */
    struct sync_invoke_ctx ctx;

    ctx.fn = fn;
    ctx.data = data;
    ctx.done = FALSE;
    pthread_mutex_init(&ctx.lock, NULL);
    pthread_cond_init(&ctx.cond, NULL);

    /* No later task calls this yet (Task 5 is the first caller), but guard
     * against a null-deref if it's ever reached before a successful
     * unix_init (e.g. on i386, where init always returns
     * STATUS_NOT_SUPPORTED since the bundle is x86_64-only). */
    if (!p_g_main_context_invoke_full || !p_g_main_context_default)
    {
        WARN("gtk_thread_invoke_sync called without a successful unix_init\n");
        pthread_mutex_destroy(&ctx.lock);
        pthread_cond_destroy(&ctx.cond);
        return FALSE;
    }

    p_g_main_context_invoke_full(p_g_main_context_default(), 0, sync_invoke_trampoline, &ctx, NULL);

    pthread_mutex_lock(&ctx.lock);
    while (!ctx.done) pthread_cond_wait(&ctx.cond, &ctx.lock);
    pthread_mutex_unlock(&ctx.lock);

    pthread_mutex_destroy(&ctx.lock);
    pthread_cond_destroy(&ctx.cond);
    return TRUE;
}

/* unix_init_impl must run its actual init work exactly once, even when
 * called concurrently from multiple PE threads (Task 5 calls
 * webview2loader_unix_init() on every environment creation). A plain
 * "static BOOL initialized, checked-and-set under gtk_thread_ready_lock"
 * is NOT enough by itself: the init body's own
 * `pthread_cond_wait(&gtk_thread_ready_cond, &gtk_thread_ready_lock)`
 * (waiting for the GTK thread to finish starting up) necessarily RELEASES
 * gtk_thread_ready_lock for the duration of the wait -- pthread_cond_wait's
 * whole contract is "atomically unlock and block, then relock before
 * returning". A second thread arriving during that window would see
 * `initialized` still FALSE and race the first thread through
 * set_webkit_relocation_env()/load_bundle_functions()/pthread_create() a
 * second time -- the exact setenv-vs-getenv race, doubled dlopen refcount,
 * and p_*-function-pointer data race the idempotency fix was meant to
 * prevent in the first place, just deferred to a narrower window instead
 * of eliminated.
 *
 * Fixed with a three-state guard instead of a plain boolean:
 *   INIT_IDLE    -- nobody has started init yet; the calling thread does it.
 *   INIT_RUNNING -- another thread is already running the init body
 *                   (including its own internal wait for the GTK thread);
 *                   later arrivals wait on init_state_cond instead of
 *                   redoing any of the work.
 *   INIT_DONE    -- init_ok holds the final, cached result.
 * `init_state` is set to INIT_RUNNING BEFORE the running thread does
 * anything that could release gtk_thread_ready_lock, so any thread that
 * acquires the lock while init is in flight -- including exactly the
 * pthread_cond_wait release window described above -- always observes
 * INIT_RUNNING, never a stale INIT_IDLE. */
enum { INIT_IDLE, INIT_RUNNING, INIT_DONE };
static int init_state = INIT_IDLE;
static BOOL init_ok;
static pthread_cond_t init_state_cond = PTHREAD_COND_INITIALIZER;

static NTSTATUS unix_init_impl(void *args)
{
    struct init_params *params = args;
    const char *dir;

    pthread_mutex_lock(&gtk_thread_ready_lock);

    if (init_state == INIT_DONE)
    {
        params->success = init_ok;
        pthread_mutex_unlock(&gtk_thread_ready_lock);
        return init_ok ? STATUS_SUCCESS : STATUS_NOT_SUPPORTED;
    }

    if (init_state == INIT_RUNNING)
    {
        /* Another thread got here first and is running (or about to run)
         * the one-time init body below, including its own wait for the GTK
         * thread to start up. Wait for it to publish INIT_DONE instead of
         * also running set_webkit_relocation_env()/load_bundle_functions()/
         * pthread_create() ourselves. */
        while (init_state == INIT_RUNNING)
            pthread_cond_wait(&init_state_cond, &gtk_thread_ready_lock);
        params->success = init_ok;
        pthread_mutex_unlock(&gtk_thread_ready_lock);
        return init_ok ? STATUS_SUCCESS : STATUS_NOT_SUPPORTED;
    }

    /* init_state == INIT_IDLE: we're the thread that does the real work.
     * Claim it immediately, while still holding the lock, before anything
     * below can release it. */
    init_state = INIT_RUNNING;

    params->success = FALSE;

    if (!(dir = getenv("TUXBLOX_WEBVIEW_DIR")) || !dir[0])
    {
        WARN("TUXBLOX_WEBVIEW_DIR not set -- not running under this repo's proton\n");
        goto done;
    }

    set_webkit_relocation_env(dir);
    if (!load_bundle_functions())
        goto done;

    /* Check pthread_create()'s return value -- if thread creation fails,
     * nothing will ever set gtk_thread_ready, so waiting on the condvar
     * below would block forever on a PE thread parked inside a
     * WINE_UNIX_CALL (not cancellable), hanging the whole app. */
    if (pthread_create(&gtk_thread, NULL, gtk_thread_proc, NULL) != 0)
    {
        WARN("pthread_create failed for the GTK thread\n");
        goto done;
    }
    /* Nothing ever joins the GTK thread (it either runs its main loop
     * forever on success, or returns immediately on failure -- see
     * gtk_thread_proc). Detach it so a failed init doesn't leak a
     * thread descriptor; harmless on the success path since a running,
     * detached thread is exactly as alive as a running, joinable one. */
    pthread_detach(gtk_thread);

    while (!gtk_thread_ready) pthread_cond_wait(&gtk_thread_ready_cond, &gtk_thread_ready_lock);

    /* gtk_thread_ready being set only means the thread reached the
     * ready-signal point, not that gtk_init_check()/g_main_loop_new()
     * actually succeeded (e.g. no usable DISPLAY/WAYLAND_DISPLAY). Treat
     * that the same as any other init failure rather than reporting
     * STATUS_SUCCESS for a main loop that either doesn't exist or was
     * never started, which would hang every later gtk_thread_invoke_sync()
     * call waiting on a loop that isn't running. */
    if (!gtk_thread_init_ok)
    {
        WARN("GTK init failed on the GTK thread (no usable display?)\n");
        goto done;
    }

    params->success = TRUE;

done:
    init_ok = params->success;
    init_state = INIT_DONE;
    pthread_cond_broadcast(&init_state_cond);
    pthread_mutex_unlock(&gtk_thread_ready_lock);
    return init_ok ? STATUS_SUCCESS : STATUS_NOT_SUPPORTED;
}

struct native_webview
{
    GtkWidget *window;
    WebKitWebView *view;
    /* Task 7 crash fix, round 15: which real X11 parent window (if any)
     * nv->window has already been XReparentWindow'd into -- see
     * sync_window_geometry_on_gtk_thread's own comment. 0 means "not
     * reparented yet" (still an independent top-level, or no valid
     * parent_xid has ever been supplied). Re-checked against the current
     * params->parent_xid on every call so a change (including recovering
     * from Wine recreating the parent's whole_window, a real, documented
     * risk noted in that same comment) re-reparents on the next call
     * rather than silently going stale. */
    unsigned long reparented_into;
};

/* Task 7 real-launch crash fix, round 2: a defensive NULL-Display* check
 * in sync_window_geometry_on_gtk_thread (see that function's own comment)
 * stopped one crash but a second, real relaunch still segfaulted at the
 * exact same call site, this time with a *non-NULL* but garbage `xid`
 * argument reaching XMoveResizeWindow (confirmed via coredump: the value
 * looked like a stale heap pointer, not a plausible X11 resource id --
 * the textbook fingerprint of reading through a freed allocation).
 *
 * Root cause: controller.c's controller_push_geometry_to_native (the
 * put_Bounds/put_IsVisible/WH_CALLWNDPROC-hook entry point for this unix
 * call) checks `ctrl->native_handle` for non-zero, then separately reads
 * it again a few lines later to fill params.handle -- two unlocked reads,
 * not one atomic snapshot. controller_destroy_native (Close()) frees this
 * struct via the destroy_webview unix call *before* it zeroes its own
 * `ctrl->native_handle` copy. That leaves a real, narrow window where a
 * concurrent put_Bounds/put_IsVisible call (or Task 5's window-move hook,
 * which can run on a different thread than whatever releases the last
 * COM ref and triggers Close()) can pass the first check, then read the
 * not-yet-zeroed handle *after* the unix-side object it points to has
 * already been freed, and hand that dangling pointer to this DLL's own
 * WEBVIEW2LOADER_UNIX_CALL(sync_window_geometry, ...) -- a genuine
 * use-after-free, not a bug in the geometry math itself.
 *
 * Fixing that race properly belongs in controller.c (this task's
 * confirmed real, concrete bug is scoped to this file per Task 7's own
 * brief); this is a defense-in-depth guard entirely on the unix side:
 * a small live-handle registry, touched only from the functions below
 * that already run exclusively on the single dedicated GTK thread via
 * gtk_thread_invoke_sync (create_webview_on_gtk_thread,
 * destroy_webview_on_gtk_thread, and the *_on_gtk_thread handlers that
 * dereference a caller-supplied handle). Because gtk_thread_invoke_sync
 * serializes every one of these onto that one thread -- never two of
 * them running concurrently -- registering/unregistering/checking this
 * array needs no lock of its own: destroy unregisters (and only then
 * frees) strictly before any later-queued handler's check can run, so a
 * stale handle is reliably caught as "not live" instead of dereferenced,
 * turning what would otherwise be a crash into the same graceful
 * "nothing to do" outcome this file already uses throughout for
 * degraded/unavailable state. */
#define MAX_LIVE_WEBVIEWS 64
static struct native_webview *live_webviews[MAX_LIVE_WEBVIEWS];

static void live_webview_register(struct native_webview *nv)
{
    int i;
    for (i = 0; i < MAX_LIVE_WEBVIEWS; i++)
        if (!live_webviews[i]) { live_webviews[i] = nv; return; }
    GTK_THREAD_LOG("live_webviews registry full (%d entries) -- UAF guard won't cover handle %p\n",
        MAX_LIVE_WEBVIEWS, nv);
}

static void live_webview_unregister(struct native_webview *nv)
{
    int i;
    for (i = 0; i < MAX_LIVE_WEBVIEWS; i++)
        if (live_webviews[i] == nv) { live_webviews[i] = NULL; return; }
}

static BOOL live_webview_is_valid(struct native_webview *nv)
{
    int i;
    if (!nv) return FALSE;
    for (i = 0; i < MAX_LIVE_WEBVIEWS; i++)
        if (live_webviews[i] == nv) return TRUE;
    return FALSE;
}

/* Handles are just the native_webview*'s address, truncated to fit a
 * UINT64 -- unix-side memory, never dereferenced on the PE side, matching
 * the "opaque handle" shape other unixlib bridges in this tree use for
 * unix-owned objects PE code only ever passes back by value. */
struct create_webview_ctx
{
    BOOL is_message_only;

    /* out */
    struct native_webview *nv;
};

/* Task 7 crash fix, round 14: a real, reproducible double-destroy crash
 * (coredump, RAX == 0xaaaaaaaaaaaaaaaa -- GLib's own "gc-friendly" freed-
 * memory poison fill -- reading through nv->window inside GTK4's own
 * gtk_window_destroy(), called from this file's own destroy_webview_
 * on_gtk_thread) surfaced for the first time only after round 12's
 * GDK_BACKEND=x11 fix made these windows genuinely X11/WM-backed for the
 * first time this whole session (rounds 1-11 were silently Wayland-
 * backed the entire time, per round 12's own root-cause finding -- a
 * window that's not really window-manager-managed can't receive a real
 * close request from one either, which is almost certainly why this
 * exact crash class never surfaced before). GTK4's own documented
 * default behavior for GtkWindow::close-request (docs.gtk.org/gtk4/
 * signal.Window.close-request.html) is to destroy the window itself
 * unless a connected handler returns TRUE to stop that. This file never
 * connected one, so any window-manager-initiated close (the WM's own
 * close button, Alt+F4, etc. -- all real possibilities once a window is
 * genuinely WM-managed) could trigger GTK4 to self-destroy nv->window
 * out from under this file's own bookkeeping; a later, entirely normal
 * Close()/Release() on the same real WebView2 controller then calls this
 * file's own destroy_webview_on_gtk_thread, which tries to destroy the
 * same (already GTK-internally-destroyed) widget a second time -- a
 * genuine double-destroy, exactly matching the observed poisoned-read
 * crash. Real WebView2 controllers are never independently closable by
 * the user or window manager at all (only via the real API's own
 * Close()), so unconditionally stopping this signal is the correct
 * semantic fix, not just a crash workaround -- matches round 13's
 * decoration fix in spirit (this window was never supposed to be a
 * normal, independently-manageable top-level in the first place). */
static gboolean on_close_request(GtkWindow *window, void *user_data)
{
    return TRUE; /* GDK_EVENT_STOP -- stop GTK4's own default handler,
                  * which would otherwise destroy the window itself. */
}

/* Login-redirect fix: hands a matched custom-scheme URI off to the OS via
 * xdg-open, exactly like a real browser/WebView2 does by default for an
 * external URI scheme it doesn't itself recognize -- letting this
 * codebase's own already-installed xdg-mime registrations
 * (install-handler.sh / launcher/src/desktop_integration.cpp --
 * x-scheme-handler/roblox-studio-auth, -studio, -player) complete the
 * hand-off to a fresh RobloxStudioBeta.exe, exactly reproducing the
 * OS-level custom-URI-protocol activation real, unmodified Roblox Studio
 * relies on (confirmed via a real Vinegar session log -- see this task's
 * own brief). fork()+execvp() with an explicit argv array -- never
 * system()/a shell string -- so the URI's own &/=/? characters survive
 * completely intact rather than being shell-word-split/glob-expanded.
 *
 * Uses this codebase's own established double-fork pattern (see
 * dlls/ntdll/unix/process.c's __wine_unix_spawnvp, "in child"/
 * "in grandchild" comments): the outer child forks a grandchild that
 * execvp()s xdg-open and immediately _exit()s itself, so the grandchild
 * (the real, possibly long-lived xdg-open process) gets reparented to
 * init/systemd rather than staying a child of this Wine process -- avoids
 * needing a persistent SIGCHLD handler or an indefinite wait to prevent a
 * zombie across what could be many logins over a long Studio session. The
 * outer child's own exit is waited on synchronously below, but since it
 * exits immediately after its own inner fork() call, this is a bounded,
 * near-instant wait, not a real block on xdg-open itself finishing.
 *
 * GTK_THREAD_LOG only -- this runs on gtk_thread (called directly from
 * on_decide_policy, a WebKit signal callback), which has no valid Wine TEB
 * -- see that macro's own comment for why WARN/TRACE/ERR are a real crash
 * hazard here. */
static void xdg_open_handoff(const char *uri)
{
    pid_t pid = fork();

    if (pid == 0)
    {
        /* in child */
        pid_t pid2 = fork();
        if (pid2 == 0)
        {
            /* in grandchild -- becomes the real, long-lived xdg-open
             * process once reparented away from this Wine process. */
            char *argv[] = { (char *)"xdg-open", (char *)uri, NULL };
            execvp("xdg-open", argv);
            _exit(127); /* only reached if execvp() itself failed */
        }
        _exit(0); /* child's only job was forking the grandchild -- exits
                   * immediately regardless of whether that inner fork
                   * itself succeeded, so the parent's waitpid below never
                   * blocks on xdg-open's own real runtime. */
    }
    if (pid > 0)
    {
        int status;
        if (waitpid(pid, &status, 0) < 0)
            GTK_THREAD_LOG("waitpid on xdg-open handoff child failed for %s: %s\n", uri, strerror(errno));
    }
    else
        GTK_THREAD_LOG("fork() failed for xdg-open handoff of %s: %s\n", uri, strerror(errno));
}

/* Deliberately narrow, prefix-only allowlist -- NOT requiring a "://"
 * authority form. The real, observed Roblox OAuth redirect is single-slash
 * (roblox-studio-auth:/?code=...&state=..., confirmed via two independent
 * real captures -- see this task's own brief); a strict "://" match would
 * silently miss the one URI this fix exists to handle. Matches exactly
 * this codebase's own existing xdg-mime scheme registrations, nothing
 * broader. */
static const char *const known_roblox_schemes[] = {
    "roblox-studio-auth:", "roblox-studio:", "roblox-player:",
};

/* WebKitWebView::decide-policy -- connected once per webview at creation
 * time (create_webview_on_gtk_thread below), not scoped to a single
 * Navigate() call the way "load-changed" is. This has to be persistent
 * for the whole webview's lifetime: the roblox-studio-auth: redirect this
 * exists to handle is entirely browser-internal (WebKit's own OAuth flow
 * navigating there after the user submits credentials on the real Roblox
 * login page), never itself the direct target of a PE-side Navigate()
 * call, so a connection scoped to one Navigate() would never even still
 * be alive when the redirect actually fires.
 *
 * Only WEBKIT_POLICY_DECISION_TYPE_NAVIGATION_ACTION is inspected, and
 * only the three known Roblox custom schemes within it are intercepted --
 * matches the brief's own scoped recommendation and this file's own
 * round-16 cautionary history (see progress.md: an earlier, reverted
 * attempt denylisted too broadly and broke WebKit's own internal
 * about:blank init). Every other decision type (NEW_WINDOW_ACTION,
 * RESPONSE) and every non-matching scheme under NAVIGATION_ACTION falls
 * through to returning FALSE -- WebKit's own documented default of
 * proceeding with webkit_policy_decision_use(), i.e. completely
 * unmodified behavior for real http/https page loads and everything else
 * WebKit needs to load normally. This is a deliberately narrow allowlist
 * of exactly the schemes this codebase itself registers via xdg-mime, not
 * a broad denylist of "unknown" schemes -- narrower than real WebView2's
 * own default (which hands off ANY scheme it doesn't itself recognize),
 * but sufficient to fix this specific bug without repeating round 16's
 * over-broad-filtering mistake; nothing else in this codebase currently
 * needs a broader net, and a broader net is strictly easier to widen later
 * than to debug after the fact. */
static gboolean on_decide_policy(WebKitWebView *view, WebKitPolicyDecision *decision,
                                  WebKitPolicyDecisionType decision_type, void *user_data)
{
    WebKitNavigationAction *action;
    WebKitURIRequest *request;
    const char *uri;
    size_t i;

    if (decision_type != WEBKIT_POLICY_DECISION_TYPE_NAVIGATION_ACTION)
        return FALSE;

    action = p_webkit_navigation_policy_decision_get_navigation_action((WebKitNavigationPolicyDecision *)decision);
    if (!action) return FALSE;
    request = p_webkit_navigation_action_get_request(action);
    if (!request) return FALSE;
    uri = p_webkit_uri_request_get_uri(request);
    if (!uri) return FALSE;

    for (i = 0; i < ARRAY_SIZE(known_roblox_schemes); i++)
    {
        if (!strncmp(uri, known_roblox_schemes[i], strlen(known_roblox_schemes[i])))
        {
            GTK_THREAD_LOG("intercepting navigation to %s -- ignoring the in-WebKit load (would otherwise "
                            "show WebKit's own generic \"URL can't be shown\" error) and handing off to "
                            "xdg-open, matching real WebView2's own default behavior for an external scheme "
                            "it doesn't itself recognize\n", uri);
            p_webkit_policy_decision_ignore(decision);
            xdg_open_handoff(uri);
            return TRUE; /* GDK_EVENT_STOP -- we handled this decision ourselves */
        }
    }
    return FALSE; /* not one of ours -- let WebKit's own default (use()) handle it */
}

static void create_webview_on_gtk_thread(void *data)
{
    struct create_webview_ctx *ctx = data;
    struct native_webview *nv = calloc(1, sizeof(*nv));

    /* Code review fix: calloc() can fail under real memory pressure --
     * dereferencing NULL two lines below would kill the single dedicated
     * GTK thread (and everything downstream of it: every future
     * gtk_thread_invoke_sync call from any controller). ctx->nv staying
     * NULL is already a real, handled failure path -- unix_create_webview_
     * impl (below) already reports STATUS_NOT_SUPPORTED whenever ctx.nv is
     * NULL, exactly the same as if gtk_thread_invoke_sync itself had
     * failed to run this at all -- so simply not setting it here is
     * sufficient, no new signaling needed. */
    if (!nv)
    {
        GTK_THREAD_LOG("calloc failed for a new native_webview -- out of memory, failing create\n");
        return;
    }

    nv->window = p_gtk_window_new();
    /* Task 7 crash fix, round 13 -- see gtk_window_set_decorated's own
     * extern comment above. Set before the window is ever shown (below)
     * so the window manager never draws chrome on it even momentarily;
     * unconditional (not gated on is_message_only) since a message-only
     * controller's window is never shown at all, so this is a harmless
     * no-op for that case rather than something that needs its own
     * branch. */
    p_gtk_window_set_decorated(nv->window, FALSE);
    /* Task 7 crash fix, round 14 -- see on_close_request's own comment
     * just above for the full crash evidence and reasoning. Connected
     * unconditionally (both is_message_only and real controllers can, in
     * principle, end up window-manager-visible/-addressable), before the
     * window is ever shown, so there's no window in existence yet that
     * could receive a close request before this handler is wired up. No
     * disconnect/cleanup needed -- the connection's lifetime is exactly
     * nv->window's own lifetime, torn down together by
     * destroy_webview_on_gtk_thread's own gtk_window_destroy call. */
    p_g_signal_connect_data(nv->window, "close-request", (GCallback)on_close_request,
                             NULL, NULL, 0);
    nv->view = p_webkit_web_view_new();
    /* Login-redirect fix -- see on_decide_policy's own comment above for
     * why this must be connected here (persistent for the webview's whole
     * lifetime) rather than scoped to a single Navigate() call. Connected
     * unconditionally, same reasoning as the close-request connection just
     * above -- even a message-only (HWND_MESSAGE/CookieManager) controller
     * has a real, live WebKitWebView that could in principle navigate
     * through this same OAuth flow. No disconnect/cleanup needed -- the
     * connection's lifetime is exactly nv->view's own lifetime, torn down
     * together by destroy_webview_on_gtk_thread's own gtk_window_destroy
     * call (which destroys nv->view along with nv->window, still its
     * child at that point). */
    p_g_signal_connect_data(nv->view, "decide-policy", (GCallback)on_decide_policy,
                             NULL, NULL, 0);
    p_gtk_window_set_child(nv->window, nv->view);
    /* Plan 3 Task 2: HWND_MESSAGE-parented controllers (the CookieManager
     * flow) still need a real, live WebKitWebView -- Navigate/GetCookies/
     * DeleteAllCookies all operate on it, already proven end-to-end in
     * Plan 2 -- but must never show a visible top-level window (real
     * WebView2's own HWND_MESSAGE semantics: the browser process runs,
     * there is simply no visible top-level HWND). Previously this call was
     * unconditional, producing the "stray empty top-level window" finding
     * parked at the end of Plan 2's final review. */
    if (!ctx->is_message_only)
        p_gtk_widget_show(nv->window);

    live_webview_register(nv); /* Task 7 UAF guard -- see struct native_webview's own comment above */
    ctx->nv = nv;
}

static NTSTATUS unix_create_webview_impl(void *args)
{
    struct create_webview_params *params = args;
    struct create_webview_ctx ctx = { params->is_message_only, NULL };

    if (!gtk_thread_invoke_sync(create_webview_on_gtk_thread, &ctx))
        WARN("gtk_thread_invoke_sync failed -- GTK thread not running\n");
    params->handle = (UINT64)(ULONG_PTR)ctx.nv;
    return ctx.nv ? STATUS_SUCCESS : STATUS_NOT_SUPPORTED;
}

/* Destroying nv->window tears down nv->view along with it (still its
 * child at this point -- see gtk_window_destroy's declaration comment
 * above), so this is the one native GTK/WebKit call needed per webview;
 * frees the small unix-side bookkeeping struct itself afterward. */
static void destroy_webview_on_gtk_thread(void *data)
{
    struct native_webview *nv = data;

    /* Task 7 UAF guard -- see struct native_webview's own comment above.
     * Unregister strictly before freeing, and before the GTK/WebKit calls
     * below too: every other *_on_gtk_thread handler that checks this
     * registry only ever runs serialized on this same thread, so once this
     * line has executed, any such handler still queued behind this one for
     * the same (now-dying) handle will see it as no-longer-live, however
     * close the PE-side caller's original handle read came to this. */
    live_webview_unregister(nv);
    p_gtk_window_destroy(nv->window);
    free(nv);
}

static NTSTATUS unix_destroy_webview_impl(void *args)
{
    struct destroy_webview_params *params = args;
    struct native_webview *nv = (struct native_webview *)(ULONG_PTR)params->handle;

    if (!nv) return STATUS_SUCCESS;
    /* Final-review fix (Important 3): if this returns FALSE,
     * destroy_webview_on_gtk_thread never ran, so `nv` (and the native
     * GTK window/WebKitWebView it owns) is never freed. There is no
     * callback/ctx to release here -- destroy_webview_on_gtk_thread does
     * its work synchronously inline, not via a later async callback -- so
     * unlike the three refcounted-ctx cookie paths below, nothing can be
     * un-waited-for; this can only be logged so the leak is visible in
     * traces instead of silent. In practice this is unreachable once a
     * webview has actually been created (see unix_create_webview_impl:
     * creating one already required a successful invoke, and this file
     * has no path that makes gtk_thread_invoke_sync start failing again
     * once it has succeeded once), but the check costs nothing and keeps
     * this call site consistent with the other 6. */
    if (!gtk_thread_invoke_sync(destroy_webview_on_gtk_thread, nv))
        WARN("gtk_thread_invoke_sync failed -- GTK thread not running, native webview leaked\n");
    return STATUS_SUCCESS;
}

/* WebKitGTK's own WebKitLoadEvent enum (webkit2/webkit-web-view.h) --
 * stable public ABI, hand-declared here for the same reason the function
 * tables above are (no real header at Wine's build time). */
enum { WEBKIT_LOAD_STARTED, WEBKIT_LOAD_REDIRECTED, WEBKIT_LOAD_COMMITTED, WEBKIT_LOAD_FINISHED };

struct navigate_ctx
{
    struct native_webview *nv;
    char *uri_utf8;
    pthread_mutex_t lock;
    pthread_cond_t cond;
    BOOL done;
    BOOL success;
    gulong handler_id;
};

static void on_load_changed(WebKitWebView *view, int load_event, void *user_data)
{
    struct navigate_ctx *ctx = user_data;

    if (load_event != WEBKIT_LOAD_FINISHED) return;

    pthread_mutex_lock(&ctx->lock);
    ctx->success = TRUE;
    ctx->done = TRUE;
    pthread_cond_signal(&ctx->cond);
    pthread_mutex_unlock(&ctx->lock);
}

/* Manual WCHAR->UTF-8 conversion via ntdll_wcstoumbs, not a library helper
 * -- follows dlls/comdlg32/unixlib.c's wcs_to_utf8 exactly, matching this
 * codebase's existing convention for the same operation rather than
 * inventing a second one. (ntdll_wcstoumbs is declared in
 * wine/unixlib.h -- already included above -- so no extra declaration is
 * needed here.) */
static char *wcs_to_utf8(const WCHAR *src)
{
    ULONG srclen, retlen;
    char *ret;

    if (!src) return NULL;
    srclen = wcslen(src);
    if (!(ret = malloc(srclen * 3 + 1))) return NULL;
    retlen = ntdll_wcstoumbs(src, srclen, ret, srclen * 3, FALSE);
    ret[retlen] = 0;
    return ret;
}

static void navigate_on_gtk_thread(void *data)
{
    struct navigate_ctx *ctx = data;

    ctx->handler_id = p_g_signal_connect_data(ctx->nv->view, "load-changed", (GCallback)on_load_changed,
                                               ctx, NULL, 0);
    p_webkit_web_view_load_uri(ctx->nv->view, ctx->uri_utf8);
}

/* Tears down the "load-changed" connection navigate_on_gtk_thread made,
 * regardless of whether on_load_changed ever actually fired for it.
 *
 * This MUST run unconditionally before unix_navigate_and_wait_impl
 * returns -- not just on the success path -- because ctx is stack-local
 * to that function: once it returns, &ctx is dangling, but the signal
 * connection stays live on the WebKitWebView until explicitly
 * disconnected. If a later "load-changed" emission (a second Navigate()
 * call, or -- the exact scenario this whole plan targets -- the page's
 * own internal redirect right after a successful Studio login) reached a
 * still-connected handler pointing at a dangling ctx, that would be a
 * callback into freed/reused stack memory.
 *
 * Deliberately NOT done by having on_load_changed self-disconnect on
 * WEBKIT_LOAD_FINISHED instead: that would leave the connection live
 * forever on the timeout path (network hang, WebKit process crash --
 * on_load_changed simply never runs), which is exactly the case the
 * bounded wait below exists to handle gracefully. Disconnecting here,
 * unconditionally, after the wait loop -- success or timeout -- is the
 * one path guaranteed to run before this function returns either way.
 * Runs via gtk_thread_invoke_sync (synchronous: blocks until actually
 * done on the GTK thread) so that by the time unix_navigate_and_wait_impl
 * returns, no further on_load_changed invocation for this ctx can occur --
 * signal emission and this disconnect both only ever happen on the single
 * dedicated GTK thread, so they can't race each other, only serialize. */
static void disconnect_load_changed_on_gtk_thread(void *data)
{
    struct navigate_ctx *ctx = data;

    if (ctx->handler_id) p_g_signal_handler_disconnect(ctx->nv->view, ctx->handler_id);
}

static NTSTATUS unix_navigate_and_wait_impl(void *args)
{
    struct navigate_params *params = args;
    struct native_webview *nv = (struct native_webview *)(ULONG_PTR)params->handle;
    struct navigate_ctx ctx = { nv, NULL, PTHREAD_MUTEX_INITIALIZER, PTHREAD_COND_INITIALIZER, FALSE, FALSE, 0 };
    struct timespec deadline;

    params->is_success = FALSE;
    if (!nv) return STATUS_INVALID_HANDLE;

    ctx.uri_utf8 = wcs_to_utf8(params->uri);

    /* Final-review fix (Important 3): if this returns FALSE,
     * navigate_on_gtk_thread never ran -- no "load-changed" signal
     * connection was ever made (ctx.handler_id stays 0) and WebKit was
     * never told to load anything, so on_load_changed can never fire.
     * Without this check, the bounded wait below would still block for
     * the full 30s waiting on a completion that can't happen, for
     * nothing. Skip straight to failure instead; the disconnect call
     * further down is also skipped since there's nothing to disconnect
     * (and it would fail the same way for the same reason). */
    if (!gtk_thread_invoke_sync(navigate_on_gtk_thread, &ctx))
    {
        WARN("gtk_thread_invoke_sync failed -- GTK thread not running, failing Navigate without waiting\n");
        free(ctx.uri_utf8);
        return STATUS_NOT_SUPPORTED;
    }

    /* Bounded wait, not indefinite: a real page can fail to ever fire
     * load-changed(FINISHED) (network failure, WebKit process crash). 30s
     * comfortably covers Studio's real login page per this investigation's
     * own earlier tracing; a stuck navigation should surface as a timeout
     * (is_success stays FALSE), not hang Roblox's calling thread forever. */
    clock_gettime(CLOCK_REALTIME, &deadline);
    deadline.tv_sec += 30;

    pthread_mutex_lock(&ctx.lock);
    while (!ctx.done)
        if (pthread_cond_timedwait(&ctx.cond, &ctx.lock, &deadline) == ETIMEDOUT) break;
    pthread_mutex_unlock(&ctx.lock);

    /* Always disconnect before touching ctx again or returning -- see the
     * comment on disconnect_load_changed_on_gtk_thread above for why this
     * can't just live inside on_load_changed instead. Final-review fix
     * (Important 3): the invoke above already succeeded once in this same
     * call and gtk_thread_invoke_sync has no path back to FALSE once the
     * GTK thread is confirmed running (see unix_destroy_webview_impl's own
     * comment on this), so this is unreachable in practice -- checked
     * anyway, with a WARN, for consistency with the other 6 call sites and
     * because ctx is stack-local here: if this ever did return FALSE, the
     * signal connection would stay live pointing at memory that's about to
     * go out of scope, which is worth surfacing loudly rather than
     * silently ignoring. */
    if (!gtk_thread_invoke_sync(disconnect_load_changed_on_gtk_thread, &ctx))
        WARN("gtk_thread_invoke_sync failed while disconnecting load-changed -- "
             "GTK thread not running, signal connection could not be torn down\n");

    params->is_success = ctx.success;
    params->navigation_id = (UINT64)(ULONG_PTR)&ctx; /* unique-enough per call; real WebView2's IDs aren't otherwise observable to us */
    free(ctx.uri_utf8);
    return STATUS_SUCCESS;
}

/* Task 8: cookie deletion.
 *
 * Deviation from the task brief: the brief specified
 * webkit_cookie_manager_replace_cookies(mgr, NULL, NULL, NULL, NULL) --
 * "empty list = delete all". Tried first, exactly as written; running the
 * resulting test binary against the real bundle (this task's own
 * verification step) produced this on stderr:
 *
 *   ** (process:NNNNN): CRITICAL **: HH:MM:SS.mmm: void
 *   webkit_cookie_manager_replace_cookies(WebKitCookieManager*, GList*,
 *   GCancellable*, GAsyncReadyCallback, gpointer): assertion 'cookies' failed
 *
 * That's glib's g_return_if_fail(cookies) firing inside WebKit's own
 * implementation and returning WITHOUT deleting anything -- the call did
 * not crash (WEBKIT_API critical warnings are non-fatal by default) but
 * silently did nothing, which is worse than a crash for a "delete all
 * cookies" API: DeleteAllCookies would report S_OK while leaving every
 * cookie in place. GLib's usual convention treats a NULL GList as a
 * perfectly valid empty list; this specific entry point's real
 * implementation doesn't accept that convention for this parameter. Real
 * fix, confirmed against the same bundle to actually delete cookies (no
 * CRITICAL, and the enumerated/deleted counts are consistent):
 * webkit_cookie_manager_get_all_cookies (async) -> _finish() to get the
 * real GList<SoupCookie*> -> webkit_cookie_manager_delete_cookie() once per
 * cookie -> g_list_free_full()+soup_cookie_free() to release the list
 * (transfer-full per webkitgtk.org's own docs for get_all_cookies_finish).
 *
 * WebKitCookieManager objects themselves are owned by the WebKitNetworkSession
 * (in turn owned by the WebKitWebView), not by us -- fetched fresh here
 * rather than cached, same as navigate_on_gtk_thread doesn't cache
 * anything off ctx->nv->view beyond its own call. */
struct delete_cookies_ctx
{
    struct native_webview *nv;
    pthread_mutex_t lock;
    pthread_cond_t cond;
    BOOL done;
    /* Starts at 2: one ref for unix_delete_all_cookies_impl's own bounded
     * wait below, one for the in-flight get_all_cookies async operation
     * (released by on_get_all_cookies_done once it actually runs).
     * Whichever side finishes touching ctx LAST frees it.
     *
     * This exists because, unlike unix_navigate_and_wait_impl's
     * "load-changed" GObject signal (which has a real synchronous
     * g_signal_handler_disconnect this file already uses to GUARANTEE no
     * further callback into a stack-local ctx after a timeout),
     * GAsyncReadyCallback has no analogous synchronous cancel/disconnect
     * primitive here: passing a GCancellable and cancelling it on timeout
     * only REQUESTS cancellation and still doesn't synchronously guarantee
     * the callback won't fire later, racing whatever unix_delete_all_
     * cookies_impl does after giving up on the wait. A stack-allocated ctx
     * (this file's usual pattern) would therefore be a real dangling-
     * pointer hazard on the timeout path -- the exact bug class already
     * found and fixed once in this codebase's own Task 7 review (a signal
     * handler outliving its stack-local closure data). Heap-allocating ctx
     * with refcounted, whichever-side-is-last-frees ownership sidesteps
     * that regardless of timing: on_get_all_cookies_done can safely run
     * (and touch ctx) at ANY time, even long after this function has
     * already timed out and returned. */
    LONG refs;
};

static void delete_cookies_ctx_release(struct delete_cookies_ctx *ctx)
{
    if (InterlockedDecrement(&ctx->refs)) return;
    pthread_mutex_destroy(&ctx->lock);
    pthread_cond_destroy(&ctx->cond);
    free(ctx);
}

static void on_get_all_cookies_done(GObject *source, GAsyncResult *res, void *user_data)
{
    struct delete_cookies_ctx *ctx = user_data;
    /* GIO/WebKit async convention: source_object is the very object the
     * _async-style call was made on -- here, the WebKitCookieManager itself
     * (passed back up to us as a plain GObject*, same simplification this
     * file already uses throughout for event-handler-ish/result-ish
     * pointers). */
    WebKitCookieManager *mgr = (WebKitCookieManager *)source;
    GList *cookies = p_webkit_cookie_manager_get_all_cookies_finish(mgr, res, NULL);
    GList *l;

    for (l = cookies; l; l = l->next)
    {
        /* Fire-and-forget: no callback needed per-cookie (matches the
         * brief's own "NULL callback = standard GLib async fire-and-forget"
         * reasoning) -- delete_cookie's cookie argument is caller-owned
         * (webkitgtk.org: "the data is owned by the caller of the method"),
         * so passing l->data here doesn't transfer ownership away from the
         * g_list_free_full() call below. */
        p_webkit_cookie_manager_delete_cookie(mgr, l->data, NULL, NULL, NULL);
    }
    if (cookies) p_g_list_free_full(cookies, (GDestroyNotify)p_soup_cookie_free);

    pthread_mutex_lock(&ctx->lock);
    ctx->done = TRUE;
    pthread_cond_signal(&ctx->cond);
    pthread_mutex_unlock(&ctx->lock);

    delete_cookies_ctx_release(ctx); /* this callback's own ref */
}

static void start_get_all_cookies_on_gtk_thread(void *data)
{
    struct delete_cookies_ctx *ctx = data;
    WebKitNetworkSession *session = p_webkit_web_view_get_network_session(ctx->nv->view);
    WebKitCookieManager *mgr = p_webkit_network_session_get_cookie_manager(session);

    /* Only STARTS the async enumeration and returns -- exactly like
     * navigate_on_gtk_thread only starts the load and connects a signal.
     * The actual completion (on_get_all_cookies_done, invoked later by the
     * GTK thread's own main loop once WebKit responds) is waited for
     * separately below, outside gtk_thread_invoke_sync. */
    p_webkit_cookie_manager_get_all_cookies(mgr, NULL, on_get_all_cookies_done, ctx);
}

static NTSTATUS unix_delete_all_cookies_impl(void *args)
{
    struct delete_all_cookies_params *params = args;
    struct native_webview *nv = (struct native_webview *)(ULONG_PTR)params->handle;
    struct delete_cookies_ctx *ctx;
    struct timespec deadline;

    if (!nv) return STATUS_INVALID_HANDLE;
    if (!(ctx = calloc(1, sizeof(*ctx)))) return STATUS_NO_MEMORY;

    ctx->nv = nv;
    ctx->refs = 2;
    pthread_mutex_init(&ctx->lock, NULL);
    pthread_cond_init(&ctx->cond, NULL);

    /* Final-review fix (Important 3): if this returns FALSE,
     * start_get_all_cookies_on_gtk_thread never ran -- webkit_cookie_manager_
     * get_all_cookies was never even started, so on_get_all_cookies_done can
     * never fire and release the callback's ref (ctx->refs's second count).
     * Without this check, ctx would leak (refs never reaches 0) AND the
     * bounded wait below would still block for the full 10s for a callback
     * that's never coming. Reclaim that ref ourselves (standing in for the
     * callback that will now never run) and bail out before the wait. */
    if (!gtk_thread_invoke_sync(start_get_all_cookies_on_gtk_thread, ctx))
    {
        WARN("gtk_thread_invoke_sync failed -- GTK thread not running, failing DeleteAllCookies without waiting\n");
        delete_cookies_ctx_release(ctx); /* the callback's ref, which will now never be released by on_get_all_cookies_done */
        delete_cookies_ctx_release(ctx); /* this function's own ref */
        return STATUS_NOT_SUPPORTED;
    }

    /* Bounded wait, not indefinite -- same rationale as
     * unix_navigate_and_wait_impl's own 30s bound (a stuck WebKit network
     * process should surface as a timeout, not hang Roblox's calling
     * thread forever). 10s here rather than 30s: unlike a real page
     * navigation, cookie-store enumeration is local, in-process/IPC work,
     * not a real network round-trip, so it's expected to complete almost
     * immediately in the normal case. */
    clock_gettime(CLOCK_REALTIME, &deadline);
    deadline.tv_sec += 10;

    pthread_mutex_lock(&ctx->lock);
    while (!ctx->done)
        if (pthread_cond_timedwait(&ctx->cond, &ctx->lock, &deadline) == ETIMEDOUT) break;
    pthread_mutex_unlock(&ctx->lock);

    delete_cookies_ctx_release(ctx); /* this function's own ref; see the
                                       * struct's own refs comment for why
                                       * this is safe to do unconditionally
                                       * here even on the timeout path. */
    return STATUS_SUCCESS;
}

/* --- Test-support only, from here to __wine_unix_call_funcs ---
 *
 * The original test_delete_all_cookies only checked that DeleteAllCookies
 * returned S_OK -- exactly the class of check that let the replace_cookies
 * bug above ship silently (the broken call also "succeeded" by that same
 * standard). unix_count_cookies below closes that gap the safe way: a
 * read-only real cookie count via the same get_all_cookies machinery
 * unix_delete_all_cookies_impl itself uses, so a test can assert
 * DeleteAllCookies actually reduced the count to zero. (An earlier version
 * of this file also had a matching "add a test cookie" unix call/PE export
 * for the other half of that verification; removed after code review found
 * it let any in-process code holding a live ICoreWebView2* inject arbitrary
 * cookies into the real store via webview2loader.dll's own export table,
 * unvalidated -- a real capability-widening risk since this Makefile.in
 * produces one unconditional production DLL, not a separate test build. See
 * struct count_cookies_params's own comment in unixlib.h and
 * tests/cookie_test_server.py for how the test adds a cookie now instead:
 * through a real HTTP response's Set-Cookie header via the already-
 * legitimate Navigate() path, not a new privileged hook.) */

struct count_cookies_ctx
{
    pthread_mutex_t lock;
    pthread_cond_t cond;
    BOOL done;
    UINT32 count;
    LONG refs; /* same pattern as struct delete_cookies_ctx/add_cookie_ctx above */
};

static void count_cookies_ctx_release(struct count_cookies_ctx *ctx)
{
    if (InterlockedDecrement(&ctx->refs)) return;
    pthread_mutex_destroy(&ctx->lock);
    pthread_cond_destroy(&ctx->cond);
    free(ctx);
}

static void on_count_cookies_done(GObject *source, GAsyncResult *res, void *user_data)
{
    struct count_cookies_ctx *ctx = user_data;
    GList *cookies = p_webkit_cookie_manager_get_all_cookies_finish((WebKitCookieManager *)source, res, NULL);
    GList *l;
    UINT32 n = 0;

    for (l = cookies; l; l = l->next) n++;
    if (cookies) p_g_list_free_full(cookies, (GDestroyNotify)p_soup_cookie_free);

    pthread_mutex_lock(&ctx->lock);
    ctx->count = n;
    ctx->done = TRUE;
    pthread_cond_signal(&ctx->cond);
    pthread_mutex_unlock(&ctx->lock);

    count_cookies_ctx_release(ctx); /* this callback's own ref */
}

struct count_cookies_start_ctx
{
    struct native_webview *nv;
    struct count_cookies_ctx *wait_ctx;
};

static void start_count_cookies_on_gtk_thread(void *data)
{
    struct count_cookies_start_ctx *sctx = data;
    WebKitNetworkSession *session = p_webkit_web_view_get_network_session(sctx->nv->view);
    WebKitCookieManager *mgr = p_webkit_network_session_get_cookie_manager(session);

    p_webkit_cookie_manager_get_all_cookies(mgr, NULL, on_count_cookies_done, sctx->wait_ctx);
}

static NTSTATUS unix_count_cookies_impl(void *args)
{
    struct count_cookies_params *params = args;
    struct native_webview *nv = (struct native_webview *)(ULONG_PTR)params->handle;
    struct count_cookies_ctx *ctx;
    struct count_cookies_start_ctx sctx;
    struct timespec deadline;

    params->count = 0;
    if (!nv) return STATUS_INVALID_HANDLE;
    if (!(ctx = calloc(1, sizeof(*ctx)))) return STATUS_NO_MEMORY;
    ctx->refs = 2;
    pthread_mutex_init(&ctx->lock, NULL);
    pthread_cond_init(&ctx->cond, NULL);

    sctx.nv = nv;
    sctx.wait_ctx = ctx;
    /* Final-review fix (Important 3): same leaked-ref/wasted-timeout hazard
     * as unix_delete_all_cookies_impl above -- if this returns FALSE,
     * start_count_cookies_on_gtk_thread never ran, on_count_cookies_done
     * never fires to release the callback's ref, and the wait below would
     * block the full 10s for nothing. Reclaim the ref ourselves and bail
     * out before waiting. */
    if (!gtk_thread_invoke_sync(start_count_cookies_on_gtk_thread, &sctx))
    {
        WARN("gtk_thread_invoke_sync failed -- GTK thread not running, failing count_cookies without waiting\n");
        count_cookies_ctx_release(ctx); /* the callback's ref, which will now never be released by on_count_cookies_done */
        count_cookies_ctx_release(ctx); /* this function's own ref */
        return STATUS_NOT_SUPPORTED;
    }

    clock_gettime(CLOCK_REALTIME, &deadline);
    deadline.tv_sec += 10;

    pthread_mutex_lock(&ctx->lock);
    while (!ctx->done)
        if (pthread_cond_timedwait(&ctx->cond, &ctx->lock, &deadline) == ETIMEDOUT) break;
    /* Only trust ctx->count if the callback actually ran (ctx->done true)
     * -- on a genuine timeout it's still 0 from the calloc above, which is
     * the honest "don't know, treat as 0" answer rather than reading a
     * count field the callback may not have written yet. */
    if (ctx->done) params->count = ctx->count;
    pthread_mutex_unlock(&ctx->lock);

    count_cookies_ctx_release(ctx);
    return STATUS_SUCCESS;
}

/* Task 11 real-bug fix: GetCookies. Same async-worker/refcounted-ctx shape
 * as unix_delete_all_cookies_impl/unix_count_cookies_impl above (see
 * struct delete_cookies_ctx's own comment for why a heap-allocated,
 * refcounted ctx is required here rather than a stack local -- the
 * GAsyncReadyCallback this hangs off has no synchronous cancel/disconnect
 * primitive, unlike navigate's "load-changed" GObject signal).
 *
 * Real ICoreWebView2CookieManager::GetCookies uri semantics (verified
 * against learn.microsoft.com's real ICoreWebView2CookieManager reference,
 * not guessed): a non-empty uri filters to cookies applicable to that URI;
 * NULL or empty returns every cookie under the profile. WebKit's own
 * webkit_cookie_manager_get_cookies (uri-scoped) vs. get_all_cookies (this
 * file's existing Task 8 helper) map onto that split directly -- no
 * hand-rolled domain/path cookie-applicability matching needed here, WebKit
 * already implements the standard algorithm underneath. */
/* Code review finding (Critical, fixed): the first version of this struct
 * stored a raw `struct get_cookies_params *params` pointer -- the PE-side
 * caller's own heap allocation -- and had on_get_cookies_done() write
 * straight into it unconditionally, with no liveness check. That's exactly
 * the bug struct count_cookies_ctx (above) was already careful to avoid: on
 * a real ETIMEDOUT, unix_get_cookies_impl returns and releases its own ref
 * while the WebKit async op can still be in flight (GAsyncReadyCallback has
 * no synchronous cancel, same reasoning as struct delete_cookies_ctx's own
 * comment). The PE side then sees success==FALSE, invokes the caller's
 * handler with E_FAIL, and frees `params`. A callback that completes AFTER
 * that point would write up to ~1.2MB into freed heap -- silent corruption,
 * the same crash class this plan already spent hours on for the SIGUSR1
 * bug. Fixed by mirroring struct count_cookies_ctx's own already-correct
 * pattern exactly: every out-field the callback writes lives IN this ctx
 * (safe for the callback to touch at any time, since ctx is refcounted and
 * only freed once both sides are done with it), and gets copied into
 * `params` only under ctx->lock, only if ctx->done, by
 * unix_get_cookies_impl itself after the wait -- never touched directly by
 * the callback. */
struct get_cookies_ctx
{
    struct native_webview *nv;
    char *uri_utf8; /* NULL => unfiltered (get_all_cookies); non-NULL => filtered (get_cookies) */
    pthread_mutex_t lock;
    pthread_cond_t cond;
    BOOL done;
    LONG refs; /* same "whichever side finishes last frees ctx" pattern as struct delete_cookies_ctx */

    /* out -- written only by on_get_cookies_done, read only by
     * unix_get_cookies_impl under ctx->lock after ctx->done is observed
     * true (see this struct's own leading comment). */
    BOOL success;
    UINT32 count;
    struct unix_cookie cookies[WEBVIEW2LOADER_MAX_COOKIES];
};

static void get_cookies_ctx_release(struct get_cookies_ctx *ctx)
{
    if (InterlockedDecrement(&ctx->refs)) return;
    pthread_mutex_destroy(&ctx->lock);
    pthread_cond_destroy(&ctx->cond);
    free(ctx->uri_utf8);
    free(ctx);
}

/* Copies one UTF-8 field (src, byte length n) into a fixed WCHAR dst[cap]
 * buffer, or fails rather than silently truncating (code review finding,
 * Important 1: an earlier version truncated via ntdll_umbstowcs's own
 * safe-truncation behavior with no diagnostic, which is memory-safe but
 * hands Studio a corrupted cookie *value* as if it were real data -- worse
 * than dropping the cookie outright for a caller that trusts what GetCookies
 * returns). Every UTF-8 encoding of a Unicode codepoint is at least as many
 * bytes as its UTF-16 encoding (1-3 UTF-8 bytes -> 1 UTF-16 unit for BMP
 * codepoints, 4 UTF-8 bytes -> a 2-unit UTF-16 surrogate pair) -- so if the
 * source's byte length already fits within the destination's WCHAR capacity,
 * the decoded string is GUARANTEED to fit too, no truncation possible. When
 * it doesn't fit that conservative check (rare for any real cookie field
 * given these caps -- see WEBVIEW2LOADER_COOKIE_*_MAX's own comment in
 * unixlib.h), this fails closed rather than guess. */
static BOOL copy_field_or_fail(const char *src, WCHAR *dst, ULONG cap, const char *field)
{
    ULONG n = src ? strlen(src) : 0;

    if (n >= cap)
    {
        GTK_THREAD_LOG("cookie %s is %u bytes, exceeding this build's %u-WCHAR cap -- dropping this cookie "
            "rather than returning a truncated value\n", field, (unsigned)n, (unsigned)(cap - 1));
        dst[0] = 0;
        return FALSE;
    }
    dst[ntdll_umbstowcs(src ? src : "", n, dst, cap - 1)] = 0;
    return TRUE;
}

/* Fills one struct unix_cookie from a real SoupCookie*. Returns FALSE (and
 * leaves dst only partially filled -- caller must not use it) if any string
 * field doesn't fit its fixed buffer; see copy_field_or_fail's own comment
 * for why that's a hard failure now, not a truncation. */
static BOOL fill_unix_cookie(struct unix_cookie *dst, SoupCookie *cookie)
{
    GDateTime *expires;

    if (!copy_field_or_fail(p_soup_cookie_get_name(cookie), dst->name, WEBVIEW2LOADER_COOKIE_NAME_MAX, "name") ||
        !copy_field_or_fail(p_soup_cookie_get_value(cookie), dst->value, WEBVIEW2LOADER_COOKIE_VALUE_MAX, "value") ||
        !copy_field_or_fail(p_soup_cookie_get_domain(cookie), dst->domain, WEBVIEW2LOADER_COOKIE_DOMAIN_MAX, "domain") ||
        !copy_field_or_fail(p_soup_cookie_get_path(cookie), dst->path, WEBVIEW2LOADER_COOKIE_PATH_MAX, "path"))
        return FALSE;

    /* NULL expires == session cookie, real libsoup semantics (soup-cookie.c's
     * own doc comment) and exactly real ICoreWebView2Cookie::IsSession's own
     * signal -- -1.0/TRUE is real WebView2's own documented sentinel for
     * "this is a session cookie" (learn.microsoft.com's real
     * ICoreWebView2Cookie reference: "The default is -1.0, which means
     * cookies are session cookies by default."), not a placeholder. */
    if ((expires = p_soup_cookie_get_expires(cookie)))
    {
        dst->expires = (double)p_g_date_time_to_unix(expires);
        dst->is_session = FALSE;
    }
    else
    {
        dst->expires = -1.0;
        dst->is_session = TRUE;
    }

    dst->is_http_only = p_soup_cookie_get_http_only(cookie) ? TRUE : FALSE;
    dst->is_secure = p_soup_cookie_get_secure(cookie) ? TRUE : FALSE;
    /* SoupSameSitePolicy's 3 values are numerically identical to
     * COREWEBVIEW2_COOKIE_SAME_SITE_KIND's (both verified against their real
     * headers -- see this function's own file-level extern comment) --
     * passed straight through, no translation table needed. */
    dst->same_site = (INT32)p_soup_cookie_get_same_site_policy(cookie);
    return TRUE;
}

static void on_get_cookies_done(GObject *source, GAsyncResult *res, void *user_data)
{
    struct get_cookies_ctx *ctx = user_data;
    GList *cookies, *l;
    UINT32 total = 0, n = 0;
    BOOL success;

    if (ctx->uri_utf8)
        cookies = p_webkit_cookie_manager_get_cookies_finish((WebKitCookieManager *)source, res, NULL);
    else
        cookies = p_webkit_cookie_manager_get_all_cookies_finish((WebKitCookieManager *)source, res, NULL);

    /* Code review finding (Important 2, fixed): the first version filled up
     * to WEBVIEW2LOADER_MAX_COOKIES and reported success==TRUE regardless,
     * silently handing back an incomplete list if the real store ever held
     * more. That's a real hazard specifically for clearAllCookiesAndRunCallbackHelper
     * (the actual real caller this whole fix targets) -- it's
     * enumerate-then-act, with no way to tell "GetCookies gave me everything"
     * from "GetCookies quietly gave me a subset". Counting the real list
     * length FIRST (a second pass, but a cheap one -- just pointer-chasing,
     * no allocation) makes exceeding the cap a real, distinguishable failure
     * (success==FALSE, same as any other GetCookies failure) instead of a
     * silently-wrong S_OK. */
    for (l = cookies; l; l = l->next) total++;

    if (total > WEBVIEW2LOADER_MAX_COOKIES)
    {
        GTK_THREAD_LOG("cookie store holds %u cookies, exceeding this build's %u-cookie cap -- failing this "
            "GetCookies call rather than returning a silently incomplete list\n",
            (unsigned)total, (unsigned)WEBVIEW2LOADER_MAX_COOKIES);
        success = FALSE;
    }
    else
    {
        success = TRUE;
        for (l = cookies; l; l = l->next)
        {
            /* fill_unix_cookie itself can still reject an individual cookie
             * whose field(s) don't fit (copy_field_or_fail) -- that cookie is
             * dropped (loudly, via that function's own GTK_THREAD_LOG -- see
             * that macro's own comment for why not ERR/WARN here, this runs
             * on gtk_thread) rather than failing the whole call; unlike the
             * total-count cap above, a
             * single oversized field is not something clearAllCookiesAndRunCallbackHelper's
             * enumerate-then-act semantics can be silently wrong about in
             * the same way (the cookie that didn't fit couldn't have been
             * meaningfully acted on by real WebView2 either, since Studio
             * receives it via the same fixed-ish LPWSTR-returning real
             * WebView2 API either way). */
            if (fill_unix_cookie(&ctx->cookies[n], l->data)) n++;
        }
    }

    if (cookies) p_g_list_free_full(cookies, (GDestroyNotify)p_soup_cookie_free);

    pthread_mutex_lock(&ctx->lock);
    ctx->success = success;
    ctx->count = n;
    ctx->done = TRUE;
    pthread_cond_signal(&ctx->cond);
    pthread_mutex_unlock(&ctx->lock);

    get_cookies_ctx_release(ctx); /* this callback's own ref */
}

static void start_get_cookies_on_gtk_thread(void *data)
{
    struct get_cookies_ctx *ctx = data;
    WebKitNetworkSession *session = p_webkit_web_view_get_network_session(ctx->nv->view);
    WebKitCookieManager *mgr = p_webkit_network_session_get_cookie_manager(session);

    if (ctx->uri_utf8)
        p_webkit_cookie_manager_get_cookies(mgr, ctx->uri_utf8, NULL, on_get_cookies_done, ctx);
    else
        p_webkit_cookie_manager_get_all_cookies(mgr, NULL, on_get_cookies_done, ctx);
}

static NTSTATUS unix_get_cookies_impl(void *args)
{
    struct get_cookies_params *params = args;
    struct native_webview *nv = (struct native_webview *)(ULONG_PTR)params->handle;
    struct get_cookies_ctx *ctx;
    char *uri_utf8;
    struct timespec deadline;

    params->success = FALSE;
    params->count = 0;
    if (!nv) return STATUS_INVALID_HANDLE;
    /* Heap-allocated (not a stack local) for the same reason struct
     * get_cookies_params itself is -- this ctx now embeds the same
     * WEBVIEW2LOADER_MAX_COOKIES-sized cookies array (~1.2MB), see this
     * struct's own leading comment for why it moved here from `params`. */
    if (!(ctx = calloc(1, sizeof(*ctx)))) return STATUS_NO_MEMORY;

    /* NULL or empty uri => unfiltered, matching real GetCookies semantics
     * (see this function's own leading comment) -- wcs_to_utf8(NULL)
     * already returns NULL, so only the empty-string case needs an extra
     * check here. */
    uri_utf8 = wcs_to_utf8(params->uri);
    if (uri_utf8 && !uri_utf8[0]) { free(uri_utf8); uri_utf8 = NULL; }

    ctx->nv = nv;
    ctx->uri_utf8 = uri_utf8;
    ctx->refs = 2;
    pthread_mutex_init(&ctx->lock, NULL);
    pthread_cond_init(&ctx->cond, NULL);

    /* Final-review fix (Important 3): same leaked-ref/wasted-timeout hazard
     * as unix_delete_all_cookies_impl/unix_count_cookies_impl above -- if
     * this returns FALSE, start_get_cookies_on_gtk_thread never ran,
     * on_get_cookies_done never fires to release the callback's ref, and
     * the wait below would block the full 10s for nothing. Reclaim the ref
     * ourselves (get_cookies_ctx_release also frees ctx->uri_utf8 once the
     * refcount actually reaches 0) and bail out before waiting. */
    if (!gtk_thread_invoke_sync(start_get_cookies_on_gtk_thread, ctx))
    {
        WARN("gtk_thread_invoke_sync failed -- GTK thread not running, failing GetCookies without waiting\n");
        get_cookies_ctx_release(ctx); /* the callback's ref, which will now never be released by on_get_cookies_done */
        get_cookies_ctx_release(ctx); /* this function's own ref */
        return STATUS_NOT_SUPPORTED;
    }

    /* Bounded wait, not indefinite -- same rationale and same 10s bound as
     * unix_delete_all_cookies_impl/unix_count_cookies_impl above (local
     * cookie-store enumeration, not a real network round-trip). */
    clock_gettime(CLOCK_REALTIME, &deadline);
    deadline.tv_sec += 10;

    pthread_mutex_lock(&ctx->lock);
    while (!ctx->done)
        if (pthread_cond_timedwait(&ctx->cond, &ctx->lock, &deadline) == ETIMEDOUT) break;
    /* Only trust ctx->success/ctx->count/ctx->cookies if the callback
     * actually ran (ctx->done true) -- mirrors unix_count_cookies_impl's own
     * identical "don't read fields the callback may not have written yet"
     * guard directly above in this file. This is also what makes the
     * Critical fix above actually safe: params (the caller's own buffer) is
     * only ever written here, under this lock, after that check -- never by
     * the callback itself, so a callback that fires after a real timeout
     * (params already freed by the PE-side caller) touches only this ctx,
     * which stays valid until BOTH sides release their ref. */
    if (ctx->done)
    {
        params->success = ctx->success;
        params->count = ctx->count;
        if (ctx->success) memcpy(params->cookies, ctx->cookies, ctx->count * sizeof(*ctx->cookies));
    }
    pthread_mutex_unlock(&ctx->lock);

    get_cookies_ctx_release(ctx); /* this function's own ref -- safe unconditionally, same as
                                    * unix_delete_all_cookies_impl's own identical release call. */
    return STATUS_SUCCESS;
}

/* Test-support only (Plan 3 Task 2): real gtk_widget_get_visible readback,
 * so a test can assert the HWND_MESSAGE skip actually took effect instead
 * of just "the call didn't crash". Read-only, same low-risk diagnostic
 * shape as unix_count_cookies_impl. */
static void get_window_visible_on_gtk_thread(void *data)
{
    struct get_window_visible_params *params = data;
    struct native_webview *nv = (struct native_webview *)(ULONG_PTR)params->handle;

    /* Task 7 UAF guard -- see struct native_webview's own comment above. */
    if (!live_webview_is_valid(nv))
    {
        GTK_THREAD_LOG("stale/destroyed native window handle %p -- reporting not visible\n", nv);
        params->visible = FALSE;
        return;
    }

    params->visible = p_gtk_widget_get_visible(nv->window);
}

static NTSTATUS unix_get_window_visible_impl(void *args)
{
    struct get_window_visible_params *params = args;
    struct native_webview *nv = (struct native_webview *)(ULONG_PTR)params->handle;

    params->visible = FALSE;
    if (!nv) return STATUS_INVALID_HANDLE;
    if (!gtk_thread_invoke_sync(get_window_visible_on_gtk_thread, params))
        WARN("gtk_thread_invoke_sync failed -- GTK thread not running\n");
    return STATUS_SUCCESS;
}

/* Task 7 diagnostic (round 7) -- see this counter's own use, further down
 * in sync_window_geometry_on_gtk_thread, for why. File-scope so it
 * persists/accumulates across every call for the life of the process
 * (this function only ever runs serialized on the single GTK thread, so
 * this needs no lock). */
static unsigned long xmove_call_count;

/* Code review fix (cleanup, round 11): the round 9-11 diagnostic
 * GTK_THREAD_LOG calls in sync_window_geometry_on_gtk_thread fire on
 * every successful call -- currently dormant only because the xid range
 * guard rejects every real xid this session, but once the underlying xid
 * bug is actually fixed this would fire on every single put_Bounds call,
 * i.e. every drag-follow frame, with no way to silence it (GTK_THREAD_LOG
 * is a plain fprintf, it doesn't go through WINEDEBUG's own channel
 * gating). Gated behind an explicit opt-in env var instead of firing
 * unconditionally or being deleted outright -- still genuinely useful for
 * whoever picks the xid investigation back up. Checked once, lazily,
 * cached here -- this function only ever runs on the single serialized
 * GTK thread, so no lock is needed for the cache either. */
static int geometry_debug_enabled = -1; /* -1 = not yet checked */

static BOOL geometry_debug_on(void)
{
    if (geometry_debug_enabled < 0)
        geometry_debug_enabled = getenv("TUXBLOX_WEBVIEW_GEOMETRY_DEBUG") ? 1 : 0;
    return geometry_debug_enabled;
}

/* Plan 3 Task 3: real position/size/visibility sync, called (via Task 4's
 * controller_push_geometry_to_native) from put_Bounds/put_IsVisible. See
 * this file's own X11_FUNCS/gdk_x11_surface_get_xid extern comments above
 * for why position sync specifically has to go through raw Xlib. */
static void sync_window_geometry_on_gtk_thread(void *data)
{
    struct sync_window_geometry_params *params = data;
    struct native_webview *nv = (struct native_webview *)(ULONG_PTR)params->handle;
    GtkNative *native;
    GdkSurface *surface;
    GdkDisplay *gdisplay;
    unsigned long xid;
    Display *display;
    int width = params->screen_bounds.right - params->screen_bounds.left;
    int height = params->screen_bounds.bottom - params->screen_bounds.top;

    params->success = FALSE;

    /* Task 7 UAF guard -- see struct native_webview's own comment above.
     * This is the exact call site a real Studio relaunch crashed in twice
     * (first via a NULL Display*, now confirmed via coredump/GDB analysis
     * to be a stale handle: XMoveResizeWindow's `xid` argument read back
     * as a heap-pointer-shaped value, not a plausible X11 resource id --
     * the fingerprint of dereferencing already-freed memory). */
    if (!live_webview_is_valid(nv))
    {
        GTK_THREAD_LOG("stale/destroyed native window handle %p -- skipping geometry sync\n", nv);
        params->success = TRUE; /* never fatal to the controller, matches this
                                  * function's existing degrade-gracefully pattern */
        return;
    }

    /* Visibility is applied unconditionally, before the params->visible
     * guard below, so a transition to hidden actually hides the window on
     * this same call rather than requiring a separate one -- the guard right
     * after only short-circuits the size/position work that's meaningless
     * once the window is (or is becoming) invisible. */
    p_gtk_widget_set_visible(nv->window, params->visible);
    if (!params->visible)
    {
        params->success = TRUE; /* nothing more to sync while hidden */
        return;
    }

    if (width > 0 && height > 0)
        p_gtk_window_set_default_size(nv->window, width, height);

    /* Position: X11-only, see this task's own header note for why GTK4 has
     * no cross-backend equivalent. Degrades gracefully (size/visibility
     * still applied above) under Wayland. */
    native = p_gtk_widget_get_native(nv->window);
    surface = native ? p_gtk_native_get_surface(native) : NULL;
    if (!surface)
    {
        GTK_THREAD_LOG("no GdkSurface yet for native window %p -- skipping position sync\n", nv->window);
        params->success = TRUE;
        return;
    }

    /* Task 7 crash fix round 6: hold a real GObject reference on `surface`
     * (and, below, on `gdisplay`) for the entire span these are used,
     * instead of just re-checking validity right before each use. Rounds
     * 4-5 each added a fresh "is this still valid?" check immediately
     * before the point that then crashed -- and each time, a real relaunch
     * crashed again right after that check passed, always in the same
     * neighborhood (inside XLockDisplay/XMoveResizeWindow, touching this
     * same connection). That is the textbook signature of a TOCTOU race:
     * if something on a thread this code doesn't control (WebKitGTK's own
     * compositing/GL threads remain the leading suspect, per the
     * XInitThreads comment below) can invalidate the surface/display
     * between a check and the use it's guarding, no amount of re-checking
     * immediately beforehand closes that gap, because "immediately
     * beforehand" still isn't atomic with the use. g_object_ref/unref is
     * GLib's own, idiomatic answer to exactly this: a GObject cannot be
     * finalized while any caller holds a reference on it, regardless of
     * what any other thread does concurrently, which closes the race
     * structurally rather than narrowing it. Released at every exit below
     * and at the end of the function. */
    p_g_object_ref(surface);

    /* Task 7 crash fix, round 11: real-time object-graph diagnostic (repo
     * owner wants real docking fixed, not just the crash guarded -- the
     * blank second native-parent window they're seeing is exactly the
     * predicted symptom of position sync never actually landing). A prior
     * root-cause investigation (docs/source reading only, no live state)
     * concluded gtk_widget_show() on a toplevel is documented as
     * synchronous realize+map, making a "called before realize" ordering
     * bug look unlikely -- but that was theory, not observation. Logging
     * the ACTUAL live gtk_widget_get_realized()/get_mapped() state right
     * here settles it with real data instead. */
    if (geometry_debug_on())
        GTK_THREAD_LOG("object graph before gdk_x11_surface_get_xid: nv=%p nv->window=%p native=%p "
                        "surface=%p realized=%d mapped=%d\n",
                        nv, nv->window, native, surface,
                        p_gtk_widget_get_realized(nv->window), p_gtk_widget_get_mapped(nv->window));

    xid = p_gdk_x11_surface_get_xid(surface);
    if (!xid)
    {
        /* Not an X11 surface (Wayland) -- gdk_x11_surface_get_xid already
         * logged GLib's own non-fatal CRITICAL. */
        params->success = TRUE;
        p_g_object_unref(surface);
        return;
    }

    /* Task 7 crash fix, round 10: real, in-context diagnostic data from an
     * actual crash (round 9, GTK_THREAD_LOG -- see this function's own git
     * log for the full forensic trail) caught this function returning a
     * non-zero but structurally-impossible xid red-handed: 140411963110096
     * (0x7fb4352ef2d0) on the very first XMoveResizeWindow call this
     * process ever made -- ruling out any theory involving corruption
     * accumulating over many calls, and confirming round 2's original
     * finding (a heap-pointer-shaped xid) was not a one-off. X11 resource
     * IDs are a protocol-level 32-bit quantity (core protocol section 2.3,
     * "Every RESOURCE ... is a 32-bit value") -- ANY value here that
     * doesn't fit in 32 bits is proof-positive not a real XID, full stop,
     * regardless of what specifically went wrong upstream to produce it
     * (surface is structurally guaranteed live here -- ref'd via
     * g_object_ref above before this call -- so this isn't a freed
     * `surface`; whatever's wrong lives inside gdk_x11_surface_get_xid's
     * own read, or in memory it reads through, which is real signal for
     * whoever picks this back up, but doesn't change what the guard below
     * needs to do). Degrades the same way the !xid check just above
     * already does for the "not an X11 surface" case -- never fatal to
     * the controller, per the plan's own Error Handling section. */
    if (xid > 0xffffffffUL)
    {
        GTK_THREAD_LOG("xid %lu (0x%lx) for native window %p exceeds the 32-bit X11 resource ID range "
                        "-- not a real XID, skipping position sync\n", xid, xid, nv->window);
        params->success = TRUE;
        p_g_object_unref(surface);
        return;
    }

    /* Code review fix (Important 1): mirror the width>0 && height>0 guard
     * gtk_window_set_default_size already gets above -- a non-positive width
     * or height here (e.g. a transient zero-size Bounds update that arrives
     * before the controller is actually hidden) would otherwise be cast to
     * `unsigned int` for XMoveResizeWindow, wrapping a negative value into a
     * huge one; X11's ConfigureWindow protocol requires width/height >= 1
     * (0 is BadValue), and there is no guarantee the server tolerates the
     * wrapped huge value either. Skipping the move (rather than clamping to
     * 1x1) matches this function's existing degrade-gracefully pattern used
     * for the "no surface"/"no XID" cases just above -- never fatal to the
     * controller, per the plan's own Error Handling section.
     *
     * Code review fix (cleanup): the GdkDisplay lookup/liveness-check/ref
     * and the raw Display* lookup below are only ever needed for the
     * actual XMoveResizeWindow call inside this same guard -- moved inside
     * it (previously ran unconditionally, wasted work on every no-op
     * zero-size call). */
    if (width > 0 && height > 0)
    {
        /* Task 7 crash fix round 5: check the GdkDisplay CONNECTION's own
         * liveness before converting it to a raw Display* at all. A
         * dedicated investigation into round 4's crash (still inside
         * XLockDisplay, dereferencing `display`, even with a live nv
         * handle and a non-NULL surface/xid/display) confirmed the
         * dlmopen/RTLD_NOLOAD namespace join used to resolve
         * p_XLockDisplay/p_XMoveResizeWindow is provably correct (same
         * loaded libX11.so.6 instance GDK itself uses) -- ruling out a
         * cross-namespace mismatch -- and narrowed this down to the most
         * concrete remaining gap: nothing here re-validates that the
         * underlying X11 connection itself is still open. The existing
         * live_webviews registry only covers the per-webview handle, not
         * this shared, longer-lived connection object -- if something
         * closes it (see gdk_display_is_closed's own extern comment
         * above) between whenever GDK last touched it and this call,
         * dereferencing it here is a real, structurally-analogous UAF to
         * the one that registry already fixed for `nv`, just one level up
         * the object graph. (Round 6: this check alone wasn't sufficient
         * either, see the g_object_ref comment above -- kept anyway as a
         * fast, cheap early-out; the ref taken just below is what
         * actually closes the race.) */
        gdisplay = p_gdk_surface_get_display(surface);
        if (!gdisplay || p_gdk_display_is_closed(gdisplay))
        {
            GTK_THREAD_LOG("no live GdkDisplay for native window %p -- skipping position sync\n", nv->window);
            params->success = TRUE;
            p_g_object_unref(surface);
            return;
        }
        p_g_object_ref(gdisplay); /* see the g_object_ref comment on `surface` above */

        /* Task 7 real-launch crash fix: gdk_x11_display_get_xdisplay can
         * return NULL here even though the xid lookup above already
         * succeeded (see this file's own investigation notes for the
         * Task 7 crash report -- best-effort root cause points at a
         * stale/UAF'd surface rather than a legitimate GDK_IS_X11_DISPLAY
         * backend mismatch, since a real X11 GdkSurface's display can't
         * validly be a non-X11 backend). Passing a NULL Display* straight
         * into XMoveResizeWindow segfaults inside libX11 (_XGetRequest
         * dereferencing display->request) -- confirmed via
         * systemd-coredump across every real launch this session. Guard
         * it the same degrade-gracefully way as the !surface/!xid checks
         * above: never fatal to the controller. */
        display = p_gdk_x11_display_get_xdisplay(gdisplay);
        if (!display)
        {
            GTK_THREAD_LOG("no Display* for native window %p -- skipping position sync\n", nv->window);
            params->success = TRUE;
            p_g_object_unref(gdisplay);
            p_g_object_unref(surface);
            return;
        }

        /* Task 7 UAF/Xlib-locking fix round 3 -- see XInitThreads' own
         * extern comment above for the full crash evidence. This raw Xlib
         * call bypasses GDK's own request serialization entirely, so it
         * has to bracket itself with XLockDisplay/XUnlockDisplay against
         * whatever else (WebKitGTK's own auxiliary compositing/GL threads
         * are the leading suspect) may be using this same Display*
         * connection concurrently -- otherwise this is exactly the kind of
         * unsynchronized access that corrupts Xlib's per-Display request
         * buffers and segfaults deep inside _XGetRequest, which is what a
         * real relaunch kept hitting here even with a live handle, a
         * non-NULL surface/xid, and a non-NULL Display*.
         *
         * Rounds 4-5 findings (see git log for the full forensic trail):
         * a real relaunch crashed again even with this locking in place
         * (round 4, inside XLockDisplay itself while dereferencing
         * `display`), and again even after adding a gdk_display_is_closed
         * liveness check immediately beforehand (round 5, same site,
         * right after that check passed) -- confirming a TOCTOU race, not
         * a one-shot validity problem. Round 6 (this version) holds real
         * GObject references on `surface` and `gdisplay` for the entire
         * span they're used (see the g_object_ref comment above), which
         * closes that race structurally: GLib's own reference counting
         * guarantees neither object can be finalized while a ref is held,
         * regardless of what any other thread does concurrently. */
        /* Task 7 diagnostic, round 9: round 7's original diagnostic (a
         * call counter + xid/display logged immediately around this call)
         * was the right instinct, but used WARN() -- itself unsafe on this
         * thread (see GTK_THREAD_LOG's own comment above) -- so it crashed
         * on its own logging bug before ever collecting real data (round
         * 7's crash), and got stripped back to a silent counter (round 8)
         * while that logging bug got fixed for real. Now that
         * GTK_THREAD_LOG is safe to call from this thread (round 8,
         * confirmed via a real relaunch: that fix genuinely eliminated the
         * wine_dbg_log crash path, but round 8's OWN real relaunch still
         * crashed with the ORIGINAL _XGetRequest/XMoveResizeWindow
         * signature -- these are two real, separate bugs, not one), retry
         * the same diagnostic idea for real. Also dumps the first 32 raw
         * bytes at `display` itself -- not because this file knows Xlib's
         * private struct layout (it deliberately doesn't, see Display's
         * own typedef comment), just as a raw byte-level sanity check: if
         * a future crash's coredump can be compared against a LOGGED
         * "last known good" dump of the same pointer, a mismatch would
         * itself be evidence of corruption between calls, no struct
         * knowledge required to observe that much. */
        ++xmove_call_count;
        if (geometry_debug_on())
        {
            /* Code review fix (cleanup): `display` is unconditionally
             * non-NULL by this point -- the `!display` check just above
             * already returned early otherwise -- so the previous
             * `display ? ... : 0` ternaries here were dead; direct reads. */
            /* params->parent_xid is a UINT64 (unixlib.h, deliberately
             * wire-size-fixed since it crosses the PE/unix boundary --
             * see that field's own comment); on the i386 build `long` is
             * 32-bit while UINT64 stays 64-bit, so %lu against the raw
             * field is a real, build-breaking format-string mismatch
             * there even though it happens to compile clean on x86_64
             * (where `long` is also 64-bit) -- caught the hard way via a
             * real -Werror=format= i386 build failure, not by inspection.
             * Cast to unsigned long at every use below, matching this
             * file's own existing xid handling (also always truncated to
             * unsigned long for the same %lu logging, never for the
             * actual Xlib calls, which keep the real params->parent_xid). */
            GTK_THREAD_LOG("before position sync: call #%lu xid=%lu display=%p parent_xid=%lu "
                            "reparented_into=%lu bytes=%016lx %016lx %016lx %016lx\n",
                            xmove_call_count, xid, (void *)display, (unsigned long)params->parent_xid,
                            nv->reparented_into,
                            *(unsigned long *)display,
                            *(unsigned long *)((char *)display + 8),
                            *(unsigned long *)((char *)display + 16),
                            *(unsigned long *)((char *)display + 24));
        }
        p_XLockDisplay(display);
        if (geometry_debug_on()) GTK_THREAD_LOG("after XLockDisplay: call #%lu (locked ok)\n", xmove_call_count);

        /* Task 7 crash fix, round 15: real embedding via X11-level
         * reparenting, not an independently-positioned floating window --
         * see controller.c's controller_push_geometry_to_native and this
         * struct field's own comment (params->parent_xid) for the full
         * rationale. Reparented once per distinct parent_xid value (not
         * every call) -- XReparentWindow itself is a real, visible
         * operation on the X server (briefly unmaps/remaps in some WM
         * implementations), so only doing it when the target actually
         * changes avoids redundant server round-trips on every single
         * put_Bounds call. Comparing against nv->reparented_into (not
         * just "have we ever reparented") also means a genuinely changed
         * parent_xid -- including recovering from Wine recreating the
         * parent's whole_window, see that field's own comment -- gets a
         * fresh reparent on the very next call. */
        /* Task 7 crash fix, round 18: unconditional (not geometry_debug_on()
         * -gated) logging around the reparent decision and the
         * XReparentWindow call itself. Added after a real relaunch showed
         * ZERO evidence either way (no reparent log line fired, because
         * geometry_debug_on() was gated behind an env var nobody set for
         * that run, AND a real XQueryTree probe against the live windows
         * afterward proved reparenting had never happened) -- there was no
         * way to tell from that run's own logs whether params->parent_xid
         * was ever non-zero, whether the reparent condition was ever
         * entered, or whether XReparentWindow itself silently failed once
         * round 17's own error handler made X protocol errors non-fatal
         * instead of crashing. This makes all of that directly observable
         * on every run, not just debug-flagged ones -- cheap enough (once
         * per put_Bounds call, not per frame) to always pay for. */
        GTK_THREAD_LOG("reparent check: call #%lu parent_xid=0x%lx reparented_into=0x%lx "
                        "(will reparent: %s)\n",
                        xmove_call_count, (unsigned long)params->parent_xid, nv->reparented_into,
                        (params->parent_xid && nv->reparented_into != params->parent_xid) ? "YES" : "no");
        if (params->parent_xid && nv->reparented_into != params->parent_xid)
        {
            int reparent_status;
            x11_error_seen_during_call = FALSE;
            GTK_THREAD_LOG("XReparentWindow: call #%lu ABOUT TO CALL xid=0x%lx into parent_xid=0x%lx\n",
                            xmove_call_count, xid, (unsigned long)params->parent_xid);
            reparent_status = p_XReparentWindow(display, xid, params->parent_xid, 0, 0);
            /* XReparentWindow is asynchronous (like almost all core X11
             * requests) -- a real protocol-level failure (e.g. BadWindow if
             * parent_xid is stale/invalid) is delivered later, out-of-band,
             * to the error handler round 17 installed, not via this call's
             * own return value. XSync forces the request (and any error
             * reply for it) to round-trip to completion before we check the
             * flag below, so "did the error handler fire during this
             * specific call" is actually meaningful instead of racing
             * ahead of the server's real response. */
            p_XSync(display, 0);
            GTK_THREAD_LOG("XReparentWindow: call #%lu RETURNED status=%d x11_error_during_call=%s\n",
                            xmove_call_count, reparent_status,
                            x11_error_seen_during_call ? "YES -- see x11_error_handler's own log line above for details" : "no");
            nv->reparented_into = params->parent_xid;
        }

        if (params->parent_xid)
        {
            /* Once reparented, XMoveResizeWindow's x/y become relative to
             * the NEW parent's own origin, not the root window's --
             * params->screen_bounds is still an absolute (root-relative)
             * rect (controller.c's own ClientToScreen-based computation,
             * unchanged), so translate it into parent_xid's own
             * coordinate space via real X11 coordinate translation rather
             * than guessing/hardcoding whatever offset the parent's own
             * window-manager decorations or Wine-drawn chrome introduce
             * between its top-left corner and the root window's origin.
             * Falls back to the untranslated absolute position (the old
             * floating-window-era behavior) if the translation call
             * itself fails -- degrade gracefully, never fatal, matching
             * this function's own established pattern throughout. */
            int dest_x = params->screen_bounds.left, dest_y = params->screen_bounds.top;
            unsigned long child_return;

            if (!p_XTranslateCoordinates(display, p_XDefaultRootWindow(display), params->parent_xid,
                                          params->screen_bounds.left, params->screen_bounds.top,
                                          &dest_x, &dest_y, &child_return))
            {
                GTK_THREAD_LOG("XTranslateCoordinates failed for parent_xid=%lu -- falling back to "
                               "untranslated absolute position\n", (unsigned long)params->parent_xid);
                dest_x = params->screen_bounds.left;
                dest_y = params->screen_bounds.top;
            }
            p_XMoveResizeWindow(display, xid, dest_x, dest_y, (unsigned int)width, (unsigned int)height);

            /* Task 7 crash fix, round 19: the real fix for a genuine,
             * evidence-confirmed bug -- put_Bounds after reparenting visibly
             * changed the real X11 window size (confirmed via a direct
             * XGetGeometry readback during this round's own investigation:
             * immediately after this exact XMoveResizeWindow call, the real
             * server-side geometry was already correct), but GDK's own
             * cached notion of the surface's size
             * (gdk_surface_get_width/height, which is what both the test's
             * readback AND -- critically -- GTK's own internal widget
             * size-allocate/layout cycle for the embedded WebKitWebView are
             * driven from) never updated, staying at GTK4's original
             * default-window size from realize time no matter how many
             * further resizes were issued. Root cause: ICCCM. Per the
             * ICCCM (the decades-old convention every real X11 window
             * manager and toolkit follows), a client's toplevel only trusts
             * a *synthetic* (XSendEvent-delivered, send_event=True)
             * ConfigureNotify as authoritative for its own on-screen
             * geometry -- a *real*, server-generated one is defined by the
             * ICCCM to report coordinates relative to whatever the window's
             * CURRENT immediate parent happens to be (meaningless for
             * absolute placement once reparented), so well-behaved
             * toolkits, GDK's X11 backend included, are SUPPOSED to ignore
             * real ConfigureNotify for a toplevel and wait for the
             * WM's synthetic one instead. A real window manager (confirmed:
             * this environment's real relaunch/test runs both use a genuine
             * WM) sends that synthetic event whenever it moves/resizes a
             * window it manages -- which is exactly why the OTHER branch
             * below (no parent_xid, still a WM-managed independent
             * top-level, this function's pre-round-15 behavior) has always
             * worked correctly. Once round 15 reparented nv->window into an
             * arbitrary application HWND instead of leaving it WM-managed,
             * nothing plays the WM's role of sending that synthetic
             * event -- so GDK's cache silently never updates again, even
             * though the real window keeps moving/resizing correctly at
             * the X11 protocol level the whole time. Fix: send it
             * ourselves, exactly like a real window manager would, with
             * ROOT-relative x/y (screen_bounds is already in that space --
             * an ICCCM synthetic ConfigureNotify's x/y are defined as
             * root-relative regardless of what the window's real immediate
             * parent is) and the real width/height we just set. */
            {
                XConfigureEvent synthetic_configure = { 0 };
                synthetic_configure.type = TUXBLOX_ConfigureNotify;
                synthetic_configure.send_event = 1;
                synthetic_configure.display = display;
                synthetic_configure.event = xid;
                synthetic_configure.window = xid;
                synthetic_configure.x = params->screen_bounds.left;
                synthetic_configure.y = params->screen_bounds.top;
                synthetic_configure.width = width;
                synthetic_configure.height = height;
                synthetic_configure.border_width = 0;
                synthetic_configure.above = 0;
                synthetic_configure.override_redirect = 0;
                p_XSendEvent(display, xid, 0, TUXBLOX_StructureNotifyMask, &synthetic_configure);
                if (geometry_debug_on())
                    GTK_THREAD_LOG("sent synthetic ConfigureNotify: call #%lu x=%d y=%d w=%d h=%d\n",
                                    xmove_call_count, synthetic_configure.x, synthetic_configure.y,
                                    width, height);
            }
        }
        else
        {
            /* No valid parent_xid this call (message-only controller, no
             * parent_window, or __wine_x11_whole_window not found/set
             * yet) -- degrade to the original floating-window behavior:
             * absolute screen position, no reparenting. Matches this
             * function's own pre-round-15 behavior exactly. */
            p_XMoveResizeWindow(display, xid, params->screen_bounds.left, params->screen_bounds.top,
                                 (unsigned int)width, (unsigned int)height);
        }

        if (geometry_debug_on()) GTK_THREAD_LOG("after XMoveResizeWindow: call #%lu returned successfully\n", xmove_call_count);
        p_XUnlockDisplay(display);
        if (geometry_debug_on()) GTK_THREAD_LOG("after XUnlockDisplay: call #%lu (unlocked ok)\n", xmove_call_count);
        p_g_object_unref(gdisplay);
    }
    p_g_object_unref(surface);
    params->success = TRUE;
}

static NTSTATUS unix_sync_window_geometry_impl(void *args)
{
    struct sync_window_geometry_params *params = args;
    struct native_webview *nv = (struct native_webview *)(ULONG_PTR)params->handle;

    params->success = FALSE;
    if (!nv) return STATUS_INVALID_HANDLE;
    if (!gtk_thread_invoke_sync(sync_window_geometry_on_gtk_thread, params))
        WARN("gtk_thread_invoke_sync failed -- GTK thread not running, geometry sync skipped\n");
    return STATUS_SUCCESS;
}

/* Test-support only (Plan 3 Task 3): real GdkSurface width/height readback
 * so a test can confirm sync_window_geometry actually changed the on-screen
 * window, not just that the call returned success. */
static void get_window_geometry_on_gtk_thread(void *data)
{
    struct get_window_geometry_params *params = data;
    struct native_webview *nv = (struct native_webview *)(ULONG_PTR)params->handle;
    GtkNative *native;
    GdkSurface *surface;

    /* Task 7 UAF guard -- see struct native_webview's own comment above. */
    if (!live_webview_is_valid(nv)) { params->success = FALSE; return; }

    native = p_gtk_widget_get_native(nv->window);
    surface = native ? p_gtk_native_get_surface(native) : NULL;
    if (!surface) { params->success = FALSE; return; }
    params->screen_bounds.left = 0;
    params->screen_bounds.top = 0;
    params->screen_bounds.right = p_gdk_surface_get_width(surface);
    params->screen_bounds.bottom = p_gdk_surface_get_height(surface);
    params->success = TRUE;
}

static NTSTATUS unix_get_window_geometry_impl(void *args)
{
    struct get_window_geometry_params *params = args;
    struct native_webview *nv = (struct native_webview *)(ULONG_PTR)params->handle;

    params->success = FALSE;
    if (!nv) return STATUS_INVALID_HANDLE;
    if (!gtk_thread_invoke_sync(get_window_geometry_on_gtk_thread, params))
        WARN("gtk_thread_invoke_sync failed -- GTK thread not running\n");
    return STATUS_SUCCESS;
}

const unixlib_entry_t __wine_unix_call_funcs[] =
{
    unix_init_impl,
    unix_create_webview_impl,
    unix_destroy_webview_impl,
    unix_navigate_and_wait_impl,
    unix_delete_all_cookies_impl,
    unix_count_cookies_impl,
    unix_get_cookies_impl,
    unix_get_window_visible_impl,
    unix_sync_window_geometry_impl,
    unix_get_window_geometry_impl,
};
