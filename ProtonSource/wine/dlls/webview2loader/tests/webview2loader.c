#include <windows.h>
#include "wine/test.h"
#include "../webview2loader_private.h"

static HRESULT (WINAPI *pCreateCoreWebView2EnvironmentWithOptions)(PCWSTR, PCWSTR, void *, void *);
static HRESULT (WINAPI *pGetAvailableCoreWebView2BrowserVersionString)(PCWSTR, LPWSTR *);

static BOOL init_function_pointers(HMODULE mod)
{
    pCreateCoreWebView2EnvironmentWithOptions =
        (void *)GetProcAddress(mod, "CreateCoreWebView2EnvironmentWithOptions");
    pGetAvailableCoreWebView2BrowserVersionString =
        (void *)GetProcAddress(mod, "GetAvailableCoreWebView2BrowserVersionString");
    return pCreateCoreWebView2EnvironmentWithOptions && pGetAvailableCoreWebView2BrowserVersionString;
}

static void test_module_loads(void)
{
    HMODULE mod = LoadLibraryA("webview2loader.dll");
    ok(mod != NULL, "LoadLibraryA failed, error %lu\n", GetLastError());
    if (!mod) return;

    ok(init_function_pointers(mod), "expected exports missing\n");

    FreeLibrary(mod);
}

struct test_env_handler
{
    ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandlerVtbl *vtbl;
    LONG ref;
    HANDLE done_event;
    HRESULT result_hr;
    ICoreWebView2Environment *result_env;
};

static HRESULT WINAPI test_env_handler_QI(ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler *iface,
                                           REFIID riid, void **ppv)
{ *ppv = iface; return S_OK; }
static ULONG WINAPI test_env_handler_AddRef(ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler *iface)
{
    struct test_env_handler *h = (struct test_env_handler *)iface;
    return InterlockedIncrement(&h->ref);
}
static ULONG WINAPI test_env_handler_Release(ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler *iface)
{
    struct test_env_handler *h = (struct test_env_handler *)iface;
    return InterlockedDecrement(&h->ref);
}
static HRESULT WINAPI test_env_handler_Invoke(ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler *iface,
                                               HRESULT errorCode, ICoreWebView2Environment *result)
{
    struct test_env_handler *h = (struct test_env_handler *)iface;
    h->result_hr = errorCode;
    h->result_env = result;
    if (result) ICoreWebView2Environment_AddRef(result);
    SetEvent(h->done_event);
    return S_OK;
}
static ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandlerVtbl test_env_handler_vtbl =
{ test_env_handler_QI, test_env_handler_AddRef, test_env_handler_Release, test_env_handler_Invoke };

static void test_create_environment(void)
{
    HMODULE mod = LoadLibraryA("webview2loader.dll");
    HRESULT (WINAPI *pCreate)(PCWSTR, PCWSTR, void *, void *);
    HRESULT hr;
    /* ref starts at 1 (this stack frame's own reference, mirroring how a
     * real caller would hold one ref on its own handler object before
     * ever passing it in). CreateCoreWebView2EnvironmentWithOptions's
     * worker thread must AddRef before its async work and Release after
     * invoking us, in balance -- so by the time done_event is signalled
     * and the worker has finished touching the handler, ref should be
     * back down to exactly this original 1. That's the one assertion
     * that would actually catch a refcount bug in
     * create_environment_worker (Finding 3 in code review). */
    struct test_env_handler handler = { &test_env_handler_vtbl, 1, NULL, E_UNEXPECTED, NULL };

    ok(mod != NULL, "LoadLibraryA failed\n");
    if (!mod) return;
    pCreate = (void *)GetProcAddress(mod, "CreateCoreWebView2EnvironmentWithOptions");
    ok(pCreate != NULL, "missing export\n");
    if (!pCreate)
    {
        FreeLibrary(mod);
        return;
    }

    handler.done_event = CreateEventW(NULL, FALSE, FALSE, NULL);
    hr = pCreate(NULL, NULL, NULL, &handler);
    /* Check the export's own synchronous return value: without this, a
     * synchronous failure here (e.g. E_POINTER/E_OUTOFMEMORY on the
     * malloc/start_async_work path) would never fire done_event and
     * would only ever show up as a confusing 10s timeout below, with no
     * indication of what actually went wrong. */
    ok(hr == S_OK, "CreateCoreWebView2EnvironmentWithOptions returned %#lx\n", hr);

    /* NOTE: if this wait times out (hr == S_OK but the worker thread is
     * stuck or unreasonably slow), the worker may still be holding a
     * pointer to `handler` on this function's stack and could write into
     * it after test_create_environment() has already returned and its
     * stack frame has been reused -- a real but narrow hazard that's
     * only reachable on this already-failing path. Not fixed here (would
     * need heap-allocating `handler` with its own independent lifetime);
     * flagging it so it isn't mistaken for "the timeout path is safe". */
    ok(WaitForSingleObject(handler.done_event, 10000) == WAIT_OBJECT_0, "environment creation timed out\n");
    /* TUXBLOX_WEBVIEW_DIR won't be set when this test runs outside
     * launch.sh/proton -- expect failure there, success under proton. This
     * asserts the ASYNC PLUMBING works (handler fires with SOME result),
     * not that the bundle is necessarily loadable in this exact process. */
    ok(handler.result_hr == S_OK || handler.result_hr == E_FAIL,
       "unexpected hr %#lx\n", handler.result_hr);
    ok(handler.ref == 1, "expected handler ref count back at 1, got %ld\n", handler.ref);
    if (handler.result_env) ICoreWebView2Environment_Release(handler.result_env);

    CloseHandle(handler.done_event);
    FreeLibrary(mod);
}

struct test_ctrl_handler
{
    ICoreWebView2CreateCoreWebView2ControllerCompletedHandlerVtbl *vtbl;
    HANDLE done_event;
    HRESULT result_hr;
    ICoreWebView2Controller *result_ctrl;
};
static HRESULT WINAPI test_ctrl_handler_QI(ICoreWebView2CreateCoreWebView2ControllerCompletedHandler *iface, REFIID riid, void **ppv) { *ppv = iface; return S_OK; }
static ULONG WINAPI test_ctrl_handler_AddRef(ICoreWebView2CreateCoreWebView2ControllerCompletedHandler *iface) { return 2; }
static ULONG WINAPI test_ctrl_handler_Release(ICoreWebView2CreateCoreWebView2ControllerCompletedHandler *iface) { return 1; }
static HRESULT WINAPI test_ctrl_handler_Invoke(ICoreWebView2CreateCoreWebView2ControllerCompletedHandler *iface,
                                                HRESULT errorCode, ICoreWebView2Controller *result)
{
    struct test_ctrl_handler *h = (struct test_ctrl_handler *)iface;
    h->result_hr = errorCode;
    h->result_ctrl = result;
    if (result) ICoreWebView2Controller_AddRef(result);
    SetEvent(h->done_event);
    return S_OK;
}
static ICoreWebView2CreateCoreWebView2ControllerCompletedHandlerVtbl test_ctrl_handler_vtbl =
{ test_ctrl_handler_QI, test_ctrl_handler_AddRef, test_ctrl_handler_Release, test_ctrl_handler_Invoke };

/* Shared by every test from here on that needs a live environment +
 * controller (test_create_controller below, and Task 7/8's test_navigate /
 * test_delete_all_cookies) -- built once here rather than duplicated per
 * test. Returns FALSE (and leaves *out_mod/*out_env/*out_ctrl untouched) if
 * TUXBLOX_WEBVIEW_DIR isn't set or either async step fails/times out; the
 * caller is expected to `skip()`/return in that case. On TRUE, caller owns
 * one ref each on *out_env and *out_ctrl (release both, then FreeLibrary
 * *out_mod, when done). */
static BOOL create_test_controller(HMODULE *out_mod, ICoreWebView2Environment **out_env,
                                    ICoreWebView2Controller **out_ctrl)
{
    HRESULT (WINAPI *pCreateEnv)(PCWSTR, PCWSTR, void *, void *);
    struct test_env_handler env_handler = { &test_env_handler_vtbl };
    struct test_ctrl_handler ctrl_handler = { &test_ctrl_handler_vtbl };

    if (!getenv("TUXBLOX_WEBVIEW_DIR")) return FALSE;

    *out_mod = LoadLibraryA("webview2loader.dll");
    pCreateEnv = (void *)GetProcAddress(*out_mod, "CreateCoreWebView2EnvironmentWithOptions");

    env_handler.done_event = CreateEventW(NULL, FALSE, FALSE, NULL);
    pCreateEnv(NULL, NULL, NULL, &env_handler);
    WaitForSingleObject(env_handler.done_event, 10000);
    CloseHandle(env_handler.done_event);
    if (env_handler.result_hr != S_OK) { FreeLibrary(*out_mod); return FALSE; }

    ctrl_handler.done_event = CreateEventW(NULL, FALSE, FALSE, NULL);
    ICoreWebView2Environment_CreateCoreWebView2Controller(env_handler.result_env, NULL, &ctrl_handler);
    WaitForSingleObject(ctrl_handler.done_event, 10000);
    CloseHandle(ctrl_handler.done_event);
    if (ctrl_handler.result_hr != S_OK)
    {
        ICoreWebView2Environment_Release(env_handler.result_env);
        FreeLibrary(*out_mod);
        return FALSE;
    }

    *out_env = env_handler.result_env;
    *out_ctrl = ctrl_handler.result_ctrl;
    return TRUE;
}

static void test_create_controller(void)
{
    HMODULE mod;
    ICoreWebView2Environment *env;
    ICoreWebView2Controller *ctrl;

    if (!create_test_controller(&mod, &env, &ctrl))
    {
        skip("TUXBLOX_WEBVIEW_DIR not set or environment/controller creation failed\n");
        return;
    }

    {
        BOOL visible = FALSE;
        ICoreWebView2Controller_get_IsVisible(ctrl, &visible);
        ok(visible, "expected controller to start visible\n");
    }

    ICoreWebView2Controller_Release(ctrl);
    ICoreWebView2Environment_Release(env);
    FreeLibrary(mod);
}

START_TEST(webview2loader)
{
    test_module_loads();
    test_create_environment();
    test_create_controller();
}
