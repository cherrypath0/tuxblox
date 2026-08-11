#include <stdarg.h>
#include <stdlib.h>

#include <windef.h>
#include <winbase.h>
#include <wine/debug.h>
#include <wine/unixlib.h>

/* Must come before webview2loader_private.h: this is the one .c file in
 * the module where DEFINE_GUID(IID_ICoreWebView2Environment, ...) and
 * DEFINE_GUID(IID_ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler,
 * ...) in that header actually allocate storage for those GUIDs, rather
 * than just extern-declaring them -- same convention as
 * dlls/mfplat/main.c's own "#include initguid.h before the private header
 * that DEFINE_GUIDs" pattern in this fork. environment.c includes the same
 * header without this, so it only sees the extern declarations and links
 * against these definitions. */
#include <initguid.h>

#include "unixlib.h"
#include "webview2loader_private.h"

WINE_DEFAULT_DEBUG_CHANNEL(webview2loader);

BOOL WINAPI DllMain(HINSTANCE inst, DWORD reason, LPVOID reserved)
{
    switch (reason)
    {
    case DLL_PROCESS_ATTACH:
        DisableThreadLibraryCalls(inst);
        if (__wine_init_unix_call()) __wine_unixlib_handle = 0;
        break;
    }
    return TRUE;
}

BOOL start_async_work(LPTHREAD_START_ROUTINE proc, void *ctx)
{
    HANDLE thread = CreateThread(NULL, 0, proc, ctx, 0, NULL);
    if (!thread) return FALSE;
    CloseHandle(thread);
    return TRUE;
}

/* Not exported. Called from CreateCoreWebView2EnvironmentWithOptions's
 * worker thread below (and still directly reachable for the unix-side
 * dlopen/GTK-thread tests that predate that wiring). */
BOOL webview2loader_unix_init(void)
{
    struct init_params params = { 0 };
    WEBVIEW2LOADER_UNIX_CALL(init, &params);
    return params.success;
}

struct create_environment_ctx
{
    ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler *handler;
};

static DWORD WINAPI create_environment_worker(void *arg)
{
    struct create_environment_ctx *ctx = arg;
    ICoreWebView2Environment *env = NULL;
    HRESULT hr;

    if (!webview2loader_unix_init())
        hr = E_FAIL;
    else
        hr = environment_create(&env);

    ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler_Invoke(ctx->handler, hr, env);
    ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler_Release(ctx->handler);
    if (env) ICoreWebView2Environment_Release(env);
    free(ctx);
    return 0;
}

HRESULT WINAPI CreateCoreWebView2EnvironmentWithOptions(PCWSTR browserExecutableFolder, PCWSTR userDataFolder,
                                                          void *environmentOptions, void *environmentCreatedHandler)
{
    struct create_environment_ctx *ctx;
    ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler *handler = environmentCreatedHandler;

    TRACE("(%s, %s, %p, %p)\n", debugstr_w(browserExecutableFolder), debugstr_w(userDataFolder),
          environmentOptions, handler);

    if (!handler) return E_POINTER;
    if (!(ctx = malloc(sizeof(*ctx)))) return E_OUTOFMEMORY;

    ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler_AddRef(handler);
    ctx->handler = handler;

    if (!start_async_work(create_environment_worker, ctx))
    {
        ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler_Release(handler);
        free(ctx);
        return E_FAIL;
    }
    return S_OK;
}

HRESULT WINAPI GetAvailableCoreWebView2BrowserVersionString(PCWSTR browserExecutableFolder, LPWSTR *versionInfo)
{
    TRACE("(%s, %p): stub\n", debugstr_w(browserExecutableFolder), versionInfo);
    if (versionInfo) *versionInfo = NULL;
    return E_FAIL;
}
