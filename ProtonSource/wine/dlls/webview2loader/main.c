#include <stdarg.h>
#include <stdlib.h>

#include <windef.h>
#include <winbase.h>
#include <winternl.h>
#include <wine/debug.h>
#include <wine/unixlib.h>

/* This is the one .c file in the module where the two
 * DEFINE_GUID(IID_ICoreWebView2Environment, ...) /
 * DEFINE_GUID(IID_ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler,
 * ...) lines in webview2loader_private.h actually allocate storage for
 * those GUIDs, rather than just extern-declaring them -- same convention
 * as dlls/mfplat/main.c's own "#include initguid.h before the specific
 * headers whose GUIDs are wanted" pattern in this fork. environment.c
 * includes the private header without INITGUID defined, so it only sees
 * the extern declarations and links against these definitions.
 *
 * <objbase.h> is included here, BEFORE <initguid.h>, on purpose: the
 * private header below also includes <objbase.h>, which pulls in widl's
 * generated headers for IUnknown/IMalloc/IMallocSpy/etc., each of which
 * uses DEFINE_GUID for ITS OWN GUIDs too. If INITGUID were still active
 * when that chain runs, main.o would end up allocating storage for every
 * GUID objbase.h's chain touches (measured: 133 of them), not just the 2
 * this module actually owns -- and the first task to add an interface
 * whose GUID isn't already accidentally covered here would collide with
 * libs/uuid's own definitions at link time. Including <objbase.h> here
 * first means the private header's own #include <objbase.h> is a no-op
 * (include guard), so INITGUID is live for exactly the two DEFINE_GUID
 * lines in webview2loader_private.h and nothing else. */
#include <objbase.h>
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
        /* CreateCoreWebView2EnvironmentWithOptions (and, from Task 6/7/8
         * onward, CreateCoreWebView2Controller/Navigate) hand work off to
         * a detached worker thread via start_async_work() and return
         * immediately; the completion handler it invokes, and the cleanup
         * after it, are all code/data inside this module. A caller is
         * free to FreeLibrary() this DLL the instant its completion
         * handler runs, which can race the worker thread still unwinding
         * afterward -- unmapping the module out from under it. Pin the
         * module for the process lifetime to close that race, same as
         * dlls/msvcrt/main.c does for the same "can't safely unload while
         * our own code might still be running" reason. */
        LdrAddRefDll(LDR_ADDREF_DLL_PIN, inst);
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

HRESULT WINAPI webview2_stub_e_notimpl(void *iface, ...)
{
    return E_NOTIMPL;
}
