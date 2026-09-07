#include <stdarg.h>
#include <stdlib.h>
#include <string.h>

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

HINSTANCE webview2loader_instance;

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
        /* Recorded for window_sync.c's WINEVENT_INCONTEXT hook, which cannot be
         * installed without a module handle. Safe to keep for the process
         * lifetime precisely because of the pin just above. */
        webview2loader_instance = inst;
        break;
    }
    return TRUE;
}

/* --- Event pump ---
 *
 * Wine's unixlib boundary only runs one way: PE code calls into the unix half,
 * never the reverse. So the way the helper "pushes" an event up is for this
 * side to park a thread inside a unix call that blocks until one arrives --
 * which is what unix_wait_event does. That thread must be dedicated, because
 * the call spends nearly all of a session blocked.
 *
 * Started lazily on the first controller creation rather than at DllMain time:
 * a process that never creates a webview (and there are several -- the
 * CookieManager flow, GetAvailableCoreWebView2BrowserVersionString probes)
 * should not carry a permanently blocked thread and a spawned helper for
 * nothing. One pump serves every webview in the process; events name their
 * target by handle.
 *
 * Handlers are invoked on THIS thread, not marshaled to Studio's UI thread.
 * That matches what this DLL already does for NavigationCompleted (see
 * navigate_worker, which Invokes from its own worker thread) rather than
 * introducing a second, inconsistent convention -- worth revisiting together
 * if either turns out to matter, since real WebView2 delivers events on the
 * thread that created the environment. */
/* One web message on its way to Studio's handler, off the pump thread. See the
 * WEBVIEW2LOADER_EVENT_WEB_MESSAGE case for why it cannot be delivered inline. */
struct web_message_dispatch
{
    UINT64 handle;
    BOOL is_string;
    WCHAR message[WEBVIEW2LOADER_WEB_MESSAGE_MAX];
    WCHAR source[WEBVIEW2LOADER_URI_MAX];
};

static DWORD WINAPI web_message_dispatch_proc(void *arg)
{
    struct web_message_dispatch *d = arg;
    ICoreWebView2 *webview = webview_find_by_handle(d->handle);

    if (webview)
    {
        webview_fire_web_message(webview, d->message, d->source, d->is_string);
        ICoreWebView2_Release(webview);
    }
    else TRACE("WebMessageReceived for an unknown/closed handle -- dropping\n");

    free(d);
    return 0;
}

static DWORD WINAPI event_pump_proc(void *arg)
{
    /* Heap, allocated once for the life of this thread: struct
     * wait_event_params grew a 128 KB web-message field, which is far too much
     * to keep on a thread stack. */
    struct wait_event_params *params = calloc(1, sizeof(*params));

    if (!params)
    {
        ERR("out of memory starting the event pump -- WebMessageReceived and NavigationStarting "
            "will not fire\n");
        return 0;
    }

    for (;;)
    {
        NTSTATUS status;

        memset(params, 0, sizeof(*params));
        status = WEBVIEW2LOADER_UNIX_CALL(wait_event, params);
        /* Plain truth test rather than a STATUS_SUCCESS comparison: this file
         * does not include <ntstatus.h> (only unixlib.c does, with the
         * WIN32_NO_STATUS dance), and every NTSTATUS success code is zero. */
        if (status)
        {
            /* No event channel, the helper died, or a frame we cannot parse.
             * All three mean this pump has nothing left to do; a respawned
             * helper gets a fresh pump from the next controller creation. */
            TRACE("event pump stopping (status %#lx)\n", (unsigned long)status);
            free(params);
            return 0;
        }

        switch (params->type)
        {
        case WEBVIEW2LOADER_EVENT_NAVIGATION_STARTING:
        {
            ICoreWebView2 *webview = webview_find_by_handle(params->handle);
            if (!webview)
            {
                /* Normal, not an error: the controller can be closed between
                 * the helper sending and this thread waking. */
                TRACE("NavigationStarting for an unknown/closed handle -- dropping\n");
                break;
            }
            webview_fire_navigation_starting(webview, params->uri, params->is_redirect);
            ICoreWebView2_Release(webview);
            break;
        }
        case WEBVIEW2LOADER_EVENT_WEB_MESSAGE:
        {
            /* Handed to a worker, NOT invoked here.
             *
             * This thread is the only thing draining the helper's event socket.
             * Studio's WebMessageReceived handler is free to call straight back
             * into WebView2 (PostWebMessageAsJson, and the Toolbox's own
             * internal:init handshake does exactly that), which takes
             * g_ipc_mutex and waits on the helper -- while the helper may be
             * blocked writing the next event into a socket only this thread
             * empties. That is a deadlock, and it hung Studio inside
             * CreateCoreWebView2EnvironmentWithOptions until it gave up on the
             * Toolbox entirely.
             *
             * Dispatching elsewhere keeps this loop free to drain no matter what
             * a handler does. NavigationStarting stays inline: it fires at most
             * once per navigation, and its handler completes the login rather
             * than calling back. */
            struct web_message_dispatch *d = malloc(sizeof(*d));

            if (!d) break;
            d->handle = params->handle;
            d->is_string = params->is_string;
            memcpy(d->message, params->message, sizeof(d->message));
            memcpy(d->source, params->uri, sizeof(d->source));
            if (!start_async_work(web_message_dispatch_proc, d))
            {
                WARN("could not dispatch a web message -- dropping it\n");
                free(d);
            }
            break;
        }
        default:
            FIXME("unhandled event type %u\n", params->type);
            break;
        }
    }
}

void webview_start_event_pump(void)
{
    static LONG started;

    /* Exactly one pump per process, however many controllers get created and
     * from however many threads. */
    if (InterlockedCompareExchange(&started, 1, 0) != 0) return;
    if (!start_async_work(event_pump_proc, NULL))
    {
        WARN("could not start the event pump -- NavigationStarting will not fire, and the helper "
             "falls back to handing redirects to xdg-open\n");
        InterlockedExchange(&started, 0); /* let a later attempt retry */
    }
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
    /* Real callers (confirmed via a real Roblox Studio launch trace) use
     * this free-function export to check "is a WebView2 runtime installed
     * at all" BEFORE ever attempting to create an environment -- it's the
     * very first webview2loader trace line Studio emits on startup. Task 3
     * left this as an unconditional E_FAIL stub, which made Studio believe
     * no runtime was present and fall back to its "WebView2 installation
     * required" login dialog, even though CreateCoreWebView2EnvironmentWithOptions/
     * CreateCoreWebView2Controller both actually work fine.
     *
     * Same rationale and same fixed version string as
     * environment_get_BrowserVersionString (environment.c)'s already-reviewed
     * ICoreWebView2Environment::get_BrowserVersionString implementation: we
     * control "the runtime" entirely (our own WebKitGTK bundle), so a fixed,
     * plausible-looking version string that never changes out from under
     * Roblox is correct here too, not a placeholder -- and it must match
     * environment_get_BrowserVersionString exactly so Roblox sees the same
     * version whichever of the two functions it calls. */
    static const WCHAR version[] = L"109.0.1518.140";
    SIZE_T len = ARRAY_SIZE(version);

    TRACE("(%s, %p)\n", debugstr_w(browserExecutableFolder), versionInfo);

    if (!versionInfo) return E_POINTER;
    if (!(*versionInfo = CoTaskMemAlloc(len * sizeof(WCHAR)))) return E_OUTOFMEMORY;
    memcpy(*versionInfo, version, len * sizeof(WCHAR));
    return S_OK;
}

/* Parses up to 4 dot-separated numeric components from a WebView2 version
 * string (e.g. "109.0.1518.140", the exact format
 * GetAvailableCoreWebView2BrowserVersionString/environment_get_BrowserVersionString
 * both return above/in environment.c). Missing trailing components (e.g.
 * "109.0") default to 0. Anything after the numeric part -- a trailing
 * " beta"/" dev"/" canary" channel suffix, or any other junk -- is
 * deliberately left unparsed and ignored: this matches
 * CompareBrowserVersions's own real documented contract (learn.microsoft.com:
 * "Directly use the versionInfo obtained from
 * GetAvailableCoreWebView2BrowserVersionString as input, channel information
 * is ignored"), not an omission. Returns FALSE only if the string has no
 * digits at all, i.e. is unparsable as a version -- the real API's
 * documented E_INVALIDARG case below. No wcstoul/wchar.h dependency on
 * purpose, matching this file's existing minimal-includes style. */
static BOOL parse_browser_version(LPCWSTR s, UINT32 parts[4])
{
    int i;
    BOOL any_digit = FALSE;

    parts[0] = parts[1] = parts[2] = parts[3] = 0;
    if (!s) return FALSE;

    for (i = 0; i < 4 && *s; i++)
    {
        UINT32 v = 0;
        while (*s >= '0' && *s <= '9')
        {
            v = v * 10 + (*s - '0');
            s++;
            any_digit = TRUE;
        }
        parts[i] = v;
        if (*s == '.') s++;
        else break;
    }
    return any_digit;
}

/* Real Roblox Studio calls this immediately after
 * GetAvailableCoreWebView2BrowserVersionString above (confirmed via a real
 * launch trace -- see that function's own comment for the same "is a
 * runtime installed" startup check this is part of). Left entirely
 * unimplemented (not even declared in webview2loader.spec) until now: once
 * GetAvailableCoreWebView2BrowserVersionString stopped being an unconditional
 * E_FAIL stub, Studio's startup check actually reached this call, and Wine's
 * builtin-DLL "unimplemented function" trampoline (winebuild's own
 * mechanism for a spec-file entry that doesn't exist at all) hard-aborts
 * the whole process the instant it's invoked -- a real, worse-than-before
 * regression (a hard native crash in place of the old harmless "WebView2
 * installation required" fallback dialog) that only became reachable as a
 * direct consequence of fixing that other function, so it's fixed here
 * alongside it rather than left as a new dangling stub.
 *
 * Real signature/semantics verified against the real Microsoft.Web.WebView2
 * NuGet package's own WebView2.h (same package/version this module's other
 * real-API work was checked against) plus learn.microsoft.com's own
 * CompareBrowserVersions reference: version1/version2 use
 * GetAvailableCoreWebView2BrowserVersionString's own output format,
 * *result is -1/0/1 for version1 </==/> version2, and E_INVALIDARG is
 * returned "if it fails to parse either version string" or any parameter
 * is NULL -- both matched exactly below. */
HRESULT WINAPI CompareBrowserVersions(PCWSTR version1, PCWSTR version2, int *result)
{
    UINT32 v1[4], v2[4];
    int i;

    TRACE("(%s, %s, %p)\n", debugstr_w(version1), debugstr_w(version2), result);

    if (!version1 || !version2 || !result) return E_INVALIDARG;
    if (!parse_browser_version(version1, v1) || !parse_browser_version(version2, v2))
        return E_INVALIDARG;

    for (i = 0; i < 4; i++)
    {
        if (v1[i] != v2[i])
        {
            *result = v1[i] < v2[i] ? -1 : 1;
            return S_OK;
        }
    }
    *result = 0;
    return S_OK;
}

HRESULT WINAPI webview2_stub_e_notimpl(void *iface, ...)
{
    return E_NOTIMPL;
}
