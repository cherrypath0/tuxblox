#include <stdarg.h>

#include <windef.h>
#include <winbase.h>
#include <wine/unixlib.h>

#include "unixlib.h"
#include "filechooser_portal.h"

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
