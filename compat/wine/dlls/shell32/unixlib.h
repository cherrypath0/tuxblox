#ifndef __WINE_SHELL32_UNIXLIB_H
#define __WINE_SHELL32_UNIXLIB_H

#include <windef.h>
#include <wine/unixlib.h>

struct portal_pick_folder_params
{
    const WCHAR *title;        /* dialog title, nul-terminated */
    const WCHAR *initial_dir;  /* may be NULL */
    WCHAR *out_path;           /* caller-allocated result buffer */
    SIZE_T out_path_len;       /* capacity of out_path, in WCHARs */
    BOOL cancelled;            /* out: TRUE if user cancelled (or nothing was picked) */
};

enum shell32_unix_funcs
{
    unix_portal_pick_folder,
};

#define SHELL32_UNIX_CALL(func, params) WINE_UNIX_CALL(unix_ ## func, params)

#endif /* __WINE_SHELL32_UNIXLIB_H */
