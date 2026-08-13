#include <stdarg.h>
#include <stdlib.h>
#include <string.h>

#include <windef.h>
#include <winbase.h>
#include <wine/debug.h>

#include "unixlib.h"
#include "webview2loader_private.h"

WINE_DEFAULT_DEBUG_CHANNEL(webview2loader);

/* Task 11 real bug fix, continued again: once every webview_vtbl add_X
 * blocker was cleared, the exact same "E_NOTIMPL from an add_X call
 * treated as fatal" pattern recurred a THIRD time, now on the
 * ENVIRONMENT side -- ICoreWebView2Environment8's own
 * add_ProcessInfosChanged (confirmed via the same direct file-based
 * logger; GetProcessInfos/remove_ProcessInfosChanged/Stop/
 * get_BrowserProcessId called right after are this same cleanup-path
 * pattern already seen for the Controller4/WebMessageReceived/
 * NavigationStarting fixes, not independent blockers -- get_BrowserProcessId
 * specifically is confirmed tolerated: it's E_NOTIMPL on the CookieManager
 * flow too, which succeeds). Same fix, same rationale: real
 * registration-only add/remove (no real dispatch -- nothing fires
 * ProcessInfosChanged, same as nothing fires the webview-side events
 * fixed earlier), GetProcessInfos itself stays E_NOTIMPL since
 * constructing a real ICoreWebView2ProcessInfoCollection is a new,
 * heavier, undefined interface with no evidence yet that a failure
 * there (as opposed to add_ProcessInfosChanged) is what's fatal. */
struct env_listener
{
    struct env_listener *next;
    IUnknown *handler;
    UINT64 token;
};

struct environment_impl
{
    ICoreWebView2Environment ICoreWebView2Environment_iface;
    LONG ref;
    struct env_listener *listeners;
    UINT64 next_token;
    CRITICAL_SECTION cs;
};

static inline struct environment_impl *impl_from_ICoreWebView2Environment(ICoreWebView2Environment *iface)
{
    return CONTAINING_RECORD(iface, struct environment_impl, ICoreWebView2Environment_iface);
}

/* Body defined further down (after environment8_vtbl, which it
 * references), same forward-declare-the-prototype-only pattern webview.c
 * already uses for webview_QueryInterface/webview2_2_vtbl. */
static HRESULT WINAPI environment_QueryInterface(ICoreWebView2Environment *iface, REFIID riid, void **ppv);

static ULONG WINAPI environment_AddRef(ICoreWebView2Environment *iface)
{
    struct environment_impl *env = impl_from_ICoreWebView2Environment(iface);
    return InterlockedIncrement(&env->ref);
}

static ULONG WINAPI environment_Release(ICoreWebView2Environment *iface)
{
    struct environment_impl *env = impl_from_ICoreWebView2Environment(iface);
    LONG ref = InterlockedDecrement(&env->ref);
    if (!ref)
    {
        struct env_listener *l = env->listeners;
        while (l) { struct env_listener *next = l->next; l->handler->lpVtbl->Release(l->handler); free(l); l = next; }
        DeleteCriticalSection(&env->cs);
        free(env);
    }
    return ref;
}

struct create_controller_ctx
{
    ICoreWebView2CreateCoreWebView2ControllerCompletedHandler *handler;
    HWND parent_window;
};

static DWORD WINAPI create_controller_worker(void *arg)
{
    struct create_controller_ctx *ctx = arg;
    struct create_webview_params params = { 0 };
    ICoreWebView2Controller *controller = NULL;
    HRESULT hr = E_FAIL;

    params.is_message_only = (ctx->parent_window == HWND_MESSAGE); /* Task 2 wires this field in */
    WEBVIEW2LOADER_UNIX_CALL(create_webview, &params);
    if (params.handle)
        hr = controller_create(params.handle, ctx->parent_window, &controller);

    ICoreWebView2CreateCoreWebView2ControllerCompletedHandler_Invoke(ctx->handler, hr, controller);
    ICoreWebView2CreateCoreWebView2ControllerCompletedHandler_Release(ctx->handler);
    if (controller) ICoreWebView2Controller_Release(controller);
    free(ctx);
    return 0;
}

static HRESULT WINAPI environment_CreateCoreWebView2Controller(ICoreWebView2Environment *iface,
                                                                 HWND parentWindow, void *handler_raw)
{
    struct create_controller_ctx *ctx;
    ICoreWebView2CreateCoreWebView2ControllerCompletedHandler *handler = handler_raw;

    TRACE("(%p, %p, %p)\n", iface, parentWindow, handler);
    if (!handler) return E_POINTER;
    if (!(ctx = malloc(sizeof(*ctx)))) return E_OUTOFMEMORY;

    ICoreWebView2CreateCoreWebView2ControllerCompletedHandler_AddRef(handler);
    ctx->handler = handler;
    ctx->parent_window = parentWindow; /* Plan 3 Task 1: previously discarded */

    if (!start_async_work(create_controller_worker, ctx))
    {
        ICoreWebView2CreateCoreWebView2ControllerCompletedHandler_Release(handler);
        free(ctx);
        return E_FAIL;
    }
    return S_OK;
}

static HRESULT WINAPI environment_CreateWebResourceResponse(ICoreWebView2Environment *iface, void *content,
                                                              int statusCode, LPCWSTR reasonPhrase,
                                                              LPCWSTR headers, void **response)
{
    FIXME("(%p, %p, %d, %s, %s, %p): stub\n", iface, content, statusCode, debugstr_w(reasonPhrase),
          debugstr_w(headers), response);
    return E_NOTIMPL;
}

static HRESULT WINAPI environment_get_BrowserVersionString(ICoreWebView2Environment *iface, LPWSTR *versionInfo)
{
    /* Real WebView2 reports the installed Edge/WebView2 runtime's version.
     * We control "the runtime" entirely (it's our own WebKitGTK bundle), so
     * a fixed, plausible-looking version string that never changes out from
     * under Roblox is correct here, not a placeholder -- this matches the
     * design spec's explicit "we control the browser version" note. */
    static const WCHAR version[] = L"109.0.1518.140";
    SIZE_T len = ARRAY_SIZE(version);

    if (!versionInfo) return E_POINTER;
    if (!(*versionInfo = CoTaskMemAlloc(len * sizeof(WCHAR)))) return E_OUTOFMEMORY;
    memcpy(*versionInfo, version, len * sizeof(WCHAR));
    return S_OK;
}

static HRESULT WINAPI environment_add_NewBrowserVersionAvailable(ICoreWebView2Environment *iface,
                                                                   void *eventHandler, void *token)
{
    /* Kept as E_NOTIMPL exactly as specified by the task brief (verified
     * against the design spec's own "E_NOTIMPL for unconfirmed call
     * sites" convention) -- NOT a no-op. A caller that registers a
     * handler here gets a real failure back, not a silent success; it
     * will never see a NewBrowserVersionAvailable event, since we never
     * report a new version, but it also never gets told registration
     * "worked" when it didn't. remove_NewBrowserVersionAvailable() below
     * still unconditionally returns S_OK: since add() always fails, no
     * real caller has a token to remove, making that S_OK effectively
     * unreachable in practice rather than actually inconsistent. */
    FIXME("(%p, %p, %p): stub\n", iface, eventHandler, token);
    return E_NOTIMPL;
}

static HRESULT WINAPI environment_remove_NewBrowserVersionAvailable(ICoreWebView2Environment *iface, void *token)
{
    return S_OK;
}

static const ICoreWebView2EnvironmentVtbl environment_vtbl =
{
    environment_QueryInterface,
    environment_AddRef,
    environment_Release,
    environment_CreateCoreWebView2Controller,
    environment_CreateWebResourceResponse,
    environment_get_BrowserVersionString,
    environment_add_NewBrowserVersionAvailable,
    environment_remove_NewBrowserVersionAvailable,
};

/* --- Task 11: ICoreWebView2Environment2..8 extension --- all 11 new slots
 * stay webview2_stub_e_notimpl: unlike the Controller2/3/4 properties, every
 * one of these either constructs a whole new object type this DLL doesn't
 * implement (ICoreWebView2WebResourceRequest, CompositionController,
 * PointerInfo, PrintSettings, ProcessInfoCollection) or registers for a
 * process-lifecycle event unrelated to the login flow -- there's no
 * evidence Studio's login-dialog path calls any of them (only the
 * QueryInterface itself), and getting one of these wrong risks a new bug
 * more than E_NOTIMPL does. See this struct's own comment in
 * webview2loader_private.h for the full rationale. base must be a verbatim
 * copy of environment_vtbl above (8 entries). */

/* Real registration-only add/remove for add_ProcessInfosChanged -- see
 * struct env_listener's own comment above for why this one specifically
 * needed a real body (unlike its siblings here, which stay E_NOTIMPL). */
static HRESULT WINAPI environment8_add_ProcessInfosChanged(ICoreWebView2Environment *iface, void *eventHandler_raw,
                                                             void *token_raw)
{
    struct environment_impl *env = impl_from_ICoreWebView2Environment(iface);
    IUnknown *handler = eventHandler_raw;
    UINT64 *token = token_raw;
    struct env_listener *l;

    if (!handler || !token) return E_POINTER;
    if (!(l = malloc(sizeof(*l)))) return E_OUTOFMEMORY;

    handler->lpVtbl->AddRef(handler);
    l->handler = handler;

    EnterCriticalSection(&env->cs);
    l->token = ++env->next_token;
    l->next = env->listeners;
    env->listeners = l;
    LeaveCriticalSection(&env->cs);

    *token = l->token;
    return S_OK;
}

static HRESULT WINAPI environment8_remove_ProcessInfosChanged(ICoreWebView2Environment *iface, void *token_raw)
{
    struct environment_impl *env = impl_from_ICoreWebView2Environment(iface);
    UINT64 token = (UINT64)(ULONG_PTR)token_raw; /* see webview_remove_NavigationCompleted's comment on this cast */
    struct env_listener **cur;

    EnterCriticalSection(&env->cs);
    for (cur = &env->listeners; *cur; cur = &(*cur)->next)
    {
        if ((*cur)->token == token)
        {
            struct env_listener *dead = *cur;
            *cur = dead->next;
            LeaveCriticalSection(&env->cs);
            dead->handler->lpVtbl->Release(dead->handler);
            free(dead);
            return S_OK;
        }
    }
    LeaveCriticalSection(&env->cs);
    return S_OK; /* real WebView2 tolerates removing an already-gone/unknown token */
}

static const struct webview2_environment8_vtbl_combined environment8_vtbl =
{
    {
        environment_QueryInterface,
        environment_AddRef,
        environment_Release,
        environment_CreateCoreWebView2Controller,
        environment_CreateWebResourceResponse,
        environment_get_BrowserVersionString,
        environment_add_NewBrowserVersionAvailable,
        environment_remove_NewBrowserVersionAvailable,
    },
    {
        (void *)webview2_stub_e_notimpl, /* CreateWebResourceRequest */
        (void *)webview2_stub_e_notimpl, /* CreateCoreWebView2CompositionController */
        (void *)webview2_stub_e_notimpl, /* CreateCoreWebView2PointerInfo */
        (void *)webview2_stub_e_notimpl, /* GetAutomationProviderForWindow */
        (void *)webview2_stub_e_notimpl, /* add_BrowserProcessExited */
        (void *)webview2_stub_e_notimpl, /* remove_BrowserProcessExited */
        (void *)webview2_stub_e_notimpl, /* CreatePrintSettings */
        (void *)webview2_stub_e_notimpl, /* get_UserDataFolder */
        environment8_add_ProcessInfosChanged,
        environment8_remove_ProcessInfosChanged,
        (void *)webview2_stub_e_notimpl, /* GetProcessInfos */
    },
};

static HRESULT WINAPI environment_QueryInterface(ICoreWebView2Environment *iface, REFIID riid, void **ppv)
{
    if (!ppv) return E_POINTER;

    if (IsEqualGUID(riid, &IID_IUnknown) || IsEqualGUID(riid, &IID_ICoreWebView2Environment))
    {
        *ppv = iface;
        ICoreWebView2Environment_AddRef(iface);
        return S_OK;
    }
    if (IsEqualGUID(riid, &IID_ICoreWebView2Environment8))
    {
        /* Real Roblox Studio QueryInterfaces for exactly this IID right
         * after a successful CreateCoreWebView2Controller for the embedded
         * login dialog -- see the extension vtable's own comment in
         * webview2loader_private.h. Same safe lpVtbl-swap technique as
         * controller_QueryInterface's IID_ICoreWebView2Controller4 branch
         * and webview_query_interface_v2's IID_ICoreWebView2_2 branch. */
        struct environment_impl *env = impl_from_ICoreWebView2Environment(iface);
        env->ICoreWebView2Environment_iface.lpVtbl = (const ICoreWebView2EnvironmentVtbl *)&environment8_vtbl;
        *ppv = iface;
        ICoreWebView2Environment_AddRef(iface);
        return S_OK;
    }
    /* Real WebView2 hosts (Roblox Studio included) routinely QueryInterface
     * a freshly-created environment for a newer ICoreWebView2Environment2/
     * 3/... to probe runtime capability before doing anything else with
     * it -- rejecting anything not explicitly handled above is correct. */
    WARN("no interface for %s\n", debugstr_guid(riid));
    *ppv = NULL;
    return E_NOINTERFACE;
}

HRESULT environment_create(ICoreWebView2Environment **out)
{
    struct environment_impl *env = calloc(1, sizeof(*env));
    if (!env) return E_OUTOFMEMORY;

    env->ICoreWebView2Environment_iface.lpVtbl = &environment_vtbl;
    env->ref = 1;
    InitializeCriticalSection(&env->cs);
    *out = &env->ICoreWebView2Environment_iface;
    return S_OK;
}
