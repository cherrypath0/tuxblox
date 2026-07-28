#ifndef __WINE_COMDLG32_FILECHOOSER_PORTAL_H
#define __WINE_COMDLG32_FILECHOOSER_PORTAL_H

#include <windef.h>

#define FILECHOOSER_MAX_FILTERS 32

struct filechooser_filter
{
    const WCHAR *name;
    const WCHAR *pattern;
};

struct filechooser_request
{
    const WCHAR *title;
    const WCHAR *initial_dir;
    const WCHAR *initial_name;
    BOOL multiple;
    BOOL directory;
    UINT filter_count;
    struct filechooser_filter filters[FILECHOOSER_MAX_FILTERS];
};

struct filechooser_result
{
    WCHAR *buf;
    SIZE_T buf_len;
    BOOL cancelled;
    BOOL truncated;
    SIZE_T required_len;  /* only meaningful when truncated: WCHARs needed to
                              hold the full result, mirroring the hint Wine's
                              native FNERR_BUFFERTOOSMALL path already gives
                              via lpstrFile[0] */
};

/* Both return FALSE if the portal wasn't reachable (caller should fall back
 * to Wine's native dialog). Shared by comdlg32's GetOpenFileNameW/
 * GetSaveFileNameW and by itemdlg.c's IFileDialog implementation. */
BOOL comdlg32_portal_open_file(const struct filechooser_request *req, struct filechooser_result *res);
BOOL comdlg32_portal_save_file(const struct filechooser_request *req, struct filechooser_result *res);

#endif /* __WINE_COMDLG32_FILECHOOSER_PORTAL_H */
