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
typedef void GObject;
typedef void GtkWidget;
typedef void GtkWindow;
typedef void GMainContext;
typedef void GMainLoop;
typedef void WebKitWebView;
typedef void WebKitNetworkSession;
typedef void WebKitCookieManager;
typedef int gboolean;
typedef unsigned int guint;
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

extern void gtk_init(void);
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
    DO_FUNC(gtk_init); \
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
    setenv("GST_PLUGIN_SYSTEM_PATH", path, 1);

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

static void *gtk_thread_proc(void *arg)
{
    p_gtk_init();
    gtk_main_loop = p_g_main_loop_new(NULL, 0);

    pthread_mutex_lock(&gtk_thread_ready_lock);
    gtk_thread_ready = TRUE;
    pthread_cond_signal(&gtk_thread_ready_cond);
    pthread_mutex_unlock(&gtk_thread_ready_lock);

    p_g_main_loop_run(gtk_main_loop);
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
    struct sync_invoke_ctx ctx = { fn, data, PTHREAD_MUTEX_INITIALIZER, PTHREAD_COND_INITIALIZER, FALSE };

    p_g_main_context_invoke_full(p_g_main_context_default(), 0, sync_invoke_trampoline, &ctx, NULL);

    pthread_mutex_lock(&ctx.lock);
    while (!ctx.done) pthread_cond_wait(&ctx.cond, &ctx.lock);
    pthread_mutex_unlock(&ctx.lock);
}

static NTSTATUS unix_init_impl(void *args)
{
    struct init_params *params = args;
    const char *dir;

    params->success = FALSE;

    if (!(dir = getenv("TUXBLOX_WEBVIEW_DIR")) || !dir[0])
        return STATUS_NOT_SUPPORTED;

    set_webkit_relocation_env(dir);
    if (!load_bundle_functions())
        return STATUS_NOT_SUPPORTED;

    pthread_mutex_lock(&gtk_thread_ready_lock);
    if (!gtk_thread_ready)
    {
        pthread_create(&gtk_thread, NULL, gtk_thread_proc, NULL);
        while (!gtk_thread_ready) pthread_cond_wait(&gtk_thread_ready_cond, &gtk_thread_ready_lock);
    }
    pthread_mutex_unlock(&gtk_thread_ready_lock);

    params->success = TRUE;
    return STATUS_SUCCESS;
}

const unixlib_entry_t __wine_unix_call_funcs[] =
{
    unix_init_impl,
};
