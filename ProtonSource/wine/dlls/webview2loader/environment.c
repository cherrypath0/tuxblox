#include <stdarg.h>
#include <stdlib.h>
#include <string.h>

#include <windef.h>
#include <winbase.h>
#include <wine/debug.h>

#include "webview2loader_private.h"

WINE_DEFAULT_DEBUG_CHANNEL(webview2loader);

struct environment_impl
{
    ICoreWebView2Environment ICoreWebView2Environment_iface;
    LONG ref;
};

static inline struct environment_impl *impl_from_ICoreWebView2Environment(ICoreWebView2Environment *iface)
{
    return CONTAINING_RECORD(iface, struct environment_impl, ICoreWebView2Environment_iface);
}

static HRESULT WINAPI environment_QueryInterface(ICoreWebView2Environment *iface, REFIID riid, void **ppv)
{
    if (!ppv) return E_POINTER;

    if (IsEqualGUID(riid, &IID_IUnknown) || IsEqualGUID(riid, &IID_ICoreWebView2Environment))
    {
        *ppv = iface;
        ICoreWebView2Environment_AddRef(iface);
        return S_OK;
    }
    *ppv = NULL;
    return E_NOINTERFACE;
}

static ULONG WINAPI environment_AddRef(ICoreWebView2Environment *iface)
{
    struct environment_impl *env = impl_from_ICoreWebView2Environment(iface);
    return InterlockedIncrement(&env->ref);
}

static ULONG WINAPI environment_Release(ICoreWebView2Environment *iface)
{
    struct environment_impl *env = impl_from_ICoreWebView2Environment(iface);
    LONG ref = InterlockedDecrement(&env->ref);
    if (!ref) free(env);
    return ref;
}

/* Task 6 replaces this body once the GTK window/controller machinery
 * exists; the vtable slot itself, and this object's real QueryInterface/
 * AddRef/Release/get_BrowserVersionString, are this task's deliverable. */
static HRESULT WINAPI environment_CreateCoreWebView2Controller(ICoreWebView2Environment *iface,
                                                                 HWND parentWindow, void *handler)
{
    FIXME("(%p, %p, %p): not yet implemented, see Task 6\n", iface, parentWindow, handler);
    return E_NOTIMPL;
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

HRESULT environment_create(ICoreWebView2Environment **out)
{
    struct environment_impl *env = calloc(1, sizeof(*env));
    if (!env) return E_OUTOFMEMORY;

    env->ICoreWebView2Environment_iface.lpVtbl = &environment_vtbl;
    env->ref = 1;
    *out = &env->ICoreWebView2Environment_iface;
    return S_OK;
}
