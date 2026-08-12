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

START_TEST(webview2loader)
{
    test_module_loads();
    test_create_environment();
    test_create_controller();
    test_navigate();
    test_navigate_twice();
    test_remove_navigation_completed();
    test_v2_base_slots_not_null();
    test_delete_all_cookies();
}
