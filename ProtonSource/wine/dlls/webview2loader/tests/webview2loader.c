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
    HANDLE done_event;
    HRESULT result_hr;
    ICoreWebView2Environment *result_env;
};

static HRESULT WINAPI test_env_handler_QI(ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler *iface,
                                           REFIID riid, void **ppv)
{ *ppv = iface; return S_OK; }
static ULONG WINAPI test_env_handler_AddRef(ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler *iface)
{ return 2; }
static ULONG WINAPI test_env_handler_Release(ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler *iface)
{ return 1; }
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
    struct test_env_handler handler = { &test_env_handler_vtbl };

    ok(mod != NULL, "LoadLibraryA failed\n");
    if (!mod) return;
    pCreate = (void *)GetProcAddress(mod, "CreateCoreWebView2EnvironmentWithOptions");
    ok(pCreate != NULL, "missing export\n");

    handler.done_event = CreateEventW(NULL, FALSE, FALSE, NULL);
    pCreate(NULL, NULL, NULL, &handler);

    ok(WaitForSingleObject(handler.done_event, 10000) == WAIT_OBJECT_0, "environment creation timed out\n");
    /* TUXBLOX_WEBVIEW_DIR won't be set when this test runs outside
     * launch.sh/proton -- expect failure there, success under proton. This
     * asserts the ASYNC PLUMBING works (handler fires with SOME result),
     * not that the bundle is necessarily loadable in this exact process. */
    ok(handler.result_hr == S_OK || handler.result_hr == E_FAIL,
       "unexpected hr %#lx\n", handler.result_hr);
    if (handler.result_env) ICoreWebView2Environment_Release(handler.result_env);

    CloseHandle(handler.done_event);
    FreeLibrary(mod);
}

START_TEST(webview2loader)
{
    test_module_loads();
    test_create_environment();
}
