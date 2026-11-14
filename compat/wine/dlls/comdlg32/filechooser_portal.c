#include <stdarg.h>

#include <windef.h>
#include <winbase.h>
#include <winuser.h>
#include <wine/unixlib.h>

#include "unixlib.h"
#include "filechooser_portal.h"

/* The owner's real X11 window, which the portal needs to make the dialog belong to it -- without
 * one the dialog is parentless, so the window manager has no reason to give the focus back to
 * Roblox when it closes. Only a genuine top-level HWND carries the property, so a child owner is
 * walked up to its root first, the same way the webview host resolves it. 0 when there is none,
 * which is what a non-X11 session gives and what the portal reads as "no parent". */
static UINT64 owner_x11_window(HWND owner)
{
    HWND top_level;

    if (!owner) return 0;
    if (!(top_level = GetAncestor(owner, GA_ROOT))) top_level = owner;
    return (UINT64)(ULONG_PTR)GetPropA(top_level, "__wine_x11_whole_window");
}

static void convert_filters(const struct filechooser_request *req, struct portal_open_save_params *params)
{
    UINT i;
    params->filter_count = min(req->filter_count, PORTAL_MAX_FILTERS);
    for (i = 0; i < params->filter_count; i++)
    {
        params->filters[i].name = req->filters[i].name;
        params->filters[i].pattern = req->filters[i].pattern;
    }
}

BOOL comdlg32_portal_open_file(const struct filechooser_request *req, struct filechooser_result *res)
{
    struct portal_open_save_params params = { 0 };

    params.parent_xid = owner_x11_window(req->owner);
    params.title = req->title;
    params.initial_dir = req->initial_dir;
    params.initial_name = req->initial_name;
    params.multiple = req->multiple;
    params.directory = req->directory;
    params.out_buf = res->buf;
    params.out_buf_len = res->buf_len;
    convert_filters(req, &params);

    if (COMDLG32_UNIX_CALL(portal_open_file, &params)) return FALSE;
    res->cancelled = params.cancelled;
    res->truncated = params.truncated;
    res->required_len = params.required_len;
    return TRUE;
}

BOOL comdlg32_portal_save_file(const struct filechooser_request *req, struct filechooser_result *res)
{
    struct portal_open_save_params params = { 0 };

    params.parent_xid = owner_x11_window(req->owner);
    params.title = req->title;
    params.initial_dir = req->initial_dir;
    params.initial_name = req->initial_name;
    params.out_buf = res->buf;
    params.out_buf_len = res->buf_len;
    convert_filters(req, &params);

    if (COMDLG32_UNIX_CALL(portal_save_file, &params)) return FALSE;
    res->cancelled = params.cancelled;
    res->truncated = params.truncated;
    res->required_len = params.required_len;
    return TRUE;
}
