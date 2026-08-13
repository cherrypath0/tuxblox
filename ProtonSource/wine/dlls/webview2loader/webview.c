#include <stdarg.h>
#include <stdlib.h>
#include <string.h>

#include <windef.h>
#include <winbase.h>
#include <winuser.h>
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

/* Task 11 real bug fix: real Roblox Studio's embedded-login-dialog flow
 * (parented to a real HWND, not the CookieManager flow's HWND_MESSAGE --
 * confirmed via a direct file-based logger added and removed during this
 * investigation, see this task's report) calls add_WebMessageReceived
 * immediately after getting the webview back, before ever calling
 * Navigate(). This was left E_NOTIMPL since Task 7/8, harmlessly, because
 * nothing on the login-dialog path had reached it before this task's
 * Controller4/Environment8 QueryInterface fix cleared the way -- Studio
 * treats a failed add_WebMessageReceived as fatal to the whole embedded
 * browser setup (it needs a reliable channel for the login page to post
 * the completed auth token back), aborting before Navigate().
 *
 * This only wires up real add/remove *registration* (a listener list,
 * mirroring nav_listener/webview_add_NavigationCompleted exactly) so the
 * call itself succeeds -- it does NOT implement the other half (an actual
 * page calling window.chrome.webview.postMessage(...) and this DLL
 * detecting and dispatching that through WebKitGTK back to these
 * listeners). That would need real unixlib.c/WebKitGTK JS-bridge work,
 * which is out of scope here: nothing in the evidence shows it's needed to
 * reach Navigate(), only that the registration call itself must not fail.
 * `handler` is stored as a bare IUnknown* (AddRef/Release only) rather than
 * a fully-typed ICoreWebView2WebMessageReceivedEventHandler*, since this
 * DLL never Invokes it -- same rationale as why this file's completion
 * handlers elsewhere are only ever typed when actually invoked. */
struct wm_listener
{
    struct wm_listener *next;
    IUnknown *handler;
    UINT64 token;
};

/* Task 11 real bug fix, continued: add_WebMessageReceived wasn't the only
 * event registration Studio's embedded-login-dialog flow calls before
 * Navigate() -- once that one stopped being fatal, the exact same
 * "E_NOTIMPL from an add_X call treated as fatal, abort before Navigate()"
 * pattern immediately recurred on add_NavigationStarting (confirmed via the
 * same direct file-based logger). Rather than chase these one rebuild cycle
 * at a time, every remaining add_X/remove_X event-registration pair on
 * ICoreWebView2 (the ones that were still plain webview2_stub_e_notimpl)
 * gets the same treatment as add_WebMessageReceived: a real, working
 * registration (so the call succeeds and returns a real token) backed by
 * one shared listener list, since none of them need to actually fire --
 * nothing in the evidence shows Studio's login-dialog path depends on any
 * of these events actually being raised, only that registering for them
 * must not fail. A single shared list is enough because this DLL never
 * walks it selectively by event type (it only ever fires
 * NavigationCompleted, which keeps its own dedicated, real listener list
 * above -- unaffected by this). */
struct generic_listener
{
    struct generic_listener *next;
    IUnknown *handler;
    UINT64 token;
};

struct webview_impl
{
    ICoreWebView2 ICoreWebView2_iface;
    LONG ref;
    UINT64 native_handle;
    LPWSTR source;
    struct nav_listener *listeners;
    struct wm_listener *wm_listeners;
    struct generic_listener *generic_listeners;
    UINT64 next_token;
    CRITICAL_SECTION cs;
    ICoreWebView2Settings *settings; /* created lazily by get_Settings, Task 11 */
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
        struct wm_listener *wml = wv->wm_listeners;
        struct generic_listener *gl = wv->generic_listeners;
        while (l) { struct nav_listener *next = l->next; ICoreWebView2NavigationCompletedEventHandler_Release(l->handler); free(l); l = next; }
        while (wml) { struct wm_listener *next = wml->next; wml->handler->lpVtbl->Release(wml->handler); free(wml); wml = next; }
        while (gl) { struct generic_listener *next = gl->next; gl->handler->lpVtbl->Release(gl->handler); free(gl); gl = next; }
        if (wv->settings) ICoreWebView2Settings_Release(wv->settings);
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

/* --- Task 11: WebMessageReceived registration -- see struct wm_listener's
 * own comment above for what this does and doesn't implement. --- */

static HRESULT WINAPI webview_add_WebMessageReceived(ICoreWebView2 *iface, void *eventHandler_raw, void *token_raw)
{
    struct webview_impl *wv = impl_from_ICoreWebView2(iface);
    IUnknown *handler = eventHandler_raw;
    UINT64 *token = token_raw;
    struct wm_listener *l;

    if (!handler || !token) return E_POINTER;
    if (!(l = malloc(sizeof(*l)))) return E_OUTOFMEMORY;

    handler->lpVtbl->AddRef(handler); /* IUnknown_AddRef isn't available under
                                        * this DLL's __WINESRC__ build flags --
                                        * calling straight through lpVtbl is
                                        * equivalent and always works. */
    l->handler = handler;

    EnterCriticalSection(&wv->cs);
    l->token = ++wv->next_token;
    l->next = wv->wm_listeners;
    wv->wm_listeners = l;
    LeaveCriticalSection(&wv->cs);

    *token = l->token;
    return S_OK;
}

static HRESULT WINAPI webview_remove_WebMessageReceived(ICoreWebView2 *iface, void *token_raw)
{
    struct webview_impl *wv = impl_from_ICoreWebView2(iface);
    UINT64 token = (UINT64)(ULONG_PTR)token_raw; /* see webview_remove_NavigationCompleted's comment on this cast */
    struct wm_listener **cur;

    EnterCriticalSection(&wv->cs);
    for (cur = &wv->wm_listeners; *cur; cur = &(*cur)->next)
    {
        if ((*cur)->token == token)
        {
            struct wm_listener *dead = *cur;
            *cur = dead->next;
            LeaveCriticalSection(&wv->cs);
            dead->handler->lpVtbl->Release(dead->handler);
            free(dead);
            return S_OK;
        }
    }
    LeaveCriticalSection(&wv->cs);
    return S_OK; /* real WebView2 tolerates removing an already-gone/unknown token */
}

/* --- Task 11: shared registration for every remaining add_X/remove_X
 * event pair -- see struct generic_listener's own comment above for why a
 * single shared list/pair of functions covers all of them. --- */

static HRESULT WINAPI webview_generic_add_event(ICoreWebView2 *iface, void *eventHandler_raw, void *token_raw)
{
    struct webview_impl *wv = impl_from_ICoreWebView2(iface);
    IUnknown *handler = eventHandler_raw;
    UINT64 *token = token_raw;
    struct generic_listener *l;

    if (!handler || !token) return E_POINTER;
    if (!(l = malloc(sizeof(*l)))) return E_OUTOFMEMORY;

    handler->lpVtbl->AddRef(handler);
    l->handler = handler;

    EnterCriticalSection(&wv->cs);
    l->token = ++wv->next_token;
    l->next = wv->generic_listeners;
    wv->generic_listeners = l;
    LeaveCriticalSection(&wv->cs);

    *token = l->token;
    return S_OK;
}

static HRESULT WINAPI webview_generic_remove_event(ICoreWebView2 *iface, void *token_raw)
{
    struct webview_impl *wv = impl_from_ICoreWebView2(iface);
    UINT64 token = (UINT64)(ULONG_PTR)token_raw; /* see webview_remove_NavigationCompleted's comment on this cast */
    struct generic_listener **cur;

    EnterCriticalSection(&wv->cs);
    for (cur = &wv->generic_listeners; *cur; cur = &(*cur)->next)
    {
        if ((*cur)->token == token)
        {
            struct generic_listener *dead = *cur;
            *cur = dead->next;
            LeaveCriticalSection(&wv->cs);
            dead->handler->lpVtbl->Release(dead->handler);
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

/* --- Task 11: AddScriptToExecuteOnDocumentCreated -- see this method's
 * own comment in webview2loader_private.h for what this does and doesn't
 * implement. Real async semantics: returns S_OK immediately, Invoke happens
 * on a worker thread, same shape as environment_CreateCoreWebView2Controller
 * / webview_Navigate elsewhere in this file. --- */

struct add_script_ctx
{
    ICoreWebView2AddScriptToExecuteOnDocumentCreatedCompletedHandler *handler;
};

static DWORD WINAPI add_script_worker(void *arg)
{
    struct add_script_ctx *ctx = arg;
    static LONG next_script_id;
    WCHAR id[32];

    wsprintfW(id, L"tuxblox-script-%d", InterlockedIncrement(&next_script_id));
    ICoreWebView2AddScriptToExecuteOnDocumentCreatedCompletedHandler_Invoke(ctx->handler, S_OK, id);
    ICoreWebView2AddScriptToExecuteOnDocumentCreatedCompletedHandler_Release(ctx->handler);
    free(ctx);
    return 0;
}

static HRESULT WINAPI webview_AddScriptToExecuteOnDocumentCreated(ICoreWebView2 *iface, LPCWSTR javaScript,
                                                                    void *handler_raw)
{
    struct add_script_ctx *ctx;
    ICoreWebView2AddScriptToExecuteOnDocumentCreatedCompletedHandler *handler = handler_raw;

    if (!handler) return E_POINTER;
    if (!(ctx = malloc(sizeof(*ctx)))) return E_OUTOFMEMORY;

    ICoreWebView2AddScriptToExecuteOnDocumentCreatedCompletedHandler_AddRef(handler);
    ctx->handler = handler;

    if (!start_async_work(add_script_worker, ctx))
    {
        ICoreWebView2AddScriptToExecuteOnDocumentCreatedCompletedHandler_Release(handler);
        free(ctx);
        return E_FAIL;
    }
    return S_OK;
}

static HRESULT WINAPI webview_RemoveScriptToExecuteOnDocumentCreated(ICoreWebView2 *iface, LPCWSTR id)
{
    /* Nothing tracks injected scripts to actually remove (see this
     * method's own comment in webview2loader_private.h) -- real WebView2
     * tolerates an unknown/already-removed id too, so unconditional S_OK
     * is not a behavioral shortcut here, it's the correct response either
     * way. */
    return S_OK;
}

/* --- Task 11: ICoreWebView2Settings -- a small, real, plain-old-fields
 * object, same shape/spirit as NavigationCompletedEventArgs above. Defaults
 * match real WebView2's own documented defaults (every property here
 * defaults to TRUE on real Windows). --- */

struct settings_impl
{
    ICoreWebView2Settings iface;
    LONG ref;
    BOOL is_script_enabled;
    BOOL is_web_message_enabled;
    BOOL are_default_script_dialogs_enabled;
    BOOL is_status_bar_enabled;
    BOOL are_dev_tools_enabled;
    BOOL are_default_context_menus_enabled;
    BOOL are_host_objects_allowed;
    BOOL is_zoom_control_enabled;
    BOOL is_built_in_error_page_enabled;
};

static inline struct settings_impl *impl_from_ICoreWebView2Settings(ICoreWebView2Settings *iface)
{
    return CONTAINING_RECORD(iface, struct settings_impl, iface);
}

static HRESULT WINAPI settings_QueryInterface(ICoreWebView2Settings *iface, REFIID riid, void **ppv)
{
    if (IsEqualGUID(riid, &IID_IUnknown) || IsEqualGUID(riid, &IID_ICoreWebView2Settings))
    { *ppv = iface; ICoreWebView2Settings_AddRef(iface); return S_OK; }
    *ppv = NULL;
    return E_NOINTERFACE;
}
static ULONG WINAPI settings_AddRef(ICoreWebView2Settings *iface)
{ return InterlockedIncrement(&impl_from_ICoreWebView2Settings(iface)->ref); }
static ULONG WINAPI settings_Release(ICoreWebView2Settings *iface)
{
    struct settings_impl *s = impl_from_ICoreWebView2Settings(iface);
    LONG ref = InterlockedDecrement(&s->ref);
    if (!ref) free(s);
    return ref;
}

/* One get/put pair per BOOL field -- named to match the real property,
 * mechanically identical bodies (matches this file's existing
 * Controller2/3/4 property pattern in controller.c). */
#define SETTINGS_BOOL_PROPERTY(Name, field) \
    static HRESULT WINAPI settings_get_##Name(ICoreWebView2Settings *iface, BOOL *value) \
    { if (!value) return E_POINTER; *value = impl_from_ICoreWebView2Settings(iface)->field; return S_OK; } \
    static HRESULT WINAPI settings_put_##Name(ICoreWebView2Settings *iface, BOOL value) \
    { impl_from_ICoreWebView2Settings(iface)->field = value; return S_OK; }

SETTINGS_BOOL_PROPERTY(IsScriptEnabled, is_script_enabled)
SETTINGS_BOOL_PROPERTY(IsWebMessageEnabled, is_web_message_enabled)
SETTINGS_BOOL_PROPERTY(AreDefaultScriptDialogsEnabled, are_default_script_dialogs_enabled)
SETTINGS_BOOL_PROPERTY(IsStatusBarEnabled, is_status_bar_enabled)
SETTINGS_BOOL_PROPERTY(AreDevToolsEnabled, are_dev_tools_enabled)
SETTINGS_BOOL_PROPERTY(AreDefaultContextMenusEnabled, are_default_context_menus_enabled)
SETTINGS_BOOL_PROPERTY(AreHostObjectsAllowed, are_host_objects_allowed)
SETTINGS_BOOL_PROPERTY(IsZoomControlEnabled, is_zoom_control_enabled)
SETTINGS_BOOL_PROPERTY(IsBuiltInErrorPageEnabled, is_built_in_error_page_enabled)
#undef SETTINGS_BOOL_PROPERTY

static const ICoreWebView2SettingsVtbl settings_vtbl =
{
    settings_QueryInterface,
    settings_AddRef,
    settings_Release,
    settings_get_IsScriptEnabled,
    settings_put_IsScriptEnabled,
    settings_get_IsWebMessageEnabled,
    settings_put_IsWebMessageEnabled,
    settings_get_AreDefaultScriptDialogsEnabled,
    settings_put_AreDefaultScriptDialogsEnabled,
    settings_get_IsStatusBarEnabled,
    settings_put_IsStatusBarEnabled,
    settings_get_AreDevToolsEnabled,
    settings_put_AreDevToolsEnabled,
    settings_get_AreDefaultContextMenusEnabled,
    settings_put_AreDefaultContextMenusEnabled,
    settings_get_AreHostObjectsAllowed,
    settings_put_AreHostObjectsAllowed,
    settings_get_IsZoomControlEnabled,
    settings_put_IsZoomControlEnabled,
    settings_get_IsBuiltInErrorPageEnabled,
    settings_put_IsBuiltInErrorPageEnabled,
};

HRESULT settings_create(ICoreWebView2Settings **out)
{
    struct settings_impl *s = calloc(1, sizeof(*s));
    if (!s) return E_OUTOFMEMORY;

    s->iface.lpVtbl = &settings_vtbl;
    s->ref = 1;
    s->is_script_enabled = TRUE;
    s->is_web_message_enabled = TRUE;
    s->are_default_script_dialogs_enabled = TRUE;
    s->is_status_bar_enabled = TRUE;
    s->are_dev_tools_enabled = TRUE;
    s->are_default_context_menus_enabled = TRUE;
    s->are_host_objects_allowed = TRUE;
    s->is_zoom_control_enabled = TRUE;
    s->is_built_in_error_page_enabled = TRUE;
    *out = &s->iface;
    return S_OK;
}

static HRESULT WINAPI webview_get_Settings(ICoreWebView2 *iface, void **settings)
{
    /* void ** here, not ICoreWebView2Settings **, to match this DLL's own
     * ICoreWebView2Vtbl.get_Settings slot declaration in
     * webview2loader_private.h (kept generic there, like every other
     * not-yet-defined-interface slot in that table) -- a mismatched
     * function pointer type on this vtable assignment would be a
     * -Werror build failure, same reason every still-stubbed slot in this
     * table is cast through (void *). */
    struct webview_impl *wv = impl_from_ICoreWebView2(iface);
    HRESULT hr;

    if (!settings) return E_POINTER;
    if (!wv->settings && FAILED(hr = settings_create(&wv->settings)))
        return hr;

    ICoreWebView2Settings_AddRef(wv->settings);
    *settings = wv->settings;
    return S_OK;
}

static const ICoreWebView2Vtbl webview_vtbl =
{
    webview_QueryInterface,
    webview_AddRef,
    webview_Release,
    webview_get_Settings,
    webview_get_Source,
    webview_Navigate,
    webview_NavigateToString,
    webview_generic_add_event, /* add_NavigationStarting */
    webview_generic_remove_event, /* remove_NavigationStarting */
    webview_generic_add_event, /* add_ContentLoading */
    webview_generic_remove_event, /* remove_ContentLoading */
    webview_generic_add_event, /* add_SourceChanged */
    webview_generic_remove_event, /* remove_SourceChanged */
    webview_generic_add_event, /* add_HistoryChanged */
    webview_generic_remove_event, /* remove_HistoryChanged */
    webview_add_NavigationCompleted,
    webview_remove_NavigationCompleted,
    webview_generic_add_event, /* add_FrameNavigationStarting */
    webview_generic_remove_event, /* remove_FrameNavigationStarting */
    webview_generic_add_event, /* add_FrameNavigationCompleted */
    webview_generic_remove_event, /* remove_FrameNavigationCompleted */
    webview_generic_add_event, /* add_ScriptDialogOpening */
    webview_generic_remove_event, /* remove_ScriptDialogOpening */
    webview_generic_add_event, /* add_PermissionRequested */
    webview_generic_remove_event, /* remove_PermissionRequested */
    webview_generic_add_event, /* add_ProcessFailed */
    webview_generic_remove_event, /* remove_ProcessFailed */
    webview_AddScriptToExecuteOnDocumentCreated,
    webview_RemoveScriptToExecuteOnDocumentCreated,
    (void *)webview2_stub_e_notimpl, /* ExecuteScript */
    (void *)webview2_stub_e_notimpl, /* CapturePreview */
    (void *)webview2_stub_e_notimpl, /* Reload */
    (void *)webview2_stub_e_notimpl, /* PostWebMessageAsJson */
    (void *)webview2_stub_e_notimpl, /* PostWebMessageAsString */
    webview_add_WebMessageReceived,
    webview_remove_WebMessageReceived,
    (void *)webview2_stub_e_notimpl, /* CallDevToolsProtocolMethod */
    (void *)webview2_stub_e_notimpl, /* get_BrowserProcessId */
    (void *)webview2_stub_e_notimpl, /* get_CanGoBack */
    (void *)webview2_stub_e_notimpl, /* get_CanGoForward */
    (void *)webview2_stub_e_notimpl, /* GoBack */
    (void *)webview2_stub_e_notimpl, /* GoForward */
    (void *)webview2_stub_e_notimpl, /* GetDevToolsProtocolEventReceiver */
    (void *)webview2_stub_e_notimpl, /* Stop */
    webview_generic_add_event, /* add_NewWindowRequested */
    webview_generic_remove_event, /* remove_NewWindowRequested */
    webview_generic_add_event, /* add_DocumentTitleChanged */
    webview_generic_remove_event, /* remove_DocumentTitleChanged */
    (void *)webview2_stub_e_notimpl, /* get_DocumentTitle */
    (void *)webview2_stub_e_notimpl, /* AddHostObjectToScript */
    (void *)webview2_stub_e_notimpl, /* RemoveHostObjectFromScript */
    (void *)webview2_stub_e_notimpl, /* OpenDevToolsWindow */
    webview_generic_add_event, /* add_ContainsFullScreenElementChanged */
    webview_generic_remove_event, /* remove_ContainsFullScreenElementChanged */
    (void *)webview2_stub_e_notimpl, /* get_ContainsFullScreenElement */
    webview_generic_add_event, /* add_WebResourceRequested */
    webview_generic_remove_event, /* remove_WebResourceRequested */
    (void *)webview2_stub_e_notimpl, /* AddWebResourceRequestedFilter */
    (void *)webview2_stub_e_notimpl, /* RemoveWebResourceRequestedFilter */
    webview_generic_add_event, /* add_WindowCloseRequested */
    webview_generic_remove_event, /* remove_WindowCloseRequested */
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
        webview_get_Settings,
        webview_get_Source, webview_Navigate, webview_NavigateToString,
        webview_generic_add_event, /* add_NavigationStarting */
        webview_generic_remove_event, /* remove_NavigationStarting */
        webview_generic_add_event, /* add_ContentLoading */
        webview_generic_remove_event, /* remove_ContentLoading */
        webview_generic_add_event, /* add_SourceChanged */
        webview_generic_remove_event, /* remove_SourceChanged */
        webview_generic_add_event, /* add_HistoryChanged */
        webview_generic_remove_event, /* remove_HistoryChanged */
        webview_add_NavigationCompleted, webview_remove_NavigationCompleted,
        webview_generic_add_event, /* add_FrameNavigationStarting */
        webview_generic_remove_event, /* remove_FrameNavigationStarting */
        webview_generic_add_event, /* add_FrameNavigationCompleted */
        webview_generic_remove_event, /* remove_FrameNavigationCompleted */
        webview_generic_add_event, /* add_ScriptDialogOpening */
        webview_generic_remove_event, /* remove_ScriptDialogOpening */
        webview_generic_add_event, /* add_PermissionRequested */
        webview_generic_remove_event, /* remove_PermissionRequested */
        webview_generic_add_event, /* add_ProcessFailed */
        webview_generic_remove_event, /* remove_ProcessFailed */
        webview_AddScriptToExecuteOnDocumentCreated,
        webview_RemoveScriptToExecuteOnDocumentCreated,
        (void *)webview2_stub_e_notimpl, /* ExecuteScript */
        (void *)webview2_stub_e_notimpl, /* CapturePreview */
        (void *)webview2_stub_e_notimpl, /* Reload */
        (void *)webview2_stub_e_notimpl, /* PostWebMessageAsJson */
        (void *)webview2_stub_e_notimpl, /* PostWebMessageAsString */
        webview_add_WebMessageReceived,
        webview_remove_WebMessageReceived,
        (void *)webview2_stub_e_notimpl, /* CallDevToolsProtocolMethod */
        (void *)webview2_stub_e_notimpl, /* get_BrowserProcessId */
        (void *)webview2_stub_e_notimpl, /* get_CanGoBack */
        (void *)webview2_stub_e_notimpl, /* get_CanGoForward */
        (void *)webview2_stub_e_notimpl, /* GoBack */
        (void *)webview2_stub_e_notimpl, /* GoForward */
        (void *)webview2_stub_e_notimpl, /* GetDevToolsProtocolEventReceiver */
        (void *)webview2_stub_e_notimpl, /* Stop */
        webview_generic_add_event, /* add_NewWindowRequested */
        webview_generic_remove_event, /* remove_NewWindowRequested */
        webview_generic_add_event, /* add_DocumentTitleChanged */
        webview_generic_remove_event, /* remove_DocumentTitleChanged */
        (void *)webview2_stub_e_notimpl, /* get_DocumentTitle */
        (void *)webview2_stub_e_notimpl, /* AddHostObjectToScript */
        (void *)webview2_stub_e_notimpl, /* RemoveHostObjectFromScript */
        (void *)webview2_stub_e_notimpl, /* OpenDevToolsWindow */
        webview_generic_add_event, /* add_ContainsFullScreenElementChanged */
        webview_generic_remove_event, /* remove_ContainsFullScreenElementChanged */
        (void *)webview2_stub_e_notimpl, /* get_ContainsFullScreenElement */
        webview_generic_add_event, /* add_WebResourceRequested */
        webview_generic_remove_event, /* remove_WebResourceRequested */
        (void *)webview2_stub_e_notimpl, /* AddWebResourceRequestedFilter */
        (void *)webview2_stub_e_notimpl, /* RemoveWebResourceRequestedFilter */
        webview_generic_add_event, /* add_WindowCloseRequested */
        webview_generic_remove_event, /* remove_WindowCloseRequested */
    },
    {
        webview_generic_add_event, /* add_WebResourceResponseReceived */
        webview_generic_remove_event, /* remove_WebResourceResponseReceived */
        (void *)webview2_stub_e_notimpl, /* NavigateWithWebResourceRequest */
        webview_generic_add_event, /* add_DOMContentLoaded */
        webview_generic_remove_event, /* remove_DOMContentLoaded */
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
    /* Final fallback for ICoreWebView2 -- reached once neither IUnknown,
     * ICoreWebView2, nor ICoreWebView2_2 matched riid. */
    WARN("no interface for %s\n", debugstr_guid(riid));
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
