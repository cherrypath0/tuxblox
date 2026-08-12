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

enum webview2loader_unix_funcs
{
    unix_init,
    unix_create_webview,
    unix_destroy_webview,
    unix_navigate_and_wait,
    /* Tasks 7-8 append further entries below this line -- always
     * appending, never reordering, since the enum's integer values are
     * the unix-call dispatch table's indices (see __wine_unix_call_funcs
     * in unixlib.c). */
};

#define WEBVIEW2LOADER_UNIX_CALL(func, params) WINE_UNIX_CALL(unix_ ## func, params)

#endif /* __WINE_WEBVIEW2LOADER_UNIXLIB_H */
