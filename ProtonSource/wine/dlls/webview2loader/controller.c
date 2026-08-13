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

    HWND parent_window;   /* Plan 3 Task 1: real HWND, or HWND_MESSAGE for
                            * the CookieManager flow -- see is_message_only.
                            * NULL is also possible (existing NULL-parent
                            * test callers); treated the same as a real
                            * HWND except geometry sync (Task 4/5) skips it,
                            * since there is no screen origin to compute
                            * against. */
    BOOL is_message_only; /* TRUE iff parent_window == HWND_MESSAGE at
                            * creation time -- cached rather than
                            * recomputed, since HWND_MESSAGE is a value, not
                            * something that can change after the fact. */
    DWORD parent_thread_id; /* Review fix (Important finding, post-Task-5):
                              * the thread id window_hook_track discovered
                              * for parent_window at track time (0 if
                              * tracking was never installed, e.g.
                              * is_message_only/NULL parent, or
                              * SetWindowsHookExW failed). controller_impl
                              * is calloc'd, so this is 0 by default without
                              * needing a separate bool. Saved here rather
                              * than re-derived from parent_window at
                              * destroy time specifically because
                              * parent_window may already be destroyed by
                              * then -- see window_hook_untrack's own
                              * comment in window_sync.c for the
                              * use-after-free this prevents. */

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

/* Plan 3 Task 5: adapter from the WH_CALLWNDPROC hook's void(void*) shape
 * (window_sync_callback) to controller_push_geometry_to_native's
 * ICoreWebView2Controller* parameter -- see window_sync.c's own header
 * comment for why the hook exists at all. */
static void CALLBACK controller_geometry_sync_callback(void *user_data)
{
    controller_push_geometry_to_native((ICoreWebView2Controller *)user_data);
}

/* Tears down the real GTK window/WebKitWebView this controller owns.
 * Idempotent: real WebView2 allows Close() to be called more than once,
 * and Release() at refcount 0 must still clean up if Close() was never
 * called at all -- both paths funnel through here exactly once. */
static void controller_destroy_native(struct controller_impl *ctrl)
{
    struct destroy_webview_params params;

    if (ctrl->destroyed) return;
    ctrl->destroyed = TRUE;

    /* Review fix (Important finding, post-Task-5): pass the thread id
     * captured at track time (ctrl->parent_thread_id), not
     * ctrl->parent_window -- by this point parent_window may already be a
     * destroyed HWND (Studio tearing down its own top-level window before
     * releasing/closing this controller), and re-deriving the thread id
     * from a dead HWND silently fails, leaking the tracked_entry whose
     * user_data points at this ctrl, which is about to be freed. Guarded
     * on parent_thread_id != 0 rather than is_message_only/parent_window
     * again, since that's the actual "was tracking ever installed"
     * signal now (0 covers is_message_only, NULL parent, and a failed
     * SetWindowsHookExW at track time, all in one check). */
    if (ctrl->parent_thread_id)
        window_hook_untrack(ctrl->parent_thread_id, ctrl->parent_window, controller_geometry_sync_callback, &ctrl->ICoreWebView2Controller_iface);

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

/* Plan 3 Task 4: pure arithmetic, no HWND/GTK touched -- client_origin is
 * the parent HWND's client-area screen origin (as ClientToScreen(hwnd,
 * &(POINT){0,0}) would report), client_bounds is the client-relative
 * RECT already received via put_Bounds. Exported as a test-support DLL
 * export (Step 3 below, webview2loader.spec) so the design spec's
 * Testing Strategy item 1 ("pure arithmetic, given known input rects") is
 * exercised without any real window. */
static RECT controller_compute_screen_bounds(POINT client_origin, RECT client_bounds)
{
    RECT out;
    out.left = client_origin.x + client_bounds.left;
    out.top = client_origin.y + client_bounds.top;
    out.right = client_origin.x + client_bounds.right;
    out.bottom = client_origin.y + client_bounds.bottom;
    return out;
}

/* Shared by put_Bounds/put_IsVisible below and by Task 5's WH_CALLWNDPROC
 * hook callback (window_sync.c) -- one place computes the absolute screen
 * rect and issues the sync_window_geometry unix call, so both triggers
 * (an explicit put_Bounds/put_IsVisible call, and the parent HWND itself
 * moving/showing/hiding) stay in lockstep. Takes ICoreWebView2Controller*
 * (the public interface pointer), not struct controller_impl* directly --
 * struct controller_impl is private to this file, but Task 5's
 * window_sync.c needs to trigger this same push from its hook callback
 * without seeing the struct's internals, so this function (declared in
 * webview2loader_private.h, defined here) is this file's one exposed,
 * opaque-pointer entry point for it, mirroring every other cross-file
 * boundary in this DLL (e.g. controller_get_native_handle). */
void controller_push_geometry_to_native(ICoreWebView2Controller *iface)
{
    struct controller_impl *ctrl = impl_from_ICoreWebView2Controller(iface);
    struct sync_window_geometry_params params;
    POINT origin = { 0, 0 };

    /* No real screen position to compute against for either case -- see
     * struct controller_impl's own field comments. Matches this plan's
     * Error Handling section: never fatal, just skip. */
    if (ctrl->is_message_only || !ctrl->parent_window) return;
    if (!ctrl->native_handle) return; /* Close()'d already */

    if (!ClientToScreen(ctrl->parent_window, &origin))
        return; /* parent HWND gone/invalid -- nothing to sync against */

    params.handle = ctrl->native_handle;
    params.screen_bounds = controller_compute_screen_bounds(origin, ctrl->bounds);
    params.visible = ctrl->visible;
    params.success = FALSE;
    WEBVIEW2LOADER_UNIX_CALL(sync_window_geometry, &params);
    /* Failure here degrades to a floating/stale window, matching this
     * plan's own Error Handling section -- never fatal to the controller. */
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
    controller_push_geometry_to_native(iface);
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
    controller_push_geometry_to_native(iface);
    return S_OK;
}

static HRESULT WINAPI controller_get_ParentWindow(ICoreWebView2Controller *iface, HWND *parentWindow)
{
    if (!parentWindow) return E_POINTER;
    *parentWindow = impl_from_ICoreWebView2Controller(iface)->parent_window;
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
    controller_get_ParentWindow, /* was: (void *)webview2_stub_e_notimpl */
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
        controller_get_ParentWindow, /* was: (void *)webview2_stub_e_notimpl */
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

HRESULT controller_create(UINT64 native_handle, HWND parent_window, ICoreWebView2Controller **out)
{
    struct controller_impl *ctrl = calloc(1, sizeof(*ctrl));

    TRACE("(%s, %p, %p)\n", wine_dbgstr_longlong(native_handle), parent_window, out);
    if (!ctrl) return E_OUTOFMEMORY;

    ctrl->ICoreWebView2Controller_iface.lpVtbl = &controller_vtbl;
    ctrl->ref = 1;
    ctrl->native_handle = native_handle;
    ctrl->visible = TRUE;
    ctrl->parent_window = parent_window;
    ctrl->is_message_only = (parent_window == HWND_MESSAGE);
    SetRect(&ctrl->bounds, 0, 0, 800, 600);
    ctrl->default_bg_color.A = 255;
    ctrl->default_bg_color.R = 255;
    ctrl->default_bg_color.G = 255;
    ctrl->default_bg_color.B = 255;
    ctrl->rasterization_scale = 1.0;
    ctrl->should_detect_monitor_scale_changes = TRUE;
    ctrl->bounds_mode = COREWEBVIEW2_BOUNDS_MODE_USE_RAW_PIXELS;
    ctrl->allow_external_drop = TRUE;

    /* Plan 3 Task 5: track this controller's top-level parent for
     * move/show/hide -- only for a real (non-HWND_MESSAGE, non-NULL)
     * parent, matching the design spec's own "installed when a controller
     * with a real parent is created" rule. Failure here is non-fatal
     * (matches this plan's Error Handling section): the controller still
     * works, it just won't track top-level moves independent of explicit
     * put_Bounds calls. */
    if (!ctrl->is_message_only && ctrl->parent_window)
        window_hook_track(ctrl->parent_window, controller_geometry_sync_callback, &ctrl->ICoreWebView2Controller_iface,
                           &ctrl->parent_thread_id);

    *out = &ctrl->ICoreWebView2Controller_iface;
    return S_OK;
}

UINT64 controller_get_native_handle(ICoreWebView2Controller *iface)
{
    return impl_from_ICoreWebView2Controller(iface)->native_handle;
}

/* Test-support-only export (Plan 3 Task 4) -- exercises the pure
 * client-origin + client-bounds composition arithmetic without any real
 * HWND, per the design spec's own Testing Strategy item 1. */
void WINAPI __wine_test_webview2loader_compute_screen_bounds(const POINT *client_origin, const RECT *client_bounds,
                                                                RECT *out)
{
    *out = controller_compute_screen_bounds(*client_origin, *client_bounds);
}
