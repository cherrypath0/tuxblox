#include <stdarg.h>
#include <stdlib.h>
#include <string.h>

#include <windef.h>
#include <winbase.h>
#include <wine/debug.h>

#include "unixlib.h"
#include "webview2loader_private.h"

WINE_DEFAULT_DEBUG_CHANNEL(webview2loader);

struct nav_listener
{
    struct nav_listener *next;
    ICoreWebView2NavigationCompletedEventHandler *handler;
    UINT64 token;
};

struct webview_impl
{
    ICoreWebView2 ICoreWebView2_iface;
    LONG ref;
    UINT64 native_handle;
    LPWSTR source;
    struct nav_listener *listeners;
    UINT64 next_token;
    CRITICAL_SECTION cs;
};

struct nav_args_impl
{
    ICoreWebView2NavigationCompletedEventArgs iface;
    LONG ref;
    BOOL is_success;
    UINT64 navigation_id;
};

static inline struct webview_impl *impl_from_ICoreWebView2(ICoreWebView2 *iface)
{
    return CONTAINING_RECORD(iface, struct webview_impl, ICoreWebView2_iface);
}

/* --- NavigationCompletedEventArgs: a small, real, throwaway object built
 * fresh per navigation completion --- */
static HRESULT WINAPI args_QueryInterface(ICoreWebView2NavigationCompletedEventArgs *iface, REFIID riid, void **ppv)
{
    if (IsEqualGUID(riid, &IID_IUnknown) || IsEqualGUID(riid, &IID_ICoreWebView2NavigationCompletedEventArgs))
    { *ppv = iface; ICoreWebView2NavigationCompletedEventArgs_AddRef(iface); return S_OK; }
    *ppv = NULL;
    return E_NOINTERFACE;
}
static ULONG WINAPI args_AddRef(ICoreWebView2NavigationCompletedEventArgs *iface)
{ return InterlockedIncrement(&((struct nav_args_impl *)iface)->ref); }
static ULONG WINAPI args_Release(ICoreWebView2NavigationCompletedEventArgs *iface)
{
    struct nav_args_impl *args = (struct nav_args_impl *)iface;
    LONG ref = InterlockedDecrement(&args->ref);
    if (!ref) free(args);
    return ref;
}
static HRESULT WINAPI args_get_IsSuccess(ICoreWebView2NavigationCompletedEventArgs *iface, BOOL *isSuccess)
{ *isSuccess = ((struct nav_args_impl *)iface)->is_success; return S_OK; }
static HRESULT WINAPI args_get_WebErrorStatus(ICoreWebView2NavigationCompletedEventArgs *iface, int *webErrorStatus)
{ *webErrorStatus = ((struct nav_args_impl *)iface)->is_success ? 0 /* COREWEBVIEW2_WEB_ERROR_STATUS_UNKNOWN=0 on success path */ : 1; return S_OK; }
static HRESULT WINAPI args_get_NavigationId(ICoreWebView2NavigationCompletedEventArgs *iface, UINT64 *navigationId)
{ *navigationId = ((struct nav_args_impl *)iface)->navigation_id; return S_OK; }
static const ICoreWebView2NavigationCompletedEventArgsVtbl args_vtbl =
{ args_QueryInterface, args_AddRef, args_Release, args_get_IsSuccess, args_get_WebErrorStatus, args_get_NavigationId };

/* --- ICoreWebView2 --- */
static HRESULT WINAPI webview_QueryInterface(ICoreWebView2 *iface, REFIID riid, void **ppv);

static ULONG WINAPI webview_AddRef(ICoreWebView2 *iface)
{ return InterlockedIncrement(&impl_from_ICoreWebView2(iface)->ref); }

static ULONG WINAPI webview_Release(ICoreWebView2 *iface)
{
    struct webview_impl *wv = impl_from_ICoreWebView2(iface);
    LONG ref = InterlockedDecrement(&wv->ref);
    if (!ref)
    {
        struct nav_listener *l = wv->listeners;
        while (l) { struct nav_listener *next = l->next; ICoreWebView2NavigationCompletedEventHandler_Release(l->handler); free(l); l = next; }
        CoTaskMemFree(wv->source);
        DeleteCriticalSection(&wv->cs);
        free(wv);
    }
    return ref;
}

static HRESULT WINAPI webview_get_Source(ICoreWebView2 *iface, LPWSTR *uri)
{
    struct webview_impl *wv = impl_from_ICoreWebView2(iface);
    SIZE_T len;

    if (!uri) return E_POINTER;
    EnterCriticalSection(&wv->cs);
    len = wv->source ? wcslen(wv->source) + 1 : 1;
    if ((*uri = CoTaskMemAlloc(len * sizeof(WCHAR))))
        memcpy(*uri, wv->source ? wv->source : L"", len * sizeof(WCHAR));
    LeaveCriticalSection(&wv->cs);
    return *uri ? S_OK : E_OUTOFMEMORY;
}

struct navigate_worker_ctx
{
    struct webview_impl *wv;
    LPWSTR uri;
};

static DWORD WINAPI navigate_worker(void *arg)
{
    struct navigate_worker_ctx *ctx = arg;
    struct webview_impl *wv = ctx->wv;
    struct navigate_params params = { 0 };
    struct nav_listener *l;
    /* Snapshot of AddRef'd handler pointers, built under wv->cs and walked
     * afterward WITHOUT the lock held. Walking the live wv->listeners
     * nodes themselves outside the lock (the previous version's approach)
     * is a use-after-free: webview_remove_NavigationCompleted unlinks and
     * free()s a node under its own independent lock/unlock, with no
     * coordination with any Invoke loop already in progress here -- if a
     * remove lands while this function's cursor is on (or about to step
     * into) that node, l->handler/l->next read freed memory. AddRef'ing
     * each handler while still holding the lock keeps the HANDLER object
     * alive even if its nav_listener node is concurrently unlinked/freed
     * by a remove_NavigationCompleted call the instant the lock is
     * released -- the snapshot array itself, and the handler objects it
     * points at, are this function's own memory/references from here on,
     * untouched by the listener list's own lifetime. */
    ICoreWebView2NavigationCompletedEventHandler **snapshot = NULL;
    SIZE_T count = 0, i;

    params.handle = wv->native_handle;
    params.uri = ctx->uri;
    WEBVIEW2LOADER_UNIX_CALL(navigate_and_wait, &params);

    EnterCriticalSection(&wv->cs);
    CoTaskMemFree(wv->source);
    wv->source = ctx->uri; /* transfer ownership */

    for (l = wv->listeners; l; l = l->next) count++;
    if (count && (snapshot = malloc(count * sizeof(*snapshot))))
    {
        for (l = wv->listeners, i = 0; l; l = l->next, i++)
        {
            ICoreWebView2NavigationCompletedEventHandler_AddRef(l->handler);
            snapshot[i] = l->handler;
        }
    }
    else count = 0; /* no listeners, or malloc failed: nothing to notify */
    LeaveCriticalSection(&wv->cs);

    for (i = 0; i < count; i++)
    {
        struct nav_args_impl *args = calloc(1, sizeof(*args));
        if (args)
        {
            args->iface.lpVtbl = &args_vtbl;
            args->ref = 1;
            args->is_success = params.is_success;
            args->navigation_id = params.navigation_id;
            ICoreWebView2NavigationCompletedEventHandler_Invoke(snapshot[i], &wv->ICoreWebView2_iface, &args->iface);
            ICoreWebView2NavigationCompletedEventArgs_Release(&args->iface);
        }
        ICoreWebView2NavigationCompletedEventHandler_Release(snapshot[i]);
    }
    free(snapshot);

    ICoreWebView2_Release(&wv->ICoreWebView2_iface); /* AddRef'd in webview_Navigate before spawning */
    free(ctx);
    return 0;
}

static HRESULT WINAPI webview_Navigate(ICoreWebView2 *iface, LPCWSTR uri)
{
    struct webview_impl *wv = impl_from_ICoreWebView2(iface);
    struct navigate_worker_ctx *ctx;
    SIZE_T len;

    TRACE("(%p, %s)\n", iface, debugstr_w(uri));
    if (!uri) return E_POINTER;
    if (!(ctx = malloc(sizeof(*ctx)))) return E_OUTOFMEMORY;

    len = wcslen(uri) + 1;
    if (!(ctx->uri = CoTaskMemAlloc(len * sizeof(WCHAR)))) { free(ctx); return E_OUTOFMEMORY; }
    memcpy(ctx->uri, uri, len * sizeof(WCHAR));
    ctx->wv = wv;

    ICoreWebView2_AddRef(iface); /* released by navigate_worker */
    if (!start_async_work(navigate_worker, ctx))
    {
        ICoreWebView2_Release(iface);
        CoTaskMemFree(ctx->uri);
        free(ctx);
        return E_FAIL;
    }
    return S_OK; /* real WebView2 semantics: returns immediately, completion via NavigationCompleted */
}

static HRESULT WINAPI webview_NavigateToString(ICoreWebView2 *iface, LPCWSTR htmlContent)
{
    /* WebKitGTK's load-html path needs a distinct unix call from
     * load-uri's (webkit_web_view_load_html, not load_uri) -- out of
     * scope for the login-dialog proof this plan targets (login is a real
     * URI navigation, not an inline-HTML one); left E_NOTIMPL rather than
     * silently mis-implemented via load_uri("data:...") which real pages
     * can behave differently under (different origin semantics). */
    return E_NOTIMPL;
}

static HRESULT WINAPI webview_add_NavigationCompleted(ICoreWebView2 *iface, void *eventHandler_raw, void *token_raw)
{
    struct webview_impl *wv = impl_from_ICoreWebView2(iface);
    ICoreWebView2NavigationCompletedEventHandler *handler = eventHandler_raw;
    UINT64 *token = token_raw;
    struct nav_listener *l;

    if (!handler || !token) return E_POINTER;
    if (!(l = malloc(sizeof(*l)))) return E_OUTOFMEMORY;

    ICoreWebView2NavigationCompletedEventHandler_AddRef(handler);
    l->handler = handler;

    EnterCriticalSection(&wv->cs);
    l->token = ++wv->next_token;
    l->next = wv->listeners;
    wv->listeners = l;
    LeaveCriticalSection(&wv->cs);

    *token = l->token;
    return S_OK;
}

static HRESULT WINAPI webview_remove_NavigationCompleted(ICoreWebView2 *iface, void *token_raw)
{
    struct webview_impl *wv = impl_from_ICoreWebView2(iface);
    /* Widen token_raw's own pointer-sized bit pattern into a UINT64 rather
     * than reading *past* it: the brief's original `*(UINT64 *)&token_raw`
     * treats the address of the local `token_raw` variable as a UINT64*
     * and dereferences it, which reads 8 bytes starting at a 4-byte
     * object on i386 builds (void* is 4 bytes there) -- caught by
     * -Werror=array-bounds during the i386 half of the build. This cast
     * chain converts the pointer VALUE itself (zero-extended on i386,
     * exact on x86_64), never reading past token_raw's own storage. */
    UINT64 token = (UINT64)(ULONG_PTR)token_raw;
    struct nav_listener **cur;

    EnterCriticalSection(&wv->cs);
    for (cur = &wv->listeners; *cur; cur = &(*cur)->next)
    {
        if ((*cur)->token == token)
        {
            struct nav_listener *dead = *cur;
            *cur = dead->next;
            LeaveCriticalSection(&wv->cs);
            ICoreWebView2NavigationCompletedEventHandler_Release(dead->handler);
            free(dead);
            return S_OK;
        }
    }
    LeaveCriticalSection(&wv->cs);
    return S_OK; /* real WebView2 tolerates removing an already-gone/unknown token */
}

static HRESULT WINAPI webview_QueryInterface(ICoreWebView2 *iface, REFIID riid, void **ppv)
{
    if (IsEqualGUID(riid, &IID_IUnknown) || IsEqualGUID(riid, &IID_ICoreWebView2))
    {
        *ppv = iface;
        ICoreWebView2_AddRef(iface);
        return S_OK;
    }
    /* Task 8 extends this with IID_ICoreWebView2_2 */
    return webview_query_interface_v2(iface, riid, ppv);
}

static const ICoreWebView2Vtbl webview_vtbl =
{
    webview_QueryInterface,
    webview_AddRef,
    webview_Release,
    (void *)webview2_stub_e_notimpl, /* get_Settings */
    webview_get_Source,
    webview_Navigate,
    webview_NavigateToString,
    (void *)webview2_stub_e_notimpl, /* add_NavigationStarting */
    (void *)webview2_stub_e_notimpl, /* remove_NavigationStarting */
    (void *)webview2_stub_e_notimpl, /* add_ContentLoading */
    (void *)webview2_stub_e_notimpl, /* remove_ContentLoading */
    (void *)webview2_stub_e_notimpl, /* add_SourceChanged */
    (void *)webview2_stub_e_notimpl, /* remove_SourceChanged */
    (void *)webview2_stub_e_notimpl, /* add_HistoryChanged */
    (void *)webview2_stub_e_notimpl, /* remove_HistoryChanged */
    webview_add_NavigationCompleted,
    webview_remove_NavigationCompleted,
    (void *)webview2_stub_e_notimpl, /* add_FrameNavigationStarting */
    (void *)webview2_stub_e_notimpl, /* remove_FrameNavigationStarting */
    (void *)webview2_stub_e_notimpl, /* add_FrameNavigationCompleted */
    (void *)webview2_stub_e_notimpl, /* remove_FrameNavigationCompleted */
    (void *)webview2_stub_e_notimpl, /* add_ScriptDialogOpening */
    (void *)webview2_stub_e_notimpl, /* remove_ScriptDialogOpening */
    (void *)webview2_stub_e_notimpl, /* add_PermissionRequested */
    (void *)webview2_stub_e_notimpl, /* remove_PermissionRequested */
    (void *)webview2_stub_e_notimpl, /* add_ProcessFailed */
    (void *)webview2_stub_e_notimpl, /* remove_ProcessFailed */
    (void *)webview2_stub_e_notimpl, /* AddScriptToExecuteOnDocumentCreated */
    (void *)webview2_stub_e_notimpl, /* RemoveScriptToExecuteOnDocumentCreated */
    (void *)webview2_stub_e_notimpl, /* ExecuteScript */
    (void *)webview2_stub_e_notimpl, /* CapturePreview */
    (void *)webview2_stub_e_notimpl, /* Reload */
    (void *)webview2_stub_e_notimpl, /* PostWebMessageAsJson */
    (void *)webview2_stub_e_notimpl, /* PostWebMessageAsString */
    (void *)webview2_stub_e_notimpl, /* add_WebMessageReceived */
    (void *)webview2_stub_e_notimpl, /* remove_WebMessageReceived */
    (void *)webview2_stub_e_notimpl, /* CallDevToolsProtocolMethod */
    (void *)webview2_stub_e_notimpl, /* get_BrowserProcessId */
    (void *)webview2_stub_e_notimpl, /* get_CanGoBack */
    (void *)webview2_stub_e_notimpl, /* get_CanGoForward */
    (void *)webview2_stub_e_notimpl, /* GoBack */
    (void *)webview2_stub_e_notimpl, /* GoForward */
    (void *)webview2_stub_e_notimpl, /* GetDevToolsProtocolEventReceiver */
    (void *)webview2_stub_e_notimpl, /* Stop */
    (void *)webview2_stub_e_notimpl, /* add_NewWindowRequested */
    (void *)webview2_stub_e_notimpl, /* remove_NewWindowRequested */
    (void *)webview2_stub_e_notimpl, /* add_DocumentTitleChanged */
    (void *)webview2_stub_e_notimpl, /* remove_DocumentTitleChanged */
    (void *)webview2_stub_e_notimpl, /* get_DocumentTitle */
    (void *)webview2_stub_e_notimpl, /* AddHostObjectToScript */
    (void *)webview2_stub_e_notimpl, /* RemoveHostObjectFromScript */
    (void *)webview2_stub_e_notimpl, /* OpenDevToolsWindow */
    (void *)webview2_stub_e_notimpl, /* add_ContainsFullScreenElementChanged */
    (void *)webview2_stub_e_notimpl, /* remove_ContainsFullScreenElementChanged */
    (void *)webview2_stub_e_notimpl, /* get_ContainsFullScreenElement */
    (void *)webview2_stub_e_notimpl, /* add_WebResourceRequested */
    (void *)webview2_stub_e_notimpl, /* remove_WebResourceRequested */
    (void *)webview2_stub_e_notimpl, /* AddWebResourceRequestedFilter */
    (void *)webview2_stub_e_notimpl, /* RemoveWebResourceRequestedFilter */
    (void *)webview2_stub_e_notimpl, /* add_WindowCloseRequested */
    (void *)webview2_stub_e_notimpl, /* remove_WindowCloseRequested */
};

HRESULT webview_create(UINT64 native_handle, ICoreWebView2 **out)
{
    struct webview_impl *wv = calloc(1, sizeof(*wv));
    if (!wv) return E_OUTOFMEMORY;

    wv->ICoreWebView2_iface.lpVtbl = &webview_vtbl;
    wv->ref = 1;
    wv->native_handle = native_handle;
    InitializeCriticalSection(&wv->cs);
    *out = &wv->ICoreWebView2_iface;
    return S_OK;
}

/* --- Task 8: ICoreWebView2_2 extension --- */

static HRESULT WINAPI webview2_get_CookieManager(ICoreWebView2 *iface, ICoreWebView2CookieManager **cookieManager)
{
    struct webview_impl *wv = impl_from_ICoreWebView2(iface);
    if (!cookieManager) return E_POINTER;
    return cookie_manager_create(wv->native_handle, cookieManager);
}

static HRESULT WINAPI webview2_get_Environment(ICoreWebView2 *iface, ICoreWebView2Environment **environment)
{
    return E_NOTIMPL; /* not needed for the login-dialog proof; no stored back-reference to the environment yet */
}

/* struct webview2_2_vtbl_combined itself is declared in
 * webview2loader_private.h (not here) -- tests/webview2loader.c's
 * test_delete_all_cookies needs to see it too, to reach through
 * webview_v2->lpVtbl and call ext.get_CookieManager directly. This is the
 * one place that actually builds an instance of it. */
static const struct webview2_2_vtbl_combined webview2_2_vtbl =
{
    /* Must be a verbatim, full 61-entry copy of webview_vtbl above (NOT 56 --
     * see the fix note on this exact spot: an earlier version of this table
     * supplied only 53 initializers here based on a wrong "56 entries"
     * premise carried over from the task brief/comments, silently
     * zero-initializing the trailing 8 real ICoreWebView2 slots
     * (remove_ContainsFullScreenElementChanged, get_ContainsFullScreenElement,
     * add_WebResourceRequested, remove_WebResourceRequested,
     * AddWebResourceRequestedFilter, RemoveWebResourceRequestedFilter,
     * add_WindowCloseRequested, remove_WindowCloseRequested) to NULL function
     * pointers per C aggregate-initialization rules -- a real interface has
     * 61 methods total (verified by counting ICoreWebView2Vtbl in
     * webview2loader_private.h and cross-checking against webview_vtbl's own
     * 61 initializers just above), and calling any of those 8 slots through
     * either the v2 pointer or the original ICoreWebView2* (same object,
     * same lpVtbl once swapped) crashed on a NULL-pointer call instead of
     * returning E_NOTIMPL. Caught by code review, not by the test suite --
     * test_delete_all_cookies never called a base-interface method past
     * slot 53. See test_v2_base_slots_not_null below for the regression
     * coverage added for this. */
    {
        webview_QueryInterface, webview_AddRef, webview_Release,
        (void *)webview2_stub_e_notimpl, /* get_Settings */
        webview_get_Source, webview_Navigate, webview_NavigateToString,
        (void *)webview2_stub_e_notimpl, /* add_NavigationStarting */
        (void *)webview2_stub_e_notimpl, /* remove_NavigationStarting */
        (void *)webview2_stub_e_notimpl, /* add_ContentLoading */
        (void *)webview2_stub_e_notimpl, /* remove_ContentLoading */
        (void *)webview2_stub_e_notimpl, /* add_SourceChanged */
        (void *)webview2_stub_e_notimpl, /* remove_SourceChanged */
        (void *)webview2_stub_e_notimpl, /* add_HistoryChanged */
        (void *)webview2_stub_e_notimpl, /* remove_HistoryChanged */
        webview_add_NavigationCompleted, webview_remove_NavigationCompleted,
        (void *)webview2_stub_e_notimpl, /* add_FrameNavigationStarting */
        (void *)webview2_stub_e_notimpl, /* remove_FrameNavigationStarting */
        (void *)webview2_stub_e_notimpl, /* add_FrameNavigationCompleted */
        (void *)webview2_stub_e_notimpl, /* remove_FrameNavigationCompleted */
        (void *)webview2_stub_e_notimpl, /* add_ScriptDialogOpening */
        (void *)webview2_stub_e_notimpl, /* remove_ScriptDialogOpening */
        (void *)webview2_stub_e_notimpl, /* add_PermissionRequested */
        (void *)webview2_stub_e_notimpl, /* remove_PermissionRequested */
        (void *)webview2_stub_e_notimpl, /* add_ProcessFailed */
        (void *)webview2_stub_e_notimpl, /* remove_ProcessFailed */
        (void *)webview2_stub_e_notimpl, /* AddScriptToExecuteOnDocumentCreated */
        (void *)webview2_stub_e_notimpl, /* RemoveScriptToExecuteOnDocumentCreated */
        (void *)webview2_stub_e_notimpl, /* ExecuteScript */
        (void *)webview2_stub_e_notimpl, /* CapturePreview */
        (void *)webview2_stub_e_notimpl, /* Reload */
        (void *)webview2_stub_e_notimpl, /* PostWebMessageAsJson */
        (void *)webview2_stub_e_notimpl, /* PostWebMessageAsString */
        (void *)webview2_stub_e_notimpl, /* add_WebMessageReceived */
        (void *)webview2_stub_e_notimpl, /* remove_WebMessageReceived */
        (void *)webview2_stub_e_notimpl, /* CallDevToolsProtocolMethod */
        (void *)webview2_stub_e_notimpl, /* get_BrowserProcessId */
        (void *)webview2_stub_e_notimpl, /* get_CanGoBack */
        (void *)webview2_stub_e_notimpl, /* get_CanGoForward */
        (void *)webview2_stub_e_notimpl, /* GoBack */
        (void *)webview2_stub_e_notimpl, /* GoForward */
        (void *)webview2_stub_e_notimpl, /* GetDevToolsProtocolEventReceiver */
        (void *)webview2_stub_e_notimpl, /* Stop */
        (void *)webview2_stub_e_notimpl, /* add_NewWindowRequested */
        (void *)webview2_stub_e_notimpl, /* remove_NewWindowRequested */
        (void *)webview2_stub_e_notimpl, /* add_DocumentTitleChanged */
        (void *)webview2_stub_e_notimpl, /* remove_DocumentTitleChanged */
        (void *)webview2_stub_e_notimpl, /* get_DocumentTitle */
        (void *)webview2_stub_e_notimpl, /* AddHostObjectToScript */
        (void *)webview2_stub_e_notimpl, /* RemoveHostObjectFromScript */
        (void *)webview2_stub_e_notimpl, /* OpenDevToolsWindow */
        (void *)webview2_stub_e_notimpl, /* add_ContainsFullScreenElementChanged */
        (void *)webview2_stub_e_notimpl, /* remove_ContainsFullScreenElementChanged */
        (void *)webview2_stub_e_notimpl, /* get_ContainsFullScreenElement */
        (void *)webview2_stub_e_notimpl, /* add_WebResourceRequested */
        (void *)webview2_stub_e_notimpl, /* remove_WebResourceRequested */
        (void *)webview2_stub_e_notimpl, /* AddWebResourceRequestedFilter */
        (void *)webview2_stub_e_notimpl, /* RemoveWebResourceRequestedFilter */
        (void *)webview2_stub_e_notimpl, /* add_WindowCloseRequested */
        (void *)webview2_stub_e_notimpl, /* remove_WindowCloseRequested */
    },
    {
        (void *)webview2_stub_e_notimpl, /* add_WebResourceResponseReceived */
        (void *)webview2_stub_e_notimpl, /* remove_WebResourceResponseReceived */
        (void *)webview2_stub_e_notimpl, /* NavigateWithWebResourceRequest */
        (void *)webview2_stub_e_notimpl, /* add_DOMContentLoaded */
        (void *)webview2_stub_e_notimpl, /* remove_DOMContentLoaded */
        webview2_get_CookieManager,
        webview2_get_Environment,
    },
};

HRESULT webview_query_interface_v2(ICoreWebView2 *iface, REFIID riid, void **ppv)
{
    if (IsEqualGUID(riid, &IID_ICoreWebView2_2))
    {
        /* Re-point lpVtbl at the combined table -- safe: struct
         * webview2_2_vtbl_combined's `base` member is layout-identical to
         * ICoreWebView2Vtbl (same fields, same order), so this cast doesn't
         * change any already-resolved slot, only adds the 7 new ones.
         *
         * Note on the vtable swap: re-pointing lpVtbl here means a caller
         * holding the original ICoreWebView2* also observes the wider
         * vtable afterward. This is intentional and harmless (the first 61
         * slots are byte-identical, so every existing ICoreWebView2_* call
         * macro still resolves to the exact same function pointers) --
         * Roblox is only ever expected to call ICoreWebView2_2 methods
         * through the pointer QueryInterface itself returned, per normal
         * COM usage, not through the original one. */
        struct webview_impl *wv = impl_from_ICoreWebView2(iface);
        wv->ICoreWebView2_iface.lpVtbl = (const ICoreWebView2Vtbl *)&webview2_2_vtbl;
        *ppv = iface;
        ICoreWebView2_AddRef(iface);
        return S_OK;
    }
    *ppv = NULL;
    return E_NOINTERFACE;
}

/* --- Test-support-only export, listed in webview2loader.spec alongside
 * the two real WebView2 API exports but NOT part of the real
 * WebView2Loader.dll surface (same "__wine_*" naming convention this fork
 * already uses elsewhere, e.g. dlls/ntdll's __wine_unix_call, for internal
 * hooks). Exists so tests/webview2loader.c's test_delete_all_cookies can
 * verify DeleteAllCookies actually reduced the real cookie count, not just
 * that it returns S_OK.
 *
 * A matching __wine_test_webview2loader_add_cookie export used to live here
 * too (to add a test cookie the same test could then verify got deleted),
 * removed after code review: unlike this read-only count, it let any
 * in-process code holding a live ICoreWebView2* inject an arbitrary cookie
 * into the real cookie store with zero validation -- a real capability
 * beyond what a normal Windows client has, shipped in the SAME production
 * DLL that replaces the real WebView2Loader.dll (this Makefile.in has no
 * test/production build split, so there was no way to keep that export out
 * of a real build). test_delete_all_cookies now adds its test cookie
 * through the already-legitimate, already-implemented Navigate() path
 * instead (a real HTTP response's Set-Cookie header, via
 * tests/cookie_test_server.py) -- a capability any real WebView2 client
 * already has, not a new DLL export. */
UINT32 WINAPI __wine_test_webview2loader_count_cookies(ICoreWebView2 *webview)
{
    struct webview_impl *wv;
    struct count_cookies_params params;

    if (!webview) return 0;
    wv = impl_from_ICoreWebView2(webview);
    params.handle = wv->native_handle;
    params.count = 0;
    WEBVIEW2LOADER_UNIX_CALL(count_cookies, &params);
    return params.count;
}
