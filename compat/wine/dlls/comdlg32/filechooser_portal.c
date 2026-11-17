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

/* The chooser belongs to another program, so when it closes the desktop has no window of ours to
 * hand the focus back to. Wine then never sees the focus return and keeps sending keystrokes where
 * it last believed they went, which leaves the program taking the mouse but not the keyboard until
 * it is clicked. Windows returns the focus to the owner when a dialog closes, so do that here
 * rather than wait to be given it. */
static void restore_owner_focus(HWND owner, HWND foreground, HWND focus)
{
    HWND top_level = NULL;
    DWORD deadline = GetTickCount() + 1000;
    BOOL quit = FALSE;
    MSG msg;

    if (owner && IsWindow(owner) && !(top_level = GetAncestor(owner, GA_ROOT))) top_level = owner;
    /* The owner is what to come back to when there is one, and what had the foreground before the
     * chooser went up when there is not -- Roblox opens some of these with no owner at all, and
     * leaving it to the owner alone restored nothing in exactly that case. */
    if (!top_level && foreground && IsWindow(foreground)) top_level = foreground;

    /* The desktop hands the focus back itself, as a driver event this queue has not seen yet, and
     * whatever is set before that arrives is undone by it. So this waits for the window to be in
     * front again first. The hand-back lands on the top-level window while the keys belong to the
     * control inside it, which is why the program then looks focused and takes no typing at all. */
    while (!quit && top_level && GetForegroundWindow() != top_level && (int)(deadline - GetTickCount()) > 0)
    {
        MsgWaitForMultipleObjects(0, NULL, FALSE, 50, QS_ALLINPUT);
        while (!quit && PeekMessageW(&msg, 0, 0, 0, PM_REMOVE))
        {
            if (msg.message == WM_QUIT) { PostQuitMessage((int)msg.wParam); quit = TRUE; break; }
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }
    }

    if (top_level) SetForegroundWindow(top_level);
    if (focus && IsWindow(focus)) SetFocus(focus);
}

struct pumped_portal_call
{
    unsigned int  func;
    void         *params;
    NTSTATUS      status;
};

static DWORD WINAPI pumped_portal_thread(void *arg)
{
    struct pumped_portal_call *call = arg;

    call->status = WINE_UNIX_CALL(call->func, call->params);
    return 0;
}

/* The chooser is another program, and the person picking a file takes as long as they take. Waiting
 * for it on the calling thread stops that thread serving its own windows, so the program draws
 * nothing and answers nothing until the chooser closes: on Windows a file dialog leaves the program
 * running behind it. So the call waits on a thread of its own and messages carry on being
 * dispatched here. A thread that cannot be made falls back to waiting in place, as before. */
static NTSTATUS portal_call_pumping(unsigned int func, void *params)
{
    struct pumped_portal_call call = { func, params, 0 };
    BOOL quit = FALSE;
    HANDLE thread;
    MSG msg;

    thread = CreateThread(NULL, 0, pumped_portal_thread, &call, 0, NULL);
    if (!thread) return WINE_UNIX_CALL(func, params);

    while (!quit)
    {
        if (MsgWaitForMultipleObjects(1, &thread, FALSE, INFINITE, QS_ALLINPUT) == WAIT_OBJECT_0) break;
        while (!quit && PeekMessageW(&msg, 0, 0, 0, PM_REMOVE))
        {
            /* The quit belongs to the loop this one interrupted, so it goes back and the
             * dispatching stops; peeking it again would spin until the chooser closed. */
            if (msg.message == WM_QUIT)
            {
                PostQuitMessage((int)msg.wParam);
                quit = TRUE;
                break;
            }
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }
    }
    if (quit) WaitForSingleObject(thread, INFINITE);

    CloseHandle(thread);
    return call.status;
}

BOOL comdlg32_portal_open_file(const struct filechooser_request *req, struct filechooser_result *res)
{
    struct portal_open_save_params params = { 0 };
    const HWND foreground = GetForegroundWindow();
    const HWND focus = GetFocus();
    NTSTATUS status;

    params.parent_xid = owner_x11_window(req->owner);
    params.title = req->title;
    params.initial_dir = req->initial_dir;
    params.initial_name = req->initial_name;
    params.multiple = req->multiple;
    params.directory = req->directory;
    params.out_buf = res->buf;
    params.out_buf_len = res->buf_len;
    convert_filters(req, &params);

    status = portal_call_pumping(unix_portal_open_file, &params);
    restore_owner_focus(req->owner, foreground, focus);
    if (status) return FALSE;
    res->cancelled = params.cancelled;
    res->truncated = params.truncated;
    res->required_len = params.required_len;
    return TRUE;
}

BOOL comdlg32_portal_save_file(const struct filechooser_request *req, struct filechooser_result *res)
{
    struct portal_open_save_params params = { 0 };
    const HWND foreground = GetForegroundWindow();
    const HWND focus = GetFocus();
    NTSTATUS status;

    params.parent_xid = owner_x11_window(req->owner);
    params.title = req->title;
    params.initial_dir = req->initial_dir;
    params.initial_name = req->initial_name;
    params.out_buf = res->buf;
    params.out_buf_len = res->buf_len;
    convert_filters(req, &params);

    status = portal_call_pumping(unix_portal_save_file, &params);
    restore_owner_focus(req->owner, foreground, focus);
    if (status) return FALSE;
    res->cancelled = params.cancelled;
    res->truncated = params.truncated;
    res->required_len = params.required_len;
    return TRUE;
}
