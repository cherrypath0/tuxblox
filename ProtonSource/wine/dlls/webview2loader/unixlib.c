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
extern void gtk_window_present(GtkWindow *window);
extern void gtk_widget_show(GtkWidget *widget);
/* Destroys the window and, since it's still set as the window's child at
 * that point, its WebKitWebView along with it -- GTK4's normal container
 * ownership model tears down children when their parent is destroyed, so
 * no separate g_object_unref() on the view is needed here. Confirmed real
 * (`nm -D libgtk-4.so.1*`) against the committed bundle. */
extern void gtk_window_destroy(GtkWindow *window);

extern GtkWidget *webkit_web_view_new(void);
extern void webkit_web_view_load_uri(WebKitWebView *web_view, const char *uri);
extern const char *webkit_web_view_get_uri(WebKitWebView *web_view);
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
    DO_FUNC(g_signal_connect_data); \
    DO_FUNC(g_signal_handler_disconnect)

#define GTK_FUNCS \
    DO_FUNC(gtk_init_check); \
    DO_FUNC(gtk_window_new); \
    DO_FUNC(gtk_window_set_child); \
    DO_FUNC(gtk_window_present); \
    DO_FUNC(gtk_widget_show); \
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

#define DO_FUNC(f) typeof(f) *p_##f
GLIB_FUNCS; GOBJECT_FUNCS; GTK_FUNCS; WEBKIT_FUNCS; SOUP_FUNCS; JAVASCRIPTCORE_FUNCS;
#undef DO_FUNC

static void *glib_handle, *gobject_handle, *gtk_handle, *webkit_handle, *soup_handle, *javascriptcore_handle;

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
};

/* Handles are just the native_webview*'s address, truncated to fit a
 * UINT64 -- unix-side memory, never dereferenced on the PE side, matching
 * the "opaque handle" shape other unixlib bridges in this tree use for
 * unix-owned objects PE code only ever passes back by value. */
static void create_webview_on_gtk_thread(void *data)
{
    struct native_webview **out = data;
    struct native_webview *nv = calloc(1, sizeof(*nv));

    nv->window = p_gtk_window_new();
    nv->view = p_webkit_web_view_new();
    p_gtk_window_set_child(nv->window, nv->view);
    p_gtk_widget_show(nv->window);

    *out = nv;
}

static NTSTATUS unix_create_webview_impl(void *args)
{
    struct create_webview_params *params = args;
    struct native_webview *nv = NULL;

    gtk_thread_invoke_sync(create_webview_on_gtk_thread, &nv);
    params->handle = (UINT64)(ULONG_PTR)nv;
    return nv ? STATUS_SUCCESS : STATUS_NOT_SUPPORTED;
}

/* Destroying nv->window tears down nv->view along with it (still its
 * child at this point -- see gtk_window_destroy's declaration comment
 * above), so this is the one native GTK/WebKit call needed per webview;
 * frees the small unix-side bookkeeping struct itself afterward. */
static void destroy_webview_on_gtk_thread(void *data)
{
    struct native_webview *nv = data;

    p_gtk_window_destroy(nv->window);
    free(nv);
}

static NTSTATUS unix_destroy_webview_impl(void *args)
{
    struct destroy_webview_params *params = args;
    struct native_webview *nv = (struct native_webview *)(ULONG_PTR)params->handle;

    if (!nv) return STATUS_SUCCESS;
    gtk_thread_invoke_sync(destroy_webview_on_gtk_thread, nv);
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

    gtk_thread_invoke_sync(navigate_on_gtk_thread, &ctx);

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
     * can't just live inside on_load_changed instead. */
    gtk_thread_invoke_sync(disconnect_load_changed_on_gtk_thread, &ctx);

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

    gtk_thread_invoke_sync(start_get_all_cookies_on_gtk_thread, ctx);

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
    gtk_thread_invoke_sync(start_count_cookies_on_gtk_thread, &sctx);

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
struct get_cookies_ctx
{
    struct native_webview *nv;
    struct get_cookies_params *params; /* PE-side's heap allocation; not owned, valid for our whole lifetime */
    char *uri_utf8; /* NULL => unfiltered (get_all_cookies); non-NULL => filtered (get_cookies) */
    pthread_mutex_t lock;
    pthread_cond_t cond;
    BOOL done;
    LONG refs; /* same "whichever side finishes last frees ctx" pattern as struct delete_cookies_ctx */
};

static void get_cookies_ctx_release(struct get_cookies_ctx *ctx)
{
    if (InterlockedDecrement(&ctx->refs)) return;
    pthread_mutex_destroy(&ctx->lock);
    pthread_cond_destroy(&ctx->cond);
    free(ctx->uri_utf8);
    free(ctx);
}

/* Fills one struct unix_cookie from a real SoupCookie*, truncating any
 * field that somehow exceeds its fixed buffer (see unixlib.h's own comment
 * on WEBVIEW2LOADER_MAX_COOKIES and the WEBVIEW2LOADER_COOKIE_*_MAX
 * constants for why fixed buffers are used at all) via ntdll_umbstowcs's
 * own documented safe-truncation behavior
 * ("Returns the number of characters converted, which may be less than the
 * entire source string") rather than risking a buffer overflow. */
static void fill_unix_cookie(struct unix_cookie *dst, SoupCookie *cookie)
{
    const char *name = p_soup_cookie_get_name(cookie);
    const char *value = p_soup_cookie_get_value(cookie);
    const char *domain = p_soup_cookie_get_domain(cookie);
    const char *path = p_soup_cookie_get_path(cookie);
    GDateTime *expires = p_soup_cookie_get_expires(cookie);
    ULONG n;

    n = name ? strlen(name) : 0;
    dst->name[ntdll_umbstowcs(name ? name : "", n, dst->name, WEBVIEW2LOADER_COOKIE_NAME_MAX - 1)] = 0;
    n = value ? strlen(value) : 0;
    dst->value[ntdll_umbstowcs(value ? value : "", n, dst->value, WEBVIEW2LOADER_COOKIE_VALUE_MAX - 1)] = 0;
    n = domain ? strlen(domain) : 0;
    dst->domain[ntdll_umbstowcs(domain ? domain : "", n, dst->domain, WEBVIEW2LOADER_COOKIE_DOMAIN_MAX - 1)] = 0;
    n = path ? strlen(path) : 0;
    dst->path[ntdll_umbstowcs(path ? path : "", n, dst->path, WEBVIEW2LOADER_COOKIE_PATH_MAX - 1)] = 0;

    /* NULL expires == session cookie, real libsoup semantics (soup-cookie.c's
     * own doc comment) and exactly real ICoreWebView2Cookie::IsSession's own
     * signal -- -1.0/TRUE is real WebView2's own documented sentinel for
     * "this is a session cookie" (learn.microsoft.com's real
     * ICoreWebView2Cookie reference: "The default is -1.0, which means
     * cookies are session cookies by default."), not a placeholder. */
    if (expires)
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
}

static void on_get_cookies_done(GObject *source, GAsyncResult *res, void *user_data)
{
    struct get_cookies_ctx *ctx = user_data;
    GList *cookies, *l;
    UINT32 n = 0;

    if (ctx->uri_utf8)
        cookies = p_webkit_cookie_manager_get_cookies_finish((WebKitCookieManager *)source, res, NULL);
    else
        cookies = p_webkit_cookie_manager_get_all_cookies_finish((WebKitCookieManager *)source, res, NULL);

    for (l = cookies; l && n < WEBVIEW2LOADER_MAX_COOKIES; l = l->next, n++)
        fill_unix_cookie(&ctx->params->cookies[n], l->data);
    /* If the real store somehow holds more than WEBVIEW2LOADER_MAX_COOKIES
     * cookies, this truncates rather than overflows -- surfaced via ERR so
     * it's visible in a trace if it ever actually matters (not expected for
     * anything Roblox's real login flow sets). */
    if (l) ERR("cookie store has more than %u cookies; result truncated\n", (unsigned)WEBVIEW2LOADER_MAX_COOKIES);

    ctx->params->count = n;
    ctx->params->success = TRUE;

    if (cookies) p_g_list_free_full(cookies, (GDestroyNotify)p_soup_cookie_free);

    pthread_mutex_lock(&ctx->lock);
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
    if (!(ctx = calloc(1, sizeof(*ctx)))) return STATUS_NO_MEMORY;

    /* NULL or empty uri => unfiltered, matching real GetCookies semantics
     * (see this function's own leading comment) -- wcs_to_utf8(NULL)
     * already returns NULL, so only the empty-string case needs an extra
     * check here. */
    uri_utf8 = wcs_to_utf8(params->uri);
    if (uri_utf8 && !uri_utf8[0]) { free(uri_utf8); uri_utf8 = NULL; }

    ctx->nv = nv;
    ctx->params = params;
    ctx->uri_utf8 = uri_utf8;
    ctx->refs = 2;
    pthread_mutex_init(&ctx->lock, NULL);
    pthread_cond_init(&ctx->cond, NULL);

    gtk_thread_invoke_sync(start_get_cookies_on_gtk_thread, ctx);

    /* Bounded wait, not indefinite -- same rationale and same 10s bound as
     * unix_delete_all_cookies_impl/unix_count_cookies_impl above (local
     * cookie-store enumeration, not a real network round-trip). */
    clock_gettime(CLOCK_REALTIME, &deadline);
    deadline.tv_sec += 10;

    pthread_mutex_lock(&ctx->lock);
    while (!ctx->done)
        if (pthread_cond_timedwait(&ctx->cond, &ctx->lock, &deadline) == ETIMEDOUT) break;
    pthread_mutex_unlock(&ctx->lock);

    get_cookies_ctx_release(ctx); /* this function's own ref -- safe unconditionally, same as
                                    * unix_delete_all_cookies_impl's own identical release call. */
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
};
