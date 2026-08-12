#include <stdarg.h>
#include <stdlib.h>

#include <windef.h>
#include <winbase.h>
#include <winuser.h>
#include <wine/debug.h>

#include "unixlib.h"
#include "webview2loader_private.h"

WINE_DEFAULT_DEBUG_CHANNEL(webview2loader);

struct controller_impl
{
    ICoreWebView2Controller ICoreWebView2Controller_iface;
    LONG ref;
    UINT64 native_handle;
    BOOL visible;
    RECT bounds;
    BOOL destroyed; /* guards native_destroy() against running twice --
                      * once via an explicit Close() and again from
                      * Release() hitting refcount 0, or two Close() calls
                      * (real WebView2's Close() is documented idempotent) */
    ICoreWebView2 *webview; /* created lazily by get_CoreWebView2, Task 7 */
};

/* Tears down the real GTK window/WebKitWebView this controller owns.
 * Idempotent: real WebView2 allows Close() to be called more than once,
 * and Release() at refcount 0 must still clean up if Close() was never
 * called at all -- both paths funnel through here exactly once. */
static void controller_destroy_native(struct controller_impl *ctrl)
{
    struct destroy_webview_params params;

    if (ctrl->destroyed) return;
    ctrl->destroyed = TRUE;

    params.handle = ctrl->native_handle;
    WEBVIEW2LOADER_UNIX_CALL(destroy_webview, &params);
}

static inline struct controller_impl *impl_from_ICoreWebView2Controller(ICoreWebView2Controller *iface)
{
    return CONTAINING_RECORD(iface, struct controller_impl, ICoreWebView2Controller_iface);
}

static HRESULT WINAPI controller_QueryInterface(ICoreWebView2Controller *iface, REFIID riid, void **ppv)
{
    if (IsEqualGUID(riid, &IID_IUnknown) || IsEqualGUID(riid, &IID_ICoreWebView2Controller))
    {
        *ppv = iface;
        ICoreWebView2Controller_AddRef(iface);
        return S_OK;
    }
    *ppv = NULL;
    return E_NOINTERFACE;
}

static ULONG WINAPI controller_AddRef(ICoreWebView2Controller *iface)
{
    return InterlockedIncrement(&impl_from_ICoreWebView2Controller(iface)->ref);
}

static ULONG WINAPI controller_Release(ICoreWebView2Controller *iface)
{
    struct controller_impl *ctrl = impl_from_ICoreWebView2Controller(iface);
    LONG ref = InterlockedDecrement(&ctrl->ref);
    if (!ref)
    {
        if (ctrl->webview) ICoreWebView2_Release(ctrl->webview);
        controller_destroy_native(ctrl);
        free(ctrl);
    }
    return ref;
}

static HRESULT WINAPI controller_get_IsVisible(ICoreWebView2Controller *iface, BOOL *isVisible)
{
    if (!isVisible) return E_POINTER;
    *isVisible = impl_from_ICoreWebView2Controller(iface)->visible;
    return S_OK;
}

static HRESULT WINAPI controller_put_IsVisible(ICoreWebView2Controller *iface, BOOL isVisible)
{
    impl_from_ICoreWebView2Controller(iface)->visible = isVisible;
    /* Task 3 of Plan 3 (window-sync) is what actually shows/hides the
     * native window in step with Studio's HWND; here we just track state,
     * matching this plan's explicit non-goal of HWND-level embedding. */
    return S_OK;
}

static HRESULT WINAPI controller_get_Bounds(ICoreWebView2Controller *iface, RECT *bounds)
{
    if (!bounds) return E_POINTER;
    *bounds = impl_from_ICoreWebView2Controller(iface)->bounds;
    return S_OK;
}

static HRESULT WINAPI controller_put_Bounds(ICoreWebView2Controller *iface, RECT bounds)
{
    impl_from_ICoreWebView2Controller(iface)->bounds = bounds;
    return S_OK;
}

static HRESULT WINAPI controller_Close(ICoreWebView2Controller *iface)
{
    TRACE("(%p)\n", iface);
    controller_destroy_native(impl_from_ICoreWebView2Controller(iface));
    return S_OK;
}

static HRESULT WINAPI controller_get_CoreWebView2(ICoreWebView2Controller *iface, ICoreWebView2 **webview)
{
    struct controller_impl *ctrl = impl_from_ICoreWebView2Controller(iface);
    HRESULT hr;

    if (!webview) return E_POINTER;
    if (!ctrl->webview && FAILED(hr = webview_create(ctrl->native_handle, &ctrl->webview)))
        return hr;

    ICoreWebView2_AddRef(ctrl->webview);
    *webview = ctrl->webview;
    return S_OK;
}

static const ICoreWebView2ControllerVtbl controller_vtbl =
{
    controller_QueryInterface,
    controller_AddRef,
    controller_Release,
    controller_get_IsVisible,
    controller_put_IsVisible,
    controller_get_Bounds,
    controller_put_Bounds,
    (void *)webview2_stub_e_notimpl, /* get_ZoomFactor */
    (void *)webview2_stub_e_notimpl, /* put_ZoomFactor */
    (void *)webview2_stub_e_notimpl, /* add_ZoomFactorChanged */
    (void *)webview2_stub_e_notimpl, /* remove_ZoomFactorChanged */
    (void *)webview2_stub_e_notimpl, /* SetBoundsAndZoomFactor */
    (void *)webview2_stub_e_notimpl, /* MoveFocus */
    (void *)webview2_stub_e_notimpl, /* add_MoveFocusRequested */
    (void *)webview2_stub_e_notimpl, /* remove_MoveFocusRequested */
    (void *)webview2_stub_e_notimpl, /* add_GotFocus */
    (void *)webview2_stub_e_notimpl, /* remove_GotFocus */
    (void *)webview2_stub_e_notimpl, /* add_LostFocus */
    (void *)webview2_stub_e_notimpl, /* remove_LostFocus */
    (void *)webview2_stub_e_notimpl, /* add_AcceleratorKeyPressed */
    (void *)webview2_stub_e_notimpl, /* remove_AcceleratorKeyPressed */
    (void *)webview2_stub_e_notimpl, /* get_ParentWindow */
    (void *)webview2_stub_e_notimpl, /* put_ParentWindow */
    (void *)webview2_stub_e_notimpl, /* NotifyParentWindowPositionChanged */
    controller_Close,
    controller_get_CoreWebView2,
};

HRESULT controller_create(UINT64 native_handle, ICoreWebView2Controller **out)
{
    struct controller_impl *ctrl = calloc(1, sizeof(*ctrl));

    TRACE("(%s, %p)\n", wine_dbgstr_longlong(native_handle), out);
    if (!ctrl) return E_OUTOFMEMORY;

    ctrl->ICoreWebView2Controller_iface.lpVtbl = &controller_vtbl;
    ctrl->ref = 1;
    ctrl->native_handle = native_handle;
    ctrl->visible = TRUE;
    SetRect(&ctrl->bounds, 0, 0, 800, 600);
    *out = &ctrl->ICoreWebView2Controller_iface;
    return S_OK;
}

UINT64 controller_get_native_handle(ICoreWebView2Controller *iface)
{
    return impl_from_ICoreWebView2Controller(iface)->native_handle;
}
