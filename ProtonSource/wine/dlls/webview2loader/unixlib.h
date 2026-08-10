#ifndef __WINE_WEBVIEW2LOADER_UNIXLIB_H
#define __WINE_WEBVIEW2LOADER_UNIXLIB_H

#include <windef.h>
#include <wine/unixlib.h>

enum webview2loader_unix_funcs
{
    unix_init,
    /* Task 4 adds unix_init's real body. Later tasks append further
     * entries here (unix_create_webview, unix_navigate_and_wait,
     * unix_delete_all_cookies, ...) -- always appending, never
     * reordering, since the enum's integer values are the unix-call
     * dispatch table's indices (see __wine_unix_call_funcs in unixlib.c). */
};

#define WEBVIEW2LOADER_UNIX_CALL(func, params) WINE_UNIX_CALL(unix_ ## func, params)

#endif /* __WINE_WEBVIEW2LOADER_UNIXLIB_H */
