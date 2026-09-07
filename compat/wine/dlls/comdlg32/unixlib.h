#ifndef __WINE_COMDLG32_UNIXLIB_H
#define __WINE_COMDLG32_UNIXLIB_H

#include <windef.h>
#include <wine/unixlib.h>

#define PORTAL_MAX_FILTERS 32

struct portal_filter
{
    const WCHAR *name;     /* e.g. L"Text files" */
    const WCHAR *pattern;  /* e.g. L"*.txt", single glob pattern */
};

struct portal_open_save_params
{
    const WCHAR *title;
    const WCHAR *initial_dir;    /* may be NULL */
    const WCHAR *initial_name;   /* may be NULL; default filename */
    BOOL multiple;               /* OpenFile only */
    BOOL directory;              /* OpenFile only: folder-picking mode */
    UINT filter_count;
    struct portal_filter filters[PORTAL_MAX_FILTERS];

    /* out */
    WCHAR *out_buf;       /* caller-allocated; on multi-select, dir then
                              nul-separated names, double-nul terminated,
                              same shape GetOpenFileNameW's OFN_EXPLORER
                              mode already produces */
    SIZE_T out_buf_len;   /* capacity of out_buf, in WCHARs */
    BOOL cancelled;
    BOOL truncated;       /* out_buf was too small */
    SIZE_T required_len;  /* only meaningful when truncated: WCHARs needed to
                              hold the full result (matches the "needed size
                              in first two bytes of lpstrFile" hint Wine's
                              native FNERR_BUFFERTOOSMALL path already gives
                              callers) */
};

enum comdlg32_unix_funcs
{
    unix_portal_open_file,
    unix_portal_save_file,
};

#define COMDLG32_UNIX_CALL(func, params) WINE_UNIX_CALL(unix_ ## func, params)

#endif /* __WINE_COMDLG32_UNIXLIB_H */
