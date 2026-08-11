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

/* Interface/object definitions land here in later tasks:
 * Task 6: ICoreWebView2Controller
 * Task 7: ICoreWebView2
 * Task 8: ICoreWebView2_2 / ICoreWebView2CookieManager
 */

#endif /* __WINE_WEBVIEW2LOADER_PRIVATE_H */
