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

START_TEST(webview2loader)
{
    test_module_loads();
    test_create_environment();
}
