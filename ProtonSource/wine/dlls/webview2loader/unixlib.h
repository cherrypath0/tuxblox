#ifndef __WINE_WEBVIEW2LOADER_UNIXLIB_H
#define __WINE_WEBVIEW2LOADER_UNIXLIB_H

#include <windef.h>
#include <wine/unixlib.h>

struct init_params
{
    /* out */
    BOOL success;
};

struct create_webview_params
{
    /* in */
    BOOL is_message_only; /* Plan 3 Task 2: TRUE => never show the native
                            * GTK window (CookieManager's HWND_MESSAGE
                            * flow). Added by Task 1 alongside the
                            * parentWindow plumbing that computes it; not
                            * yet honored by unix_create_webview_impl until
                            * Task 2. */

    /* out */
    UINT64 handle; /* 0 on failure */
};

struct destroy_webview_params
{
    UINT64 handle; /* in; a no-op if 0 */
};

struct navigate_params
{
    UINT64 handle;
    const WCHAR *uri; /* PE-side owns this; unix side converts to UTF-8 and doesn't retain it past the call */

    /* out */
    BOOL is_success;
    UINT64 navigation_id;
};

struct delete_all_cookies_params
{
    UINT64 handle;
};

/* Test-support only: real cookie count via the same get_all_cookies
 * machinery unix_delete_all_cookies_impl uses, so a test can assert
 * DeleteAllCookies actually reduced the count to zero rather than only
 * checking its return code. Read-only (no way to inject/mutate cookie
 * state through this), unlike an earlier version of this file's now-removed
 * unix_add_test_cookie / __wine_test_webview2loader_add_cookie: that one
 * was reviewed and rejected -- it let any in-process code holding a live
 * ICoreWebView2* inject arbitrary cookies into the real cookie store via
 * webview2loader.dll's own PE export table (webview2loader.spec), with no
 * validation, entirely outside the (still-E_NOTIMPL) ICoreWebView2Cookie-
 * Manager COM surface. That's exactly the "capability a normal Windows
 * client wouldn't have" CONTRIBUTING.md says will not be merged, regardless
 * of intent, and it shipped in the SAME production DLL that replaces the
 * real WebView2Loader.dll (this Makefile.in has no test/production build
 * split -- there is only one webview2loader.dll). See
 * tests/cookie_test_server.py and test_delete_all_cookies's own comment for
 * how the test now adds a cookie instead: through a real HTTP response's
 * Set-Cookie header via the already-legitimate, already-implemented
 * Navigate() path -- a capability any real WebView2 client already has,
 * not a new DLL export. */
struct count_cookies_params
{
    UINT64 handle;

    /* out */
    UINT32 count;
};

/* Task 11 real-bug fix (GetCookies E_NOTIMPL blocking Studio login -- see
 * cookie_manager.c's cm_GetCookies): real cookie fields marshaled back
 * across the unix-call boundary as fixed-size WCHAR buffers rather than a
 * dynamically-sized allocation. This isn't a shortcut -- PE-side code
 * allocates via Wine's own allocator (ultimately ucrtbase/msvcrt-backed)
 * while this unixlib.so's own malloc/free calls are plain glibc, a
 * DIFFERENT heap; a pointer this .so malloc()'d can't safely be handed to
 * PE-side code to read past the single WINE_UNIX_CALL and then freed by
 * PE-side free()/CoTaskMemFree(). Every existing unix call in this file
 * sidesteps that by only ever marshaling fixed-size/POD data (see
 * struct navigate_params, struct count_cookies_params above) -- this
 * follows the same rule rather than inventing cross-heap ownership
 * transfer. The bounds below are generous for any cookie Roblox's real
 * login flow is expected to set (RFC 6265 requires user agents support at
 * least 4096 bytes for name+value alone), not arbitrary.
 *
 * struct get_cookies_params is heap-allocated by the PE-side caller
 * (cookie_manager.c's get_cookies_worker calloc()s it, never a stack
 * local) specifically because it embeds WEBVIEW2LOADER_MAX_COOKIES full
 * struct unix_cookie entries by value -- large enough (~1.2MB) that
 * putting it on a thread's stack would be a real overflow risk. */
#define WEBVIEW2LOADER_MAX_COOKIES 128
#define WEBVIEW2LOADER_COOKIE_NAME_MAX 256
#define WEBVIEW2LOADER_COOKIE_VALUE_MAX 4096
#define WEBVIEW2LOADER_COOKIE_DOMAIN_MAX 256
#define WEBVIEW2LOADER_COOKIE_PATH_MAX 512

struct unix_cookie
{
    WCHAR name[WEBVIEW2LOADER_COOKIE_NAME_MAX];
    WCHAR value[WEBVIEW2LOADER_COOKIE_VALUE_MAX];
    WCHAR domain[WEBVIEW2LOADER_COOKIE_DOMAIN_MAX];
    WCHAR path[WEBVIEW2LOADER_COOKIE_PATH_MAX];
    double expires;   /* -1.0 == session cookie, matching real
                        * ICoreWebView2Cookie::get_Expires's own documented
                        * default/session-cookie sentinel value (verified
                        * against learn.microsoft.com's real
                        * ICoreWebView2Cookie reference, not guessed). */
    INT32 same_site;  /* COREWEBVIEW2_COOKIE_SAME_SITE_KIND value --
                        * NONE=0/LAX=1/STRICT=2, numerically identical to
                        * libsoup's own SoupSameSitePolicy, verified via
                        * both real headers rather than assumed. */
    BOOL is_session;
    BOOL is_http_only;
    BOOL is_secure;
};

struct get_cookies_params
{
    UINT64 handle;
    const WCHAR *uri; /* PE-side owns this; NULL or empty = all cookies under
                        * the profile, matching real ICoreWebView2CookieManager::
                        * GetCookies's own documented uri semantics (verified
                        * against learn.microsoft.com's real
                        * ICoreWebView2CookieManager reference: "If uri is
                        * empty string or null, all cookies under the same
                        * profile are returned."). Unix side converts to
                        * UTF-8 and doesn't retain it past the call. */
    UINT32 offset; /* in; index of the first cookie to return. A store bigger
                     * than WEBVIEW2LOADER_MAX_COOKIES used to fail this call
                     * outright, which broke Studio's own cookie clearing on any
                     * profile with more than 128 cookies (it enumerates
                     * UNFILTERED, i.e. every cookie under the profile). The
                     * caller now walks the store one page at a time -- see
                     * cookie_manager.c's get_cookies_worker. */

    /* out */
    BOOL success; /* FALSE only on a real failure (invalid handle, or the
                    * async WebKit call never completed within the bounded
                    * wait) -- mirrors struct count_cookies_params's own
                    * "only trust the result if the callback actually ran"
                    * convention. */
    UINT32 count; /* number of `cookies` entries actually filled by THIS call,
                    * i.e. this page; at most WEBVIEW2LOADER_MAX_COOKIES. */
    UINT32 total; /* cookies in the whole store/filter, independent of offset --
                    * what the caller pages against. */
    struct unix_cookie cookies[WEBVIEW2LOADER_MAX_COOKIES];
};

/* Deleting one specific cookie. All four string fields of `cookie` are
 * load-bearing: libsoup matches a delete on domain (hash lookup) plus
 * name/value/path (soup_cookie_equal) -- see webkitgtk-bundle/host/navigate.c's
 * own cookies_delete_one comment for the verified source trail. Everything else
 * in struct unix_cookie is ignored and left unset. */
struct delete_cookie_params
{
    UINT64 handle;
    struct unix_cookie cookie;
};

struct get_window_visible_params
{
    UINT64 handle;

    /* out */
    BOOL visible;
};

struct sync_window_geometry_params
{
    UINT64 handle;
    RECT screen_bounds; /* absolute on-screen rect; PE side has already
                          * composed ClientToScreen with the client-relative
                          * Bounds (see controller.c's
                          * controller_push_geometry_to_native) */
    BOOL visible;
    /* Task 7 crash fix, round 15: real X11 window ID of the parent HWND's
     * own "whole window" (winex11.drv's own __wine_x11_whole_window
     * property, read by controller.c via GetPropA -- see that call site's
     * own comment), or 0 if unavailable (message-only controller, no
     * parent_window, prop not set/found). When non-zero, the unix side
     * reparents the native webview as a genuine X11 child of this window
     * instead of leaving it an independent floating top-level manually
     * repositioned to overlay the parent -- see sync_window_geometry_
     * on_gtk_thread's own comment for the full rationale (repo owner's
     * explicit, direct ask: "webview will be in the SAME window as the
     * one roblox launches, not separate"). */
    UINT64 parent_xid;

    /* out */
    BOOL success;
};

struct get_window_geometry_params
{
    UINT64 handle;

    /* out */
    BOOL success;
    RECT screen_bounds; /* current on-screen rect as GTK/X11 report it back --
                          * test-support only, mirrors sync_window_geometry's
                          * own field name */
};

/* --- Event delivery (host -> PE) ---
 *
 * Mirrors enum wv2l_event / struct wv2l_ev_* on the wire (see
 * webview2loader_ipc_protocol.h's own "Event channel" comment for why events
 * need a channel of their own rather than riding the request socket).
 *
 * The PE side consumes these by parking a dedicated thread in
 * unix_wait_event, which blocks until an event arrives -- Wine's unixlib
 * boundary is one-way (PE calls unix, never the reverse), so a blocking call
 * the PE side chooses to make is how the unix half "pushes" anything upward.
 * That thread must be dedicated: the call blocks for as long as nothing
 * happens, which is most of a session. */
#define WEBVIEW2LOADER_URI_MAX 2048

enum webview2loader_event_type
{
    WEBVIEW2LOADER_EVENT_NAVIGATION_STARTING,
};

struct wait_event_params
{
    /* out -- only meaningful when the call returns STATUS_SUCCESS */
    UINT32 type;   /* enum webview2loader_event_type */
    UINT64 handle; /* which webview, already generation-tagged for the PE side */
    WCHAR uri[WEBVIEW2LOADER_URI_MAX];
    BOOL is_redirect;
};

enum webview2loader_unix_funcs
{
    unix_init,
    unix_create_webview,
    unix_destroy_webview,
    unix_navigate_and_wait,
    unix_delete_all_cookies,
    unix_count_cookies,
    unix_get_cookies,
    unix_get_window_visible, /* Plan 3 Task 2: test-support only */
    unix_sync_window_geometry,
    unix_get_window_geometry, /* test-support only */
    unix_delete_cookie,
    unix_wait_event,
    /* Further tasks append further entries below this line -- always
     * appending, never reordering, since the enum's integer values are
     * the unix-call dispatch table's indices (see __wine_unix_call_funcs
     * in unixlib.c). */
};

#define WEBVIEW2LOADER_UNIX_CALL(func, params) WINE_UNIX_CALL(unix_ ## func, params)

#endif /* __WINE_WEBVIEW2LOADER_UNIXLIB_H */
