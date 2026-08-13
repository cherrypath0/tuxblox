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

    /* Task 11: Controller2/3/4 state-only properties -- see the extension
     * vtable's own comment in webview2loader_private.h for why these get
     * real bodies instead of E_NOTIMPL. Defaults match real WebView2's own
     * documented defaults, not arbitrary picks. */
    COREWEBVIEW2_COLOR default_bg_color;   /* real default: opaque white */
    double rasterization_scale;            /* real default: 1.0 */
    BOOL should_detect_monitor_scale_changes; /* real default: TRUE */
    COREWEBVIEW2_BOUNDS_MODE bounds_mode;  /* matches this controller's
                                             * existing raw-pixel put_Bounds
                                             * behavior */
    BOOL allow_external_drop;              /* real default: TRUE */
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

    /* Final-review fix (Important 1, native_handle use-after-free): the
     * unix call above just freed the unix-side native_webview struct
     * ctrl->native_handle points at. Zero this controller's own copy, and
     * invalidate ctrl->webview's copy too (a distinct object, created
     * lazily by get_CoreWebView2) -- every ICoreWebView2CookieManager ever
     * handed out from that webview reads its handle live from it rather
     * than caching one (see cookie_manager_create), so invalidating the
     * webview here also covers every outstanding cookie manager. Any call
     * that reaches the unix side after this point (Navigate, GetCookies,
     * cookie ops, get_Source, ...) now sees handle 0, which every unix
     * call already treats as STATUS_INVALID_HANDLE -- a clean failure
     * instead of a dereference of freed unix-side memory. */
    ctrl->native_handle = 0;
    if (ctrl->webview) webview_invalidate_native_handle(ctrl->webview);
}

static inline struct controller_impl *impl_from_ICoreWebView2Controller(ICoreWebView2Controller *iface)
{
    return CONTAINING_RECORD(iface, struct controller_impl, ICoreWebView2Controller_iface);
}

/* Body defined further down (after controller4_vtbl, which it references),
 * same forward-declare-the-prototype-only pattern webview.c already uses
 * for webview_QueryInterface/webview2_2_vtbl. */
static HRESULT WINAPI controller_QueryInterface(ICoreWebView2Controller *iface, REFIID riid, void **ppv);

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
        /* Must run before releasing ctrl->webview below: controller_destroy_native
         * (Final-review fix, Important 1) invalidates ctrl->webview's own
         * native_handle copy via webview_invalidate_native_handle, which needs
         * ctrl->webview to still be a live object. Releasing it first could drop
         * its refcount to 0 right here (if no other caller is holding a
         * reference of its own) and free it, making the invalidation call
         * afterward a dereference of freed memory -- swapping the order avoids
         * that regardless of whether any external reference is outstanding. */
        controller_destroy_native(ctrl);
        if (ctrl->webview) ICoreWebView2_Release(ctrl->webview);
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

/* --- Task 11: ICoreWebView2Controller2/3/4 extension --- */

static HRESULT WINAPI controller4_get_DefaultBackgroundColor(ICoreWebView2Controller *iface, COREWEBVIEW2_COLOR *value)
{
    if (!value) return E_POINTER;
    *value = impl_from_ICoreWebView2Controller(iface)->default_bg_color;
    return S_OK;
}

static HRESULT WINAPI controller4_put_DefaultBackgroundColor(ICoreWebView2Controller *iface, COREWEBVIEW2_COLOR value)
{
    impl_from_ICoreWebView2Controller(iface)->default_bg_color = value;
    return S_OK;
}

static HRESULT WINAPI controller4_get_RasterizationScale(ICoreWebView2Controller *iface, double *scale)
{
    if (!scale) return E_POINTER;
    *scale = impl_from_ICoreWebView2Controller(iface)->rasterization_scale;
    return S_OK;
}

static HRESULT WINAPI controller4_put_RasterizationScale(ICoreWebView2Controller *iface, double scale)
{
    impl_from_ICoreWebView2Controller(iface)->rasterization_scale = scale;
    return S_OK;
}

static HRESULT WINAPI controller4_get_ShouldDetectMonitorScaleChanges(ICoreWebView2Controller *iface, BOOL *value)
{
    if (!value) return E_POINTER;
    *value = impl_from_ICoreWebView2Controller(iface)->should_detect_monitor_scale_changes;
    return S_OK;
}

static HRESULT WINAPI controller4_put_ShouldDetectMonitorScaleChanges(ICoreWebView2Controller *iface, BOOL value)
{
    impl_from_ICoreWebView2Controller(iface)->should_detect_monitor_scale_changes = value;
    return S_OK;
}

static HRESULT WINAPI controller4_get_BoundsMode(ICoreWebView2Controller *iface, COREWEBVIEW2_BOUNDS_MODE *boundsMode)
{
    if (!boundsMode) return E_POINTER;
    *boundsMode = impl_from_ICoreWebView2Controller(iface)->bounds_mode;
    return S_OK;
}

static HRESULT WINAPI controller4_put_BoundsMode(ICoreWebView2Controller *iface, COREWEBVIEW2_BOUNDS_MODE boundsMode)
{
    impl_from_ICoreWebView2Controller(iface)->bounds_mode = boundsMode;
    return S_OK;
}

static HRESULT WINAPI controller4_get_AllowExternalDrop(ICoreWebView2Controller *iface, BOOL *value)
{
    if (!value) return E_POINTER;
    *value = impl_from_ICoreWebView2Controller(iface)->allow_external_drop;
    return S_OK;
}

static HRESULT WINAPI controller4_put_AllowExternalDrop(ICoreWebView2Controller *iface, BOOL value)
{
    impl_from_ICoreWebView2Controller(iface)->allow_external_drop = value;
    return S_OK;
}

/* base must be a verbatim copy of controller_vtbl above (26 entries) --
 * see webview2_2_vtbl's own fix note in webview.c for what happens when
 * this copy silently drifts short. */
static const struct webview2_controller4_vtbl_combined controller4_vtbl =
{
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
    },
    {
        controller4_get_DefaultBackgroundColor,
        controller4_put_DefaultBackgroundColor,
        controller4_get_RasterizationScale,
        controller4_put_RasterizationScale,
        controller4_get_ShouldDetectMonitorScaleChanges,
        controller4_put_ShouldDetectMonitorScaleChanges,
        (void *)webview2_stub_e_notimpl, /* add_RasterizationScaleChanged */
        (void *)webview2_stub_e_notimpl, /* remove_RasterizationScaleChanged */
        controller4_get_BoundsMode,
        controller4_put_BoundsMode,
        controller4_get_AllowExternalDrop,
        controller4_put_AllowExternalDrop,
    },
};

static HRESULT WINAPI controller_QueryInterface(ICoreWebView2Controller *iface, REFIID riid, void **ppv)
{
    if (IsEqualGUID(riid, &IID_IUnknown) || IsEqualGUID(riid, &IID_ICoreWebView2Controller))
    {
        *ppv = iface;
        ICoreWebView2Controller_AddRef(iface);
        return S_OK;
    }
    if (IsEqualGUID(riid, &IID_ICoreWebView2Controller4))
    {
        /* Real Roblox Studio QueryInterfaces for exactly this IID right
         * after a successful CreateCoreWebView2Controller for the embedded
         * login dialog -- see the extension vtable's own comment in
         * webview2loader_private.h. Re-point lpVtbl at the combined table,
         * same safe swap technique as webview_query_interface_v2 uses for
         * ICoreWebView2_2 (the combined struct's `base` member is
         * layout-identical to ICoreWebView2ControllerVtbl). */
        struct controller_impl *ctrl = impl_from_ICoreWebView2Controller(iface);
        ctrl->ICoreWebView2Controller_iface.lpVtbl = (const ICoreWebView2ControllerVtbl *)&controller4_vtbl;
        *ppv = iface;
        ICoreWebView2Controller_AddRef(iface);
        return S_OK;
    }
    WARN("no interface for %s\n", debugstr_guid(riid));
    *ppv = NULL;
    return E_NOINTERFACE;
}

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
    /* Task 11: real WebView2 documented defaults for the Controller2/3/4
     * properties -- see the field comments on struct controller_impl. */
    ctrl->default_bg_color.A = 255;
    ctrl->default_bg_color.R = 255;
    ctrl->default_bg_color.G = 255;
    ctrl->default_bg_color.B = 255;
    ctrl->rasterization_scale = 1.0;
    ctrl->should_detect_monitor_scale_changes = TRUE;
    ctrl->bounds_mode = COREWEBVIEW2_BOUNDS_MODE_USE_RAW_PIXELS;
    ctrl->allow_external_drop = TRUE;
    *out = &ctrl->ICoreWebView2Controller_iface;
    return S_OK;
}

UINT64 controller_get_native_handle(ICoreWebView2Controller *iface)
{
    return impl_from_ICoreWebView2Controller(iface)->native_handle;
}
