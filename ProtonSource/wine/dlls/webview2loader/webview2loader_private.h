#ifndef __WINE_WEBVIEW2LOADER_PRIVATE_H
#define __WINE_WEBVIEW2LOADER_PRIVATE_H

#include <windef.h>
#include <winbase.h>
#include <objbase.h>

/* Fire-and-forget worker thread: runs proc(ctx) on a new thread and
 * immediately closes the handle (we never join; each proc is responsible
 * for freeing ctx and, for the three async-completion call sites, for
 * invoking the caller's completion handler itself before returning).
 * Shared by every "async" WebView2 entry point (CreateCoreWebView2Environment-
 * WithOptions, CreateCoreWebView2Controller, Navigate) since real WebView2
 * returns immediately and reports completion via a callback/event later. */
BOOL start_async_work(LPTHREAD_START_ROUTINE proc, void *ctx);

/* Runs unix_init on the unixlib side: dlopens the bundled GLib/GObject/
 * GTK4/WebKitGTK-6.0 libraries, sets the WebKitGTK relocation env vars, and
 * starts the dedicated GTK main-loop thread. Returns TRUE on success (the
 * bundle loaded and the GTK thread is up and ready), FALSE otherwise (e.g.
 * TUXBLOX_WEBVIEW_DIR unset, or the bundle failed to dlopen/dlsym). */
BOOL webview2loader_unix_init(void);

/* Task 5: ICoreWebView2Environment.
 *
 * Deviation from the task brief: the brief's Step 1 has this block start
 * with "#define COBJMACROS" + "#include <wine/wtypes.h>". This fork has no
 * such header (only include/wtypes.idl, widl-generated at build time into
 * wtypes.h). REFIID/GUID/DEFINE_GUID/IsEqualGUID/IID_IUnknown are already
 * reachable here via the <objbase.h> include above (objbase.h pulls in
 * <unknwn.h>, whose generated form itself pulls in the generated
 * <wtypes.h>) -- so nothing extra needs including for those. COBJMACROS is
 * kept, but since these two interfaces are hand-written here rather than
 * widl-generated from an .idl file, the "#ifdef COBJMACROS" call macros
 * widl would normally emit don't exist automatically; they're written out
 * by hand below, following the exact expansion widl itself uses
 * (tools/widl/header.c:943 -- "(This)->lpVtbl->Method(This,args)"). */
#define COBJMACROS

typedef struct ICoreWebView2Environment ICoreWebView2Environment;
typedef struct ICoreWebView2EnvironmentVtbl
{
    HRESULT (WINAPI *QueryInterface)(ICoreWebView2Environment *This, REFIID riid, void **ppv);
    ULONG   (WINAPI *AddRef)(ICoreWebView2Environment *This);
    ULONG   (WINAPI *Release)(ICoreWebView2Environment *This);
    HRESULT (WINAPI *CreateCoreWebView2Controller)(ICoreWebView2Environment *This, HWND parentWindow, void *handler);
    HRESULT (WINAPI *CreateWebResourceResponse)(ICoreWebView2Environment *This, void *content, int statusCode,
                                                  LPCWSTR reasonPhrase, LPCWSTR headers, void **response);
    HRESULT (WINAPI *get_BrowserVersionString)(ICoreWebView2Environment *This, LPWSTR *versionInfo);
    HRESULT (WINAPI *add_NewBrowserVersionAvailable)(ICoreWebView2Environment *This, void *eventHandler, void *token);
    HRESULT (WINAPI *remove_NewBrowserVersionAvailable)(ICoreWebView2Environment *This, void *token);
} ICoreWebView2EnvironmentVtbl;
struct ICoreWebView2Environment { const ICoreWebView2EnvironmentVtbl *lpVtbl; };

#ifdef COBJMACROS
#define ICoreWebView2Environment_QueryInterface(This,riid,ppv) (This)->lpVtbl->QueryInterface(This,riid,ppv)
#define ICoreWebView2Environment_AddRef(This) (This)->lpVtbl->AddRef(This)
#define ICoreWebView2Environment_Release(This) (This)->lpVtbl->Release(This)
#define ICoreWebView2Environment_CreateCoreWebView2Controller(This,parentWindow,handler) \
    (This)->lpVtbl->CreateCoreWebView2Controller(This,parentWindow,handler)
#define ICoreWebView2Environment_CreateWebResourceResponse(This,content,statusCode,reasonPhrase,headers,response) \
    (This)->lpVtbl->CreateWebResourceResponse(This,content,statusCode,reasonPhrase,headers,response)
#define ICoreWebView2Environment_get_BrowserVersionString(This,versionInfo) \
    (This)->lpVtbl->get_BrowserVersionString(This,versionInfo)
#define ICoreWebView2Environment_add_NewBrowserVersionAvailable(This,eventHandler,token) \
    (This)->lpVtbl->add_NewBrowserVersionAvailable(This,eventHandler,token)
#define ICoreWebView2Environment_remove_NewBrowserVersionAvailable(This,token) \
    (This)->lpVtbl->remove_NewBrowserVersionAvailable(This,token)
#endif

DEFINE_GUID(IID_ICoreWebView2Environment, 0xb96d755e, 0x0319, 0x4e92, 0xa2, 0x96, 0x23, 0x43, 0x6f, 0x46, 0xa1, 0xfc);
DEFINE_GUID(IID_ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler,
            0x4e8a3389, 0xc9d8, 0x4bd2, 0xb6, 0xb5, 0x12, 0x4f, 0xee, 0x6c, 0xc1, 0x4d);

/* Real environment-completed-handler interface (single Invoke method).
 * Roblox's own vtable, called back into -- never implemented by us. */
typedef struct ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler
    ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler;
typedef struct ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandlerVtbl
{
    HRESULT (WINAPI *QueryInterface)(ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler *This, REFIID riid, void **ppv);
    ULONG   (WINAPI *AddRef)(ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler *This);
    ULONG   (WINAPI *Release)(ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler *This);
    HRESULT (WINAPI *Invoke)(ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler *This, HRESULT errorCode,
                              ICoreWebView2Environment *result);
} ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandlerVtbl;
struct ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler
{ const ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandlerVtbl *lpVtbl; };

#ifdef COBJMACROS
#define ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler_QueryInterface(This,riid,ppv) \
    (This)->lpVtbl->QueryInterface(This,riid,ppv)
#define ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler_AddRef(This) (This)->lpVtbl->AddRef(This)
#define ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler_Release(This) (This)->lpVtbl->Release(This)
#define ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler_Invoke(This,errorCode,result) \
    (This)->lpVtbl->Invoke(This,errorCode,result)
#endif

/* Constructs an ICoreWebView2Environment (refcount 1). */
HRESULT environment_create(ICoreWebView2Environment **out);

/* Task 6: ICoreWebView2Controller.
 *
 * ICoreWebView2 itself is only forward-declared -- its full ~40-method
 * vtable is Task 7's deliverable, not this task's. But controller_Release
 * and controller_get_CoreWebView2 (controller.c) need to AddRef/Release a
 * cached ICoreWebView2*, and this header's hand-written-COBJMACROS
 * convention (see the Task 5 comment above) expands to "(This)->lpVtbl->
 * Method(This,...)" -- which needs an actual lpVtbl member to compile
 * against, not just an opaque incomplete type. Define the minimal 3-slot
 * vtbl every COM interface starts with (QueryInterface/AddRef/Release, in
 * that fixed order -- COM's IUnknown-layout-compatibility guarantee) so
 * ICoreWebView2_AddRef/_Release below compile now. Task 7 replaces this
 * whole typedef block with the real, full vtbl, keeping these same first
 * three slots, so nothing written against this partial definition needs to
 * change when it does. */
typedef struct ICoreWebView2 ICoreWebView2;
typedef struct ICoreWebView2Vtbl
{
    HRESULT (WINAPI *QueryInterface)(ICoreWebView2 *This, REFIID riid, void **ppv);
    ULONG   (WINAPI *AddRef)(ICoreWebView2 *This);
    ULONG   (WINAPI *Release)(ICoreWebView2 *This);
} ICoreWebView2Vtbl;
struct ICoreWebView2 { const ICoreWebView2Vtbl *lpVtbl; };

#ifdef COBJMACROS
#define ICoreWebView2_QueryInterface(This,riid,ppv) (This)->lpVtbl->QueryInterface(This,riid,ppv)
#define ICoreWebView2_AddRef(This) (This)->lpVtbl->AddRef(This)
#define ICoreWebView2_Release(This) (This)->lpVtbl->Release(This)
#endif

typedef struct ICoreWebView2Controller ICoreWebView2Controller;
typedef struct ICoreWebView2ControllerVtbl
{
    HRESULT (WINAPI *QueryInterface)(ICoreWebView2Controller *This, REFIID riid, void **ppv);
    ULONG   (WINAPI *AddRef)(ICoreWebView2Controller *This);
    ULONG   (WINAPI *Release)(ICoreWebView2Controller *This);
    HRESULT (WINAPI *get_IsVisible)(ICoreWebView2Controller *This, BOOL *isVisible);
    HRESULT (WINAPI *put_IsVisible)(ICoreWebView2Controller *This, BOOL isVisible);
    HRESULT (WINAPI *get_Bounds)(ICoreWebView2Controller *This, RECT *bounds);
    HRESULT (WINAPI *put_Bounds)(ICoreWebView2Controller *This, RECT bounds);
    HRESULT (WINAPI *get_ZoomFactor)(ICoreWebView2Controller *This, double *zoomFactor);
    HRESULT (WINAPI *put_ZoomFactor)(ICoreWebView2Controller *This, double zoomFactor);
    HRESULT (WINAPI *add_ZoomFactorChanged)(ICoreWebView2Controller *This, void *eventHandler, void *token);
    HRESULT (WINAPI *remove_ZoomFactorChanged)(ICoreWebView2Controller *This, void *token);
    HRESULT (WINAPI *SetBoundsAndZoomFactor)(ICoreWebView2Controller *This, RECT bounds, double zoomFactor);
    HRESULT (WINAPI *MoveFocus)(ICoreWebView2Controller *This, int reason);
    HRESULT (WINAPI *add_MoveFocusRequested)(ICoreWebView2Controller *This, void *eventHandler, void *token);
    HRESULT (WINAPI *remove_MoveFocusRequested)(ICoreWebView2Controller *This, void *token);
    HRESULT (WINAPI *add_GotFocus)(ICoreWebView2Controller *This, void *eventHandler, void *token);
    HRESULT (WINAPI *remove_GotFocus)(ICoreWebView2Controller *This, void *token);
    HRESULT (WINAPI *add_LostFocus)(ICoreWebView2Controller *This, void *eventHandler, void *token);
    HRESULT (WINAPI *remove_LostFocus)(ICoreWebView2Controller *This, void *token);
    HRESULT (WINAPI *add_AcceleratorKeyPressed)(ICoreWebView2Controller *This, void *eventHandler, void *token);
    HRESULT (WINAPI *remove_AcceleratorKeyPressed)(ICoreWebView2Controller *This, void *token);
    HRESULT (WINAPI *get_ParentWindow)(ICoreWebView2Controller *This, HWND *parentWindow);
    HRESULT (WINAPI *put_ParentWindow)(ICoreWebView2Controller *This, HWND parentWindow);
    HRESULT (WINAPI *NotifyParentWindowPositionChanged)(ICoreWebView2Controller *This);
    HRESULT (WINAPI *Close)(ICoreWebView2Controller *This);
    HRESULT (WINAPI *get_CoreWebView2)(ICoreWebView2Controller *This, ICoreWebView2 **webview);
} ICoreWebView2ControllerVtbl;
struct ICoreWebView2Controller { const ICoreWebView2ControllerVtbl *lpVtbl; };

#ifdef COBJMACROS
#define ICoreWebView2Controller_QueryInterface(This,riid,ppv) (This)->lpVtbl->QueryInterface(This,riid,ppv)
#define ICoreWebView2Controller_AddRef(This) (This)->lpVtbl->AddRef(This)
#define ICoreWebView2Controller_Release(This) (This)->lpVtbl->Release(This)
#define ICoreWebView2Controller_get_IsVisible(This,isVisible) (This)->lpVtbl->get_IsVisible(This,isVisible)
#define ICoreWebView2Controller_put_IsVisible(This,isVisible) (This)->lpVtbl->put_IsVisible(This,isVisible)
#define ICoreWebView2Controller_get_Bounds(This,bounds) (This)->lpVtbl->get_Bounds(This,bounds)
#define ICoreWebView2Controller_put_Bounds(This,bounds) (This)->lpVtbl->put_Bounds(This,bounds)
#define ICoreWebView2Controller_get_ZoomFactor(This,zoomFactor) (This)->lpVtbl->get_ZoomFactor(This,zoomFactor)
#define ICoreWebView2Controller_put_ZoomFactor(This,zoomFactor) (This)->lpVtbl->put_ZoomFactor(This,zoomFactor)
#define ICoreWebView2Controller_add_ZoomFactorChanged(This,eventHandler,token) \
    (This)->lpVtbl->add_ZoomFactorChanged(This,eventHandler,token)
#define ICoreWebView2Controller_remove_ZoomFactorChanged(This,token) \
    (This)->lpVtbl->remove_ZoomFactorChanged(This,token)
#define ICoreWebView2Controller_SetBoundsAndZoomFactor(This,bounds,zoomFactor) \
    (This)->lpVtbl->SetBoundsAndZoomFactor(This,bounds,zoomFactor)
#define ICoreWebView2Controller_MoveFocus(This,reason) (This)->lpVtbl->MoveFocus(This,reason)
#define ICoreWebView2Controller_add_MoveFocusRequested(This,eventHandler,token) \
    (This)->lpVtbl->add_MoveFocusRequested(This,eventHandler,token)
#define ICoreWebView2Controller_remove_MoveFocusRequested(This,token) \
    (This)->lpVtbl->remove_MoveFocusRequested(This,token)
#define ICoreWebView2Controller_add_GotFocus(This,eventHandler,token) \
    (This)->lpVtbl->add_GotFocus(This,eventHandler,token)
#define ICoreWebView2Controller_remove_GotFocus(This,token) (This)->lpVtbl->remove_GotFocus(This,token)
#define ICoreWebView2Controller_add_LostFocus(This,eventHandler,token) \
    (This)->lpVtbl->add_LostFocus(This,eventHandler,token)
#define ICoreWebView2Controller_remove_LostFocus(This,token) (This)->lpVtbl->remove_LostFocus(This,token)
#define ICoreWebView2Controller_add_AcceleratorKeyPressed(This,eventHandler,token) \
    (This)->lpVtbl->add_AcceleratorKeyPressed(This,eventHandler,token)
#define ICoreWebView2Controller_remove_AcceleratorKeyPressed(This,token) \
    (This)->lpVtbl->remove_AcceleratorKeyPressed(This,token)
#define ICoreWebView2Controller_get_ParentWindow(This,parentWindow) (This)->lpVtbl->get_ParentWindow(This,parentWindow)
#define ICoreWebView2Controller_put_ParentWindow(This,parentWindow) (This)->lpVtbl->put_ParentWindow(This,parentWindow)
#define ICoreWebView2Controller_NotifyParentWindowPositionChanged(This) \
    (This)->lpVtbl->NotifyParentWindowPositionChanged(This)
#define ICoreWebView2Controller_Close(This) (This)->lpVtbl->Close(This)
#define ICoreWebView2Controller_get_CoreWebView2(This,webview) (This)->lpVtbl->get_CoreWebView2(This,webview)
#endif

DEFINE_GUID(IID_ICoreWebView2Controller, 0x4d00c0d1, 0x9434, 0x4eb6, 0x80, 0x78, 0x86, 0x97, 0xa5, 0x60, 0x33, 0x4f);
DEFINE_GUID(IID_ICoreWebView2CreateCoreWebView2ControllerCompletedHandler,
            0x6c4819f3, 0xc9b7, 0x4260, 0x81, 0x27, 0xc9, 0xf5, 0xbd, 0xe7, 0xf6, 0x8c);

typedef struct ICoreWebView2CreateCoreWebView2ControllerCompletedHandler
    ICoreWebView2CreateCoreWebView2ControllerCompletedHandler;
typedef struct ICoreWebView2CreateCoreWebView2ControllerCompletedHandlerVtbl
{
    HRESULT (WINAPI *QueryInterface)(ICoreWebView2CreateCoreWebView2ControllerCompletedHandler *This, REFIID riid, void **ppv);
    ULONG   (WINAPI *AddRef)(ICoreWebView2CreateCoreWebView2ControllerCompletedHandler *This);
    ULONG   (WINAPI *Release)(ICoreWebView2CreateCoreWebView2ControllerCompletedHandler *This);
    HRESULT (WINAPI *Invoke)(ICoreWebView2CreateCoreWebView2ControllerCompletedHandler *This, HRESULT errorCode,
                              ICoreWebView2Controller *result);
} ICoreWebView2CreateCoreWebView2ControllerCompletedHandlerVtbl;
struct ICoreWebView2CreateCoreWebView2ControllerCompletedHandler
{ const ICoreWebView2CreateCoreWebView2ControllerCompletedHandlerVtbl *lpVtbl; };

#ifdef COBJMACROS
#define ICoreWebView2CreateCoreWebView2ControllerCompletedHandler_QueryInterface(This,riid,ppv) \
    (This)->lpVtbl->QueryInterface(This,riid,ppv)
#define ICoreWebView2CreateCoreWebView2ControllerCompletedHandler_AddRef(This) (This)->lpVtbl->AddRef(This)
#define ICoreWebView2CreateCoreWebView2ControllerCompletedHandler_Release(This) (This)->lpVtbl->Release(This)
#define ICoreWebView2CreateCoreWebView2ControllerCompletedHandler_Invoke(This,errorCode,result) \
    (This)->lpVtbl->Invoke(This,errorCode,result)
#endif

/* Constructs an ICoreWebView2Controller (refcount 1) wrapping the unix-side
 * native_handle returned by the unix_create_webview call. */
HRESULT controller_create(UINT64 native_handle, ICoreWebView2Controller **out);

/* webview_create_for_controller: Task 7 calls this from webview.c to build
 * the real ICoreWebView2 lazily and cache it on the controller, since
 * get_CoreWebView2 needs somewhere to store it. */
void controller_set_webview(ICoreWebView2Controller *iface, ICoreWebView2 *webview);
UINT64 controller_get_native_handle(ICoreWebView2Controller *iface);

/* Generic ignore-args E_NOTIMPL stub, cast to whatever vtable slot type is
 * needed. Deliberate: with 20+ genuinely-unimplemented methods per
 * interface (real WebView2 has ~50 on ICoreWebView2 alone), writing a
 * distinct 3-line function per slot adds nothing -- none of them read their
 * arguments, and every real caller invokes them through the correctly-typed
 * vtable slot, so the mismatched declared signature is never observed. */
HRESULT WINAPI webview2_stub_e_notimpl(void *iface, ...);

/* Interface/object definitions land here in later tasks:
 * Task 7: ICoreWebView2 (full vtbl, replacing the partial one above)
 * Task 8: ICoreWebView2_2 / ICoreWebView2CookieManager
 */

#endif /* __WINE_WEBVIEW2LOADER_PRIVATE_H */
