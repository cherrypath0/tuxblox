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

/* Task 7: ICoreWebView2 -- the real, full vtable (replacing Task 6's
 * 3-slot QueryInterface/AddRef/Release-only placeholder, which existed
 * only so controller.c's ICoreWebView2_AddRef/_Release macros had an
 * lpVtbl member to compile against before this interface's real shape was
 * defined). Slot order verified against the real WebView2.h
 * (ICoreWebView2Vtbl, 53 methods beyond IUnknown). Only get_Source,
 * Navigate, NavigateToString, add/remove_NavigationCompleted have real
 * bodies (webview.c); everything else is webview2_stub_e_notimpl. */
typedef struct ICoreWebView2 ICoreWebView2;
typedef struct ICoreWebView2Vtbl
{
    HRESULT (WINAPI *QueryInterface)(ICoreWebView2 *This, REFIID riid, void **ppv);
    ULONG   (WINAPI *AddRef)(ICoreWebView2 *This);
    ULONG   (WINAPI *Release)(ICoreWebView2 *This);
    HRESULT (WINAPI *get_Settings)(ICoreWebView2 *This, void **settings);
    HRESULT (WINAPI *get_Source)(ICoreWebView2 *This, LPWSTR *uri);
    HRESULT (WINAPI *Navigate)(ICoreWebView2 *This, LPCWSTR uri);
    HRESULT (WINAPI *NavigateToString)(ICoreWebView2 *This, LPCWSTR htmlContent);
    HRESULT (WINAPI *add_NavigationStarting)(ICoreWebView2 *This, void *eventHandler, void *token);
    HRESULT (WINAPI *remove_NavigationStarting)(ICoreWebView2 *This, void *token);
    HRESULT (WINAPI *add_ContentLoading)(ICoreWebView2 *This, void *eventHandler, void *token);
    HRESULT (WINAPI *remove_ContentLoading)(ICoreWebView2 *This, void *token);
    HRESULT (WINAPI *add_SourceChanged)(ICoreWebView2 *This, void *eventHandler, void *token);
    HRESULT (WINAPI *remove_SourceChanged)(ICoreWebView2 *This, void *token);
    HRESULT (WINAPI *add_HistoryChanged)(ICoreWebView2 *This, void *eventHandler, void *token);
    HRESULT (WINAPI *remove_HistoryChanged)(ICoreWebView2 *This, void *token);
    HRESULT (WINAPI *add_NavigationCompleted)(ICoreWebView2 *This, void *eventHandler, void *token);
    HRESULT (WINAPI *remove_NavigationCompleted)(ICoreWebView2 *This, void *token);
    HRESULT (WINAPI *add_FrameNavigationStarting)(ICoreWebView2 *This, void *eventHandler, void *token);
    HRESULT (WINAPI *remove_FrameNavigationStarting)(ICoreWebView2 *This, void *token);
    HRESULT (WINAPI *add_FrameNavigationCompleted)(ICoreWebView2 *This, void *eventHandler, void *token);
    HRESULT (WINAPI *remove_FrameNavigationCompleted)(ICoreWebView2 *This, void *token);
    HRESULT (WINAPI *add_ScriptDialogOpening)(ICoreWebView2 *This, void *eventHandler, void *token);
    HRESULT (WINAPI *remove_ScriptDialogOpening)(ICoreWebView2 *This, void *token);
    HRESULT (WINAPI *add_PermissionRequested)(ICoreWebView2 *This, void *eventHandler, void *token);
    HRESULT (WINAPI *remove_PermissionRequested)(ICoreWebView2 *This, void *token);
    HRESULT (WINAPI *add_ProcessFailed)(ICoreWebView2 *This, void *eventHandler, void *token);
    HRESULT (WINAPI *remove_ProcessFailed)(ICoreWebView2 *This, void *token);
    HRESULT (WINAPI *AddScriptToExecuteOnDocumentCreated)(ICoreWebView2 *This, LPCWSTR javaScript, void *handler);
    HRESULT (WINAPI *RemoveScriptToExecuteOnDocumentCreated)(ICoreWebView2 *This, LPCWSTR id);
    HRESULT (WINAPI *ExecuteScript)(ICoreWebView2 *This, LPCWSTR javaScript, void *handler);
    HRESULT (WINAPI *CapturePreview)(ICoreWebView2 *This, int imageFormat, void *imageStream, void *handler);
    HRESULT (WINAPI *Reload)(ICoreWebView2 *This);
    HRESULT (WINAPI *PostWebMessageAsJson)(ICoreWebView2 *This, LPCWSTR webMessageAsJson);
    HRESULT (WINAPI *PostWebMessageAsString)(ICoreWebView2 *This, LPCWSTR webMessageAsString);
    HRESULT (WINAPI *add_WebMessageReceived)(ICoreWebView2 *This, void *eventHandler, void *token);
    HRESULT (WINAPI *remove_WebMessageReceived)(ICoreWebView2 *This, void *token);
    HRESULT (WINAPI *CallDevToolsProtocolMethod)(ICoreWebView2 *This, LPCWSTR methodName, LPCWSTR parametersAsJson, void *handler);
    HRESULT (WINAPI *get_BrowserProcessId)(ICoreWebView2 *This, UINT32 *value);
    HRESULT (WINAPI *get_CanGoBack)(ICoreWebView2 *This, BOOL *canGoBack);
    HRESULT (WINAPI *get_CanGoForward)(ICoreWebView2 *This, BOOL *canGoForward);
    HRESULT (WINAPI *GoBack)(ICoreWebView2 *This);
    HRESULT (WINAPI *GoForward)(ICoreWebView2 *This);
    HRESULT (WINAPI *GetDevToolsProtocolEventReceiver)(ICoreWebView2 *This, LPCWSTR eventName, void **receiver);
    HRESULT (WINAPI *Stop)(ICoreWebView2 *This);
    HRESULT (WINAPI *add_NewWindowRequested)(ICoreWebView2 *This, void *eventHandler, void *token);
    HRESULT (WINAPI *remove_NewWindowRequested)(ICoreWebView2 *This, void *token);
    HRESULT (WINAPI *add_DocumentTitleChanged)(ICoreWebView2 *This, void *eventHandler, void *token);
    HRESULT (WINAPI *remove_DocumentTitleChanged)(ICoreWebView2 *This, void *token);
    HRESULT (WINAPI *get_DocumentTitle)(ICoreWebView2 *This, LPWSTR *title);
    HRESULT (WINAPI *AddHostObjectToScript)(ICoreWebView2 *This, LPCWSTR name, VARIANT *object);
    HRESULT (WINAPI *RemoveHostObjectFromScript)(ICoreWebView2 *This, LPCWSTR name);
    HRESULT (WINAPI *OpenDevToolsWindow)(ICoreWebView2 *This);
    HRESULT (WINAPI *add_ContainsFullScreenElementChanged)(ICoreWebView2 *This, void *eventHandler, void *token);
    HRESULT (WINAPI *remove_ContainsFullScreenElementChanged)(ICoreWebView2 *This, void *token);
    HRESULT (WINAPI *get_ContainsFullScreenElement)(ICoreWebView2 *This, BOOL *containsFullScreenElement);
    HRESULT (WINAPI *add_WebResourceRequested)(ICoreWebView2 *This, void *eventHandler, void *token);
    HRESULT (WINAPI *remove_WebResourceRequested)(ICoreWebView2 *This, void *token);
    HRESULT (WINAPI *AddWebResourceRequestedFilter)(ICoreWebView2 *This, LPCWSTR uri, int resourceContext);
    HRESULT (WINAPI *RemoveWebResourceRequestedFilter)(ICoreWebView2 *This, LPCWSTR uri, int resourceContext);
    HRESULT (WINAPI *add_WindowCloseRequested)(ICoreWebView2 *This, void *eventHandler, void *token);
    HRESULT (WINAPI *remove_WindowCloseRequested)(ICoreWebView2 *This, void *token);
} ICoreWebView2Vtbl;
struct ICoreWebView2 { const ICoreWebView2Vtbl *lpVtbl; };

#ifdef COBJMACROS
#define ICoreWebView2_QueryInterface(This,riid,ppv) (This)->lpVtbl->QueryInterface(This,riid,ppv)
#define ICoreWebView2_AddRef(This) (This)->lpVtbl->AddRef(This)
#define ICoreWebView2_Release(This) (This)->lpVtbl->Release(This)
#define ICoreWebView2_get_Settings(This,settings) (This)->lpVtbl->get_Settings(This,settings)
#define ICoreWebView2_get_Source(This,uri) (This)->lpVtbl->get_Source(This,uri)
#define ICoreWebView2_Navigate(This,uri) (This)->lpVtbl->Navigate(This,uri)
#define ICoreWebView2_NavigateToString(This,htmlContent) (This)->lpVtbl->NavigateToString(This,htmlContent)
#define ICoreWebView2_add_NavigationStarting(This,eventHandler,token) \
    (This)->lpVtbl->add_NavigationStarting(This,eventHandler,token)
#define ICoreWebView2_remove_NavigationStarting(This,token) (This)->lpVtbl->remove_NavigationStarting(This,token)
#define ICoreWebView2_add_ContentLoading(This,eventHandler,token) \
    (This)->lpVtbl->add_ContentLoading(This,eventHandler,token)
#define ICoreWebView2_remove_ContentLoading(This,token) (This)->lpVtbl->remove_ContentLoading(This,token)
#define ICoreWebView2_add_SourceChanged(This,eventHandler,token) \
    (This)->lpVtbl->add_SourceChanged(This,eventHandler,token)
#define ICoreWebView2_remove_SourceChanged(This,token) (This)->lpVtbl->remove_SourceChanged(This,token)
#define ICoreWebView2_add_HistoryChanged(This,eventHandler,token) \
    (This)->lpVtbl->add_HistoryChanged(This,eventHandler,token)
#define ICoreWebView2_remove_HistoryChanged(This,token) (This)->lpVtbl->remove_HistoryChanged(This,token)
#define ICoreWebView2_add_NavigationCompleted(This,eventHandler,token) \
    (This)->lpVtbl->add_NavigationCompleted(This,eventHandler,token)
#define ICoreWebView2_remove_NavigationCompleted(This,token) (This)->lpVtbl->remove_NavigationCompleted(This,token)
#define ICoreWebView2_add_FrameNavigationStarting(This,eventHandler,token) \
    (This)->lpVtbl->add_FrameNavigationStarting(This,eventHandler,token)
#define ICoreWebView2_remove_FrameNavigationStarting(This,token) \
    (This)->lpVtbl->remove_FrameNavigationStarting(This,token)
#define ICoreWebView2_add_FrameNavigationCompleted(This,eventHandler,token) \
    (This)->lpVtbl->add_FrameNavigationCompleted(This,eventHandler,token)
#define ICoreWebView2_remove_FrameNavigationCompleted(This,token) \
    (This)->lpVtbl->remove_FrameNavigationCompleted(This,token)
#define ICoreWebView2_add_ScriptDialogOpening(This,eventHandler,token) \
    (This)->lpVtbl->add_ScriptDialogOpening(This,eventHandler,token)
#define ICoreWebView2_remove_ScriptDialogOpening(This,token) (This)->lpVtbl->remove_ScriptDialogOpening(This,token)
#define ICoreWebView2_add_PermissionRequested(This,eventHandler,token) \
    (This)->lpVtbl->add_PermissionRequested(This,eventHandler,token)
#define ICoreWebView2_remove_PermissionRequested(This,token) (This)->lpVtbl->remove_PermissionRequested(This,token)
#define ICoreWebView2_add_ProcessFailed(This,eventHandler,token) \
    (This)->lpVtbl->add_ProcessFailed(This,eventHandler,token)
#define ICoreWebView2_remove_ProcessFailed(This,token) (This)->lpVtbl->remove_ProcessFailed(This,token)
#define ICoreWebView2_AddScriptToExecuteOnDocumentCreated(This,javaScript,handler) \
    (This)->lpVtbl->AddScriptToExecuteOnDocumentCreated(This,javaScript,handler)
#define ICoreWebView2_RemoveScriptToExecuteOnDocumentCreated(This,id) \
    (This)->lpVtbl->RemoveScriptToExecuteOnDocumentCreated(This,id)
#define ICoreWebView2_ExecuteScript(This,javaScript,handler) (This)->lpVtbl->ExecuteScript(This,javaScript,handler)
#define ICoreWebView2_CapturePreview(This,imageFormat,imageStream,handler) \
    (This)->lpVtbl->CapturePreview(This,imageFormat,imageStream,handler)
#define ICoreWebView2_Reload(This) (This)->lpVtbl->Reload(This)
#define ICoreWebView2_PostWebMessageAsJson(This,webMessageAsJson) \
    (This)->lpVtbl->PostWebMessageAsJson(This,webMessageAsJson)
#define ICoreWebView2_PostWebMessageAsString(This,webMessageAsString) \
    (This)->lpVtbl->PostWebMessageAsString(This,webMessageAsString)
#define ICoreWebView2_add_WebMessageReceived(This,eventHandler,token) \
    (This)->lpVtbl->add_WebMessageReceived(This,eventHandler,token)
#define ICoreWebView2_remove_WebMessageReceived(This,token) (This)->lpVtbl->remove_WebMessageReceived(This,token)
#define ICoreWebView2_CallDevToolsProtocolMethod(This,methodName,parametersAsJson,handler) \
    (This)->lpVtbl->CallDevToolsProtocolMethod(This,methodName,parametersAsJson,handler)
#define ICoreWebView2_get_BrowserProcessId(This,value) (This)->lpVtbl->get_BrowserProcessId(This,value)
#define ICoreWebView2_get_CanGoBack(This,canGoBack) (This)->lpVtbl->get_CanGoBack(This,canGoBack)
#define ICoreWebView2_get_CanGoForward(This,canGoForward) (This)->lpVtbl->get_CanGoForward(This,canGoForward)
#define ICoreWebView2_GoBack(This) (This)->lpVtbl->GoBack(This)
#define ICoreWebView2_GoForward(This) (This)->lpVtbl->GoForward(This)
#define ICoreWebView2_GetDevToolsProtocolEventReceiver(This,eventName,receiver) \
    (This)->lpVtbl->GetDevToolsProtocolEventReceiver(This,eventName,receiver)
#define ICoreWebView2_Stop(This) (This)->lpVtbl->Stop(This)
#define ICoreWebView2_add_NewWindowRequested(This,eventHandler,token) \
    (This)->lpVtbl->add_NewWindowRequested(This,eventHandler,token)
#define ICoreWebView2_remove_NewWindowRequested(This,token) (This)->lpVtbl->remove_NewWindowRequested(This,token)
#define ICoreWebView2_add_DocumentTitleChanged(This,eventHandler,token) \
    (This)->lpVtbl->add_DocumentTitleChanged(This,eventHandler,token)
#define ICoreWebView2_remove_DocumentTitleChanged(This,token) \
    (This)->lpVtbl->remove_DocumentTitleChanged(This,token)
#define ICoreWebView2_get_DocumentTitle(This,title) (This)->lpVtbl->get_DocumentTitle(This,title)
#define ICoreWebView2_AddHostObjectToScript(This,name,object) (This)->lpVtbl->AddHostObjectToScript(This,name,object)
#define ICoreWebView2_RemoveHostObjectFromScript(This,name) (This)->lpVtbl->RemoveHostObjectFromScript(This,name)
#define ICoreWebView2_OpenDevToolsWindow(This) (This)->lpVtbl->OpenDevToolsWindow(This)
#define ICoreWebView2_add_ContainsFullScreenElementChanged(This,eventHandler,token) \
    (This)->lpVtbl->add_ContainsFullScreenElementChanged(This,eventHandler,token)
#define ICoreWebView2_remove_ContainsFullScreenElementChanged(This,token) \
    (This)->lpVtbl->remove_ContainsFullScreenElementChanged(This,token)
#define ICoreWebView2_get_ContainsFullScreenElement(This,containsFullScreenElement) \
    (This)->lpVtbl->get_ContainsFullScreenElement(This,containsFullScreenElement)
#define ICoreWebView2_add_WebResourceRequested(This,eventHandler,token) \
    (This)->lpVtbl->add_WebResourceRequested(This,eventHandler,token)
#define ICoreWebView2_remove_WebResourceRequested(This,token) (This)->lpVtbl->remove_WebResourceRequested(This,token)
#define ICoreWebView2_AddWebResourceRequestedFilter(This,uri,resourceContext) \
    (This)->lpVtbl->AddWebResourceRequestedFilter(This,uri,resourceContext)
#define ICoreWebView2_RemoveWebResourceRequestedFilter(This,uri,resourceContext) \
    (This)->lpVtbl->RemoveWebResourceRequestedFilter(This,uri,resourceContext)
#define ICoreWebView2_add_WindowCloseRequested(This,eventHandler,token) \
    (This)->lpVtbl->add_WindowCloseRequested(This,eventHandler,token)
#define ICoreWebView2_remove_WindowCloseRequested(This,token) (This)->lpVtbl->remove_WindowCloseRequested(This,token)
#endif

DEFINE_GUID(IID_ICoreWebView2, 0x76eceacb, 0x0462, 0x4d94, 0xac, 0x83, 0x42, 0x3a, 0x67, 0x93, 0x77, 0x5e);
DEFINE_GUID(IID_ICoreWebView2NavigationCompletedEventHandler,
            0xd33a35bf, 0x1c49, 0x4f98, 0x93, 0xab, 0x00, 0x6e, 0x05, 0x33, 0xfe, 0x1c);
DEFINE_GUID(IID_ICoreWebView2NavigationCompletedEventArgs,
            0x30d68b7d, 0x20d9, 0x4752, 0xa9, 0xca, 0xec, 0x84, 0x48, 0xfb, 0xb5, 0xc1);

typedef struct ICoreWebView2NavigationCompletedEventHandler ICoreWebView2NavigationCompletedEventHandler;
typedef struct ICoreWebView2NavigationCompletedEventArgs ICoreWebView2NavigationCompletedEventArgs;

typedef struct ICoreWebView2NavigationCompletedEventHandlerVtbl
{
    HRESULT (WINAPI *QueryInterface)(ICoreWebView2NavigationCompletedEventHandler *This, REFIID riid, void **ppv);
    ULONG   (WINAPI *AddRef)(ICoreWebView2NavigationCompletedEventHandler *This);
    ULONG   (WINAPI *Release)(ICoreWebView2NavigationCompletedEventHandler *This);
    HRESULT (WINAPI *Invoke)(ICoreWebView2NavigationCompletedEventHandler *This, ICoreWebView2 *sender,
                              ICoreWebView2NavigationCompletedEventArgs *args);
} ICoreWebView2NavigationCompletedEventHandlerVtbl;
struct ICoreWebView2NavigationCompletedEventHandler { const ICoreWebView2NavigationCompletedEventHandlerVtbl *lpVtbl; };

#ifdef COBJMACROS
#define ICoreWebView2NavigationCompletedEventHandler_QueryInterface(This,riid,ppv) \
    (This)->lpVtbl->QueryInterface(This,riid,ppv)
#define ICoreWebView2NavigationCompletedEventHandler_AddRef(This) (This)->lpVtbl->AddRef(This)
#define ICoreWebView2NavigationCompletedEventHandler_Release(This) (This)->lpVtbl->Release(This)
#define ICoreWebView2NavigationCompletedEventHandler_Invoke(This,sender,args) \
    (This)->lpVtbl->Invoke(This,sender,args)
#endif

typedef struct ICoreWebView2NavigationCompletedEventArgsVtbl
{
    HRESULT (WINAPI *QueryInterface)(ICoreWebView2NavigationCompletedEventArgs *This, REFIID riid, void **ppv);
    ULONG   (WINAPI *AddRef)(ICoreWebView2NavigationCompletedEventArgs *This);
    ULONG   (WINAPI *Release)(ICoreWebView2NavigationCompletedEventArgs *This);
    HRESULT (WINAPI *get_IsSuccess)(ICoreWebView2NavigationCompletedEventArgs *This, BOOL *isSuccess);
    HRESULT (WINAPI *get_WebErrorStatus)(ICoreWebView2NavigationCompletedEventArgs *This, int *webErrorStatus);
    HRESULT (WINAPI *get_NavigationId)(ICoreWebView2NavigationCompletedEventArgs *This, UINT64 *navigationId);
} ICoreWebView2NavigationCompletedEventArgsVtbl;
struct ICoreWebView2NavigationCompletedEventArgs { const ICoreWebView2NavigationCompletedEventArgsVtbl *lpVtbl; };

#ifdef COBJMACROS
#define ICoreWebView2NavigationCompletedEventArgs_QueryInterface(This,riid,ppv) \
    (This)->lpVtbl->QueryInterface(This,riid,ppv)
#define ICoreWebView2NavigationCompletedEventArgs_AddRef(This) (This)->lpVtbl->AddRef(This)
#define ICoreWebView2NavigationCompletedEventArgs_Release(This) (This)->lpVtbl->Release(This)
#define ICoreWebView2NavigationCompletedEventArgs_get_IsSuccess(This,isSuccess) \
    (This)->lpVtbl->get_IsSuccess(This,isSuccess)
#define ICoreWebView2NavigationCompletedEventArgs_get_WebErrorStatus(This,webErrorStatus) \
    (This)->lpVtbl->get_WebErrorStatus(This,webErrorStatus)
#define ICoreWebView2NavigationCompletedEventArgs_get_NavigationId(This,navigationId) \
    (This)->lpVtbl->get_NavigationId(This,navigationId)
#endif

/* Constructs an ICoreWebView2 (refcount 1) wrapping the unix-side
 * native_handle owned by the controller that created it. */
HRESULT webview_create(UINT64 native_handle, ICoreWebView2 **out);

/* Task 8 extension point: QueryInterface's fallback for any IID beyond
 * IUnknown/ICoreWebView2 (e.g. IID_ICoreWebView2_2). Defined for real in
 * webview.c already (returns E_NOINTERFACE) -- Task 8 replaces that body,
 * not this declaration. */
HRESULT webview_query_interface_v2(ICoreWebView2 *iface, REFIID riid, void **ppv);

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

UINT64 controller_get_native_handle(ICoreWebView2Controller *iface);

/* Generic ignore-args E_NOTIMPL stub, cast to whatever vtable slot type is
 * needed. Deliberate: with 20+ genuinely-unimplemented methods per
 * interface (real WebView2 has ~50 on ICoreWebView2 alone), writing a
 * distinct 3-line function per slot adds nothing -- none of them read their
 * arguments, and every real caller invokes them through the correctly-typed
 * vtable slot, so the mismatched declared signature is never observed. */
HRESULT WINAPI webview2_stub_e_notimpl(void *iface, ...);

/* Task 8: ICoreWebView2_2 / ICoreWebView2CookieManager.
 *
 * ICoreWebView2_2's vtable is a strict, identically-signatured 61-entry
 * prefix extension of ICoreWebView2's (this is how Microsoft's own WebView2
 * COM versioning works -- each numbered interface only ever appends; 61 is
 * ICoreWebView2Vtbl's real member count, verified by counting it below and
 * cross-checked against webview.c's own already-correct 61-entry
 * webview_vtbl -- an earlier version of this comment said 56, which was
 * simply wrong and led to a real bug: see the fix note at webview2_2_vtbl's
 * definition in webview.c for what that caused and how it was caught), so
 * this reuses the same underlying struct webview_impl object and a single
 * combined vtable rather than a second, near-duplicate 61-entry stub table:
 * QueryInterface for IID_ICoreWebView2_2 returns the same object pointer
 * with the combined vtable installed, which is safe specifically because
 * both webview.c (Task 7) and cookie_manager.c (this task) are written by
 * us and agree on that layout. */

DEFINE_GUID(IID_ICoreWebView2_2, 0x9E8F0CF8, 0xE670, 0x4B5E, 0xB2, 0xBC, 0x73, 0xE0, 0x61, 0xE3, 0x18, 0x4C);
DEFINE_GUID(IID_ICoreWebView2CookieManager, 0x177CD9E7, 0xB6F5, 0x451A, 0x94, 0xA0, 0x5D, 0x7A, 0x3A, 0x4C, 0x41, 0x41);

/* Forward declaration: ICoreWebView2GetCookiesCompletedHandler's full
 * definition lives further down (Task 11 section, below), but GetCookies's
 * vtable slot needs the real typed pointer here, not `void *` -- unlike
 * every other completion-handler-typed vtable slot in this header
 * (CreateCoreWebView2Controller/CreateCoreWebView2EnvironmentWithOptions
 * both deliberately keep `void *handler`), GetCookies is the one method
 * this fix actually implements against a caller that constructs a real
 * ICoreWebView2GetCookiesCompletedHandler*, so the tighter type costs
 * nothing and catches a real type mismatch at compile time instead of
 * only at the call site's own cast. */
typedef struct ICoreWebView2GetCookiesCompletedHandler ICoreWebView2GetCookiesCompletedHandler;

typedef struct ICoreWebView2CookieManager ICoreWebView2CookieManager;
typedef struct ICoreWebView2CookieManagerVtbl
{
    HRESULT (WINAPI *QueryInterface)(ICoreWebView2CookieManager *This, REFIID riid, void **ppv);
    ULONG   (WINAPI *AddRef)(ICoreWebView2CookieManager *This);
    ULONG   (WINAPI *Release)(ICoreWebView2CookieManager *This);
    HRESULT (WINAPI *CreateCookie)(ICoreWebView2CookieManager *This, LPCWSTR name, LPCWSTR value, LPCWSTR domain,
                                    LPCWSTR path, void **cookie);
    HRESULT (WINAPI *CopyCookie)(ICoreWebView2CookieManager *This, void *cookie, void **result);
    HRESULT (WINAPI *GetCookies)(ICoreWebView2CookieManager *This, LPCWSTR uri,
                                  ICoreWebView2GetCookiesCompletedHandler *handler);
    HRESULT (WINAPI *AddOrUpdateCookie)(ICoreWebView2CookieManager *This, void *cookie);
    HRESULT (WINAPI *DeleteCookie)(ICoreWebView2CookieManager *This, void *cookie);
    HRESULT (WINAPI *DeleteCookies)(ICoreWebView2CookieManager *This, LPCWSTR name, LPCWSTR uri);
    HRESULT (WINAPI *DeleteCookiesWithDomainAndPath)(ICoreWebView2CookieManager *This, LPCWSTR name, LPCWSTR domain, LPCWSTR path);
    HRESULT (WINAPI *DeleteAllCookies)(ICoreWebView2CookieManager *This);
} ICoreWebView2CookieManagerVtbl;
struct ICoreWebView2CookieManager { const ICoreWebView2CookieManagerVtbl *lpVtbl; };

#ifdef COBJMACROS
#define ICoreWebView2CookieManager_QueryInterface(This,riid,ppv) (This)->lpVtbl->QueryInterface(This,riid,ppv)
#define ICoreWebView2CookieManager_AddRef(This) (This)->lpVtbl->AddRef(This)
#define ICoreWebView2CookieManager_Release(This) (This)->lpVtbl->Release(This)
#define ICoreWebView2CookieManager_CreateCookie(This,name,value,domain,path,cookie) \
    (This)->lpVtbl->CreateCookie(This,name,value,domain,path,cookie)
#define ICoreWebView2CookieManager_CopyCookie(This,cookie,result) (This)->lpVtbl->CopyCookie(This,cookie,result)
#define ICoreWebView2CookieManager_GetCookies(This,uri,handler) (This)->lpVtbl->GetCookies(This,uri,handler)
#define ICoreWebView2CookieManager_AddOrUpdateCookie(This,cookie) (This)->lpVtbl->AddOrUpdateCookie(This,cookie)
#define ICoreWebView2CookieManager_DeleteCookie(This,cookie) (This)->lpVtbl->DeleteCookie(This,cookie)
#define ICoreWebView2CookieManager_DeleteCookies(This,name,uri) (This)->lpVtbl->DeleteCookies(This,name,uri)
#define ICoreWebView2CookieManager_DeleteCookiesWithDomainAndPath(This,name,domain,path) \
    (This)->lpVtbl->DeleteCookiesWithDomainAndPath(This,name,domain,path)
#define ICoreWebView2CookieManager_DeleteAllCookies(This) (This)->lpVtbl->DeleteAllCookies(This)
#endif

/* ICoreWebView2_2's vtable: ICoreWebView2's 61 entries verbatim (see
 * webview.c's webview_vtbl -- must stay byte-identical in the first 61
 * slots) plus 7 more. Only declared here as a slot COUNT/ORDER contract via
 * the extension struct below; webview.c builds the actual combined table. */
typedef struct
{
    HRESULT (WINAPI *add_WebResourceResponseReceived)(ICoreWebView2 *This, void *eventHandler, void *token);
    HRESULT (WINAPI *remove_WebResourceResponseReceived)(ICoreWebView2 *This, void *token);
    HRESULT (WINAPI *NavigateWithWebResourceRequest)(ICoreWebView2 *This, void *request);
    HRESULT (WINAPI *add_DOMContentLoaded)(ICoreWebView2 *This, void *eventHandler, void *token);
    HRESULT (WINAPI *remove_DOMContentLoaded)(ICoreWebView2 *This, void *token);
    HRESULT (WINAPI *get_CookieManager)(ICoreWebView2 *This, ICoreWebView2CookieManager **cookieManager);
    HRESULT (WINAPI *get_Environment)(ICoreWebView2 *This, ICoreWebView2Environment **environment);
} webview2_2_extension_vtbl;

/* Combined vtable: webview_vtbl's 61 entries (Task 7, byte-identical
 * layout/order -- ICoreWebView2_2 only ever appends per Microsoft's own
 * versioning discipline) plus the 7 ICoreWebView2_2-specific ones. Defined
 * as one struct so a single object can be QueryInterface'd as either IID
 * and get a working, layout-correct vtable pointer back either way.
 *
 * Declared here (not in webview.c, where the brief's Step 5 originally put
 * it) because tests/webview2loader.c's test_delete_all_cookies (Step 7)
 * needs to reach through webview_v2->lpVtbl to call ext.get_CookieManager
 * directly -- the brief's own Step 7 text says as much ("struct
 * webview2_2_vtbl_combined and its ext member must be visible to this
 * test -- move that struct's definition from webview.c into
 * webview2loader_private.h so both files share it"). webview.c still
 * defines and owns the actual `webview2_2_vtbl` instance/data. */
struct webview2_2_vtbl_combined
{
    ICoreWebView2Vtbl base;
    webview2_2_extension_vtbl ext;
};

/* Constructs an ICoreWebView2CookieManager (refcount 1) bound to the given
 * unix-side native_handle (the same handle struct webview_impl carries --
 * see webview2_get_CookieManager in webview.c). */
HRESULT cookie_manager_create(UINT64 native_handle, ICoreWebView2CookieManager **out);

/* Task 11 real-bug fix: GetCookies was left E_NOTIMPL by Task 8, and real
 * Roblox Studio's own startup flow (clearAllCookiesAndRunCallbackHelper,
 * per a real captured FLog trace -- see this task's own report for the
 * exact log lines) depends on it succeeding. These three interfaces are
 * the real, verified ICoreWebView2CookieManager::GetCookies completion
 * chain -- ICoreWebView2Cookie (one cookie), ICoreWebView2CookieList (the
 * result GetCookies hands back), ICoreWebView2GetCookiesCompletedHandler
 * (the completion callback GetCookies invokes). Verified byte-for-byte
 * against the real Microsoft.Web.WebView2 1.0.4129.50 NuGet package's own
 * WebView2.h (downloaded directly from
 * https://api.nuget.org/v3-flatcontainer/microsoft.web.webview2/1.0.4129.50/
 * and grepped for real, not reconstructed from memory or guessed), same
 * verification method this plan's Global Constraints section used for
 * every other IID/vtable in this file. IIDs and vtable slot order below
 * are a direct transcription of that real header's
 * ICoreWebView2Cookie/ICoreWebView2CookieList/
 * ICoreWebView2GetCookiesCompletedHandler definitions. */

/* COREWEBVIEW2_COOKIE_SAME_SITE_KIND: verified real values (WebView2.h) --
 * numerically identical to libsoup's own SoupSameSitePolicy
 * (SOUP_SAME_SITE_POLICY_NONE/LAX/STRICT, soup-cookie.h), confirmed via
 * both real headers rather than assumed, so unixlib.c can pass
 * soup_cookie_get_same_site_policy()'s return value straight through as
 * this enum's underlying INT32 without a translation table. */
typedef enum
{
    COREWEBVIEW2_COOKIE_SAME_SITE_KIND_NONE = 0,
    COREWEBVIEW2_COOKIE_SAME_SITE_KIND_LAX = 1,
    COREWEBVIEW2_COOKIE_SAME_SITE_KIND_STRICT = 2,
} COREWEBVIEW2_COOKIE_SAME_SITE_KIND;

DEFINE_GUID(IID_ICoreWebView2Cookie, 0xAD26D6BE, 0x1486, 0x43E6, 0xBF, 0x87, 0xA2, 0x03, 0x40, 0x06, 0xCA, 0x21);

/* Real vtable order (WebView2.h's ICoreWebView2CookieVtbl): note there is
 * deliberately no put_Name/put_Domain/put_Path in the real interface --
 * only Value/Expires/IsHttpOnly/SameSite/IsSecure have setters; Name,
 * Domain, and Path are get-only on an already-constructed cookie (real
 * WebView2 semantics: those three are fixed at CreateCookie time). Not an
 * omission here -- verified against the real header, which has no such
 * slots either. */
typedef struct ICoreWebView2Cookie ICoreWebView2Cookie;
typedef struct ICoreWebView2CookieVtbl
{
    HRESULT (WINAPI *QueryInterface)(ICoreWebView2Cookie *This, REFIID riid, void **ppv);
    ULONG   (WINAPI *AddRef)(ICoreWebView2Cookie *This);
    ULONG   (WINAPI *Release)(ICoreWebView2Cookie *This);
    HRESULT (WINAPI *get_Name)(ICoreWebView2Cookie *This, LPWSTR *name);
    HRESULT (WINAPI *get_Value)(ICoreWebView2Cookie *This, LPWSTR *value);
    HRESULT (WINAPI *put_Value)(ICoreWebView2Cookie *This, LPCWSTR value);
    HRESULT (WINAPI *get_Domain)(ICoreWebView2Cookie *This, LPWSTR *domain);
    HRESULT (WINAPI *get_Path)(ICoreWebView2Cookie *This, LPWSTR *path);
    HRESULT (WINAPI *get_Expires)(ICoreWebView2Cookie *This, double *expires);
    HRESULT (WINAPI *put_Expires)(ICoreWebView2Cookie *This, double expires);
    HRESULT (WINAPI *get_IsHttpOnly)(ICoreWebView2Cookie *This, BOOL *isHttpOnly);
    HRESULT (WINAPI *put_IsHttpOnly)(ICoreWebView2Cookie *This, BOOL isHttpOnly);
    HRESULT (WINAPI *get_SameSite)(ICoreWebView2Cookie *This, COREWEBVIEW2_COOKIE_SAME_SITE_KIND *sameSite);
    HRESULT (WINAPI *put_SameSite)(ICoreWebView2Cookie *This, COREWEBVIEW2_COOKIE_SAME_SITE_KIND sameSite);
    HRESULT (WINAPI *get_IsSecure)(ICoreWebView2Cookie *This, BOOL *isSecure);
    HRESULT (WINAPI *put_IsSecure)(ICoreWebView2Cookie *This, BOOL isSecure);
    HRESULT (WINAPI *get_IsSession)(ICoreWebView2Cookie *This, BOOL *isSession);
} ICoreWebView2CookieVtbl;
struct ICoreWebView2Cookie { const ICoreWebView2CookieVtbl *lpVtbl; };

#ifdef COBJMACROS
#define ICoreWebView2Cookie_QueryInterface(This,riid,ppv) (This)->lpVtbl->QueryInterface(This,riid,ppv)
#define ICoreWebView2Cookie_AddRef(This) (This)->lpVtbl->AddRef(This)
#define ICoreWebView2Cookie_Release(This) (This)->lpVtbl->Release(This)
#define ICoreWebView2Cookie_get_Name(This,name) (This)->lpVtbl->get_Name(This,name)
#define ICoreWebView2Cookie_get_Value(This,value) (This)->lpVtbl->get_Value(This,value)
#define ICoreWebView2Cookie_put_Value(This,value) (This)->lpVtbl->put_Value(This,value)
#define ICoreWebView2Cookie_get_Domain(This,domain) (This)->lpVtbl->get_Domain(This,domain)
#define ICoreWebView2Cookie_get_Path(This,path) (This)->lpVtbl->get_Path(This,path)
#define ICoreWebView2Cookie_get_Expires(This,expires) (This)->lpVtbl->get_Expires(This,expires)
#define ICoreWebView2Cookie_put_Expires(This,expires) (This)->lpVtbl->put_Expires(This,expires)
#define ICoreWebView2Cookie_get_IsHttpOnly(This,isHttpOnly) (This)->lpVtbl->get_IsHttpOnly(This,isHttpOnly)
#define ICoreWebView2Cookie_put_IsHttpOnly(This,isHttpOnly) (This)->lpVtbl->put_IsHttpOnly(This,isHttpOnly)
#define ICoreWebView2Cookie_get_SameSite(This,sameSite) (This)->lpVtbl->get_SameSite(This,sameSite)
#define ICoreWebView2Cookie_put_SameSite(This,sameSite) (This)->lpVtbl->put_SameSite(This,sameSite)
#define ICoreWebView2Cookie_get_IsSecure(This,isSecure) (This)->lpVtbl->get_IsSecure(This,isSecure)
#define ICoreWebView2Cookie_put_IsSecure(This,isSecure) (This)->lpVtbl->put_IsSecure(This,isSecure)
#define ICoreWebView2Cookie_get_IsSession(This,isSession) (This)->lpVtbl->get_IsSession(This,isSession)
#endif

DEFINE_GUID(IID_ICoreWebView2CookieList, 0xf7f6f714, 0x5d2a, 0x43c6, 0x95, 0x03, 0x34, 0x6e, 0xce, 0x02, 0xd1, 0x86);

typedef struct ICoreWebView2CookieList ICoreWebView2CookieList;
typedef struct ICoreWebView2CookieListVtbl
{
    HRESULT (WINAPI *QueryInterface)(ICoreWebView2CookieList *This, REFIID riid, void **ppv);
    ULONG   (WINAPI *AddRef)(ICoreWebView2CookieList *This);
    ULONG   (WINAPI *Release)(ICoreWebView2CookieList *This);
    HRESULT (WINAPI *get_Count)(ICoreWebView2CookieList *This, UINT32 *value);
    HRESULT (WINAPI *GetValueAtIndex)(ICoreWebView2CookieList *This, UINT32 index, ICoreWebView2Cookie **value);
} ICoreWebView2CookieListVtbl;
struct ICoreWebView2CookieList { const ICoreWebView2CookieListVtbl *lpVtbl; };

#ifdef COBJMACROS
#define ICoreWebView2CookieList_QueryInterface(This,riid,ppv) (This)->lpVtbl->QueryInterface(This,riid,ppv)
#define ICoreWebView2CookieList_AddRef(This) (This)->lpVtbl->AddRef(This)
#define ICoreWebView2CookieList_Release(This) (This)->lpVtbl->Release(This)
#define ICoreWebView2CookieList_get_Count(This,value) (This)->lpVtbl->get_Count(This,value)
#define ICoreWebView2CookieList_GetValueAtIndex(This,index,value) (This)->lpVtbl->GetValueAtIndex(This,index,value)
#endif

DEFINE_GUID(IID_ICoreWebView2GetCookiesCompletedHandler,
            0x5a4f5069, 0x5c15, 0x47c3, 0x86, 0x46, 0xf4, 0xde, 0x1c, 0x11, 0x66, 0x70);

/* ICoreWebView2GetCookiesCompletedHandler itself was already forward-declared
 * above, alongside ICoreWebView2CookieManagerVtbl's GetCookies slot -- see
 * that forward declaration's own comment for why. */
typedef struct ICoreWebView2GetCookiesCompletedHandlerVtbl
{
    HRESULT (WINAPI *QueryInterface)(ICoreWebView2GetCookiesCompletedHandler *This, REFIID riid, void **ppv);
    ULONG   (WINAPI *AddRef)(ICoreWebView2GetCookiesCompletedHandler *This);
    ULONG   (WINAPI *Release)(ICoreWebView2GetCookiesCompletedHandler *This);
    HRESULT (WINAPI *Invoke)(ICoreWebView2GetCookiesCompletedHandler *This, HRESULT errorCode,
                              ICoreWebView2CookieList *result);
} ICoreWebView2GetCookiesCompletedHandlerVtbl;
struct ICoreWebView2GetCookiesCompletedHandler { const ICoreWebView2GetCookiesCompletedHandlerVtbl *lpVtbl; };

#ifdef COBJMACROS
#define ICoreWebView2GetCookiesCompletedHandler_QueryInterface(This,riid,ppv) \
    (This)->lpVtbl->QueryInterface(This,riid,ppv)
#define ICoreWebView2GetCookiesCompletedHandler_AddRef(This) (This)->lpVtbl->AddRef(This)
#define ICoreWebView2GetCookiesCompletedHandler_Release(This) (This)->lpVtbl->Release(This)
#define ICoreWebView2GetCookiesCompletedHandler_Invoke(This,errorCode,result) \
    (This)->lpVtbl->Invoke(This,errorCode,result)
#endif

#endif /* __WINE_WEBVIEW2LOADER_PRIVATE_H */
