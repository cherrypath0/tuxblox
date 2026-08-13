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

    /* out */
    BOOL success; /* FALSE only on a real failure (invalid handle, or the
                    * async WebKit call never completed within the bounded
                    * wait) -- mirrors struct count_cookies_params's own
                    * "only trust the result if the callback actually ran"
                    * convention. */
    UINT32 count; /* number of `cookies` entries actually filled; capped at
                    * WEBVIEW2LOADER_MAX_COOKIES -- see that constant's own
                    * comment above for why a hard cap exists at all. */
    struct unix_cookie cookies[WEBVIEW2LOADER_MAX_COOKIES];
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
    /* Further tasks append further entries below this line -- always
     * appending, never reordering, since the enum's integer values are
     * the unix-call dispatch table's indices (see __wine_unix_call_funcs
     * in unixlib.c). */
};

#define WEBVIEW2LOADER_UNIX_CALL(func, params) WINE_UNIX_CALL(unix_ ## func, params)

#endif /* __WINE_WEBVIEW2LOADER_UNIXLIB_H */
