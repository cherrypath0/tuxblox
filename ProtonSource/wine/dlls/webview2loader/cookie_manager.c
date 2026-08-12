#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include <windef.h>
#include <winbase.h>
#include <wine/debug.h>

#include "unixlib.h"
#include "webview2loader_private.h"

WINE_DEFAULT_DEBUG_CHANNEL(webview2loader);

struct cookie_manager_impl
{
    ICoreWebView2CookieManager ICoreWebView2CookieManager_iface;
    LONG ref;
    UINT64 native_handle;
};

static inline struct cookie_manager_impl *impl_from_iface(ICoreWebView2CookieManager *iface)
{
    return CONTAINING_RECORD(iface, struct cookie_manager_impl, ICoreWebView2CookieManager_iface);
}

static HRESULT WINAPI cm_QueryInterface(ICoreWebView2CookieManager *iface, REFIID riid, void **ppv)
{
    if (IsEqualGUID(riid, &IID_IUnknown) || IsEqualGUID(riid, &IID_ICoreWebView2CookieManager))
    { *ppv = iface; ICoreWebView2CookieManager_AddRef(iface); return S_OK; }
    *ppv = NULL;
    return E_NOINTERFACE;
}
static ULONG WINAPI cm_AddRef(ICoreWebView2CookieManager *iface) { return InterlockedIncrement(&impl_from_iface(iface)->ref); }
static ULONG WINAPI cm_Release(ICoreWebView2CookieManager *iface)
{
    LONG ref = InterlockedDecrement(&impl_from_iface(iface)->ref);
    if (!ref) free(impl_from_iface(iface));
    return ref;
}

/* --- Task 11 real-bug fix: ICoreWebView2Cookie -- a real, standalone
 * snapshot object (real WebView2 semantics per learn.microsoft.com's own
 * ICoreWebView2Cookie reference: "You can modify the cookie objects... and
 * the changes will be applied to the webview" only once passed back through
 * AddOrUpdateCookie -- still E_NOTIMPL, out of this fix's scope; GetCookies
 * itself, which real Roblox Studio's login flow actually calls, is what's
 * being fixed here). Name/Domain/Path have no setters -- verified against
 * the real header (webview2loader_private.h's own comment on
 * ICoreWebView2CookieVtbl has the detail), so cookie_impl treats those
 * three as fixed at construction, matching real semantics exactly. */
struct cookie_impl
{
    ICoreWebView2Cookie ICoreWebView2Cookie_iface;
    LONG ref;
    LPWSTR name, value, domain, path;
    double expires;
    COREWEBVIEW2_COOKIE_SAME_SITE_KIND same_site;
    BOOL is_session, is_http_only, is_secure;
};

static inline struct cookie_impl *impl_from_ICoreWebView2Cookie(ICoreWebView2Cookie *iface)
{ return CONTAINING_RECORD(iface, struct cookie_impl, ICoreWebView2Cookie_iface); }

static HRESULT WINAPI cookie_QueryInterface(ICoreWebView2Cookie *iface, REFIID riid, void **ppv)
{
    if (IsEqualGUID(riid, &IID_IUnknown) || IsEqualGUID(riid, &IID_ICoreWebView2Cookie))
    { *ppv = iface; ICoreWebView2Cookie_AddRef(iface); return S_OK; }
    *ppv = NULL;
    return E_NOINTERFACE;
}
static ULONG WINAPI cookie_AddRef(ICoreWebView2Cookie *iface)
{ return InterlockedIncrement(&impl_from_ICoreWebView2Cookie(iface)->ref); }
static ULONG WINAPI cookie_Release(ICoreWebView2Cookie *iface)
{
    struct cookie_impl *c = impl_from_ICoreWebView2Cookie(iface);
    LONG ref = InterlockedDecrement(&c->ref);
    if (!ref)
    {
        CoTaskMemFree(c->name);
        CoTaskMemFree(c->value);
        CoTaskMemFree(c->domain);
        CoTaskMemFree(c->path);
        free(c);
    }
    return ref;
}

/* Shared by every get_* string accessor below: real WebView2 semantics
 * return a FRESH CoTaskMemAlloc'd copy the caller must free (verified
 * against the real header/docs -- "The caller must free the returned
 * string with CoTaskMemFree"), not the object's own internal storage,
 * matching webview_get_Source's own already-established copy-out
 * convention in webview.c. */
static HRESULT copy_out_wstr(LPCWSTR src, LPWSTR *out)
{
    SIZE_T len;
    if (!out) return E_POINTER;
    len = src ? wcslen(src) + 1 : 1;
    if (!(*out = CoTaskMemAlloc(len * sizeof(WCHAR)))) return E_OUTOFMEMORY;
    memcpy(*out, src ? src : L"", len * sizeof(WCHAR));
    return S_OK;
}

static HRESULT WINAPI cookie_get_Name(ICoreWebView2Cookie *iface, LPWSTR *name)
{ return copy_out_wstr(impl_from_ICoreWebView2Cookie(iface)->name, name); }
static HRESULT WINAPI cookie_get_Value(ICoreWebView2Cookie *iface, LPWSTR *value)
{ return copy_out_wstr(impl_from_ICoreWebView2Cookie(iface)->value, value); }
static HRESULT WINAPI cookie_put_Value(ICoreWebView2Cookie *iface, LPCWSTR value)
{
    struct cookie_impl *c = impl_from_ICoreWebView2Cookie(iface);
    LPWSTR copy;
    HRESULT hr = copy_out_wstr(value, &copy);
    if (FAILED(hr)) return hr;
    CoTaskMemFree(c->value);
    c->value = copy;
    return S_OK;
}
static HRESULT WINAPI cookie_get_Domain(ICoreWebView2Cookie *iface, LPWSTR *domain)
{ return copy_out_wstr(impl_from_ICoreWebView2Cookie(iface)->domain, domain); }
static HRESULT WINAPI cookie_get_Path(ICoreWebView2Cookie *iface, LPWSTR *path)
{ return copy_out_wstr(impl_from_ICoreWebView2Cookie(iface)->path, path); }
static HRESULT WINAPI cookie_get_Expires(ICoreWebView2Cookie *iface, double *expires)
{ if (!expires) return E_POINTER; *expires = impl_from_ICoreWebView2Cookie(iface)->expires; return S_OK; }
static HRESULT WINAPI cookie_put_Expires(ICoreWebView2Cookie *iface, double expires)
{
    struct cookie_impl *c = impl_from_ICoreWebView2Cookie(iface);
    /* Real semantics (learn.microsoft.com's own ICoreWebView2Cookie
     * reference): "Cookies are session cookies... if Expires is set to
     * -1.0. NaN, infinity, and any negative value set other than -1.0 is
     * disallowed." */
    if (expires != -1.0 && (isnan(expires) || isinf(expires) || expires < 0)) return E_INVALIDARG;
    c->expires = expires;
    c->is_session = (expires == -1.0);
    return S_OK;
}
static HRESULT WINAPI cookie_get_IsHttpOnly(ICoreWebView2Cookie *iface, BOOL *isHttpOnly)
{ if (!isHttpOnly) return E_POINTER; *isHttpOnly = impl_from_ICoreWebView2Cookie(iface)->is_http_only; return S_OK; }
static HRESULT WINAPI cookie_put_IsHttpOnly(ICoreWebView2Cookie *iface, BOOL isHttpOnly)
{ impl_from_ICoreWebView2Cookie(iface)->is_http_only = isHttpOnly; return S_OK; }
static HRESULT WINAPI cookie_get_SameSite(ICoreWebView2Cookie *iface, COREWEBVIEW2_COOKIE_SAME_SITE_KIND *sameSite)
{ if (!sameSite) return E_POINTER; *sameSite = impl_from_ICoreWebView2Cookie(iface)->same_site; return S_OK; }
static HRESULT WINAPI cookie_put_SameSite(ICoreWebView2Cookie *iface, COREWEBVIEW2_COOKIE_SAME_SITE_KIND sameSite)
{ impl_from_ICoreWebView2Cookie(iface)->same_site = sameSite; return S_OK; }
static HRESULT WINAPI cookie_get_IsSecure(ICoreWebView2Cookie *iface, BOOL *isSecure)
{ if (!isSecure) return E_POINTER; *isSecure = impl_from_ICoreWebView2Cookie(iface)->is_secure; return S_OK; }
static HRESULT WINAPI cookie_put_IsSecure(ICoreWebView2Cookie *iface, BOOL isSecure)
{ impl_from_ICoreWebView2Cookie(iface)->is_secure = isSecure; return S_OK; }
static HRESULT WINAPI cookie_get_IsSession(ICoreWebView2Cookie *iface, BOOL *isSession)
{ if (!isSession) return E_POINTER; *isSession = impl_from_ICoreWebView2Cookie(iface)->is_session; return S_OK; }

static const ICoreWebView2CookieVtbl cookie_vtbl =
{
    cookie_QueryInterface, cookie_AddRef, cookie_Release,
    cookie_get_Name, cookie_get_Value, cookie_put_Value, cookie_get_Domain, cookie_get_Path,
    cookie_get_Expires, cookie_put_Expires, cookie_get_IsHttpOnly, cookie_put_IsHttpOnly,
    cookie_get_SameSite, cookie_put_SameSite, cookie_get_IsSecure, cookie_put_IsSecure, cookie_get_IsSession,
};

/* Builds one real ICoreWebView2Cookie (refcount 1) from a unix-side
 * struct unix_cookie (see unixlib.h). File-scope only -- unlike
 * cookie_manager_create, nothing outside this file needs to construct one
 * directly; GetCookies (below) is the only real producer. */
static HRESULT cookie_create_from_unix(const struct unix_cookie *uc, ICoreWebView2Cookie **out)
{
    struct cookie_impl *c = calloc(1, sizeof(*c));
    HRESULT hr;

    if (!c) return E_OUTOFMEMORY;
    c->ICoreWebView2Cookie_iface.lpVtbl = &cookie_vtbl;
    c->ref = 1;

    if (FAILED(hr = copy_out_wstr(uc->name, &c->name)) ||
        FAILED(hr = copy_out_wstr(uc->value, &c->value)) ||
        FAILED(hr = copy_out_wstr(uc->domain, &c->domain)) ||
        FAILED(hr = copy_out_wstr(uc->path, &c->path)))
    {
        ICoreWebView2Cookie_Release(&c->ICoreWebView2Cookie_iface);
        return hr;
    }
    c->expires = uc->expires;
    c->same_site = (COREWEBVIEW2_COOKIE_SAME_SITE_KIND)uc->same_site;
    c->is_session = uc->is_session;
    c->is_http_only = uc->is_http_only;
    c->is_secure = uc->is_secure;

    *out = &c->ICoreWebView2Cookie_iface;
    return S_OK;
}

/* --- Task 11: ICoreWebView2CookieList -- the real result GetCookies hands
 * its completion handler, per real WebView2.h's ICoreWebView2CookieListVtbl
 * (get_Count + GetValueAtIndex, verified real -- see
 * webview2loader_private.h's own comment on this interface). */
struct cookie_list_impl
{
    ICoreWebView2CookieList ICoreWebView2CookieList_iface;
    LONG ref;
    ICoreWebView2Cookie **cookies; /* owns one ref on each entry */
    UINT32 count;
};

static inline struct cookie_list_impl *impl_from_ICoreWebView2CookieList(ICoreWebView2CookieList *iface)
{ return CONTAINING_RECORD(iface, struct cookie_list_impl, ICoreWebView2CookieList_iface); }

static HRESULT WINAPI cookie_list_QueryInterface(ICoreWebView2CookieList *iface, REFIID riid, void **ppv)
{
    if (IsEqualGUID(riid, &IID_IUnknown) || IsEqualGUID(riid, &IID_ICoreWebView2CookieList))
    { *ppv = iface; ICoreWebView2CookieList_AddRef(iface); return S_OK; }
    *ppv = NULL;
    return E_NOINTERFACE;
}
static ULONG WINAPI cookie_list_AddRef(ICoreWebView2CookieList *iface)
{ return InterlockedIncrement(&impl_from_ICoreWebView2CookieList(iface)->ref); }
static ULONG WINAPI cookie_list_Release(ICoreWebView2CookieList *iface)
{
    struct cookie_list_impl *l = impl_from_ICoreWebView2CookieList(iface);
    LONG ref = InterlockedDecrement(&l->ref);
    if (!ref)
    {
        UINT32 i;
        for (i = 0; i < l->count; i++) ICoreWebView2Cookie_Release(l->cookies[i]);
        free(l->cookies);
        free(l);
    }
    return ref;
}
static HRESULT WINAPI cookie_list_get_Count(ICoreWebView2CookieList *iface, UINT32 *value)
{ if (!value) return E_POINTER; *value = impl_from_ICoreWebView2CookieList(iface)->count; return S_OK; }
static HRESULT WINAPI cookie_list_GetValueAtIndex(ICoreWebView2CookieList *iface, UINT32 index, ICoreWebView2Cookie **value)
{
    struct cookie_list_impl *l = impl_from_ICoreWebView2CookieList(iface);
    if (!value) return E_POINTER;
    if (index >= l->count) { *value = NULL; return E_INVALIDARG; }
    ICoreWebView2Cookie_AddRef(l->cookies[index]);
    *value = l->cookies[index];
    return S_OK;
}
static const ICoreWebView2CookieListVtbl cookie_list_vtbl =
{ cookie_list_QueryInterface, cookie_list_AddRef, cookie_list_Release, cookie_list_get_Count, cookie_list_GetValueAtIndex };

/* Takes ownership of `cookies` (an array of `count` already-ref'd
 * ICoreWebView2Cookie* -- one ref each, transferred in, not AddRef'd
 * again here) and wraps it in a real ICoreWebView2CookieList (refcount 1).
 * `cookies` may be NULL iff count == 0. */
static HRESULT cookie_list_create(ICoreWebView2Cookie **cookies, UINT32 count, ICoreWebView2CookieList **out)
{
    struct cookie_list_impl *l = calloc(1, sizeof(*l));
    if (!l) return E_OUTOFMEMORY;
    l->ICoreWebView2CookieList_iface.lpVtbl = &cookie_list_vtbl;
    l->ref = 1;
    l->cookies = cookies;
    l->count = count;
    *out = &l->ICoreWebView2CookieList_iface;
    return S_OK;
}

/* --- Task 11: GetCookies itself -- the real fix. Same
 * async-worker-thread + WEBVIEW2LOADER_UNIX_CALL shape every other "real"
 * WebView2 method in this codebase uses (environment.c's
 * CreateCoreWebView2Controller / webview.c's Navigate are the direct
 * models): PE-side worker thread does the unix call and marshals the
 * result into real COM objects, then invokes the caller's completion
 * handler. */
struct get_cookies_worker_ctx
{
    UINT64 native_handle;
    LPWSTR uri; /* CoTaskMemAlloc'd copy, or NULL -- mirrors webview.c's navigate_worker_ctx::uri */
    ICoreWebView2GetCookiesCompletedHandler *handler;
};

static DWORD WINAPI get_cookies_worker(void *arg)
{
    struct get_cookies_worker_ctx *ctx = arg;
    /* Heap-allocated, not a stack local -- struct get_cookies_params embeds
     * WEBVIEW2LOADER_MAX_COOKIES full struct unix_cookie entries by value
     * (~1.2MB total); see unixlib.h's own comment on that struct for why. */
    struct get_cookies_params *params = calloc(1, sizeof(*params));
    ICoreWebView2Cookie **cookies = NULL;
    ICoreWebView2CookieList *list = NULL;
    HRESULT hr;
    UINT32 i, built = 0;

    if (!params) { hr = E_OUTOFMEMORY; goto invoke; }

    params->handle = ctx->native_handle;
    params->uri = ctx->uri;
    WEBVIEW2LOADER_UNIX_CALL(get_cookies, params);

    if (!params->success) { hr = E_FAIL; goto invoke; }

    if (params->count && !(cookies = calloc(params->count, sizeof(*cookies)))) { hr = E_OUTOFMEMORY; goto invoke; }

    for (i = 0; i < params->count; i++)
    {
        if (FAILED(hr = cookie_create_from_unix(&params->cookies[i], &cookies[i]))) goto fail_partial;
        built = i + 1; /* tracks how many entries fail_partial must release on a later iteration's failure */
    }

    if (FAILED(hr = cookie_list_create(cookies, built, &list)))
        goto fail_partial;

    hr = S_OK;
    goto invoke;

fail_partial:
    /* OOM (or similarly rare failure) partway through building the cookie
     * array -- release what was already built and fail cleanly rather than
     * hand back a silently-truncated list. */
    for (i = 0; i < built; i++) ICoreWebView2Cookie_Release(cookies[i]);
    free(cookies);

invoke:
    ICoreWebView2GetCookiesCompletedHandler_Invoke(ctx->handler, hr, list);
    if (list) ICoreWebView2CookieList_Release(list);
    ICoreWebView2GetCookiesCompletedHandler_Release(ctx->handler);
    CoTaskMemFree(ctx->uri);
    free(params);
    free(ctx);
    return 0;
}

static HRESULT WINAPI cm_GetCookies(ICoreWebView2CookieManager *iface, LPCWSTR uri,
                                     ICoreWebView2GetCookiesCompletedHandler *handler)
{
    struct cookie_manager_impl *cm = impl_from_iface(iface);
    struct get_cookies_worker_ctx *ctx;

    TRACE("(%p, %s, %p)\n", iface, debugstr_w(uri), handler);
    if (!handler) return E_POINTER;
    if (!(ctx = malloc(sizeof(*ctx)))) return E_OUTOFMEMORY;

    ctx->uri = NULL;
    if (uri && FAILED(copy_out_wstr(uri, &ctx->uri))) { free(ctx); return E_OUTOFMEMORY; }
    ctx->native_handle = cm->native_handle;

    ICoreWebView2GetCookiesCompletedHandler_AddRef(handler);
    ctx->handler = handler;

    if (!start_async_work(get_cookies_worker, ctx))
    {
        ICoreWebView2GetCookiesCompletedHandler_Release(handler);
        CoTaskMemFree(ctx->uri);
        free(ctx);
        return E_FAIL;
    }
    return S_OK; /* real WebView2 semantics: returns immediately, result via the completion handler */
}

static HRESULT WINAPI cm_DeleteAllCookies(ICoreWebView2CookieManager *iface)
{
    struct delete_all_cookies_params params = { impl_from_iface(iface)->native_handle };

    TRACE("(%p)\n", iface);
    WEBVIEW2LOADER_UNIX_CALL(delete_all_cookies, &params);
    return S_OK;
}

static const ICoreWebView2CookieManagerVtbl cm_vtbl =
{
    cm_QueryInterface,
    cm_AddRef,
    cm_Release,
    (void *)webview2_stub_e_notimpl, /* CreateCookie */
    (void *)webview2_stub_e_notimpl, /* CopyCookie */
    cm_GetCookies,
    (void *)webview2_stub_e_notimpl, /* AddOrUpdateCookie */
    (void *)webview2_stub_e_notimpl, /* DeleteCookie */
    (void *)webview2_stub_e_notimpl, /* DeleteCookies */
    (void *)webview2_stub_e_notimpl, /* DeleteCookiesWithDomainAndPath */
    cm_DeleteAllCookies,
};

HRESULT cookie_manager_create(UINT64 native_handle, ICoreWebView2CookieManager **out)
{
    struct cookie_manager_impl *cm = calloc(1, sizeof(*cm));
    if (!cm) return E_OUTOFMEMORY;

    cm->ICoreWebView2CookieManager_iface.lpVtbl = &cm_vtbl;
    cm->ref = 1;
    cm->native_handle = native_handle;
    *out = &cm->ICoreWebView2CookieManager_iface;
    return S_OK;
}
