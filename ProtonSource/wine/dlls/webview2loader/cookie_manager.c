#include <stdarg.h>
#include <stdlib.h>

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
    (void *)webview2_stub_e_notimpl, /* GetCookies */
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
