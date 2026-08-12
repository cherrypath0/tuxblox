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

enum webview2loader_unix_funcs
{
    unix_init,
    unix_create_webview,
    unix_destroy_webview,
    unix_navigate_and_wait,
    unix_delete_all_cookies,
    unix_count_cookies,
    /* Tasks 7-8 append further entries below this line -- always
     * appending, never reordering, since the enum's integer values are
     * the unix-call dispatch table's indices (see __wine_unix_call_funcs
     * in unixlib.c). */
};

#define WEBVIEW2LOADER_UNIX_CALL(func, params) WINE_UNIX_CALL(unix_ ## func, params)

#endif /* __WINE_WEBVIEW2LOADER_UNIXLIB_H */
