/* Standalone, human-driven harness for webview2loader.dll -- design spec's
 * "Testing Strategy" item 2: drives the real loader end-to-end (environment
 * -> controller -> navigate -> NavigationCompleted) against a local test
 * page, independent of Roblox. Not part of `make test` (it needs a real,
 * visible display and stays open for manual visual confirmation instead of
 * exiting immediately) -- run by hand:
 *
 *   TUXBLOX_WEBVIEW_DIR=<dist>/lib/tuxblox-webview/ \
 *     wine webview2_e2e.exe <path-to-e2e_fixture.html>
 *
 * Expected: a GTK window appears showing the fixture's green page and
 * "webview2loader e2e: rendering OK", and this program prints
 * "NavigationCompleted: IsSuccess=1", "MoveFocus: S_OK" and
 * "WindowCloseRequested: fired", then keeps the window up for ten seconds of
 * visual inspection (WV2L_E2E_SECONDS changes that). Everything but the drag
 * behaviour it decides on its own, so it doubles as a plain pass/fail check. */
#include <stdio.h>
#include <windows.h>

#include "../webview2loader_private.h"

static HANDLE nav_done;
static BOOL nav_success;

typedef struct { ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandlerVtbl *vtbl; ICoreWebView2Environment *env; HANDLE ev; } env_handler_t;
static HRESULT WINAPI env_QI(ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler *i, REFIID r, void **p) { *p = i; return S_OK; }
static ULONG WINAPI env_AddRef(ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler *i) { return 2; }
static ULONG WINAPI env_Release(ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler *i) { return 1; }
static HRESULT WINAPI env_Invoke(ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler *i, HRESULT hr, ICoreWebView2Environment *result)
{ env_handler_t *h = (env_handler_t *)i; h->env = result; if (result) ICoreWebView2Environment_AddRef(result); printf("environment created: hr=%#lx\n", hr); SetEvent(h->ev); return S_OK; }
static ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandlerVtbl env_vtbl = { env_QI, env_AddRef, env_Release, env_Invoke };

typedef struct { ICoreWebView2CreateCoreWebView2ControllerCompletedHandlerVtbl *vtbl; ICoreWebView2Controller *ctrl; HANDLE ev; } ctrl_handler_t;
static HRESULT WINAPI ctrl_QI(ICoreWebView2CreateCoreWebView2ControllerCompletedHandler *i, REFIID r, void **p) { *p = i; return S_OK; }
static ULONG WINAPI ctrl_AddRef(ICoreWebView2CreateCoreWebView2ControllerCompletedHandler *i) { return 2; }
static ULONG WINAPI ctrl_Release(ICoreWebView2CreateCoreWebView2ControllerCompletedHandler *i) { return 1; }
static HRESULT WINAPI ctrl_Invoke(ICoreWebView2CreateCoreWebView2ControllerCompletedHandler *i, HRESULT hr, ICoreWebView2Controller *result)
{ ctrl_handler_t *h = (ctrl_handler_t *)i; h->ctrl = result; if (result) ICoreWebView2Controller_AddRef(result); printf("controller created: hr=%#lx\n", hr); SetEvent(h->ev); return S_OK; }
static ICoreWebView2CreateCoreWebView2ControllerCompletedHandlerVtbl ctrl_vtbl = { ctrl_QI, ctrl_AddRef, ctrl_Release, ctrl_Invoke };

typedef struct { ICoreWebView2NavigationCompletedEventHandlerVtbl *vtbl; } nav_handler_t;
static HRESULT WINAPI nav_QI(ICoreWebView2NavigationCompletedEventHandler *i, REFIID r, void **p) { *p = i; return S_OK; }
static ULONG WINAPI nav_AddRef(ICoreWebView2NavigationCompletedEventHandler *i) { return 2; }
static ULONG WINAPI nav_Release(ICoreWebView2NavigationCompletedEventHandler *i) { return 1; }
static HRESULT WINAPI nav_Invoke(ICoreWebView2NavigationCompletedEventHandler *i, ICoreWebView2 *sender, ICoreWebView2NavigationCompletedEventArgs *args)
{
    ICoreWebView2NavigationCompletedEventArgs_get_IsSuccess(args, &nav_success);
    printf("NavigationCompleted: IsSuccess=%d\n", nav_success);
    SetEvent(nav_done);
    return S_OK;
}
static ICoreWebView2NavigationCompletedEventHandlerVtbl nav_vtbl = { nav_QI, nav_AddRef, nav_Release, nav_Invoke };

static HANDLE close_fired;
typedef struct { ICoreWebView2WindowCloseRequestedEventHandlerVtbl *vtbl; } close_handler_t;
static HRESULT WINAPI close_QI(ICoreWebView2WindowCloseRequestedEventHandler *i, REFIID r, void **p) { *p = i; return S_OK; }
static ULONG WINAPI close_AddRef(ICoreWebView2WindowCloseRequestedEventHandler *i) { return 2; }
static ULONG WINAPI close_Release(ICoreWebView2WindowCloseRequestedEventHandler *i) { return 1; }
static HRESULT WINAPI close_Invoke(ICoreWebView2WindowCloseRequestedEventHandler *i, ICoreWebView2 *sender, IUnknown *args)
{ printf("WindowCloseRequested: fired\n"); SetEvent(close_fired); return S_OK; }
static ICoreWebView2WindowCloseRequestedEventHandlerVtbl close_vtbl = { close_QI, close_AddRef, close_Release, close_Invoke };

static HANDLE script_done;
typedef struct { ICoreWebView2ExecuteScriptCompletedHandlerVtbl *vtbl; } script_handler_t;
static HRESULT WINAPI script_QI(ICoreWebView2ExecuteScriptCompletedHandler *i, REFIID r, void **p) { *p = i; return S_OK; }
static ULONG WINAPI script_AddRef(ICoreWebView2ExecuteScriptCompletedHandler *i) { return 2; }
static ULONG WINAPI script_Release(ICoreWebView2ExecuteScriptCompletedHandler *i) { return 1; }
static HRESULT WINAPI script_Invoke(ICoreWebView2ExecuteScriptCompletedHandler *i, HRESULT hr, LPCWSTR result)
{ printf("ExecuteScript completed: hr=%#lx\n", hr); SetEvent(script_done); return S_OK; }
static ICoreWebView2ExecuteScriptCompletedHandlerVtbl script_vtbl = { script_QI, script_AddRef, script_Release, script_Invoke };

/* Every completion handler this harness waits for is invoked on the thread that asked for the work,
 * through its message queue -- WebView2 is single-threaded-apartment and real hosts all run a message
 * loop. A plain WaitForSingleObject blocks that queue and the completion never arrives, so each wait
 * here pumps. */
static void pump_once(void)
{
    MSG msg;
    while (PeekMessageW(&msg, NULL, 0, 0, PM_REMOVE)) { TranslateMessage(&msg); DispatchMessageW(&msg); }
}

static BOOL wait_pumping(HANDLE h, DWORD timeout_ms)
{
    DWORD start = GetTickCount();

    for (;;)
    {
        DWORD elapsed = GetTickCount() - start;
        DWORD r = MsgWaitForMultipleObjects(1, &h, FALSE, timeout_ms > elapsed ? timeout_ms - elapsed : 0,
                                             QS_ALLINPUT);
        if (r == WAIT_OBJECT_0) return TRUE;
        if (r != WAIT_OBJECT_0 + 1) return FALSE;
        pump_once();
    }
}

static void pump_for(DWORD ms)
{
    DWORD start = GetTickCount();

    while (GetTickCount() - start < ms)
    {
        MsgWaitForMultipleObjects(0, NULL, FALSE, 100, QS_ALLINPUT);
        pump_once();
    }
}

int main(int argc, char **argv)
{
    HMODULE mod;
    HRESULT (WINAPI *pCreate)(PCWSTR, PCWSTR, void *, void *);
    env_handler_t env_h = { &env_vtbl };
    ctrl_handler_t ctrl_h = { &ctrl_vtbl };
    nav_handler_t nav_h = { &nav_vtbl };
    close_handler_t close_h = { &close_vtbl };
    script_handler_t script_h = { &script_vtbl };
    BOOL close_ok, focus_ok;
    ICoreWebView2 *webview;
    WCHAR uri[MAX_PATH + 8] = L"file://";
    WNDCLASSA wc = { 0 };
    HWND parent;
    RECT bounds = { 20, 20, 620, 420 };

    if (argc < 2) { fprintf(stderr, "usage: %s <path-to-e2e_fixture.html>\n", argv[0]); return 1; }
    MultiByteToWideChar(CP_ACP, 0, argv[1], -1, uri + 7, MAX_PATH);

    /* Plan 3 Task 6: a real, visible, DRAGGABLE parent window -- previously
     * this harness passed NULL as parentWindow, so it never exercised any
     * of Plan 3's window-docking work (Tasks 1-5). WS_OVERLAPPEDWINDOW
     * gives it a real titlebar a human can drag, exercising the
     * WH_CALLWNDPROC hook (Task 5) as well as put_Bounds (Task 4). */
    wc.lpfnWndProc = DefWindowProcA;
    wc.hInstance = GetModuleHandleA(NULL);
    wc.lpszClassName = "tuxblox_webview2_e2e_parent";
    RegisterClassA(&wc);
    parent = CreateWindowExA(0, wc.lpszClassName, "webview2loader e2e -- drag me", WS_OVERLAPPEDWINDOW,
                              CW_USEDEFAULT, CW_USEDEFAULT, 700, 500, NULL, NULL, wc.hInstance, NULL);
    ShowWindow(parent, SW_SHOW);

    if (!(mod = LoadLibraryA("webview2loader.dll"))) { fprintf(stderr, "LoadLibrary failed: %lu\n", GetLastError()); return 1; }
    pCreate = (void *)GetProcAddress(mod, "CreateCoreWebView2EnvironmentWithOptions");

    env_h.ev = CreateEventW(NULL, FALSE, FALSE, NULL);
    pCreate(NULL, NULL, NULL, &env_h);
    wait_pumping(env_h.ev, 10000);
    if (!env_h.env) { fprintf(stderr, "environment creation failed\n"); return 1; }

    ctrl_h.ev = CreateEventW(NULL, FALSE, FALSE, NULL);
    ICoreWebView2Environment_CreateCoreWebView2Controller(env_h.env, parent, &ctrl_h);
    wait_pumping(ctrl_h.ev, 10000);
    if (!ctrl_h.ctrl) { fprintf(stderr, "controller creation failed\n"); return 1; }

    ICoreWebView2Controller_put_Bounds(ctrl_h.ctrl, bounds);
    ICoreWebView2Controller_put_IsVisible(ctrl_h.ctrl, TRUE);

    ICoreWebView2Controller_get_CoreWebView2(ctrl_h.ctrl, &webview);

    nav_done = CreateEventW(NULL, FALSE, FALSE, NULL);
    {
        UINT64 token;
        ICoreWebView2_add_NavigationCompleted(webview, &nav_h, &token);
    }
    close_fired = CreateEventW(NULL, FALSE, FALSE, NULL);
    {
        UINT64 token;
        ICoreWebView2_add_WindowCloseRequested(webview, &close_h, &token);
    }
    ICoreWebView2_Navigate(webview, uri);
    wait_pumping(nav_done, 30000);

    /* Both of these the harness can decide by itself, unlike the drag check below: a page asking to
     * close itself has to reach the program as WindowCloseRequested, and MoveFocus has to hand the
     * page the keyboard. Each used to be a silent E_NOTIMPL, so each is checked here. */
    focus_ok = ICoreWebView2Controller_MoveFocus(ctrl_h.ctrl, 0) == S_OK;
    printf("MoveFocus: %s\n", focus_ok ? "S_OK" : "FAILED");

    script_done = CreateEventW(NULL, FALSE, FALSE, NULL);
    ICoreWebView2_ExecuteScript(webview, L"window.close()", &script_h);
    wait_pumping(script_done, 10000);
    close_ok = wait_pumping(close_fired, 10000);
    printf("WindowCloseRequested: %s\n", close_ok ? "fired" : "NEVER FIRED");

    {
        /* Pumped rather than waiting on stdin: the window only tracks the one it is docked to while
         * this thread keeps dispatching, and a run with no human at it should finish on its own. */
        const char *secs_env = getenv("WV2L_E2E_SECONDS");
        DWORD secs = secs_env ? (DWORD)atoi(secs_env) : 10;

        printf("Drag/move/minimize/restore the 'webview2loader e2e -- drag me' window for the next "
               "%lu seconds -- the page's window should visually track it.\n", secs);
        pump_for(secs * 1000);
    }

    ICoreWebView2_Release(webview);
    ICoreWebView2Controller_Release(ctrl_h.ctrl);
    ICoreWebView2Environment_Release(env_h.env);
    DestroyWindow(parent);
    FreeLibrary(mod);
    return (nav_success && close_ok && focus_ok) ? 0 : 1;
}
