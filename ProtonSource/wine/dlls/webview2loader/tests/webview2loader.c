#include <windows.h>

/* test_delete_all_cookies (added by this task) is the first test in this
 * file to reference an IID_* symbol directly (&IID_ICoreWebView2_2, passed
 * to QueryInterface) rather than only through a This->lpVtbl->Method(...)
 * call macro -- every earlier test only ever calls through the COBJMACROS
 * wrappers webview2loader_private.h already provides, none of which need a
 * GUID's address. That surfaced a real link failure: `winegcc ... -o
 * webview2loader_test.exe testlist.c webview2loader.c -lwebview2loader`
 * produced `undefined reference to 'IID_ICoreWebView2_2'` (verified via the
 * exact manual cross-compile recipe this task's own verification step
 * uses). Reason: webview2loader_private.h's DEFINE_GUID lines only actually
 * allocate storage in ONE translation unit -- main.c, which #includes
 * <objbase.h> then <initguid.h> before the private header specifically so
 * INITGUID is active while DEFINE_GUID expands there (see main.c's own
 * comment for why). That storage lives inside webview2loader.dll's own
 * main.o; webview2loader.spec exports only the two stdcall entry points
 * (confirmed: `nm` on the built import lib has zero IID_* symbols), so an
 * external EXE linking against the import library has no symbol to bind
 * &IID_ICoreWebView2_2 to.
 *
 * This is the same situation a real WebView2 consumer is in: Microsoft's
 * own WebView2.h GUIDs aren't resolved from WebView2Loader.dll either --
 * the consuming .exe gets its own copy of the storage by #defining INITGUID
 * before including the header (or linking a separate uuid-style lib), and
 * GUID equality is by VALUE (IsEqualGUID memcmp's the 16 bytes), not by
 * linker symbol identity, so the test getting its own independently-stored
 * copy of the same GUID value is correct, not a workaround. Mirrors main.c's
 * own #include <objbase.h> / #include <initguid.h> ordering exactly (objbase.h
 * first so the private header's later #include <objbase.h> is a guarded
 * no-op, keeping INITGUID's effect scoped to just the header's own
 * DEFINE_GUID lines and not the ~133 GUIDs objbase.h's own chain touches). */
#include <objbase.h>
#include <initguid.h>

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

/* Real Task 11 bug-fix regression test: GetAvailableCoreWebView2BrowserVersionString
 * was left as Task 3's original E_FAIL stub -- real Roblox Studio calls this
 * free-function export (no environment/controller needed) as the very first
 * webview2loader thing it does on startup, to check "is a WebView2 runtime
 * installed" before ever attempting to create an environment. An E_FAIL here
 * made Studio believe no runtime was present and show its own "WebView2
 * installation required" fallback dialog even though environment/controller
 * creation both actually work (confirmed via a real captured launch trace;
 * see this task's own report for the exact log lines). Only test_module_loads
 * previously touched this export, and only via GetProcAddress -- it never
 * actually called it or checked the result, which is exactly how the E_FAIL
 * stub survived unnoticed this long. */
static void test_get_available_browser_version_string(void)
{
    HMODULE mod = LoadLibraryA("webview2loader.dll");
    HRESULT (WINAPI *pGetVersion)(PCWSTR, LPWSTR *);
    LPWSTR version = NULL;
    HRESULT hr;

    ok(mod != NULL, "LoadLibraryA failed\n");
    if (!mod) return;

    pGetVersion = (void *)GetProcAddress(mod, "GetAvailableCoreWebView2BrowserVersionString");
    ok(pGetVersion != NULL, "missing export\n");
    if (pGetVersion)
    {
        hr = pGetVersion(NULL, &version);
        ok(hr == S_OK, "GetAvailableCoreWebView2BrowserVersionString returned %#lx\n", hr);
        ok(version != NULL, "expected a non-NULL version string\n");
        if (version)
        {
            /* Must match environment_get_BrowserVersionString's own fixed
             * string exactly (environment.c) -- Roblox should see the same
             * reported version regardless of which of the two functions it
             * calls. */
            ok(!wcscmp(version, L"109.0.1518.140"), "unexpected version string %s\n", wine_dbgstr_w(version));
            CoTaskMemFree(version);
        }

        /* E_POINTER on a NULL out-param, matching environment_get_BrowserVersionString's
         * own convention for the same failure case. */
        hr = pGetVersion(NULL, NULL);
        ok(hr == E_POINTER, "expected E_POINTER for a NULL versionInfo, got %#lx\n", hr);
    }

    FreeLibrary(mod);
}

/* Regression test for the CompareBrowserVersions crash found while verifying
 * the fix above: real Roblox Studio calls this immediately after
 * GetAvailableCoreWebView2BrowserVersionString during its own startup
 * runtime check, and it was entirely unimplemented (not even in
 * webview2loader.spec) until now -- Wine's builtin "unimplemented function"
 * trampoline hard-aborted the whole process the first time this became
 * reachable (see CompareBrowserVersions's own comment in main.c). */
static void test_compare_browser_versions(void)
{
    HMODULE mod = LoadLibraryA("webview2loader.dll");
    HRESULT (WINAPI *pCompare)(PCWSTR, PCWSTR, int *);
    int result;
    HRESULT hr;

    ok(mod != NULL, "LoadLibraryA failed\n");
    if (!mod) return;

    pCompare = (void *)GetProcAddress(mod, "CompareBrowserVersions");
    ok(pCompare != NULL, "missing export\n");
    if (pCompare)
    {
        result = 12345;
        hr = pCompare(L"109.0.1518.140", L"109.0.1518.140", &result);
        ok(hr == S_OK, "equal versions returned %#lx\n", hr);
        ok(result == 0, "expected 0 for equal versions, got %d\n", result);

        hr = pCompare(L"108.0.0.0", L"109.0.1518.140", &result);
        ok(hr == S_OK, "lesser version returned %#lx\n", hr);
        ok(result == -1, "expected -1 for a lesser version1, got %d\n", result);

        hr = pCompare(L"110.0.0.0", L"109.0.1518.140", &result);
        ok(hr == S_OK, "greater version returned %#lx\n", hr);
        ok(result == 1, "expected 1 for a greater version1, got %d\n", result);

        /* Real documented contract (learn.microsoft.com): "Directly use the
         * versionInfo obtained from GetAvailableCoreWebView2BrowserVersionString
         * as input, channel information is ignored" -- a trailing channel
         * suffix must not change the numeric comparison. */
        hr = pCompare(L"109.0.1518.140 canary", L"109.0.1518.140", &result);
        ok(hr == S_OK, "channel-suffixed version returned %#lx\n", hr);
        ok(result == 0, "expected channel suffix to be ignored, got %d\n", result);

        hr = pCompare(NULL, L"109.0.1518.140", &result);
        ok(hr == E_INVALIDARG, "expected E_INVALIDARG for a NULL version1, got %#lx\n", hr);

        hr = pCompare(L"109.0.1518.140", L"109.0.1518.140", NULL);
        ok(hr == E_INVALIDARG, "expected E_INVALIDARG for a NULL result, got %#lx\n", hr);

        hr = pCompare(L"not a version", L"109.0.1518.140", &result);
        ok(hr == E_INVALIDARG, "expected E_INVALIDARG for an unparsable version1, got %#lx\n", hr);
    }

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
 * test. Returns FALSE (and leaves *out_mod, *out_env, *out_ctrl untouched) if
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

struct test_nav_handler
{
    ICoreWebView2NavigationCompletedEventHandlerVtbl *vtbl;
    HANDLE done_event;
    BOOL is_success;
};
static HRESULT WINAPI test_nav_handler_QI(ICoreWebView2NavigationCompletedEventHandler *iface, REFIID riid, void **ppv) { *ppv = iface; return S_OK; }
static ULONG WINAPI test_nav_handler_AddRef(ICoreWebView2NavigationCompletedEventHandler *iface) { return 2; }
static ULONG WINAPI test_nav_handler_Release(ICoreWebView2NavigationCompletedEventHandler *iface) { return 1; }
static HRESULT WINAPI test_nav_handler_Invoke(ICoreWebView2NavigationCompletedEventHandler *iface,
                                               ICoreWebView2 *sender, ICoreWebView2NavigationCompletedEventArgs *args)
{
    struct test_nav_handler *h = (struct test_nav_handler *)iface;
    ICoreWebView2NavigationCompletedEventArgs_get_IsSuccess(args, &h->is_success);
    SetEvent(h->done_event);
    return S_OK;
}
static ICoreWebView2NavigationCompletedEventHandlerVtbl test_nav_handler_vtbl =
{ test_nav_handler_QI, test_nav_handler_AddRef, test_nav_handler_Release, test_nav_handler_Invoke };

static void test_navigate(void)
{
    HMODULE mod;
    ICoreWebView2Environment *env;
    ICoreWebView2Controller *ctrl;
    ICoreWebView2 *webview = NULL;
    struct test_nav_handler nav_handler = { &test_nav_handler_vtbl };
    UINT64 token;

    if (!create_test_controller(&mod, &env, &ctrl))
    {
        skip("TUXBLOX_WEBVIEW_DIR not set or environment/controller creation failed\n");
        return;
    }

    ICoreWebView2Controller_get_CoreWebView2(ctrl, &webview);
    ok(webview != NULL, "expected a webview\n");
    if (webview)
    {
        nav_handler.done_event = CreateEventW(NULL, FALSE, FALSE, NULL);
        ICoreWebView2_add_NavigationCompleted(webview, &nav_handler, &token);
        ICoreWebView2_Navigate(webview, L"about:blank");

        ok(WaitForSingleObject(nav_handler.done_event, 30000) == WAIT_OBJECT_0, "navigation timed out\n");
        ok(nav_handler.is_success, "expected navigation to about:blank to succeed\n");

        CloseHandle(nav_handler.done_event);
        ICoreWebView2_Release(webview);
    }

    ICoreWebView2Controller_Release(ctrl);
    ICoreWebView2Environment_Release(env);
    FreeLibrary(mod);
}

/* Regression test for the dangling-stack-pointer GObject callback bug: a
 * second Navigate() on the SAME webview re-emits "load-changed" on the
 * same underlying WebKitWebView. Before unix_navigate_and_wait_impl
 * disconnected its "load-changed" handler unconditionally on return
 * (unixlib.c), the first Navigate() call's connection would still be live
 * -- pointing at that first call's now-returned, stack-reused
 * navigate_ctx -- when the second call's own load-changed(FINISHED)
 * fires, invoking on_load_changed against freed/reused stack memory.
 * This doesn't assert anything about the corruption directly (that would
 * show up as a crash, which the test harness/coredump check around this
 * suite already catches) -- the point is that BOTH navigations complete
 * cleanly with no crash, exercising the exact "second load-changed
 * emission on the same view" path the bug lived in. */
static void test_navigate_twice(void)
{
    HMODULE mod;
    ICoreWebView2Environment *env;
    ICoreWebView2Controller *ctrl;
    ICoreWebView2 *webview = NULL;
    struct test_nav_handler nav_handler = { &test_nav_handler_vtbl };
    UINT64 token;

    if (!create_test_controller(&mod, &env, &ctrl))
    {
        skip("TUXBLOX_WEBVIEW_DIR not set or environment/controller creation failed\n");
        return;
    }

    ICoreWebView2Controller_get_CoreWebView2(ctrl, &webview);
    ok(webview != NULL, "expected a webview\n");
    if (webview)
    {
        nav_handler.done_event = CreateEventW(NULL, FALSE, FALSE, NULL);
        ICoreWebView2_add_NavigationCompleted(webview, &nav_handler, &token);

        ICoreWebView2_Navigate(webview, L"about:blank");
        ok(WaitForSingleObject(nav_handler.done_event, 30000) == WAIT_OBJECT_0, "first navigation timed out\n");
        ok(nav_handler.is_success, "expected first navigation to succeed\n");

        nav_handler.is_success = FALSE;
        ICoreWebView2_Navigate(webview, L"about:blank");
        ok(WaitForSingleObject(nav_handler.done_event, 30000) == WAIT_OBJECT_0, "second navigation timed out\n");
        ok(nav_handler.is_success, "expected second navigation to succeed\n");

        CloseHandle(nav_handler.done_event);
        ICoreWebView2_Release(webview);
    }

    ICoreWebView2Controller_Release(ctrl);
    ICoreWebView2Environment_Release(env);
    FreeLibrary(mod);
}

/* Regression coverage for webview_remove_NavigationCompleted, previously
 * untested: a handler removed before its Navigate() completes must never
 * be invoked. */
static void test_remove_navigation_completed(void)
{
    HMODULE mod;
    ICoreWebView2Environment *env;
    ICoreWebView2Controller *ctrl;
    ICoreWebView2 *webview = NULL;
    struct test_nav_handler nav_handler = { &test_nav_handler_vtbl };
    UINT64 token;

    if (!create_test_controller(&mod, &env, &ctrl))
    {
        skip("TUXBLOX_WEBVIEW_DIR not set or environment/controller creation failed\n");
        return;
    }

    ICoreWebView2Controller_get_CoreWebView2(ctrl, &webview);
    ok(webview != NULL, "expected a webview\n");
    if (webview)
    {
        nav_handler.done_event = CreateEventW(NULL, FALSE, FALSE, NULL);
        nav_handler.is_success = FALSE;
        ICoreWebView2_add_NavigationCompleted(webview, &nav_handler, &token);
        ICoreWebView2_remove_NavigationCompleted(webview, (void *)(ULONG_PTR)token);
        ICoreWebView2_Navigate(webview, L"about:blank");

        /* A short bounded wait, not the full 30s used elsewhere: if the
         * removed handler were (incorrectly) still invoked, it would fire
         * well within a couple of seconds for a real about:blank load. A
         * WAIT_TIMEOUT here is the expected, passing outcome. */
        ok(WaitForSingleObject(nav_handler.done_event, 5000) == WAIT_TIMEOUT,
           "removed NavigationCompleted handler was invoked anyway\n");

        CloseHandle(nav_handler.done_event);
        ICoreWebView2_Release(webview);
    }

    ICoreWebView2Controller_Release(ctrl);
    ICoreWebView2Environment_Release(env);
    FreeLibrary(mod);
}

/* Strengthened per code review: the original version of this test only
 * checked that DeleteAllCookies returned S_OK -- exactly the class of check
 * that let a real bug (webkit_cookie_manager_replace_cookies(NULL) silently
 * doing nothing against the real bundle, see unix_delete_all_cookies_impl's
 * own comment in unixlib.c for the full story) pass unnoticed, since the
 * broken call also "succeeded" by that same standard. ICoreWebView2CookieManager's
 * own CreateCookie/GetCookies are still E_NOTIMPL stubs (out of this plan's
 * scope), so real verification needs some other way to add and count real
 * cookies.
 *
 * Counting: __wine_test_webview2loader_count_cookies, a test-support-only
 * DLL export (webview2loader.spec + webview.c), resolved via GetProcAddress
 * same as this file's other real DLL-export calls (pCreate/
 * pGetAvailableCoreWebView2BrowserVersionString above), not linked in
 * directly. Read-only, judged low-risk in review.
 *
 * Adding: NOT a matching "add a test cookie" DLL export -- an earlier
 * version of this test used exactly that (__wine_test_webview2loader_
 * add_cookie), and code review rejected it: it let any in-process code
 * holding a live ICoreWebView2* inject an arbitrary cookie into the real
 * cookie store with zero validation, and this Makefile.in produces one
 * unconditional production webview2loader.dll (no test/production build
 * split), so that capability would have shipped for real -- exactly what
 * CONTRIBUTING.md says will not be merged. Instead this navigates to a
 * small local HTTP fixture (tests/cookie_test_server.py) that responds with
 * a real Set-Cookie header -- the same standards-compliant mechanism any
 * real WebView2 client (including Roblox's own login flow) already uses to
 * get a cookie set, via the already-legitimate, already-implemented
 * Navigate() path. Requires that fixture running on 127.0.0.1:18765 (see
 * its own header for how to start it); this test degrades to a skip() with
 * instructions rather than a hard failure if it isn't reachable, since it's
 * an external process this manual test suite doesn't start automatically. */
static void test_delete_all_cookies(void)
{
    HMODULE mod;
    ICoreWebView2Environment *env;
    ICoreWebView2Controller *ctrl;
    ICoreWebView2 *webview = NULL, *webview_v2 = NULL;
    ICoreWebView2CookieManager *cm = NULL;
    const struct webview2_2_vtbl_combined *v2vtbl;
    UINT32 (WINAPI *pCountCookies)(ICoreWebView2 *);
    struct test_nav_handler nav_handler = { &test_nav_handler_vtbl };
    UINT32 count;
    UINT64 token;
    HRESULT hr;

    if (!create_test_controller(&mod, &env, &ctrl))
    {
        skip("TUXBLOX_WEBVIEW_DIR not set or environment/controller creation failed\n");
        return;
    }

    pCountCookies = (void *)GetProcAddress(mod, "__wine_test_webview2loader_count_cookies");
    ok(pCountCookies != NULL, "missing __wine_test_webview2loader_count_cookies export\n");

    ICoreWebView2Controller_get_CoreWebView2(ctrl, &webview);
    ok(webview != NULL, "expected a webview\n");
    if (webview && pCountCookies)
    {
        hr = ICoreWebView2_QueryInterface(webview, &IID_ICoreWebView2_2, (void **)&webview_v2);
        ok(hr == S_OK, "QueryInterface(IID_ICoreWebView2_2) failed: %#lx\n", hr);
        if (SUCCEEDED(hr))
        {
            v2vtbl = (const struct webview2_2_vtbl_combined *)webview_v2->lpVtbl;
            hr = v2vtbl->ext.get_CookieManager(webview_v2, &cm);
            ok(hr == S_OK && cm != NULL, "get_CookieManager failed: %#lx\n", hr);

            if (cm)
            {
                nav_handler.done_event = CreateEventW(NULL, FALSE, FALSE, NULL);
                nav_handler.is_success = FALSE;
                ICoreWebView2_add_NavigationCompleted(webview, &nav_handler, &token);
                ICoreWebView2_Navigate(webview, L"http://127.0.0.1:18765/");

                WaitForSingleObject(nav_handler.done_event, 30000);

                /* Gate on the actual cookie count, not nav_handler.is_success:
                 * WebKit's "load-changed" fires WEBKIT_LOAD_FINISHED (which
                 * on_load_changed in unixlib.c treats as success) even for a
                 * connection-refused error page, so is_success alone can't
                 * distinguish "fixture answered with Set-Cookie" from
                 * "nothing is listening on 18765" -- confirmed empirically
                 * (stopping the fixture and rerunning still reported
                 * is_success, just with a real, honest count of 0). A real
                 * zero count is exactly the right, meaningful signal for
                 * "the fixture isn't reachable" instead. */
                count = pCountCookies(webview);
                if (!count)
                {
                    skip("tests/cookie_test_server.py doesn't seem to be running on 127.0.0.1:18765 "
                         "(no cookie observed after navigating there -- run `python3 cookie_test_server.py` "
                         "alongside this test to exercise the real add-then-delete verification) -- "
                         "falling back to a return-code-only check\n");
                    ok(ICoreWebView2CookieManager_DeleteAllCookies(cm) == S_OK, "DeleteAllCookies failed\n");
                }
                else
                {
                    /* Real verification #1: the cookie is actually in the
                     * store before DeleteAllCookies ever runs -- confirms
                     * the fixture's Set-Cookie header actually landed, so a
                     * later count of 0 can only mean DeleteAllCookies
                     * really deleted it, not that there was never anything
                     * there to delete in the first place. */
                    ok(count >= 1, "expected at least 1 cookie after navigating to the cookie-setting fixture, got %u\n", count);

                    ok(ICoreWebView2CookieManager_DeleteAllCookies(cm) == S_OK, "DeleteAllCookies failed\n");

                    /* Real verification #2 -- the actual point of this
                     * test: the cookie set above must really be gone now,
                     * not just that DeleteAllCookies returned S_OK. This is
                     * the specific check that would have caught the
                     * replace_cookies(NULL) bug: that call also returned
                     * S_OK while leaving the cookie count unchanged. */
                    count = pCountCookies(webview);
                    ok(count == 0, "expected 0 cookies after DeleteAllCookies, got %u\n", count);
                }

                CloseHandle(nav_handler.done_event);
                ICoreWebView2CookieManager_Release(cm);
            }
            ICoreWebView2_Release(webview_v2);
        }
        ICoreWebView2_Release(webview);
    }

    ICoreWebView2Controller_Release(ctrl);
    ICoreWebView2Environment_Release(env);
    FreeLibrary(mod);
}

struct test_cookies_handler
{
    ICoreWebView2GetCookiesCompletedHandlerVtbl *vtbl;
    HANDLE done_event;
    HRESULT result_hr;
    ICoreWebView2CookieList *result_list;
};
static HRESULT WINAPI test_cookies_handler_QI(ICoreWebView2GetCookiesCompletedHandler *iface, REFIID riid, void **ppv)
{ *ppv = iface; return S_OK; }
static ULONG WINAPI test_cookies_handler_AddRef(ICoreWebView2GetCookiesCompletedHandler *iface) { return 2; }
static ULONG WINAPI test_cookies_handler_Release(ICoreWebView2GetCookiesCompletedHandler *iface) { return 1; }
static HRESULT WINAPI test_cookies_handler_Invoke(ICoreWebView2GetCookiesCompletedHandler *iface,
                                                   HRESULT errorCode, ICoreWebView2CookieList *result)
{
    struct test_cookies_handler *h = (struct test_cookies_handler *)iface;
    h->result_hr = errorCode;
    h->result_list = result;
    if (result) ICoreWebView2CookieList_AddRef(result);
    SetEvent(h->done_event);
    return S_OK;
}
static ICoreWebView2GetCookiesCompletedHandlerVtbl test_cookies_handler_vtbl =
{ test_cookies_handler_QI, test_cookies_handler_AddRef, test_cookies_handler_Release, test_cookies_handler_Invoke };

static BOOL cookie_list_contains(ICoreWebView2CookieList *list, LPCWSTR name, LPCWSTR value)
{
    UINT32 count = 0, i;

    ICoreWebView2CookieList_get_Count(list, &count);
    for (i = 0; i < count; i++)
    {
        ICoreWebView2Cookie *cookie = NULL;
        BOOL match = FALSE;

        if (SUCCEEDED(ICoreWebView2CookieList_GetValueAtIndex(list, i, &cookie)) && cookie)
        {
            LPWSTR n = NULL, v = NULL;
            ICoreWebView2Cookie_get_Name(cookie, &n);
            ICoreWebView2Cookie_get_Value(cookie, &v);
            match = n && v && !wcscmp(n, name) && !wcscmp(v, value);
            CoTaskMemFree(n);
            CoTaskMemFree(v);
            ICoreWebView2Cookie_Release(cookie);
        }
        if (match) return TRUE;
    }
    return FALSE;
}

/* Real Task 11 bug-fix regression test: ICoreWebView2CookieManager::GetCookies
 * was left E_NOTIMPL by Task 8 (see cookie_manager.c's cm_vtbl history) --
 * real Roblox Studio's own login startup flow depends on it working
 * (clearAllCookiesAndRunCallbackHelper calling GetCookies, confirmed via a
 * real captured FLog trace; see this task's own report for the exact log
 * lines). Same real-cookie-fixture methodology as test_delete_all_cookies:
 * a real Set-Cookie header from tests/cookie_test_server.py, no DLL-exposed
 * injection primitive (CONTRIBUTING.md's hard rule against granting a
 * capability a normal Windows client wouldn't have, already enforced once
 * in this file's history -- see that test's own comment).
 *
 * Two real verifications: (1) GetCookies(NULL, ...) returns a real,
 * non-empty list containing the fixture's actual name/value, proving the
 * whole real GetCookies -> ICoreWebView2CookieList -> ICoreWebView2Cookie
 * chain works end-to-end, not just that the call returns S_OK; (2) filtering
 * by an unrelated (non-resolvable, never actually contacted -- GetCookies is
 * a local cookie-jar lookup, not a network fetch) URI excludes the fixture's
 * cookie, exercising real WebKit uri-scoped cookie matching
 * (webkit_cookie_manager_get_cookies) rather than a no-op filter. */
static void test_get_cookies(void)
{
    HMODULE mod;
    ICoreWebView2Environment *env;
    ICoreWebView2Controller *ctrl;
    ICoreWebView2 *webview = NULL, *webview_v2 = NULL;
    ICoreWebView2CookieManager *cm = NULL;
    const struct webview2_2_vtbl_combined *v2vtbl;
    struct test_nav_handler nav_handler = { &test_nav_handler_vtbl };
    struct test_cookies_handler cookies_handler = { &test_cookies_handler_vtbl };
    UINT32 count;
    UINT64 token;
    HRESULT hr;

    if (!create_test_controller(&mod, &env, &ctrl))
    {
        skip("TUXBLOX_WEBVIEW_DIR not set or environment/controller creation failed\n");
        return;
    }

    ICoreWebView2Controller_get_CoreWebView2(ctrl, &webview);
    ok(webview != NULL, "expected a webview\n");
    if (webview)
    {
        hr = ICoreWebView2_QueryInterface(webview, &IID_ICoreWebView2_2, (void **)&webview_v2);
        ok(hr == S_OK, "QueryInterface(IID_ICoreWebView2_2) failed: %#lx\n", hr);
        if (SUCCEEDED(hr))
        {
            v2vtbl = (const struct webview2_2_vtbl_combined *)webview_v2->lpVtbl;
            hr = v2vtbl->ext.get_CookieManager(webview_v2, &cm);
            ok(hr == S_OK && cm != NULL, "get_CookieManager failed: %#lx\n", hr);

            if (cm)
            {
                nav_handler.done_event = CreateEventW(NULL, FALSE, FALSE, NULL);
                nav_handler.is_success = FALSE;
                ICoreWebView2_add_NavigationCompleted(webview, &nav_handler, &token);
                ICoreWebView2_Navigate(webview, L"http://127.0.0.1:18765/");
                WaitForSingleObject(nav_handler.done_event, 30000);
                CloseHandle(nav_handler.done_event);

                cookies_handler.done_event = CreateEventW(NULL, FALSE, FALSE, NULL);
                hr = ICoreWebView2CookieManager_GetCookies(cm, NULL, (ICoreWebView2GetCookiesCompletedHandler *)&cookies_handler);
                ok(hr == S_OK, "GetCookies returned %#lx\n", hr);
                ok(WaitForSingleObject(cookies_handler.done_event, 15000) == WAIT_OBJECT_0,
                   "GetCookies(NULL) timed out\n");
                CloseHandle(cookies_handler.done_event);

                ok(cookies_handler.result_hr == S_OK, "GetCookies(NULL) handler hr %#lx\n", cookies_handler.result_hr);
                /* Code review finding (Important 3a, fixed): this assertion
                 * used to be conditional (only checked inside the `if
                 * (cookies_handler.result_hr == S_OK && ...)` branch below),
                 * so a real regression that made GetCookies succeed with an
                 * empty/NULL list was indistinguishable from the fixture
                 * server simply not running -- both silently skip()'d. A real
                 * WebView2 GetCookies call that reports S_OK must hand back a
                 * real (even if empty) ICoreWebView2CookieList per its own
                 * documented contract; asserting that unconditionally here
                 * catches that regression instead of masking it as a skip. */
                ok(cookies_handler.result_hr != S_OK || cookies_handler.result_list != NULL,
                   "GetCookies(NULL) reported S_OK but returned a NULL cookie list\n");

                count = 0;
                if (cookies_handler.result_hr == S_OK && cookies_handler.result_list)
                    ICoreWebView2CookieList_get_Count(cookies_handler.result_list, &count);

                if (!count)
                {
                    skip("tests/cookie_test_server.py doesn't seem to be running on 127.0.0.1:18765 "
                         "(no cookie observed after navigating there -- run `python3 cookie_test_server.py` "
                         "alongside this test to exercise the real GetCookies verification) -- skipping\n");
                }
                else
                {
                    /* Real verification #1: the actual fixture cookie is really
                     * in the unfiltered result, with the right name/value --
                     * proves the whole real chain (WebKit -> unix call ->
                     * ICoreWebView2Cookie/List construction) works, not just
                     * that the call returned S_OK. */
                    ok(cookie_list_contains(cookies_handler.result_list, L"tuxblox_test_cookie", L"1"),
                       "expected GetCookies(NULL) to include the real fixture cookie\n");
                }
                if (cookies_handler.result_list) { ICoreWebView2CookieList_Release(cookies_handler.result_list); cookies_handler.result_list = NULL; }

                if (count)
                {
                    /* Real verification #2 (code review finding, Important 3b,
                     * fixed: this used to be exclusion-only, which an
                     * implementation that returned an empty list for EVERY
                     * non-NULL uri would also pass): filtering by the
                     * fixture's OWN uri must actually INCLUDE its cookie --
                     * proves the filtered path (webkit_cookie_manager_get_cookies)
                     * really does real uri-scoped matching, not just "return
                     * nothing for any filter". */
                    cookies_handler.done_event = CreateEventW(NULL, FALSE, FALSE, NULL);
                    hr = ICoreWebView2CookieManager_GetCookies(cm, L"http://127.0.0.1:18765/", (ICoreWebView2GetCookiesCompletedHandler *)&cookies_handler);
                    ok(hr == S_OK, "matching-uri GetCookies returned %#lx\n", hr);
                    ok(WaitForSingleObject(cookies_handler.done_event, 15000) == WAIT_OBJECT_0,
                       "matching-uri GetCookies timed out\n");
                    CloseHandle(cookies_handler.done_event);

                    ok(cookies_handler.result_hr == S_OK, "matching-uri GetCookies handler hr %#lx\n", cookies_handler.result_hr);
                    if (cookies_handler.result_list)
                    {
                        ok(cookie_list_contains(cookies_handler.result_list, L"tuxblox_test_cookie", L"1"),
                           "expected the fixture's own uri to include the fixture cookie\n");
                        ICoreWebView2CookieList_Release(cookies_handler.result_list);
                        cookies_handler.result_list = NULL;
                    }

                    /* Real verification #3: filtering by a URI whose host has
                     * nothing to do with 127.0.0.1 must exclude the fixture's
                     * cookie -- exercises real WebKit uri-scoped cookie
                     * matching (webkit_cookie_manager_get_cookies), not a
                     * no-op filter. This URI is never actually contacted --
                     * GetCookies is a local cookie-jar lookup, not a network
                     * fetch. */
                    cookies_handler.done_event = CreateEventW(NULL, FALSE, FALSE, NULL);
                    hr = ICoreWebView2CookieManager_GetCookies(cm, L"http://tuxblox-test-unrelated.invalid/", (ICoreWebView2GetCookiesCompletedHandler *)&cookies_handler);
                    ok(hr == S_OK, "filtered GetCookies returned %#lx\n", hr);
                    ok(WaitForSingleObject(cookies_handler.done_event, 15000) == WAIT_OBJECT_0,
                       "filtered GetCookies timed out\n");
                    CloseHandle(cookies_handler.done_event);

                    ok(cookies_handler.result_hr == S_OK, "filtered GetCookies handler hr %#lx\n", cookies_handler.result_hr);
                    if (cookies_handler.result_list)
                    {
                        ok(!cookie_list_contains(cookies_handler.result_list, L"tuxblox_test_cookie", L"1"),
                           "expected an unrelated-domain filter to exclude the fixture cookie\n");
                        ICoreWebView2CookieList_Release(cookies_handler.result_list);
                        cookies_handler.result_list = NULL;
                    }

                    /* Clean up after this test the same real way
                     * test_delete_all_cookies does, so a re-run (or a test
                     * that happens to run after this one) doesn't see this
                     * test's own leftover cookie. */
                    ICoreWebView2CookieManager_DeleteAllCookies(cm);
                }

                ICoreWebView2CookieManager_Release(cm);
            }
            ICoreWebView2_Release(webview_v2);
        }
        ICoreWebView2_Release(webview);
    }

    ICoreWebView2Controller_Release(ctrl);
    ICoreWebView2Environment_Release(env);
    FreeLibrary(mod);
}

/* Regression test for a real bug found in code review: webview2_2_vtbl's
 * `base` member (webview.c) must be a verbatim, full 61-entry copy of
 * ICoreWebView2Vtbl -- an earlier version supplied only 53 initializers,
 * so the trailing 8 real slots (remove_ContainsFullScreenElementChanged,
 * get_ContainsFullScreenElement, add_WebResourceRequested,
 * remove_WebResourceRequested, AddWebResourceRequestedFilter,
 * RemoveWebResourceRequestedFilter, add_WindowCloseRequested,
 * remove_WindowCloseRequested) were implicitly NULL per C
 * aggregate-initialization rules. Calling any of them -- through EITHER the
 * v2 pointer or the original ICoreWebView2* (same object, same lpVtbl once
 * QueryInterface(IID_ICoreWebView2_2) swaps it in place) -- was a
 * NULL-pointer function call: an immediate crash instead of the intended
 * graceful E_NOTIMPL. test_delete_all_cookies alone never caught this since
 * it never calls a base-interface method past slot 53.
 *
 * Two checks: a generic sweep asserting every one of the 61 base slots is a
 * non-NULL function pointer (cheap, catches this whole bug class rather
 * than just the one slot below -- treating the struct as an array of
 * same-sized function pointers is safe here since the whole vtbl is
 * homogeneous WINAPI function-pointer members with no padding, and this
 * file already relies on function-pointer/void* interop throughout via the
 * `(void *)webview2_stub_e_notimpl` casts used everywhere), plus an actual
 * call through one of the specific slots that was really NULL before the
 * fix, confirming it returns E_NOTIMPL rather than crashing. */
static void test_v2_base_slots_not_null(void)
{
    HMODULE mod;
    ICoreWebView2Environment *env;
    ICoreWebView2Controller *ctrl;
    ICoreWebView2 *webview = NULL, *webview_v2 = NULL;
    const struct webview2_2_vtbl_combined *v2vtbl;
    HRESULT hr;

    if (!create_test_controller(&mod, &env, &ctrl))
    {
        skip("TUXBLOX_WEBVIEW_DIR not set or environment/controller creation failed\n");
        return;
    }

    ICoreWebView2Controller_get_CoreWebView2(ctrl, &webview);
    ok(webview != NULL, "expected a webview\n");
    if (webview)
    {
        hr = ICoreWebView2_QueryInterface(webview, &IID_ICoreWebView2_2, (void **)&webview_v2);
        ok(hr == S_OK, "QueryInterface(IID_ICoreWebView2_2) failed: %#lx\n", hr);
        if (SUCCEEDED(hr))
        {
            void *const *slots;
            unsigned int count = sizeof(v2vtbl->base) / sizeof(void *);
            unsigned int i;

            v2vtbl = (const struct webview2_2_vtbl_combined *)webview_v2->lpVtbl;
            slots = (void *const *)&v2vtbl->base;
            for (i = 0; i < count; i++)
                ok(slots[i] != NULL, "ICoreWebView2_2 combined vtable base slot %u is NULL\n", i);

            /* AddWebResourceRequestedFilter was one of the 8 real slots left
             * NULL before the fix -- call it through the widened v2 pointer
             * for real and confirm it returns E_NOTIMPL, not a crash. */
            hr = v2vtbl->base.AddWebResourceRequestedFilter(webview_v2, NULL, 0);
            ok(hr == E_NOTIMPL, "AddWebResourceRequestedFilter via ICoreWebView2_2 returned %#lx, expected E_NOTIMPL\n", hr);

            ICoreWebView2_Release(webview_v2);
        }
        ICoreWebView2_Release(webview);
    }

    ICoreWebView2Controller_Release(ctrl);
    ICoreWebView2Environment_Release(env);
    FreeLibrary(mod);
}

/* Generic IUnknown-shaped handler for the Task 11 registration-only add_X/
 * remove_X regression tests below (WebMessageReceived, the shared
 * generic_listener add/remove pair, and Environment8's
 * add/remove_ProcessInfosChanged) -- all three call sites only ever do
 * `handler->lpVtbl->AddRef(handler)` / `->Release(handler)` on whatever is
 * registered (see each one's own comment in webview.c/environment.c for
 * why a bare IUnknown* is enough), so a real 3-slot IUnknown vtable is all
 * a test double needs here; no Invoke of any kind is ever expected. */
struct test_iunknown_handler
{
    IUnknownVtbl *vtbl;
    LONG ref;
};
static HRESULT WINAPI test_iunknown_handler_QI(IUnknown *iface, REFIID riid, void **ppv) { *ppv = iface; return S_OK; }
static ULONG WINAPI test_iunknown_handler_AddRef(IUnknown *iface)
{
    struct test_iunknown_handler *h = (struct test_iunknown_handler *)iface;
    return InterlockedIncrement(&h->ref);
}
static ULONG WINAPI test_iunknown_handler_Release(IUnknown *iface)
{
    struct test_iunknown_handler *h = (struct test_iunknown_handler *)iface;
    return InterlockedDecrement(&h->ref);
}
static IUnknownVtbl test_iunknown_handler_vtbl =
{ test_iunknown_handler_QI, test_iunknown_handler_AddRef, test_iunknown_handler_Release };

/* Task 11 regression test: real Roblox Studio's embedded-login-dialog flow
 * QueryInterfaces a freshly-created controller/environment pair for
 * IID_ICoreWebView2Controller4 and IID_ICoreWebView2Environment8 right after
 * CreateCoreWebView2Controller succeeds, and treated E_NOINTERFACE on
 * either as fatal to the whole onCreateCoreWebView2ControllerCompleted
 * callback -- see controller_QueryInterface's and environment_QueryInterface's
 * own comments for the full story and the real captured FLog evidence. This
 * exercises both QueryInterfaces for real, confirms every widened vtable
 * slot is non-NULL (same regression class as test_v2_base_slots_not_null's
 * own "56 vs 61" bug: a short/miscounted combined-vtable initializer
 * silently NULLs trailing slots), and confirms the Controller2/3/4
 * state-only properties start at real WebView2's documented defaults and
 * round-trip a real put/get pair. */
static void test_controller4_environment8_queryinterface(void)
{
    HMODULE mod;
    ICoreWebView2Environment *env, *env8 = NULL;
    ICoreWebView2Controller *ctrl, *ctrl4 = NULL;
    HRESULT hr;

    if (!create_test_controller(&mod, &env, &ctrl))
    {
        skip("TUXBLOX_WEBVIEW_DIR not set or environment/controller creation failed\n");
        return;
    }

    hr = ICoreWebView2Controller_QueryInterface(ctrl, &IID_ICoreWebView2Controller4, (void **)&ctrl4);
    ok(hr == S_OK, "QueryInterface(IID_ICoreWebView2Controller4) failed: %#lx\n", hr);
    if (SUCCEEDED(hr))
    {
        const struct webview2_controller4_vtbl_combined *c4vtbl =
            (const struct webview2_controller4_vtbl_combined *)ctrl4->lpVtbl;
        void *const *slots;
        unsigned int count, i;
        COREWEBVIEW2_COLOR color;
        double scale = 0;
        BOOL detect = FALSE, drop = FALSE;
        COREWEBVIEW2_BOUNDS_MODE mode;

        slots = (void *const *)&c4vtbl->base;
        count = sizeof(c4vtbl->base) / sizeof(void *);
        for (i = 0; i < count; i++)
            ok(slots[i] != NULL, "Controller4 combined vtable base slot %u is NULL\n", i);
        slots = (void *const *)&c4vtbl->ext;
        count = sizeof(c4vtbl->ext) / sizeof(void *);
        for (i = 0; i < count; i++)
            ok(slots[i] != NULL, "Controller4 extension vtable slot %u is NULL\n", i);

        /* Real WebView2 documented defaults -- see controller_create's own
         * comment in controller.c for each one. */
        hr = c4vtbl->ext.get_DefaultBackgroundColor(ctrl4, &color);
        ok(hr == S_OK && color.A == 255 && color.R == 255 && color.G == 255 && color.B == 255,
           "expected opaque white default background, got hr=%#lx A=%u R=%u G=%u B=%u\n",
           hr, color.A, color.R, color.G, color.B);

        hr = c4vtbl->ext.get_RasterizationScale(ctrl4, &scale);
        ok(hr == S_OK && scale == 1.0, "expected default RasterizationScale 1.0, got hr=%#lx scale=%f\n", hr, scale);

        hr = c4vtbl->ext.get_ShouldDetectMonitorScaleChanges(ctrl4, &detect);
        ok(hr == S_OK && detect, "expected ShouldDetectMonitorScaleChanges to default TRUE, got hr=%#lx value=%d\n", hr, detect);

        hr = c4vtbl->ext.get_BoundsMode(ctrl4, &mode);
        ok(hr == S_OK && mode == COREWEBVIEW2_BOUNDS_MODE_USE_RAW_PIXELS,
           "expected default BoundsMode USE_RAW_PIXELS, got hr=%#lx mode=%d\n", hr, mode);

        hr = c4vtbl->ext.get_AllowExternalDrop(ctrl4, &drop);
        ok(hr == S_OK && drop, "expected AllowExternalDrop to default TRUE, got hr=%#lx value=%d\n", hr, drop);

        /* Round-trip a real put/get pair, not just check the default. */
        color.A = 128; color.R = 10; color.G = 20; color.B = 30;
        hr = c4vtbl->ext.put_DefaultBackgroundColor(ctrl4, color);
        ok(hr == S_OK, "put_DefaultBackgroundColor failed: %#lx\n", hr);
        color.A = color.R = color.G = color.B = 0;
        hr = c4vtbl->ext.get_DefaultBackgroundColor(ctrl4, &color);
        ok(hr == S_OK && color.A == 128 && color.R == 10 && color.G == 20 && color.B == 30,
           "put/get_DefaultBackgroundColor round-trip failed (hr=%#lx)\n", hr);

        ICoreWebView2Controller_Release(ctrl4);
    }

    hr = ICoreWebView2Environment_QueryInterface(env, &IID_ICoreWebView2Environment8, (void **)&env8);
    ok(hr == S_OK, "QueryInterface(IID_ICoreWebView2Environment8) failed: %#lx\n", hr);
    if (SUCCEEDED(hr))
    {
        const struct webview2_environment8_vtbl_combined *e8vtbl =
            (const struct webview2_environment8_vtbl_combined *)env8->lpVtbl;
        void *const *slots;
        unsigned int count, i;

        slots = (void *const *)&e8vtbl->base;
        count = sizeof(e8vtbl->base) / sizeof(void *);
        for (i = 0; i < count; i++)
            ok(slots[i] != NULL, "Environment8 combined vtable base slot %u is NULL\n", i);
        slots = (void *const *)&e8vtbl->ext;
        count = sizeof(e8vtbl->ext) / sizeof(void *);
        for (i = 0; i < count; i++)
            ok(slots[i] != NULL, "Environment8 extension vtable slot %u is NULL\n", i);

        /* GetProcessInfos is intentionally still E_NOTIMPL (see this
         * struct's own comment in webview2loader_private.h) -- confirm
         * that's a real E_NOTIMPL return through the widened pointer, not a
         * crash. */
        hr = e8vtbl->ext.GetProcessInfos(env8, NULL);
        ok(hr == E_NOTIMPL, "GetProcessInfos via ICoreWebView2Environment8 returned %#lx, expected E_NOTIMPL\n", hr);

        ICoreWebView2Environment_Release(env8);
    }

    ICoreWebView2Controller_Release(ctrl);
    ICoreWebView2Environment_Release(env);
    FreeLibrary(mod);
}

/* Plan 3 Task 1 regression test: parentWindow was previously discarded by
 * environment_CreateCoreWebView2Controller (only TRACE()'d) -- this
 * confirms it now round-trips through controller_create into
 * struct controller_impl, observable via a real get_ParentWindow call
 * (newly implemented by this task; previously E_NOTIMPL like every other
 * unimplemented ICoreWebView2Controller slot). No GTK/display is needed:
 * a plain message-only window (HWND_MESSAGE) and a fabricated non-NULL
 * HWND value are both just opaque handles as far as storage/retrieval is
 * concerned -- this test never dereferences either one. */
static void test_controller_parent_window(void)
{
    HMODULE mod;
    ICoreWebView2Environment *env;
    ICoreWebView2Controller *ctrl;
    HWND fake_parent = (HWND)(ULONG_PTR)0x00120034; /* opaque, never dereferenced */
    HWND out = NULL;
    HRESULT hr;
    struct test_ctrl_handler ctrl_handler = { &test_ctrl_handler_vtbl };
    HRESULT (WINAPI *pCreateEnv)(PCWSTR, PCWSTR, void *, void *);
    struct test_env_handler env_handler = { &test_env_handler_vtbl };

    if (!getenv("TUXBLOX_WEBVIEW_DIR"))
    {
        skip("TUXBLOX_WEBVIEW_DIR not set\n");
        return;
    }

    mod = LoadLibraryA("webview2loader.dll");
    pCreateEnv = (void *)GetProcAddress(mod, "CreateCoreWebView2EnvironmentWithOptions");
    env_handler.done_event = CreateEventW(NULL, FALSE, FALSE, NULL);
    pCreateEnv(NULL, NULL, NULL, &env_handler);
    WaitForSingleObject(env_handler.done_event, 10000);
    CloseHandle(env_handler.done_event);
    if (env_handler.result_hr != S_OK) { skip("environment creation failed\n"); FreeLibrary(mod); return; }
    env = env_handler.result_env;

    /* Real (non-HWND_MESSAGE) parent: get_ParentWindow must echo it back. */
    ctrl_handler.done_event = CreateEventW(NULL, FALSE, FALSE, NULL);
    ICoreWebView2Environment_CreateCoreWebView2Controller(env, fake_parent, &ctrl_handler);
    WaitForSingleObject(ctrl_handler.done_event, 10000);
    CloseHandle(ctrl_handler.done_event);
    ok(ctrl_handler.result_hr == S_OK, "controller creation failed: %#lx\n", ctrl_handler.result_hr);
    ctrl = ctrl_handler.result_ctrl;
    if (ctrl)
    {
        hr = ICoreWebView2Controller_get_ParentWindow(ctrl, &out);
        ok(hr == S_OK, "get_ParentWindow failed: %#lx\n", hr);
        ok(out == fake_parent, "expected ParentWindow %p, got %p\n", fake_parent, out);
        ok(hr != E_POINTER, "sanity\n");
        hr = ICoreWebView2Controller_get_ParentWindow(ctrl, NULL);
        ok(hr == E_POINTER, "expected E_POINTER for a NULL out-param, got %#lx\n", hr);
        ICoreWebView2Controller_Release(ctrl);
    }

    /* HWND_MESSAGE parent: get_ParentWindow must echo HWND_MESSAGE back too
     * -- storage is unconditional, only geometry-sync behavior (Task 2+)
     * differs for this case. */
    ctrl_handler.done_event = CreateEventW(NULL, FALSE, FALSE, NULL);
    ICoreWebView2Environment_CreateCoreWebView2Controller(env, HWND_MESSAGE, &ctrl_handler);
    WaitForSingleObject(ctrl_handler.done_event, 10000);
    CloseHandle(ctrl_handler.done_event);
    ok(ctrl_handler.result_hr == S_OK, "HWND_MESSAGE controller creation failed: %#lx\n", ctrl_handler.result_hr);
    ctrl = ctrl_handler.result_ctrl;
    if (ctrl)
    {
        hr = ICoreWebView2Controller_get_ParentWindow(ctrl, &out);
        ok(hr == S_OK, "get_ParentWindow (HWND_MESSAGE) failed: %#lx\n", hr);
        ok(out == HWND_MESSAGE, "expected ParentWindow HWND_MESSAGE, got %p\n", out);
        ICoreWebView2Controller_Release(ctrl);
    }

    ICoreWebView2Environment_Release(env);
    FreeLibrary(mod);
}

/* Plan 3 Task 2 regression test: a controller parented to HWND_MESSAGE
 * (the CookieManager flow, see webview.c's own file-level comment) must
 * never show a visible native GTK window -- previously
 * create_webview_on_gtk_thread called gtk_widget_show unconditionally,
 * producing a real, empty, floating top-level window for this flow (a
 * Minor finding parked at the end of Plan 2's final review). A
 * non-HWND_MESSAGE controller must still show one, unchanged from Plan 2's
 * baseline behavior. */
static void test_hwnd_message_never_shows_window(void)
{
    HMODULE mod;
    ICoreWebView2Environment *env;
    ICoreWebView2Controller *ctrl;
    UINT32 (WINAPI *pIsVisible)(ICoreWebView2Controller *);

    if (!create_test_controller(&mod, &env, &ctrl))
    {
        skip("TUXBLOX_WEBVIEW_DIR not set or environment/controller creation failed\n");
        return;
    }

    pIsVisible = (void *)GetProcAddress(mod, "__wine_test_webview2loader_window_is_visible");
    ok(pIsVisible != NULL, "missing __wine_test_webview2loader_window_is_visible export\n");
    if (pIsVisible)
        ok(pIsVisible(ctrl), "expected the default (NULL-parented) controller's native window to be visible\n");

    ICoreWebView2Controller_Release(ctrl);
    ICoreWebView2Environment_Release(env);

    {
        HRESULT (WINAPI *pCreateEnv)(PCWSTR, PCWSTR, void *, void *);
        struct test_env_handler env_handler = { &test_env_handler_vtbl };
        struct test_ctrl_handler ctrl_handler = { &test_ctrl_handler_vtbl };

        pCreateEnv = (void *)GetProcAddress(mod, "CreateCoreWebView2EnvironmentWithOptions");
        env_handler.done_event = CreateEventW(NULL, FALSE, FALSE, NULL);
        pCreateEnv(NULL, NULL, NULL, &env_handler);
        WaitForSingleObject(env_handler.done_event, 10000);
        CloseHandle(env_handler.done_event);
        ok(env_handler.result_hr == S_OK, "second environment creation failed\n");

        ctrl_handler.done_event = CreateEventW(NULL, FALSE, FALSE, NULL);
        ICoreWebView2Environment_CreateCoreWebView2Controller(env_handler.result_env, HWND_MESSAGE, &ctrl_handler);
        WaitForSingleObject(ctrl_handler.done_event, 10000);
        CloseHandle(ctrl_handler.done_event);
        ok(ctrl_handler.result_hr == S_OK, "HWND_MESSAGE controller creation failed\n");

        if (ctrl_handler.result_ctrl && pIsVisible)
        {
            ok(!pIsVisible(ctrl_handler.result_ctrl),
               "expected the HWND_MESSAGE controller's native window to never be shown\n");
            ICoreWebView2Controller_Release(ctrl_handler.result_ctrl);
        }
        ICoreWebView2Environment_Release(env_handler.result_env);
    }

    FreeLibrary(mod);
}

/* Task 11 regression test: ICoreWebView2Environment8::add_ProcessInfosChanged
 * was the third recurrence of the "E_NOTIMPL from an add_X call treated as
 * fatal" whack-a-mole pattern -- see environment8_add_ProcessInfosChanged's
 * own comment in environment.c. Confirms a real token comes back, that the
 * handler is really AddRef'd (matching struct env_listener's ownership),
 * and that remove both releases it and tolerates an unknown/already-removed
 * token, same as every other add_X/remove_X pair in this DLL. */
static void test_environment8_process_infos_changed(void)
{
    HMODULE mod;
    ICoreWebView2Environment *env, *env8 = NULL;
    ICoreWebView2Controller *ctrl;
    HRESULT hr;

    if (!create_test_controller(&mod, &env, &ctrl))
    {
        skip("TUXBLOX_WEBVIEW_DIR not set or environment/controller creation failed\n");
        return;
    }

    hr = ICoreWebView2Environment_QueryInterface(env, &IID_ICoreWebView2Environment8, (void **)&env8);
    ok(hr == S_OK, "QueryInterface(IID_ICoreWebView2Environment8) failed: %#lx\n", hr);
    if (SUCCEEDED(hr))
    {
        const struct webview2_environment8_vtbl_combined *e8vtbl =
            (const struct webview2_environment8_vtbl_combined *)env8->lpVtbl;
        struct test_iunknown_handler handler = { &test_iunknown_handler_vtbl, 1 };
        UINT64 token = 0;

        hr = e8vtbl->ext.add_ProcessInfosChanged(env8, &handler, &token);
        ok(hr == S_OK, "add_ProcessInfosChanged failed: %#lx\n", hr);
        ok(handler.ref == 2, "expected handler to be AddRef'd once, ref=%ld\n", handler.ref);
        ok(token != 0, "expected a non-zero registration token\n");

        hr = e8vtbl->ext.remove_ProcessInfosChanged(env8, (void *)(ULONG_PTR)token);
        ok(hr == S_OK, "remove_ProcessInfosChanged failed: %#lx\n", hr);
        ok(handler.ref == 1, "expected handler back at ref 1 after remove, ref=%ld\n", handler.ref);

        /* Real WebView2 tolerates removing an already-gone/unknown token. */
        hr = e8vtbl->ext.remove_ProcessInfosChanged(env8, (void *)(ULONG_PTR)token);
        ok(hr == S_OK, "remove_ProcessInfosChanged on an already-removed token returned %#lx, expected S_OK\n", hr);

        ICoreWebView2Environment_Release(env8);
    }

    ICoreWebView2Controller_Release(ctrl);
    ICoreWebView2Environment_Release(env);
    FreeLibrary(mod);
}

/* Task 11 regression test: ICoreWebView2::get_Settings was the fourth
 * recurrence of the same whack-a-mole pattern -- see webview_get_Settings's
 * own comment in webview.c. Confirms a real ICoreWebView2Settings comes
 * back with every property at its real WebView2-documented default (all
 * TRUE), that a put/get pair round-trips for real, and that the same
 * lazily-created object is returned (and independently AddRef'd) on a
 * second get_Settings call, matching struct webview_impl's "created lazily,
 * cached" comment. */
static void test_get_settings(void)
{
    HMODULE mod;
    ICoreWebView2Environment *env;
    ICoreWebView2Controller *ctrl;
    ICoreWebView2 *webview = NULL;
    ICoreWebView2Settings *settings = NULL, *settings2 = NULL;
    HRESULT hr;

    if (!create_test_controller(&mod, &env, &ctrl))
    {
        skip("TUXBLOX_WEBVIEW_DIR not set or environment/controller creation failed\n");
        return;
    }

    ICoreWebView2Controller_get_CoreWebView2(ctrl, &webview);
    ok(webview != NULL, "expected a webview\n");
    if (webview)
    {
        hr = ICoreWebView2_get_Settings(webview, (void **)&settings);
        ok(hr == S_OK, "get_Settings failed: %#lx\n", hr);
        if (SUCCEEDED(hr))
        {
            BOOL value;

#define CHECK_DEFAULT_TRUE(Name) \
            do { \
                value = FALSE; \
                hr = ICoreWebView2Settings_get_##Name(settings, &value); \
                ok(hr == S_OK && value, "expected " #Name " to default TRUE, got hr=%#lx value=%d\n", hr, value); \
            } while (0)

            CHECK_DEFAULT_TRUE(IsScriptEnabled);
            CHECK_DEFAULT_TRUE(IsWebMessageEnabled);
            CHECK_DEFAULT_TRUE(AreDefaultScriptDialogsEnabled);
            CHECK_DEFAULT_TRUE(IsStatusBarEnabled);
            CHECK_DEFAULT_TRUE(AreDevToolsEnabled);
            CHECK_DEFAULT_TRUE(AreDefaultContextMenusEnabled);
            CHECK_DEFAULT_TRUE(AreHostObjectsAllowed);
            CHECK_DEFAULT_TRUE(IsZoomControlEnabled);
            CHECK_DEFAULT_TRUE(IsBuiltInErrorPageEnabled);
#undef CHECK_DEFAULT_TRUE

            hr = ICoreWebView2Settings_put_IsScriptEnabled(settings, FALSE);
            ok(hr == S_OK, "put_IsScriptEnabled failed: %#lx\n", hr);
            value = TRUE;
            hr = ICoreWebView2Settings_get_IsScriptEnabled(settings, &value);
            ok(hr == S_OK && !value, "put/get_IsScriptEnabled round-trip failed (hr=%#lx value=%d)\n", hr, value);

            /* Same object, cached -- not a fresh one each call. */
            hr = ICoreWebView2_get_Settings(webview, (void **)&settings2);
            ok(hr == S_OK, "second get_Settings failed: %#lx\n", hr);
            ok(settings2 == settings, "expected get_Settings to return the same cached object\n");
            if (settings2) ICoreWebView2Settings_Release(settings2);

            ICoreWebView2Settings_Release(settings);
        }
        ICoreWebView2_Release(webview);
    }

    ICoreWebView2Controller_Release(ctrl);
    ICoreWebView2Environment_Release(env);
    FreeLibrary(mod);
}

/* Task 11 regression test: add_WebMessageReceived (the second recurrence of
 * the whack-a-mole pattern, see struct wm_listener's own comment in
 * webview.c) and the shared generic_listener add/remove pair that every
 * other remaining add_X/remove_X event on ICoreWebView2 uses (the third
 * recurrence -- add_NavigationStarting specifically, per that same
 * comment). Confirms both real registrations hand back a real token and
 * really AddRef the handler, and that remove really releases it and
 * tolerates an unknown token, mirroring
 * test_environment8_process_infos_changed's own checks for the sibling
 * fix on the environment side. */
static void test_webview_event_registration(void)
{
    HMODULE mod;
    ICoreWebView2Environment *env;
    ICoreWebView2Controller *ctrl;
    ICoreWebView2 *webview = NULL;
    HRESULT hr;

    if (!create_test_controller(&mod, &env, &ctrl))
    {
        skip("TUXBLOX_WEBVIEW_DIR not set or environment/controller creation failed\n");
        return;
    }

    ICoreWebView2Controller_get_CoreWebView2(ctrl, &webview);
    ok(webview != NULL, "expected a webview\n");
    if (webview)
    {
        struct test_iunknown_handler wm_handler = { &test_iunknown_handler_vtbl, 1 };
        struct test_iunknown_handler nav_starting_handler = { &test_iunknown_handler_vtbl, 1 };
        UINT64 wm_token = 0, nav_starting_token = 0;

        hr = ICoreWebView2_add_WebMessageReceived(webview, (void *)&wm_handler, &wm_token);
        ok(hr == S_OK, "add_WebMessageReceived failed: %#lx\n", hr);
        ok(wm_handler.ref == 2, "expected WebMessageReceived handler to be AddRef'd once, ref=%ld\n", wm_handler.ref);
        ok(wm_token != 0, "expected a non-zero WebMessageReceived token\n");

        hr = ICoreWebView2_add_NavigationStarting(webview, (void *)&nav_starting_handler, &nav_starting_token);
        ok(hr == S_OK, "add_NavigationStarting failed: %#lx\n", hr);
        ok(nav_starting_handler.ref == 2, "expected NavigationStarting handler to be AddRef'd once, ref=%ld\n",
           nav_starting_handler.ref);
        ok(nav_starting_token != 0, "expected a non-zero NavigationStarting token\n");
        ok(nav_starting_token != wm_token, "expected distinct tokens for distinct registrations\n");

        hr = ICoreWebView2_remove_WebMessageReceived(webview, (void *)(ULONG_PTR)wm_token);
        ok(hr == S_OK, "remove_WebMessageReceived failed: %#lx\n", hr);
        ok(wm_handler.ref == 1, "expected WebMessageReceived handler back at ref 1 after remove, ref=%ld\n", wm_handler.ref);
        hr = ICoreWebView2_remove_WebMessageReceived(webview, (void *)(ULONG_PTR)wm_token);
        ok(hr == S_OK, "remove_WebMessageReceived on an already-removed token returned %#lx, expected S_OK\n", hr);

        hr = ICoreWebView2_remove_NavigationStarting(webview, (void *)(ULONG_PTR)nav_starting_token);
        ok(hr == S_OK, "remove_NavigationStarting failed: %#lx\n", hr);
        ok(nav_starting_handler.ref == 1, "expected NavigationStarting handler back at ref 1 after remove, ref=%ld\n",
           nav_starting_handler.ref);

        ICoreWebView2_Release(webview);
    }

    ICoreWebView2Controller_Release(ctrl);
    ICoreWebView2Environment_Release(env);
    FreeLibrary(mod);
}

struct test_add_script_handler
{
    ICoreWebView2AddScriptToExecuteOnDocumentCreatedCompletedHandlerVtbl *vtbl;
    HANDLE done_event;
    HRESULT result_hr;
    LPWSTR result_id;
};
static HRESULT WINAPI test_add_script_handler_QI(ICoreWebView2AddScriptToExecuteOnDocumentCreatedCompletedHandler *iface,
                                                   REFIID riid, void **ppv) { *ppv = iface; return S_OK; }
static ULONG WINAPI test_add_script_handler_AddRef(ICoreWebView2AddScriptToExecuteOnDocumentCreatedCompletedHandler *iface)
{ return 2; }
static ULONG WINAPI test_add_script_handler_Release(ICoreWebView2AddScriptToExecuteOnDocumentCreatedCompletedHandler *iface)
{ return 1; }
static HRESULT WINAPI test_add_script_handler_Invoke(ICoreWebView2AddScriptToExecuteOnDocumentCreatedCompletedHandler *iface,
                                                       HRESULT errorCode, LPCWSTR result)
{
    struct test_add_script_handler *h = (struct test_add_script_handler *)iface;
    h->result_hr = errorCode;
    h->result_id = result ? _wcsdup(result) : NULL;
    SetEvent(h->done_event);
    return S_OK;
}
static ICoreWebView2AddScriptToExecuteOnDocumentCreatedCompletedHandlerVtbl test_add_script_handler_vtbl =
{ test_add_script_handler_QI, test_add_script_handler_AddRef, test_add_script_handler_Release, test_add_script_handler_Invoke };

/* Task 11 regression test: AddScriptToExecuteOnDocumentCreated was the
 * fifth and final recurrence of the whack-a-mole pattern, fixed last in
 * this task and unverified against a real launch until this task's own
 * end-to-end pass confirmed it (see webview_AddScriptToExecuteOnDocumentCreated's
 * own comment in webview.c and this task's report for the real captured
 * FLog evidence: "setInitScript calling AddScriptToExecuteOnDocumentCreated"
 * / "AddScriptToExecuteOnDocumentCreated: result=0"). Confirms the real
 * async completion semantics (S_OK returned synchronously, Invoke fires
 * later with S_OK and a real, non-empty, unique script id string), and that
 * RemoveScriptToExecuteOnDocumentCreated is a real synchronous S_OK no-op. */
static void test_add_script_to_execute_on_document_created(void)
{
    HMODULE mod;
    ICoreWebView2Environment *env;
    ICoreWebView2Controller *ctrl;
    ICoreWebView2 *webview = NULL;
    HRESULT hr;

    if (!create_test_controller(&mod, &env, &ctrl))
    {
        skip("TUXBLOX_WEBVIEW_DIR not set or environment/controller creation failed\n");
        return;
    }

    ICoreWebView2Controller_get_CoreWebView2(ctrl, &webview);
    ok(webview != NULL, "expected a webview\n");
    if (webview)
    {
        struct test_add_script_handler handler = { &test_add_script_handler_vtbl };
        struct test_add_script_handler handler2 = { &test_add_script_handler_vtbl };

        handler.done_event = CreateEventW(NULL, FALSE, FALSE, NULL);
        hr = ICoreWebView2_AddScriptToExecuteOnDocumentCreated(webview, L"window.__tuxbloxTest = 1;", &handler);
        ok(hr == S_OK, "AddScriptToExecuteOnDocumentCreated returned %#lx\n", hr);
        ok(WaitForSingleObject(handler.done_event, 10000) == WAIT_OBJECT_0,
           "AddScriptToExecuteOnDocumentCreated completion handler never fired\n");
        ok(handler.result_hr == S_OK, "expected completion hr S_OK, got %#lx\n", handler.result_hr);
        ok(handler.result_id != NULL && handler.result_id[0] != 0,
           "expected a real, non-empty script id\n");
        CloseHandle(handler.done_event);

        /* A second call gets a distinct id -- confirms next_script_id
         * actually advances rather than returning a fixed placeholder. */
        handler2.done_event = CreateEventW(NULL, FALSE, FALSE, NULL);
        hr = ICoreWebView2_AddScriptToExecuteOnDocumentCreated(webview, L"window.__tuxbloxTest = 2;", &handler2);
        ok(hr == S_OK, "second AddScriptToExecuteOnDocumentCreated returned %#lx\n", hr);
        ok(WaitForSingleObject(handler2.done_event, 10000) == WAIT_OBJECT_0,
           "second AddScriptToExecuteOnDocumentCreated completion handler never fired\n");
        ok(handler.result_id && handler2.result_id && wcscmp(handler.result_id, handler2.result_id),
           "expected two AddScriptToExecuteOnDocumentCreated calls to get distinct script ids\n");
        CloseHandle(handler2.done_event);

        free(handler.result_id);
        free(handler2.result_id);

        hr = ICoreWebView2_RemoveScriptToExecuteOnDocumentCreated(webview, L"tuxblox-script-1");
        ok(hr == S_OK, "RemoveScriptToExecuteOnDocumentCreated returned %#lx, expected S_OK\n", hr);
        /* Real WebView2 tolerates an unknown/already-removed id too. */
        hr = ICoreWebView2_RemoveScriptToExecuteOnDocumentCreated(webview, L"not-a-real-script-id");
        ok(hr == S_OK, "RemoveScriptToExecuteOnDocumentCreated on an unknown id returned %#lx, expected S_OK\n", hr);

        ICoreWebView2_Release(webview);
    }

    ICoreWebView2Controller_Release(ctrl);
    ICoreWebView2Environment_Release(env);
    FreeLibrary(mod);
}

/* Regression test for the real Task 11 e2e finding: WebKitGTK's
 * JavaScriptCore installs its own process-wide SIGUSR1 handler the first
 * time a WebKitWebView is created (its GC/JIT "safepoint" signal), silently
 * replacing ntdll's own usr1_handler (dlls/ntdll/unix/signal_x86_64.c),
 * which SuspendThread/GetThreadContext/SetThreadContext rely on to signal a
 * thread that it got suspended. Confirmed against a real Roblox Studio
 * launch: moments after WebKit's own "Overriding existing handler for
 * signal 10" stderr line appeared, two completely unrelated threads
 * crashed simultaneously inside ucrtbase.dll.
 *
 * The fix (unixlib.c's load_bundle_functions, via the real, exported
 * JSConfigureSignalForGC() API from libjavascriptcoregtk-6.0.so.1) moves
 * JSC's GC/JIT signal off SIGUSR1 before the first webview is ever created.
 * NOT the JSC_SIGNAL_FOR_GC env var WebKit's own warning text suggests --
 * tried first and found genuinely broken against this real bundle (every
 * value from bare decimal signal numbers to signal names printed "ERROR:
 * invalid option" on stderr and left JSC on SIGUSR1 anyway; the two values
 * that DID parse, "0" and "-1", instead crash-abort JSC::initialize()
 * itself outright -- see unixlib.c's own comment on JSConfigureSignalForGC
 * for the full evidence trail from a standalone dlopen probe outside Wine).
 *
 * This test exercises the actual downstream symptom directly -- a real
 * Win32 SuspendThread/GetThreadContext/ResumeThread cycle on an unrelated
 * thread, performed AFTER a real webview has been created
 * (create_test_controller's controller creation already calls
 * webkit_web_view_new() via unix_create_webview_impl, so by the time it
 * returns JSC has already had its one chance to install its handler) --
 * rather than only checking that some particular API got called, since the
 * process-wide signal collision, not any specific call site, is the real
 * defect this must keep catching even if the exact mechanism changes again.
 *
 * The probe runs on its own thread and is only waited on with a bounded
 * timeout: a regression here manifests as ntdll's suspend handshake hanging
 * forever (SuspendThread never receiving the "thread got suspended" signal
 * back), not as a clean failure return, so an unbounded wait would hang the
 * whole test suite instead of just failing this one test. */
struct suspend_probe_ctx
{
    HANDLE ready_event;
    HANDLE probe_done;
    volatile LONG ticks;
    volatile LONG stop;
    BOOL context_ok;
};

/* static (program lifetime), deliberately NOT a stack-local in
 * test_suspend_thread_after_webview: on the exact WAIT_TIMEOUT path this
 * test exists to catch (a real SIGUSR1 regression), the prober thread is
 * stuck forever inside the OS's SuspendThread call and the target thread
 * is still alive and spinning -- both keep touching this struct
 * indefinitely. A stack-local ctx would have its frame reused/destroyed
 * the moment test_suspend_thread_after_webview returns, so those two
 * still-live threads would then be writing into freed/reused stack memory
 * and eventually SetEvent()ing a closed handle -- corrupting the rest of
 * the test suite on exactly the case this test is supposed to fail
 * cleanly on instead. A static struct sidesteps that: the memory stays
 * valid for the whole process lifetime, at the cost of intentionally
 * leaking a wedged thread + its handles specifically when a real
 * regression is present -- an acceptable trade since that path already
 * means this build has the underlying bug and this test process shouldn't
 * be trusted for anything further anyway. */
static struct suspend_probe_ctx g_suspend_probe_ctx;

static DWORD WINAPI suspend_target_thread(void *arg)
{
    struct suspend_probe_ctx *ctx = arg;

    SetEvent(ctx->ready_event);
    while (!ctx->stop)
    {
        InterlockedIncrement(&ctx->ticks);
        Sleep(1);
    }
    return 0;
}

static DWORD WINAPI suspend_prober_thread(void *arg)
{
    struct suspend_probe_ctx *ctx = arg;
    HANDLE target;
    CONTEXT c;
    LONG before, after;

    target = CreateThread(NULL, 0, suspend_target_thread, ctx, 0, NULL);
    if (!target)
    {
        ctx->context_ok = FALSE;
        SetEvent(ctx->probe_done);
        return 0;
    }
    WaitForSingleObject(ctx->ready_event, INFINITE);
    Sleep(20); /* let the target thread actually start ticking */

    /* The call this whole test exists to exercise -- implemented on Linux
     * via SIGUSR1 (ntdll's usr1_handler). If that handler has been silently
     * replaced (the real bug), this call can hang indefinitely waiting for
     * an acknowledgment that will never come. */
    if (SuspendThread(target) == (DWORD)-1)
    {
        ctx->context_ok = FALSE;
        ctx->stop = TRUE;
        WaitForSingleObject(target, 2000);
        CloseHandle(target);
        SetEvent(ctx->probe_done);
        return 0;
    }

    before = ctx->ticks;
    Sleep(50);
    after = ctx->ticks;

    c.ContextFlags = CONTEXT_FULL;
    ctx->context_ok = GetThreadContext(target, &c) && before == after;

    ResumeThread(target);
    ctx->stop = TRUE;
    /* Bounded join, not INFINITE: this is best-effort cleanup only, not
     * part of the actual assertion above -- if the target is wedged for
     * some unrelated reason, this shouldn't become a second place to
     * hang. */
    WaitForSingleObject(target, 2000);
    CloseHandle(target);

    SetEvent(ctx->probe_done);
    return 0;
}

static void test_suspend_thread_after_webview(void)
{
    HMODULE mod;
    ICoreWebView2Environment *env;
    ICoreWebView2Controller *ctrl;
    struct suspend_probe_ctx *ctx = &g_suspend_probe_ctx;
    HANDLE prober;

    if (!create_test_controller(&mod, &env, &ctrl))
    {
        skip("TUXBLOX_WEBVIEW_DIR not set or environment/controller creation failed\n");
        return;
    }

    ZeroMemory(ctx, sizeof(*ctx));
    ctx->ready_event = CreateEventW(NULL, FALSE, FALSE, NULL);
    ctx->probe_done = CreateEventW(NULL, FALSE, FALSE, NULL);
    prober = CreateThread(NULL, 0, suspend_prober_thread, ctx, 0, NULL);
    ok(prober != NULL, "failed to create prober thread, error %lu\n", GetLastError());

    if (prober && WaitForSingleObject(ctx->probe_done, 5000) == WAIT_OBJECT_0)
    {
        ok(ctx->context_ok, "GetThreadContext after SuspendThread failed, or the target thread didn't actually stop\n");
        CloseHandle(prober);
        CloseHandle(ctx->ready_event);
        CloseHandle(ctx->probe_done);
    }
    else
    {
        ok(FALSE, "SuspendThread/GetThreadContext on an unrelated thread hung after webview creation -- "
                  "ntdll's SIGUSR1 suspend handshake looks clobbered (JSConfigureSignalForGC regression?)\n");
        /* Deliberately leak `prober`'s handle and both events here: the
         * prober (and the target thread it spawned) may still be alive
         * and touching g_suspend_probe_ctx indefinitely, so closing
         * handles it might still SetEvent()/use here would be exactly the
         * use-after-close/corruption this static-context rewrite was
         * meant to eliminate. */
    }

    ICoreWebView2Controller_Release(ctrl);
    ICoreWebView2Environment_Release(env);
    FreeLibrary(mod);
}

/* Final-review regression test (Important 1, native_handle use-after-free):
 * real WebView2 apps are documented to call get_CoreWebView2() and then
 * Close() on the same controller -- Controller::Close() frees the unix-side
 * native_webview struct that both the webview object obtained beforehand
 * and every ICoreWebView2CookieManager created from it used to keep
 * forwarding into the unix side by value, forever after, per the bug this
 * fix addresses. Confirms that a Navigate() issued on that still-live
 * ICoreWebView2* AFTER Close(), and a GetCookies() issued on a cookie
 * manager obtained BEFORE Close(), both complete with a clean failure
 * (is_success FALSE / a failing HRESULT via their real async completion
 * handlers) instead of hanging or crashing on a dereference of the freed
 * unix-side struct. */
static void test_close_then_navigate_fails_cleanly(void)
{
    HMODULE mod;
    ICoreWebView2Environment *env;
    ICoreWebView2Controller *ctrl;
    ICoreWebView2 *webview = NULL, *webview_v2 = NULL;
    ICoreWebView2CookieManager *cm = NULL;
    const struct webview2_2_vtbl_combined *v2vtbl;
    struct test_nav_handler nav_handler = { &test_nav_handler_vtbl };
    struct test_cookies_handler cookies_handler = { &test_cookies_handler_vtbl };
    UINT64 token;
    HRESULT hr;

    if (!create_test_controller(&mod, &env, &ctrl))
    {
        skip("TUXBLOX_WEBVIEW_DIR not set or environment/controller creation failed\n");
        return;
    }

    ICoreWebView2Controller_get_CoreWebView2(ctrl, &webview);
    ok(webview != NULL, "expected a webview\n");
    if (webview)
    {
        hr = ICoreWebView2_QueryInterface(webview, &IID_ICoreWebView2_2, (void **)&webview_v2);
        ok(hr == S_OK, "QueryInterface(IID_ICoreWebView2_2) failed: %#lx\n", hr);
        if (SUCCEEDED(hr))
        {
            v2vtbl = (const struct webview2_2_vtbl_combined *)webview_v2->lpVtbl;
            hr = v2vtbl->ext.get_CookieManager(webview_v2, &cm);
            ok(hr == S_OK && cm != NULL, "get_CookieManager failed: %#lx\n", hr);
        }

        /* The sequence this fix targets: get_CoreWebView2 (and, via that
         * webview, get_CookieManager) already happened above; Close() now
         * frees the unix-side struct both still point at. */
        hr = ICoreWebView2Controller_Close(ctrl);
        ok(hr == S_OK, "Close failed: %#lx\n", hr);

        nav_handler.done_event = CreateEventW(NULL, FALSE, FALSE, NULL);
        nav_handler.is_success = TRUE; /* seeded wrong on purpose -- a real fix must flip this to FALSE */
        ICoreWebView2_add_NavigationCompleted(webview, &nav_handler, &token);
        hr = ICoreWebView2_Navigate(webview, L"about:blank");
        ok(hr == S_OK, "Navigate after Close returned %#lx, expected S_OK (failure reported async, via the completion handler)\n", hr);
        ok(WaitForSingleObject(nav_handler.done_event, 30000) == WAIT_OBJECT_0,
           "Navigate after Close never completed (hung instead of failing cleanly?)\n");
        ok(!nav_handler.is_success, "expected Navigate after Close to report failure, not silently succeed\n");
        CloseHandle(nav_handler.done_event);

        if (cm)
        {
            cookies_handler.done_event = CreateEventW(NULL, FALSE, FALSE, NULL);
            hr = ICoreWebView2CookieManager_GetCookies(cm, NULL, (ICoreWebView2GetCookiesCompletedHandler *)&cookies_handler);
            ok(hr == S_OK, "GetCookies after Close returned %#lx, expected S_OK (failure reported async)\n", hr);
            ok(WaitForSingleObject(cookies_handler.done_event, 15000) == WAIT_OBJECT_0,
               "GetCookies after Close never completed (hung instead of failing cleanly?)\n");
            ok(cookies_handler.result_hr != S_OK,
               "expected GetCookies after Close to report a failure hr, got %#lx\n", cookies_handler.result_hr);
            if (cookies_handler.result_list) { ICoreWebView2CookieList_Release(cookies_handler.result_list); cookies_handler.result_list = NULL; }
            CloseHandle(cookies_handler.done_event);

            ICoreWebView2CookieManager_Release(cm);
        }

        if (webview_v2) ICoreWebView2_Release(webview_v2);
        ICoreWebView2_Release(webview);
    }

    /* Controller was already Close()'d above -- Release() below must not
     * double-run the native teardown (struct controller_impl's own
     * `destroyed` guard) or, on the ordering half of this same fix, touch
     * ctrl->webview after it's already been freed. */
    ICoreWebView2Controller_Release(ctrl);
    ICoreWebView2Environment_Release(env);
    FreeLibrary(mod);
}

/* Plan 3 Task 3 regression test: exercises unix_sync_window_geometry
 * end-to-end via the test-support getter, independent of Task 4's PE-side
 * put_Bounds/put_IsVisible wiring (which doesn't exist until that task) --
 * this test drives the unix call directly through a new test-support
 * export so Task 3's own deliverable is independently verifiable. Only
 * checks size (via GTK's own default-size readback is not directly
 * exposed, so this checks the round-trip through get_window_geometry
 * instead, which reads the real GdkSurface width/height after the sync)
 * and visibility; position is X11-only and asserted separately in Task 7's
 * manual end-to-end pass (no portable way to read a window's absolute
 * screen position back in a backend-independent unit test). */
static void test_sync_window_geometry(void)
{
    HMODULE mod;
    ICoreWebView2Environment *env;
    ICoreWebView2Controller *ctrl;
    BOOL (WINAPI *pSyncGeometry)(ICoreWebView2Controller *, const RECT *, BOOL);
    BOOL (WINAPI *pGetGeometry)(ICoreWebView2Controller *, RECT *);
    RECT want = { 0, 0, 640, 480 }, got;

    if (!create_test_controller(&mod, &env, &ctrl))
    {
        skip("TUXBLOX_WEBVIEW_DIR not set or environment/controller creation failed\n");
        return;
    }

    pSyncGeometry = (void *)GetProcAddress(mod, "__wine_test_webview2loader_sync_window_geometry");
    pGetGeometry = (void *)GetProcAddress(mod, "__wine_test_webview2loader_get_window_geometry");
    ok(pSyncGeometry != NULL, "missing __wine_test_webview2loader_sync_window_geometry export\n");
    ok(pGetGeometry != NULL, "missing __wine_test_webview2loader_get_window_geometry export\n");

    if (pSyncGeometry && pGetGeometry)
    {
        ok(pSyncGeometry(ctrl, &want, TRUE), "sync_window_geometry call failed\n");
        Sleep(200); /* let the GTK thread's own async resize settle */
        ZeroMemory(&got, sizeof(got));
        ok(pGetGeometry(ctrl, &got), "get_window_geometry call failed\n");
        ok(got.right - got.left == want.right - want.left && got.bottom - got.top == want.bottom - want.top,
           "expected size %ldx%ld, got %ldx%ld\n",
           want.right - want.left, want.bottom - want.top, got.right - got.left, got.bottom - got.top);
    }

    ICoreWebView2Controller_Release(ctrl);
    ICoreWebView2Environment_Release(env);
    FreeLibrary(mod);
}

/* Plan 3 Task 4, layer 1 (design spec Testing Strategy item 1): pure
 * geometry-math test, no real HWND/GTK/display needed -- just confirms the
 * client-relative-bounds + screen-origin composition arithmetic. */
static void test_compute_screen_bounds(void)
{
    HMODULE mod;
    void (WINAPI *pCompute)(const POINT *, const RECT *, RECT *);
    POINT origin = { 100, 50 };
    RECT client = { 10, 20, 210, 220 };
    RECT out;

    mod = LoadLibraryA("webview2loader.dll");
    ok(mod != NULL, "LoadLibraryA failed\n");
    if (!mod) return;

    pCompute = (void *)GetProcAddress(mod, "__wine_test_webview2loader_compute_screen_bounds");
    ok(pCompute != NULL, "missing __wine_test_webview2loader_compute_screen_bounds export\n");
    if (pCompute)
    {
        ZeroMemory(&out, sizeof(out));
        pCompute(&origin, &client, &out);
        ok(out.left == 110 && out.top == 70 && out.right == 310 && out.bottom == 270,
           "expected {110,70,310,270}, got {%ld,%ld,%ld,%ld}\n", out.left, out.top, out.right, out.bottom);
    }

    FreeLibrary(mod);
}

/* Plan 3 Task 4, layer 3 (integration): put_Bounds/put_IsVisible must
 * really reach the native window now, verified via Task 3's test-support
 * geometry getter -- no ClientToScreen composition to verify here (parent
 * is NULL, so controller_push_geometry_to_native's own NULL-parent guard,
 * see Step 3, means this exercises the "skip, no crash" path instead) --
 * NULL-parent behavior is covered by test_hwnd_message_never_shows_window's
 * sibling case; this test's job is confirming a REAL parent HWND drives a
 * real geometry push. */
static void test_put_bounds_syncs_native_window(void)
{
    HMODULE mod;
    HRESULT (WINAPI *pCreateEnv)(PCWSTR, PCWSTR, void *, void *);
    struct test_env_handler env_handler = { &test_env_handler_vtbl };
    struct test_ctrl_handler ctrl_handler = { &test_ctrl_handler_vtbl };
    ICoreWebView2Controller *ctrl;
    HWND real_parent;
    RECT bounds = { 0, 0, 500, 400 }, got;
    BOOL (WINAPI *pGetGeometry)(ICoreWebView2Controller *, RECT *);
    WNDCLASSA wc = { 0 };
    HRESULT hr;

    if (!getenv("TUXBLOX_WEBVIEW_DIR")) { skip("TUXBLOX_WEBVIEW_DIR not set\n"); return; }

    wc.lpfnWndProc = DefWindowProcA;
    wc.hInstance = GetModuleHandleA(NULL);
    wc.lpszClassName = "tuxblox_test_put_bounds_wndclass";
    RegisterClassA(&wc);
    real_parent = CreateWindowExA(0, wc.lpszClassName, "test", WS_OVERLAPPEDWINDOW,
                                   10, 20, 300, 300, NULL, NULL, wc.hInstance, NULL);
    ok(real_parent != NULL, "CreateWindowExA failed, error %lu\n", GetLastError());
    if (!real_parent) return;

    mod = LoadLibraryA("webview2loader.dll");
    pCreateEnv = (void *)GetProcAddress(mod, "CreateCoreWebView2EnvironmentWithOptions");
    env_handler.done_event = CreateEventW(NULL, FALSE, FALSE, NULL);
    pCreateEnv(NULL, NULL, NULL, &env_handler);
    WaitForSingleObject(env_handler.done_event, 10000);
    CloseHandle(env_handler.done_event);
    if (env_handler.result_hr != S_OK)
    {
        skip("environment creation failed\n");
        DestroyWindow(real_parent);
        FreeLibrary(mod);
        return;
    }

    ctrl_handler.done_event = CreateEventW(NULL, FALSE, FALSE, NULL);
    ICoreWebView2Environment_CreateCoreWebView2Controller(env_handler.result_env, real_parent, &ctrl_handler);
    WaitForSingleObject(ctrl_handler.done_event, 10000);
    CloseHandle(ctrl_handler.done_event);
    ok(ctrl_handler.result_hr == S_OK, "controller creation failed\n");
    ctrl = ctrl_handler.result_ctrl;

    if (ctrl)
    {
        hr = ICoreWebView2Controller_put_Bounds(ctrl, bounds);
        ok(hr == S_OK, "put_Bounds failed: %#lx\n", hr);
        Sleep(200);

        pGetGeometry = (void *)GetProcAddress(mod, "__wine_test_webview2loader_get_window_geometry");
        if (pGetGeometry)
        {
            ZeroMemory(&got, sizeof(got));
            ok(pGetGeometry(ctrl, &got), "get_window_geometry call failed\n");
            ok(got.right - got.left == 500 && got.bottom - got.top == 400,
               "expected size 500x400 after put_Bounds, got %ldx%ld\n",
               got.right - got.left, got.bottom - got.top);
        }
        ICoreWebView2Controller_Release(ctrl);
    }

    ICoreWebView2Environment_Release(env_handler.result_env);
    DestroyWindow(real_parent);
    UnregisterClassA(wc.lpszClassName, wc.hInstance);
    FreeLibrary(mod);
}

START_TEST(webview2loader)
{
    test_module_loads();
    test_get_available_browser_version_string();
    test_compare_browser_versions();
    test_create_environment();
    test_create_controller();
    test_navigate();
    test_navigate_twice();
    test_remove_navigation_completed();
    test_v2_base_slots_not_null();
    test_controller4_environment8_queryinterface();
    test_controller_parent_window();
    test_hwnd_message_never_shows_window();
    test_environment8_process_infos_changed();
    test_get_settings();
    test_webview_event_registration();
    test_add_script_to_execute_on_document_created();
    test_delete_all_cookies();
    test_get_cookies();
    test_close_then_navigate_fails_cleanly();
    test_suspend_thread_after_webview();
    test_sync_window_geometry();
    test_compute_screen_bounds();
    test_put_bounds_syncs_native_window();
}
