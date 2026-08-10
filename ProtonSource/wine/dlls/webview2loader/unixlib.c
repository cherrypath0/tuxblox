#if 0
#pragma makedep unix
#endif

#include "config.h"

#include <dlfcn.h>
#include <limits.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <ntstatus.h>
#define WIN32_NO_STATUS
#include <windef.h>
#include <winbase.h>
#include <winternl.h>

#include <wine/debug.h>
#include <wine/unixlib.h>

#include "unixlib.h"

WINE_DEFAULT_DEBUG_CHANNEL(webview2loader);

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
typedef gboolean (*GSourceFunc)(void *user_data);
typedef void (*GCallback)(void);

extern void g_free(void *mem);
extern GMainContext *g_main_context_default(void);
extern void g_main_context_invoke_full(GMainContext *context, int priority, GSourceFunc function,
                                        void *data, void (*notify)(void *data));
extern GMainLoop *g_main_loop_new(GMainContext *context, gboolean is_running);
extern void g_main_loop_run(GMainLoop *loop);
extern void g_main_loop_quit(GMainLoop *loop);

extern void g_object_unref(void *object);
/* g_signal_connect_data lives in libgobject-2.0.so, not libglib-2.0.so --
 * the GObject signal system is part of GObject, not core GLib (confirmed
 * via `nm -D libgobject-2.0.so.0* | grep g_signal_connect_data` against the
 * committed bundle in Task 4 Step 1; it is NOT exported by libglib-2.0). */
extern unsigned long g_signal_connect_data(void *instance, const char *detailed_signal,
                                            GCallback c_handler, void *data,
                                            void (*destroy_data)(void *data, void *closure),
                                            int connect_flags);

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
extern void gtk_window_present(GtkWindow *window);
extern void gtk_widget_show(GtkWidget *widget);

extern GtkWidget *webkit_web_view_new(void);
extern void webkit_web_view_load_uri(WebKitWebView *web_view, const char *uri);
extern const char *webkit_web_view_get_uri(WebKitWebView *web_view);
extern WebKitNetworkSession *webkit_web_view_get_network_session(WebKitWebView *web_view);
extern WebKitCookieManager *webkit_network_session_get_cookie_manager(WebKitNetworkSession *session);

#define GLIB_FUNCS \
    DO_FUNC(g_main_context_default); \
    DO_FUNC(g_main_context_invoke_full); \
    DO_FUNC(g_main_loop_new); \
    DO_FUNC(g_main_loop_run); \
    DO_FUNC(g_main_loop_quit); \
    DO_FUNC(g_free)

#define GOBJECT_FUNCS \
    DO_FUNC(g_object_unref); \
    DO_FUNC(g_signal_connect_data)

#define GTK_FUNCS \
    DO_FUNC(gtk_init_check); \
    DO_FUNC(gtk_window_new); \
    DO_FUNC(gtk_window_set_child); \
    DO_FUNC(gtk_window_present); \
    DO_FUNC(gtk_widget_show)

/* NB: no webkit_cookie_manager_delete_all_cookies here -- Task 4 Step 1's
 * `nm -D ... | grep -i cookie` against the real committed bundle found no
 * such symbol; WebKitGTK 2.52.5's WebKitCookieManager only exposes
 * webkit_cookie_manager_delete_cookie (single cookie, needs a
 * WebKitCookieManager + a URI, both async) and webkit_cookie_manager_
 * replace_cookies (bulk, also async, taking a GCancellable/
 * GAsyncReadyCallback/GList of cookies). "Delete all" isn't a single call
 * in this API -- Task 8 has to actually design the real shape (most likely
 * replace_cookies with an empty GList, or get_all_cookies_finish + a
 * delete_cookie loop), which needs GList/GCancellable/GAsyncReadyCallback/
 * GAsyncResult/GError hand-declarations this task has no reason to
 * speculatively add. Deliberately left for Task 8 to add for real rather
 * than guessed here.
 */
#define WEBKIT_FUNCS \
    DO_FUNC(webkit_web_view_new); \
    DO_FUNC(webkit_web_view_load_uri); \
    DO_FUNC(webkit_web_view_get_uri); \
    DO_FUNC(webkit_web_view_get_network_session); \
    DO_FUNC(webkit_network_session_get_cookie_manager)

#define DO_FUNC(f) typeof(f) *p_##f
GLIB_FUNCS; GOBJECT_FUNCS; GTK_FUNCS; WEBKIT_FUNCS;
#undef DO_FUNC

static void *glib_handle, *gobject_handle, *gtk_handle, *webkit_handle;

static void *load_one(const char *dir, const char *relpath)
{
    char path[PATH_MAX];
    void *h;

    snprintf(path, sizeof(path), "%s/%s", dir, relpath);
    h = dlopen(path, RTLD_NOW);
    if (!h) WARN("failed to load %s: %s\n", path, dlerror());
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
    const char *dir = getenv("TUXBLOX_WEBVIEW_DIR");
    if (!dir || !dir[0])
    {
        WARN("TUXBLOX_WEBVIEW_DIR not set -- not running under this repo's proton\n");
        return FALSE;
    }

    if (!(glib_handle = load_one(dir, "lib/x86_64-linux-gnu/libglib-2.0.so.0"))) return FALSE;
    if (!(gobject_handle = load_one(dir, "lib/x86_64-linux-gnu/libgobject-2.0.so.0"))) return FALSE;
    if (!(gtk_handle = load_one(dir, "lib/x86_64-linux-gnu/libgtk-4.so.1"))) return FALSE;
    if (!(webkit_handle = load_one(dir, "lib/libwebkitgtk-6.0.so.4"))) return FALSE;

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

    return TRUE;
}

/* WEBKIT_EXEC_PATH and friends -- the full env-var relocation contract
 * verified end-to-end in webkitgtk-bundle/README.md, derived here from the
 * single TUXBLOX_WEBVIEW_DIR value rather than being set piecemeal by
 * proton (see Task 2's rationale comment). setenv(), not a one-time cache:
 * safe to call before any dlopen of the bundle's own libraries, since
 * unlike LD_LIBRARY_PATH these are read by WebKitGTK itself via getenv()
 * at actual use time, not cached by ld.so at process start. */
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
     * unsuffixed name. */
    setenv("GST_PLUGIN_SYSTEM_PATH", path, 1);
    setenv("GST_PLUGIN_SYSTEM_PATH_1_0", path, 1);

    snprintf(path, sizeof(path), "%s/share", dir);
    setenv("XDG_DATA_DIRS", path, 1);

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

static void *gtk_thread_proc(void *arg)
{
    BOOL ok = p_gtk_init_check();

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

void gtk_thread_invoke_sync(void (*fn)(void *data), void *data)
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
        return;
    }

    p_g_main_context_invoke_full(p_g_main_context_default(), 0, sync_invoke_trampoline, &ctx, NULL);

    pthread_mutex_lock(&ctx.lock);
    while (!ctx.done) pthread_cond_wait(&ctx.cond, &ctx.lock);
    pthread_mutex_unlock(&ctx.lock);

    pthread_mutex_destroy(&ctx.lock);
    pthread_cond_destroy(&ctx.cond);
}

/* unix_init_impl must run its actual init work exactly once. Task 5 (a
 * later task) calls webview2loader_unix_init() on every environment
 * creation, which can happen repeatedly and from multiple PE threads. Doing
 * the env-var setenv()s / library dlopen()s / thread-creation more than
 * once would: (a) race setenv() against concurrent getenv() calls
 * elsewhere in this heavily multithreaded process -- a real glibc
 * environ-realloc crash risk; (b) leak a dlopen() refcount per call with no
 * matching dlclose(); (c) let two PE threads calling unix_init concurrently
 * data-race on the p_* function-pointer globals. `initialized`/`init_ok`
 * cache the one-time result, guarded by the same lock already used for the
 * GTK-thread-ready handshake below. */
static BOOL initialized;
static BOOL init_ok;

static NTSTATUS unix_init_impl(void *args)
{
    struct init_params *params = args;
    const char *dir;

    pthread_mutex_lock(&gtk_thread_ready_lock);

    if (initialized)
    {
        params->success = init_ok;
        pthread_mutex_unlock(&gtk_thread_ready_lock);
        return init_ok ? STATUS_SUCCESS : STATUS_NOT_SUPPORTED;
    }

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
    initialized = TRUE;
    pthread_mutex_unlock(&gtk_thread_ready_lock);
    return init_ok ? STATUS_SUCCESS : STATUS_NOT_SUPPORTED;
}

const unixlib_entry_t __wine_unix_call_funcs[] =
{
    unix_init_impl,
};
