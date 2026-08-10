#include <stdarg.h>

#include <windef.h>
#include <winbase.h>
#include <wine/debug.h>
#include <wine/unixlib.h>

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

/* Not exported -- just so Task 4's dlopen/GTK-thread machinery is
 * independently testable before any real DLL export calls into it.
 * CreateCoreWebView2EnvironmentWithOptions starts calling this in Task 5. */
BOOL webview2loader_unix_init(void)
{
    struct init_params params = { 0 };
    WEBVIEW2LOADER_UNIX_CALL(init, &params);
    return params.success;
}

HRESULT WINAPI CreateCoreWebView2EnvironmentWithOptions(PCWSTR browserExecutableFolder, PCWSTR userDataFolder,
                                                          void *environmentOptions, void *environmentCreatedHandler)
{
    TRACE("(%s, %s, %p, %p): stub\n", debugstr_w(browserExecutableFolder), debugstr_w(userDataFolder),
          environmentOptions, environmentCreatedHandler);
    return E_FAIL;
}

HRESULT WINAPI GetAvailableCoreWebView2BrowserVersionString(PCWSTR browserExecutableFolder, LPWSTR *versionInfo)
{
    TRACE("(%s, %p): stub\n", debugstr_w(browserExecutableFolder), versionInfo);
    if (versionInfo) *versionInfo = NULL;
    return E_FAIL;
}
