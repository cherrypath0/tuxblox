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

/* NavigationStarting gets its own list rather than sharing generic_listeners.
 * It is the one formerly-generic event this DLL actually fires, so it needs to
 * be walked selectively -- invoking every generic handler (ContentLoading,
 * SourceChanged, HistoryChanged, ...) with NavigationStarting args would call
 * each of them through the wrong interface. */
struct nav_starting_listener
{
    struct nav_starting_listener *next;
    ICoreWebView2NavigationStartingEventHandler *handler;
    UINT64 token;
};

struct webview_impl
{
    ICoreWebView2 ICoreWebView2_iface;
    LONG ref;
    UINT64 native_handle;
    LPWSTR source;
    struct nav_listener *listeners;
    struct nav_starting_listener *nav_starting_listeners;
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

/* --- handle -> webview registry ---
 *
 * Events arriving on the event channel name a webview by its native handle;
 * this is how the pump thread turns that back into the COM object to fire on.
 * Needed only because events flow the "wrong" way: every other path already
 * has the ICoreWebView2* in hand.
 *
 * Keyed on the handle the webview was CREATED with, deliberately not on
 * wv->native_handle, which Close() zeroes -- an event that races a Close would
 * otherwise fail to match anything and be silently dropped. Liveness is
 * handled by the ref taken below instead.
 *
 * Same statically-initialised CRITICAL_SECTION pattern window_sync.c already
 * uses for its own cross-thread registry. */
#define MAX_TRACKED_WEBVIEWS 64

struct tracked_webview { UINT64 handle; ICoreWebView2 *webview; };
static struct tracked_webview g_tracked[MAX_TRACKED_WEBVIEWS];

static CRITICAL_SECTION tracked_cs;
static CRITICAL_SECTION_DEBUG tracked_cs_debug =
{
    0, 0, &tracked_cs,
    { &tracked_cs_debug.ProcessLocksList, &tracked_cs_debug.ProcessLocksList },
      0, 0, { (DWORD_PTR)(__FILE__ ": tracked_cs") }
};
static CRITICAL_SECTION tracked_cs = { &tracked_cs_debug, -1, 0, 0, 0, 0 };

static void webview_track(UINT64 handle, ICoreWebView2 *webview)
{
    int i;
    if (!handle) return;
    EnterCriticalSection(&tracked_cs);
    for (i = 0; i < MAX_TRACKED_WEBVIEWS; i++)
        if (!g_tracked[i].webview) { g_tracked[i].handle = handle; g_tracked[i].webview = webview; break; }
    LeaveCriticalSection(&tracked_cs);
    if (i == MAX_TRACKED_WEBVIEWS)
        WARN("webview registry full (%d) -- events for handle %s will not be delivered\n",
             MAX_TRACKED_WEBVIEWS, wine_dbgstr_longlong(handle));
}

static void webview_untrack(ICoreWebView2 *webview)
{
    int i;
    EnterCriticalSection(&tracked_cs);
    for (i = 0; i < MAX_TRACKED_WEBVIEWS; i++)
        if (g_tracked[i].webview == webview) { g_tracked[i].webview = NULL; g_tracked[i].handle = 0; }
    LeaveCriticalSection(&tracked_cs);
}

/* Returns an AddRef'd webview for `handle`, or NULL. The ref is taken under the
 * same lock that guards the table, so the object cannot reach refcount zero
 * between being found here and being used by the caller -- which matters
 * because the caller is the pump thread and the owner is Studio's UI thread.
 * Caller releases. */
ICoreWebView2 *webview_find_by_handle(UINT64 handle)
{
    ICoreWebView2 *found = NULL;
    int i;

    if (!handle) return NULL;
    EnterCriticalSection(&tracked_cs);
    for (i = 0; i < MAX_TRACKED_WEBVIEWS; i++)
        if (g_tracked[i].webview && g_tracked[i].handle == handle)
        {
            found = g_tracked[i].webview;
            ICoreWebView2_AddRef(found);
            break;
        }
    LeaveCriticalSection(&tracked_cs);
    return found;
}

/* Final-review fix (Important 1, native_handle use-after-free): see these
 * functions' own declaration comments in webview2loader_private.h. Guarded
 * by wv->cs, same lock webview_get_Source/navigate_worker already use for
 * wv->source -- native_handle can now be written (invalidated) from a
 * different thread than the ones reading it (a controller's Close() vs. an
 * in-flight Navigate/GetCookies worker thread), so this needs the same
 * protection, not a bare field read. */
UINT64 webview_get_native_handle(ICoreWebView2 *iface)
{
    struct webview_impl *wv = impl_from_ICoreWebView2(iface);
    UINT64 handle;

    EnterCriticalSection(&wv->cs);
    handle = wv->native_handle;
    LeaveCriticalSection(&wv->cs);
    return handle;
}

void webview_invalidate_native_handle(ICoreWebView2 *iface)
{
    struct webview_impl *wv = impl_from_ICoreWebView2(iface);

    EnterCriticalSection(&wv->cs);
    wv->native_handle = 0;
    LeaveCriticalSection(&wv->cs);
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

/* --- NavigationStartingEventArgs: same throwaway-object shape as
 * NavigationCompletedEventArgs above, built fresh per event. --- */
struct nav_starting_args_impl
{
    ICoreWebView2NavigationStartingEventArgs iface;
    LONG ref;
    LPWSTR uri;      /* owned; freed on release */
    BOOL is_redirect;
    BOOL cancel;
    UINT64 navigation_id;
};

static HRESULT WINAPI nsargs_QueryInterface(ICoreWebView2NavigationStartingEventArgs *iface, REFIID riid, void **ppv)
{
    if (IsEqualGUID(riid, &IID_IUnknown))
    { *ppv = iface; ICoreWebView2NavigationStartingEventArgs_AddRef(iface); return S_OK; }
    *ppv = NULL;
    return E_NOINTERFACE;
}
static ULONG WINAPI nsargs_AddRef(ICoreWebView2NavigationStartingEventArgs *iface)
{ return InterlockedIncrement(&((struct nav_starting_args_impl *)iface)->ref); }
static ULONG WINAPI nsargs_Release(ICoreWebView2NavigationStartingEventArgs *iface)
{
    struct nav_starting_args_impl *a = (struct nav_starting_args_impl *)iface;
    LONG ref = InterlockedDecrement(&a->ref);
    if (!ref) { CoTaskMemFree(a->uri); free(a); }
    return ref;
}
static HRESULT WINAPI nsargs_get_Uri(ICoreWebView2NavigationStartingEventArgs *iface, LPWSTR *uri)
{
    struct nav_starting_args_impl *a = (struct nav_starting_args_impl *)iface;
    SIZE_T len;

    if (!uri) return E_POINTER;
    /* Real WebView2 hands back a fresh CoTaskMemAlloc'd copy the caller frees
     * (learn.microsoft.com: "The caller must free the returned string with
     * CoTaskMemFree") -- same copy-out convention webview_get_Source already
     * follows, not a pointer to our own storage. */
    len = a->uri ? wcslen(a->uri) + 1 : 1;
    if (!(*uri = CoTaskMemAlloc(len * sizeof(WCHAR)))) return E_OUTOFMEMORY;
    memcpy(*uri, a->uri ? a->uri : L"", len * sizeof(WCHAR));
    return S_OK;
}
static HRESULT WINAPI nsargs_get_IsUserInitiated(ICoreWebView2NavigationStartingEventArgs *iface, BOOL *value)
{
    /* The navigations this fires for are page-driven redirects out of the OAuth
     * flow, not user gestures. Reporting FALSE is the honest answer for that
     * case; WebKit's own decide-policy does expose a gesture flag, which is
     * worth plumbing through if anything ever depends on it. */
    if (!value) return E_POINTER;
    *value = FALSE;
    return S_OK;
}
static HRESULT WINAPI nsargs_get_IsRedirected(ICoreWebView2NavigationStartingEventArgs *iface, BOOL *value)
{
    if (!value) return E_POINTER;
    *value = ((struct nav_starting_args_impl *)iface)->is_redirect;
    return S_OK;
}
static HRESULT WINAPI nsargs_get_RequestHeaders(ICoreWebView2NavigationStartingEventArgs *iface, void **headers)
{
    /* Real slot, deliberately unimplemented: it returns an
     * ICoreWebView2HttpRequestHeaders this DLL has no implementation of, and
     * real WebView2 documents the headers as unmodifiable during this event
     * anyway. The slot must exist so every later slot keeps its index. */
    if (headers) *headers = NULL;
    return E_NOTIMPL;
}
static HRESULT WINAPI nsargs_get_Cancel(ICoreWebView2NavigationStartingEventArgs *iface, BOOL *cancel)
{
    if (!cancel) return E_POINTER;
    *cancel = ((struct nav_starting_args_impl *)iface)->cancel;
    return S_OK;
}
static HRESULT WINAPI nsargs_put_Cancel(ICoreWebView2NavigationStartingEventArgs *iface, BOOL cancel)
{
    /* Recorded and readable back, but nothing acts on it: the helper already
     * suppressed this navigation before sending the event (see
     * on_decide_policy's webkit_policy_decision_ignore), which is what lets the
     * event be one-way instead of a synchronous round trip. A handler setting
     * Cancel=TRUE therefore gets exactly the outcome it asked for; one setting
     * FALSE does not get the navigation resumed, which is the one real
     * divergence from Windows here. Studio's login handler cancels, so this
     * matters for correctness of the flow we're fixing, not for it. */
    ((struct nav_starting_args_impl *)iface)->cancel = cancel;
    return S_OK;
}
static HRESULT WINAPI nsargs_get_NavigationId(ICoreWebView2NavigationStartingEventArgs *iface, UINT64 *id)
{
    if (!id) return E_POINTER;
    *id = ((struct nav_starting_args_impl *)iface)->navigation_id;
    return S_OK;
}

/* Slot order verified against Microsoft.Web.WebView2 1.0.4129.50's own
 * WebView2.h -- see the vtable's declaration comment in
 * webview2loader_private.h for why the docs page must not be used for this. */
static const ICoreWebView2NavigationStartingEventArgsVtbl nsargs_vtbl =
{
    nsargs_QueryInterface,
    nsargs_AddRef,
    nsargs_Release,
    nsargs_get_Uri,
    nsargs_get_IsUserInitiated,
    nsargs_get_IsRedirected,
    nsargs_get_RequestHeaders,
    nsargs_get_Cancel,
    nsargs_put_Cancel,
    nsargs_get_NavigationId,
};

/* Fires NavigationStarting on every registered handler. Called from the event
 * pump thread (see main.c's event_pump_proc).
 *
 * Snapshot-then-invoke, exactly like navigate_worker's own NavigationCompleted
 * dispatch and for the same reason: a handler must not be invoked with wv->cs
 * held (it can call back into this object), but walking the live list without
 * the lock races remove_NavigationStarting freeing a node. AddRef'ing each
 * handler under the lock keeps it alive even if its node is unlinked the
 * instant the lock drops. */
void webview_fire_navigation_starting(ICoreWebView2 *iface, const WCHAR *uri, BOOL is_redirect)
{
    struct webview_impl *wv = impl_from_ICoreWebView2(iface);
    ICoreWebView2NavigationStartingEventHandler **snapshot = NULL;
    struct nav_starting_listener *l;
    SIZE_T count = 0, i;

    EnterCriticalSection(&wv->cs);
    for (l = wv->nav_starting_listeners; l; l = l->next) count++;
    if (count && (snapshot = malloc(count * sizeof(*snapshot))))
    {
        for (l = wv->nav_starting_listeners, i = 0; l; l = l->next, i++)
        {
            ICoreWebView2NavigationStartingEventHandler_AddRef(l->handler);
            snapshot[i] = l->handler;
        }
    }
    else count = 0;
    LeaveCriticalSection(&wv->cs);

    for (i = 0; i < count; i++)
    {
        struct nav_starting_args_impl *args = calloc(1, sizeof(*args));
        if (args)
        {
            SIZE_T len = uri ? wcslen(uri) + 1 : 1;
            args->iface.lpVtbl = &nsargs_vtbl;
            args->ref = 1;
            args->is_redirect = is_redirect;
            args->navigation_id = 0;
            if ((args->uri = CoTaskMemAlloc(len * sizeof(WCHAR))))
                memcpy(args->uri, uri ? uri : L"", len * sizeof(WCHAR));
            ICoreWebView2NavigationStartingEventHandler_Invoke(snapshot[i], &wv->ICoreWebView2_iface, &args->iface);
            ICoreWebView2NavigationStartingEventArgs_Release(&args->iface);
        }
        ICoreWebView2NavigationStartingEventHandler_Release(snapshot[i]);
    }
    free(snapshot);
}

/* --- ICoreWebView2WebMessageReceivedEventArgs, built fresh per event, same
 * shape as nav_starting_args_impl above. Vtable order is the official
 * header's -- see webview2loader_private.h's own comment on this interface. --- */
struct wm_args_impl
{
    ICoreWebView2WebMessageReceivedEventArgs iface;
    LONG ref;
    LPWSTR source;  /* owned */
    LPWSTR message; /* owned; JSON text, or the raw string when is_string */
    BOOL is_string;
};

static HRESULT WINAPI wmargs_QueryInterface(ICoreWebView2WebMessageReceivedEventArgs *iface, REFIID riid, void **ppv)
{
    if (IsEqualGUID(riid, &IID_IUnknown))
    { *ppv = iface; ICoreWebView2WebMessageReceivedEventArgs_AddRef(iface); return S_OK; }
    *ppv = NULL;
    return E_NOINTERFACE;
}
static ULONG WINAPI wmargs_AddRef(ICoreWebView2WebMessageReceivedEventArgs *iface)
{ return InterlockedIncrement(&((struct wm_args_impl *)iface)->ref); }
static ULONG WINAPI wmargs_Release(ICoreWebView2WebMessageReceivedEventArgs *iface)
{
    struct wm_args_impl *a = (struct wm_args_impl *)iface;
    LONG ref = InterlockedDecrement(&a->ref);
    if (!ref) { CoTaskMemFree(a->source); CoTaskMemFree(a->message); free(a); }
    return ref;
}

/* Same copy-out convention as nsargs_get_Uri: a fresh CoTaskMemAlloc'd string
 * the caller frees, never a pointer into our own storage. */
static HRESULT wmargs_copy_out(const WCHAR *src, LPWSTR *out)
{
    SIZE_T len = src ? wcslen(src) + 1 : 1;
    if (!out) return E_POINTER;
    if (!(*out = CoTaskMemAlloc(len * sizeof(WCHAR)))) return E_OUTOFMEMORY;
    memcpy(*out, src ? src : L"", len * sizeof(WCHAR));
    return S_OK;
}

static HRESULT WINAPI wmargs_get_Source(ICoreWebView2WebMessageReceivedEventArgs *iface, LPWSTR *value)
{ return wmargs_copy_out(((struct wm_args_impl *)iface)->source, value); }

static HRESULT WINAPI wmargs_get_WebMessageAsJson(ICoreWebView2WebMessageReceivedEventArgs *iface, LPWSTR *value)
{
    struct wm_args_impl *a = (struct wm_args_impl *)iface;

    /* Real WebView2: get_WebMessageAsJson ALWAYS returns JSON, whichever
     * postMessage overload the page used. The helper already JSON-encodes the
     * string case before sending, so message is valid JSON either way and this
     * is a plain copy-out rather than a re-encode here. */
    return wmargs_copy_out(a->message, value);
}

static HRESULT WINAPI wmargs_TryGetWebMessageAsString(ICoreWebView2WebMessageReceivedEventArgs *iface, LPWSTR *value)
{
    struct wm_args_impl *a = (struct wm_args_impl *)iface;

    /* Real WebView2 fails this (E_INVALIDARG) when the page posted anything
     * other than a string -- Studio is entitled to use that as the test for
     * which overload the page used, so answering with JSON text here would be
     * a lie it could act on. */
    if (!value) return E_POINTER;
    if (!a->is_string) { *value = NULL; return E_INVALIDARG; }
    return wmargs_copy_out(a->message, value);
}

static const ICoreWebView2WebMessageReceivedEventArgsVtbl wmargs_vtbl =
{
    wmargs_QueryInterface,
    wmargs_AddRef,
    wmargs_Release,
    wmargs_get_Source,
    wmargs_get_WebMessageAsJson,
    wmargs_TryGetWebMessageAsString,
};

/* Called from the event pump (main.c) when the helper reports that the page
 * called window.chrome.webview.postMessage. Snapshot-under-lock then invoke
 * outside it, exactly as webview_fire_navigation_starting does -- a handler is
 * free to call back into this object, and holding wv->cs across Invoke would
 * deadlock the moment one did. */
void webview_fire_web_message(ICoreWebView2 *iface, const WCHAR *message, const WCHAR *source, BOOL is_string)
{
    struct webview_impl *wv = impl_from_ICoreWebView2(iface);
    ICoreWebView2WebMessageReceivedEventHandler **snapshot = NULL;
    struct wm_listener *l;
    SIZE_T count = 0, i;

    EnterCriticalSection(&wv->cs);
    for (l = wv->wm_listeners; l; l = l->next) count++;
    if (count && (snapshot = malloc(count * sizeof(*snapshot))))
    {
        for (l = wv->wm_listeners, i = 0; l; l = l->next, i++)
        {
            ICoreWebView2WebMessageReceivedEventHandler_AddRef(
                (ICoreWebView2WebMessageReceivedEventHandler *)l->handler);
            snapshot[i] = (ICoreWebView2WebMessageReceivedEventHandler *)l->handler;
        }
    }
    else count = 0;
    LeaveCriticalSection(&wv->cs);

    /* One-shot MESSAGE per webview reporting how many handlers actually exist.
     * A count of zero means the helper delivered a page message that Studio was
     * never told about -- indistinguishable from "the page sent nothing" unless
     * it is stated. Once per webview, so an active page cannot flood the log. */
    {
        static const void *last_reported;
        if (last_reported != (const void *)wv)
        {
            MESSAGE("webview2loader: WebMessageReceived -> %u handler(s) registered for webview %s\n",
                    (unsigned)count, wine_dbgstr_longlong(wv->native_handle));
            last_reported = wv;
        }
    }

    for (i = 0; i < count; i++)
    {
        struct wm_args_impl *args = calloc(1, sizeof(*args));
        if (args)
        {
            SIZE_T mlen = message ? wcslen(message) + 1 : 1;
            SIZE_T slen = source ? wcslen(source) + 1 : 1;

            args->iface.lpVtbl = &wmargs_vtbl;
            args->ref = 1;
            args->is_string = is_string;
            if ((args->message = CoTaskMemAlloc(mlen * sizeof(WCHAR))))
                memcpy(args->message, message ? message : L"", mlen * sizeof(WCHAR));
            if ((args->source = CoTaskMemAlloc(slen * sizeof(WCHAR))))
                memcpy(args->source, source ? source : L"", slen * sizeof(WCHAR));
            ICoreWebView2WebMessageReceivedEventHandler_Invoke(snapshot[i], &wv->ICoreWebView2_iface, &args->iface);
            ICoreWebView2WebMessageReceivedEventArgs_Release(&args->iface);
        }
        ICoreWebView2WebMessageReceivedEventHandler_Release(snapshot[i]);
    }
    free(snapshot);
}

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
        struct nav_starting_listener *nsl = wv->nav_starting_listeners;
        struct wm_listener *wml = wv->wm_listeners;
        struct generic_listener *gl = wv->generic_listeners;

        /* Out of the registry before any teardown: the pump thread only ever
         * touches this object through webview_find_by_handle, which AddRefs
         * under the registry lock, so removing it here means no new reference
         * can be handed out while it is being destroyed. (Placed after the
         * declarations above, not among them -- Wine builds C90 with
         * -Werror=declaration-after-statement.) */
        webview_untrack(iface);
        while (l) { struct nav_listener *next = l->next; ICoreWebView2NavigationCompletedEventHandler_Release(l->handler); free(l); l = next; }
        while (nsl) { struct nav_starting_listener *next = nsl->next; ICoreWebView2NavigationStartingEventHandler_Release(nsl->handler); free(nsl); nsl = next; }
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

    /* Final-review fix (Important 1): read live, under wv->cs, rather than
     * a bare wv->native_handle -- if Close() ran on the owning controller
     * after this worker was spawned but before it got here, this observes
     * 0 (see webview_invalidate_native_handle), and the unix call below
     * fails cleanly with STATUS_INVALID_HANDLE instead of dereferencing
     * freed unix-side memory. */
    params.handle = webview_get_native_handle(&wv->ICoreWebView2_iface);
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

/* --- NavigationStarting registration (real, and really fired) ---
 *
 * Was webview_generic_add_event/remove: registration succeeded and returned a
 * token, but nothing ever invoked the handler. Same shape as
 * add_NavigationCompleted, just against its own list. */

static HRESULT WINAPI webview_add_NavigationStarting(ICoreWebView2 *iface, void *eventHandler_raw, void *token_raw)
{
    struct webview_impl *wv = impl_from_ICoreWebView2(iface);
    ICoreWebView2NavigationStartingEventHandler *handler = eventHandler_raw;
    UINT64 *token = token_raw;
    struct nav_starting_listener *l;

    if (!handler || !token) return E_POINTER;
    if (!(l = malloc(sizeof(*l)))) return E_OUTOFMEMORY;

    ICoreWebView2NavigationStartingEventHandler_AddRef(handler);
    l->handler = handler;

    EnterCriticalSection(&wv->cs);
    l->token = ++wv->next_token;
    l->next = wv->nav_starting_listeners;
    wv->nav_starting_listeners = l;
    LeaveCriticalSection(&wv->cs);

    *token = l->token;
    return S_OK;
}

static HRESULT WINAPI webview_remove_NavigationStarting(ICoreWebView2 *iface, void *token_raw)
{
    struct webview_impl *wv = impl_from_ICoreWebView2(iface);
    UINT64 token = (UINT64)(ULONG_PTR)token_raw; /* see webview_remove_NavigationCompleted's comment on this cast */
    struct nav_starting_listener **cur;

    EnterCriticalSection(&wv->cs);
    for (cur = &wv->nav_starting_listeners; *cur; cur = &(*cur)->next)
    {
        if ((*cur)->token == token)
        {
            struct nav_starting_listener *dead = *cur;
            *cur = dead->next;
            LeaveCriticalSection(&wv->cs);
            ICoreWebView2NavigationStartingEventHandler_Release(dead->handler);
            free(dead);
            return S_OK;
        }
    }
    LeaveCriticalSection(&wv->cs);
    return S_OK; /* real WebView2 tolerates removing an already-gone/unknown token */
}

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
    HRESULT hr; /* result of the registration the caller already performed */
};

/* Installs Studio's document-start script in the real webview.
 *
 * This used to invent a script id, report S_OK and DISCARD the JavaScript --
 * `javaScript` was never even read. That was survivable while only the login
 * dialog mattered (its flow completes through NavigationStarting on the
 * roblox-studio-auth: redirect and needs no injected script), and it is the
 * confirmed cause of the Toolbox hang: a real session shows the Toolbox
 * navigating to create.roblox.com/store/models with status 200 and
 * NavigationCompleted firing, then nothing for ~60s until Studio falls back to
 * its non-webview Toolbox. The page loads; the host bridge this script installs
 * never arrives, so the page waits forever.
 *
 * Still async, and still reports the id the same way: real WebView2 completes
 * this through the handler rather than the return value, and Studio's own
 * "setInitScript" call site depends on that shape. What changed is that the
 * script now reaches the helper before the handler is invoked, so a page that
 * loads after this completes actually sees it. */
/* Only the completion callback is async -- the registration itself already
 * happened synchronously in the caller. See that function's own comment for why
 * the ordering matters. */
static DWORD WINAPI add_script_worker(void *arg)
{
    struct add_script_ctx *ctx = arg;
    static LONG next_script_id;
    WCHAR id[32];

    wsprintfW(id, L"tuxblox-script-%d", InterlockedIncrement(&next_script_id));
    ICoreWebView2AddScriptToExecuteOnDocumentCreatedCompletedHandler_Invoke(ctx->handler, ctx->hr, id);
    ICoreWebView2AddScriptToExecuteOnDocumentCreatedCompletedHandler_Release(ctx->handler);
    free(ctx);
    return 0;
}

/* Studio -> page. Both overloads share one unix call, distinguished by
 * is_string, because the only thing that differs downstream is how the helper
 * hands the value to the page's own 'message' listeners: AsJson is parsed into
 * a value, AsString is delivered as a plain string.
 *
 * Synchronous, like the other direct calls in this file (count_cookies,
 * sync_window_geometry) and unlike the async Navigate/AddScript pair -- real
 * WebView2 posts these immediately and reports failure through the HRESULT,
 * with no completion handler for a caller to wait on. */
static HRESULT webview_post_web_message(ICoreWebView2 *iface, LPCWSTR message, BOOL is_string)
{
    struct webview_impl *wv = impl_from_ICoreWebView2(iface);
    struct post_web_message_params params = { 0 };

    if (!message) return E_POINTER;
    if (!wv->native_handle) return E_FAIL; /* Close()'d already */

    params.handle = wv->native_handle;
    params.message = message;
    params.is_string = is_string;
    if (WEBVIEW2LOADER_UNIX_CALL(post_web_message, &params) || !params.is_success)
    {
        WARN("could not post a web message to the page on webview %s\n",
             wine_dbgstr_longlong(wv->native_handle));
        return E_FAIL;
    }
    return S_OK;
}

static HRESULT WINAPI webview_PostWebMessageAsJson(ICoreWebView2 *iface, LPCWSTR webMessageAsJson)
{ return webview_post_web_message(iface, webMessageAsJson, FALSE); }

static HRESULT WINAPI webview_PostWebMessageAsString(ICoreWebView2 *iface, LPCWSTR webMessageAsString)
{ return webview_post_web_message(iface, webMessageAsString, TRUE); }

static HRESULT WINAPI webview_AddScriptToExecuteOnDocumentCreated(ICoreWebView2 *iface, LPCWSTR javaScript,
                                                                    void *handler_raw)
{
    struct webview_impl *wv = impl_from_ICoreWebView2(iface);
    struct add_script_ctx *ctx;
    ICoreWebView2AddScriptToExecuteOnDocumentCreatedCompletedHandler *handler = handler_raw;
    struct add_user_script_params params = { 0 };
    HRESULT hr = S_OK;

    if (!handler) return E_POINTER;
    if (!javaScript) return E_POINTER;

    /* Registration is SYNCHRONOUS, before this returns -- deliberately, and
     * unlike every other async method in this file.
     *
     * webview_Navigate also dispatches through start_async_work, so if the
     * registration ran on its own worker too, the two would race for
     * g_ipc_mutex with no ordering between them. Studio's own log shows it
     * moving from setInitScript to the navigation in about 4ms, so the
     * navigate worker winning is not a corner case. WebKit user scripts only
     * apply to document loads that START after registration -- losing that
     * race means the script is registered correctly and still misses the very
     * page it was meant for, which looks exactly like the bug it was supposed
     * to fix.
     *
     * Doing the round trip here means that by the time Studio regains control
     * and can call Navigate, the helper already holds the script. Same pattern
     * as sync_window_geometry and count_cookies, which are likewise synchronous
     * unix calls on the caller's own thread. The completion handler stays async
     * below, because that is the part real WebView2's contract is about. */
    params.handle = wv->native_handle;
    params.script = javaScript;
    if (WEBVIEW2LOADER_UNIX_CALL(add_user_script, &params) || !params.is_success)
    {
        /* Reported to Studio rather than swallowed: silently succeeding here is
         * what made the Toolbox hang look like a rendering problem. */
        ERR("failed to install document-start script on webview %s -- the page will not get "
            "Studio's host bridge\n", wine_dbgstr_longlong(wv->native_handle));
        hr = E_FAIL;
    }

    if (!(ctx = calloc(1, sizeof(*ctx)))) return E_OUTOFMEMORY;
    ICoreWebView2AddScriptToExecuteOnDocumentCreatedCompletedHandler_AddRef(handler);
    ctx->handler = handler;
    ctx->hr = hr;

    if (!start_async_work(add_script_worker, ctx))
    {
        ICoreWebView2AddScriptToExecuteOnDocumentCreatedCompletedHandler_Release(handler);
        free(ctx);
        return E_FAIL;
    }
    return S_OK;
}

/* --- ExecuteScript ---
 *
 * The blocker Studio named itself, in its own log, once the document-start
 * script above actually started reaching the page:
 *
 *   Warning [FLog::StudioEmbeddedBrowserWebView2] executeJavaScript failed
 *   with error code '-2147467263'   (0x80004001 == E_NOTIMPL)
 *
 * Studio drives the Toolbox through ExecuteScript, not PostWebMessageAsJson.
 * The page sends messageBusEvent / internal:init with a uuid and waits for
 * Studio to answer with injected JavaScript; while this was a stub the answer
 * never came and the page spun forever, retrying with fresh uuids.
 *
 * SERIALIZED THROUGH ONE WORKER, not a thread per call like Navigate.
 *
 * Real WebView2 executes scripts in the order they were requested, and a
 * message bridge is exactly the kind of caller that depends on it -- a reply
 * that overtakes the handshake that set up its own state is a corrupt protocol,
 * not a slow one. start_async_work spawns a fresh thread per call, and those
 * threads then race for g_ipc_mutex in whatever order the scheduler picks, so
 * using it here would give up that ordering for nothing.
 *
 * A queue rather than doing the round trip on the caller's own thread (the way
 * AddScriptToExecuteOnDocumentCreated does): ExecuteScript is called
 * repeatedly, from Studio's own threads, for as long as the Toolbox is open,
 * and each call waits on the helper's bounded evaluation. Blocking Studio there
 * would hand it a UI stall whenever the page or another in-flight opcode is
 * slow -- and this module has already paid once for making a Studio thread wait
 * on the helper (see the WebMessageReceived deadlock). Returning immediately
 * and completing through the handler is also what the real API promises.
 *
 * The worker exits when the queue drains and is restarted by the next enqueue,
 * so a process that opens the Toolbox once does not keep a thread parked for
 * the rest of the session. */
struct execute_script_request
{
    struct execute_script_request *next;
    ICoreWebView2 *webview;                             /* AddRef'd by the enqueuer */
    ICoreWebView2ExecuteScriptCompletedHandler *handler; /* AddRef'd by the enqueuer */
    WCHAR *script;                                       /* owned; freed by the worker */
};

static struct execute_script_request *script_queue_head, *script_queue_tail;
static BOOL script_worker_running;

static CRITICAL_SECTION script_queue_cs;
static CRITICAL_SECTION_DEBUG script_queue_cs_debug =
{
    0, 0, &script_queue_cs,
    { &script_queue_cs_debug.ProcessLocksList, &script_queue_cs_debug.ProcessLocksList },
    0, 0, { (DWORD_PTR)(__FILE__ ": script_queue_cs") }
};
static CRITICAL_SECTION script_queue_cs = { &script_queue_cs_debug, -1, 0, 0, 0, 0 };

static void execute_script_request_free(struct execute_script_request *req)
{
    ICoreWebView2ExecuteScriptCompletedHandler_Release(req->handler);
    ICoreWebView2_Release(req->webview);
    free(req->script);
    free(req);
}

static DWORD WINAPI execute_script_worker(void *arg)
{
    struct execute_script_params *params;

    /* One buffer for the whole drain: struct execute_script_params embeds a
     * 128 KB result buffer, so it is heap-allocated (see its own comment in
     * unixlib.h) -- reusing it across queued requests keeps that to one
     * allocation per worker rather than one per script. */
    if (!(params = calloc(1, sizeof(*params))))
    {
        /* Still has to drain, or every queued handler is silently abandoned and
         * Studio waits on completions that can never arrive. */
        ERR("out of memory for the ExecuteScript buffer -- failing every queued script\n");
    }

    for (;;)
    {
        struct execute_script_request *req;
        HRESULT hr = E_FAIL;
        LPCWSTR result = L"null";

        EnterCriticalSection(&script_queue_cs);
        if (!(req = script_queue_head))
        {
            /* Cleared under the lock, so an enqueuer that sees FALSE and starts
             * a new worker cannot be racing this one's last dequeue. */
            script_worker_running = FALSE;
            LeaveCriticalSection(&script_queue_cs);
            free(params);
            return 0;
        }
        if (!(script_queue_head = req->next)) script_queue_tail = NULL;
        LeaveCriticalSection(&script_queue_cs);

        if (params)
        {
            /* Read live rather than captured at enqueue time, same reason
             * navigate_worker does: a Close() between the call and here makes
             * this 0, and the unix call fails cleanly instead of naming a
             * webview the helper has already torn down. */
            params->handle = webview_get_native_handle(req->webview);
            params->script = req->script;
            params->is_success = FALSE;
            if (!WEBVIEW2LOADER_UNIX_CALL(execute_script, params) && params->is_success)
            {
                hr = S_OK;
                result = params->result;
            }
            else
                WARN("the helper could not evaluate a script on webview %s\n",
                     wine_dbgstr_longlong(params->handle));
        }

        /* Invoked even on failure, always with a readable string: real
         * WebView2 always completes an ExecuteScript it accepted, and a caller
         * left waiting forever is precisely the failure mode this method exists
         * to fix. */
        ICoreWebView2ExecuteScriptCompletedHandler_Invoke(req->handler, hr, result);
        execute_script_request_free(req);
    }
}

static HRESULT WINAPI webview_ExecuteScript(ICoreWebView2 *iface, LPCWSTR javaScript, void *handler_raw)
{
    ICoreWebView2ExecuteScriptCompletedHandler *handler = handler_raw;
    struct execute_script_request *req;
    BOOL need_worker;
    SIZE_T len;

    TRACE("(%p, %s, %p)\n", iface, debugstr_w(javaScript), handler_raw);

    if (!javaScript) return E_POINTER;
    /* A NULL handler is legal on real WebView2 -- "run this and don't tell me"
     * -- but every caller here is answering a page that is waiting, so a
     * completion is always wanted and a missing one is a caller bug worth
     * reporting rather than a fire-and-forget shortcut to support. */
    if (!handler) return E_POINTER;

    if (!(req = calloc(1, sizeof(*req)))) return E_OUTOFMEMORY;
    len = wcslen(javaScript) + 1;
    if (!(req->script = malloc(len * sizeof(WCHAR)))) { free(req); return E_OUTOFMEMORY; }
    memcpy(req->script, javaScript, len * sizeof(WCHAR));

    ICoreWebView2_AddRef(iface);
    ICoreWebView2ExecuteScriptCompletedHandler_AddRef(handler);
    req->webview = iface;
    req->handler = handler;

    EnterCriticalSection(&script_queue_cs);
    if (script_queue_tail) script_queue_tail->next = req;
    else script_queue_head = req;
    script_queue_tail = req;
    need_worker = !script_worker_running;
    /* Set before the thread exists, under the same lock the worker clears it
     * under, so two concurrent enqueues can never both start a worker. */
    if (need_worker) script_worker_running = TRUE;
    LeaveCriticalSection(&script_queue_cs);

    if (need_worker && !start_async_work(execute_script_worker, NULL))
    {
        /* Unlink this request again rather than leaving it queued behind a
         * worker that does not exist -- anything already queued is a different
         * worker's problem and is left alone. */
        struct execute_script_request *cur, *prev = NULL;

        EnterCriticalSection(&script_queue_cs);
        script_worker_running = FALSE;
        for (cur = script_queue_head; cur && cur != req; cur = cur->next) prev = cur;
        if (cur)
        {
            if (prev) prev->next = cur->next;
            else script_queue_head = cur->next;
            if (script_queue_tail == cur) script_queue_tail = prev;
        }
        LeaveCriticalSection(&script_queue_cs);
        execute_script_request_free(req);
        return E_FAIL;
    }
    return S_OK; /* real WebView2 semantics: completion arrives through the handler */
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
    /* Not a cached native_handle: the same reasoning cookie_manager_impl
     * documents -- the handle is re-read live so a Close()'d webview is seen
     * as gone rather than addressed by a stale value. */
    ICoreWebView2 *webview;
    LPWSTR user_agent; /* owned; NULL until Studio sets one */
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
    /* Settings2 is layout-compatible with the base by construction (see
     * webview2_settings2_vtbl_combined), so one pointer answers both. */
    if (IsEqualGUID(riid, &IID_IUnknown) || IsEqualGUID(riid, &IID_ICoreWebView2Settings) ||
        IsEqualGUID(riid, &IID_ICoreWebView2Settings2))
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
    if (!ref) { free(s->user_agent); free(s); }
    return ref;
}

/* One get/put pair per BOOL field -- named to match the real property,
 * mechanically identical bodies (matches this file's existing
 * Controller2/3/4 property pattern in controller.c). */
/* Pushes the properties that have a real WebKit equivalent down to the helper.
 *
 * Called from every put_, not just the three that map: it is one small unix
 * call on a settings change (which happens a handful of times per webview, at
 * creation), and pushing the whole set keeps the helper's view consistent
 * without each setter needing to know whether it is one of the mapped ones. */
static void settings_push(struct settings_impl *s)
{
    struct apply_settings_params params = { 0 };

    if (!s->webview) return;
    if (!(params.handle = webview_get_native_handle(s->webview))) return; /* Close()'d */

    params.is_script_enabled = s->is_script_enabled;
    params.are_dev_tools_enabled = s->are_dev_tools_enabled;
    params.are_default_context_menus_enabled = s->are_default_context_menus_enabled;
    if (WEBVIEW2LOADER_UNIX_CALL(apply_settings, &params) || !params.is_success)
        WARN("could not apply settings to the real webview\n");
}

#define SETTINGS_BOOL_PROPERTY(Name, field) \
    static HRESULT WINAPI settings_get_##Name(ICoreWebView2Settings *iface, BOOL *value) \
    { if (!value) return E_POINTER; *value = impl_from_ICoreWebView2Settings(iface)->field; return S_OK; } \
    static HRESULT WINAPI settings_put_##Name(ICoreWebView2Settings *iface, BOOL value) \
    { \
        struct settings_impl *s = impl_from_ICoreWebView2Settings(iface); \
        s->field = value; \
        settings_push(s); \
        return S_OK; \
    }

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

/* The base-only ICoreWebView2SettingsVtbl that used to live here is gone:
 * every settings object now uses settings2_vtbl below, whose `base` member is
 * that same table verbatim. Keeping a second copy would just be two things to
 * keep in sync. */

static HRESULT WINAPI settings2_get_UserAgent(ICoreWebView2Settings *iface, LPWSTR *value)
{
    struct settings_impl *s = impl_from_ICoreWebView2Settings(iface);
    SIZE_T len;

    if (!value) return E_POINTER;
    len = s->user_agent ? wcslen(s->user_agent) + 1 : 1;
    if (!(*value = CoTaskMemAlloc(len * sizeof(WCHAR)))) return E_OUTOFMEMORY;
    memcpy(*value, s->user_agent ? s->user_agent : L"", len * sizeof(WCHAR));
    return S_OK;
}

/* Studio calls this with a WebView2-identifying User-Agent; the Toolbox page
 * uses that token to decide whether to render its embedded UI or the ordinary
 * consumer store. Applied to the real webview immediately rather than cached
 * for the next navigation, matching real WebView2, whose documented behaviour
 * is that the new agent takes effect on subsequent requests. */
static HRESULT WINAPI settings2_put_UserAgent(ICoreWebView2Settings *iface, LPCWSTR value)
{
    struct settings_impl *s = impl_from_ICoreWebView2Settings(iface);
    struct set_user_agent_params params = { 0 };
    LPWSTR copy;
    SIZE_T len;

    if (!value) return E_POINTER;

    len = wcslen(value) + 1;
    if (!(copy = malloc(len * sizeof(WCHAR)))) return E_OUTOFMEMORY;
    memcpy(copy, value, len * sizeof(WCHAR));
    free(s->user_agent);
    s->user_agent = copy;

    if (!s->webview) return S_OK; /* nothing to apply it to yet */

    params.handle = webview_get_native_handle(s->webview);
    params.user_agent = value;
    if (!params.handle) return S_OK; /* Close()'d; the stored value is still correct */

    if (WEBVIEW2LOADER_UNIX_CALL(set_user_agent, &params) || !params.is_success)
    {
        WARN("could not apply the user agent to the real webview -- the page may render its "
             "website layout instead of the embedded one\n");
        return E_FAIL;
    }
    return S_OK;
}

/* Base table copied verbatim, then the two Settings2 slots -- the layout
 * Settings2's inheritance requires. */
static const struct webview2_settings2_vtbl_combined settings2_vtbl =
{
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
    },
    {
        settings2_get_UserAgent,
        settings2_put_UserAgent,
    },
};

HRESULT settings_create(ICoreWebView2 *webview, ICoreWebView2Settings **out)
{
    struct settings_impl *s = calloc(1, sizeof(*s));
    if (!s) return E_OUTOFMEMORY;

    /* Always the combined table: an object that answered only the base IID
     * would make Studio's Settings2 QueryInterface fail, which is the bug this
     * whole interface exists to fix. */
    s->iface.lpVtbl = (const ICoreWebView2SettingsVtbl *)&settings2_vtbl;
    s->webview = webview;
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
    if (!wv->settings && FAILED(hr = settings_create(iface, &wv->settings)))
        return hr;

    ICoreWebView2Settings_AddRef(wv->settings);
    *settings = wv->settings;
    return S_OK;
}

/* TuxBlox: self-identifying E_NOTIMPL stubs.
 *
 * Every unimplemented ICoreWebView2 slot used to share one variadic
 * webview2_stub_e_notimpl (main.c), which returns the right thing but cannot
 * say WHICH method Studio asked for. That was fine while the only goal was
 * "registration must not fail", and useless the moment a feature depending on
 * one of these -- the Toolbox -- hangs instead of working: the shared stub
 * makes "Studio called ExecuteScript and gave up" indistinguishable from
 * "Studio never called anything at all".
 *
 * Reported via MESSAGE, not FIXME/WARN: proton.py defaults WINEDEBUG to "-all",
 * which suppresses every debug channel, so a FIXME here would be invisible in
 * exactly the user-submitted logs this exists to make readable. Reported ONCE
 * per method (InterlockedExchange on a per-stub static) so a caller in a retry
 * loop cannot flood the log.
 *
 * Same variadic (void *iface, ...) shape as the shared stub these replace, and
 * for the same reason: these slots have mutually incompatible real signatures
 * and every entry is cast through (void *) anyway, so no stub can declare the
 * true one. Extra arguments are ignored. */
#define WV2L_DEFINE_NOTIMPL_STUB(name)                                        \
    static HRESULT WINAPI webview_notimpl_##name(void *iface, ...)            \
    {                                                                         \
        static LONG reported;                                                 \
        (void)iface;                                                          \
        if (!InterlockedExchange(&reported, 1))                               \
            MESSAGE("webview2loader: ICoreWebView2::" #name " is not "        \
                    "implemented -- returning E_NOTIMPL to Studio\n");        \
        return E_NOTIMPL;                                                     \
    }

WV2L_DEFINE_NOTIMPL_STUB(CapturePreview)
WV2L_DEFINE_NOTIMPL_STUB(Reload)
WV2L_DEFINE_NOTIMPL_STUB(CallDevToolsProtocolMethod)
WV2L_DEFINE_NOTIMPL_STUB(get_BrowserProcessId)
WV2L_DEFINE_NOTIMPL_STUB(get_CanGoBack)
WV2L_DEFINE_NOTIMPL_STUB(get_CanGoForward)
WV2L_DEFINE_NOTIMPL_STUB(GoBack)
WV2L_DEFINE_NOTIMPL_STUB(GoForward)
WV2L_DEFINE_NOTIMPL_STUB(GetDevToolsProtocolEventReceiver)
WV2L_DEFINE_NOTIMPL_STUB(Stop)
WV2L_DEFINE_NOTIMPL_STUB(get_DocumentTitle)
WV2L_DEFINE_NOTIMPL_STUB(AddHostObjectToScript)
WV2L_DEFINE_NOTIMPL_STUB(RemoveHostObjectFromScript)
WV2L_DEFINE_NOTIMPL_STUB(OpenDevToolsWindow)
WV2L_DEFINE_NOTIMPL_STUB(get_ContainsFullScreenElement)
WV2L_DEFINE_NOTIMPL_STUB(AddWebResourceRequestedFilter)
WV2L_DEFINE_NOTIMPL_STUB(RemoveWebResourceRequestedFilter)
WV2L_DEFINE_NOTIMPL_STUB(NavigateWithWebResourceRequest)

static const ICoreWebView2Vtbl webview_vtbl =
{
    webview_QueryInterface,
    webview_AddRef,
    webview_Release,
    webview_get_Settings,
    webview_get_Source,
    webview_Navigate,
    webview_NavigateToString,
    webview_add_NavigationStarting,
    webview_remove_NavigationStarting,
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
    webview_ExecuteScript,
    (void *)webview_notimpl_CapturePreview, /* CapturePreview */
    (void *)webview_notimpl_Reload, /* Reload */
    webview_PostWebMessageAsJson,
    webview_PostWebMessageAsString,
    webview_add_WebMessageReceived,
    webview_remove_WebMessageReceived,
    (void *)webview_notimpl_CallDevToolsProtocolMethod, /* CallDevToolsProtocolMethod */
    (void *)webview_notimpl_get_BrowserProcessId, /* get_BrowserProcessId */
    (void *)webview_notimpl_get_CanGoBack, /* get_CanGoBack */
    (void *)webview_notimpl_get_CanGoForward, /* get_CanGoForward */
    (void *)webview_notimpl_GoBack, /* GoBack */
    (void *)webview_notimpl_GoForward, /* GoForward */
    (void *)webview_notimpl_GetDevToolsProtocolEventReceiver, /* GetDevToolsProtocolEventReceiver */
    (void *)webview_notimpl_Stop, /* Stop */
    webview_generic_add_event, /* add_NewWindowRequested */
    webview_generic_remove_event, /* remove_NewWindowRequested */
    webview_generic_add_event, /* add_DocumentTitleChanged */
    webview_generic_remove_event, /* remove_DocumentTitleChanged */
    (void *)webview_notimpl_get_DocumentTitle, /* get_DocumentTitle */
    (void *)webview_notimpl_AddHostObjectToScript, /* AddHostObjectToScript */
    (void *)webview_notimpl_RemoveHostObjectFromScript, /* RemoveHostObjectFromScript */
    (void *)webview_notimpl_OpenDevToolsWindow, /* OpenDevToolsWindow */
    webview_generic_add_event, /* add_ContainsFullScreenElementChanged */
    webview_generic_remove_event, /* remove_ContainsFullScreenElementChanged */
    (void *)webview_notimpl_get_ContainsFullScreenElement, /* get_ContainsFullScreenElement */
    webview_generic_add_event, /* add_WebResourceRequested */
    webview_generic_remove_event, /* remove_WebResourceRequested */
    (void *)webview_notimpl_AddWebResourceRequestedFilter, /* AddWebResourceRequestedFilter */
    (void *)webview_notimpl_RemoveWebResourceRequestedFilter, /* RemoveWebResourceRequestedFilter */
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
    /* So the event pump can find this object again when the helper reports a
     * NavigationStarting for it -- see webview_find_by_handle. */
    webview_track(native_handle, &wv->ICoreWebView2_iface);
    *out = &wv->ICoreWebView2_iface;
    return S_OK;
}

/* --- Task 8: ICoreWebView2_2 extension --- */

static HRESULT WINAPI webview2_get_CookieManager(ICoreWebView2 *iface, ICoreWebView2CookieManager **cookieManager)
{
    if (!cookieManager) return E_POINTER;
    /* Final-review fix (Important 1): pass the owning webview itself, not
     * a UINT64 native_handle snapshot -- see cookie_manager_create's own
     * declaration comment in webview2loader_private.h for why. */
    return cookie_manager_create(iface, cookieManager);
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
        webview_add_NavigationStarting,
        webview_remove_NavigationStarting,
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
        webview_ExecuteScript,
        (void *)webview_notimpl_CapturePreview, /* CapturePreview */
        (void *)webview_notimpl_Reload, /* Reload */
        webview_PostWebMessageAsJson,
        webview_PostWebMessageAsString,
        webview_add_WebMessageReceived,
        webview_remove_WebMessageReceived,
        (void *)webview_notimpl_CallDevToolsProtocolMethod, /* CallDevToolsProtocolMethod */
        (void *)webview_notimpl_get_BrowserProcessId, /* get_BrowserProcessId */
        (void *)webview_notimpl_get_CanGoBack, /* get_CanGoBack */
        (void *)webview_notimpl_get_CanGoForward, /* get_CanGoForward */
        (void *)webview_notimpl_GoBack, /* GoBack */
        (void *)webview_notimpl_GoForward, /* GoForward */
        (void *)webview_notimpl_GetDevToolsProtocolEventReceiver, /* GetDevToolsProtocolEventReceiver */
        (void *)webview_notimpl_Stop, /* Stop */
        webview_generic_add_event, /* add_NewWindowRequested */
        webview_generic_remove_event, /* remove_NewWindowRequested */
        webview_generic_add_event, /* add_DocumentTitleChanged */
        webview_generic_remove_event, /* remove_DocumentTitleChanged */
        (void *)webview_notimpl_get_DocumentTitle, /* get_DocumentTitle */
        (void *)webview_notimpl_AddHostObjectToScript, /* AddHostObjectToScript */
        (void *)webview_notimpl_RemoveHostObjectFromScript, /* RemoveHostObjectFromScript */
        (void *)webview_notimpl_OpenDevToolsWindow, /* OpenDevToolsWindow */
        webview_generic_add_event, /* add_ContainsFullScreenElementChanged */
        webview_generic_remove_event, /* remove_ContainsFullScreenElementChanged */
        (void *)webview_notimpl_get_ContainsFullScreenElement, /* get_ContainsFullScreenElement */
        webview_generic_add_event, /* add_WebResourceRequested */
        webview_generic_remove_event, /* remove_WebResourceRequested */
        (void *)webview_notimpl_AddWebResourceRequestedFilter, /* AddWebResourceRequestedFilter */
        (void *)webview_notimpl_RemoveWebResourceRequestedFilter, /* RemoveWebResourceRequestedFilter */
        webview_generic_add_event, /* add_WindowCloseRequested */
        webview_generic_remove_event, /* remove_WindowCloseRequested */
    },
    {
        webview_generic_add_event, /* add_WebResourceResponseReceived */
        webview_generic_remove_event, /* remove_WebResourceResponseReceived */
        (void *)webview_notimpl_NavigateWithWebResourceRequest, /* NavigateWithWebResourceRequest */
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
    struct count_cookies_params params;

    if (!webview) return 0;
    /* Final-review fix (Important 1): read live via the same accessor
     * every other consumer of this webview's native_handle now uses,
     * instead of reaching into struct webview_impl directly. */
    params.handle = webview_get_native_handle(webview);
    params.count = 0;
    WEBVIEW2LOADER_UNIX_CALL(count_cookies, &params);
    return params.count;
}

/* Test-support-only export (Plan 3 Task 2) -- see
 * __wine_test_webview2loader_count_cookies's own comment just above for the
 * naming convention and why this stays read-only/diagnostic rather than a
 * capability grant. Takes the CONTROLLER (not a webview) since the native
 * window/handle a controller owns is what Task 2 needs to verify -- the
 * controller's own native_handle happens to be numerically the same handle
 * the webview created from it uses (both wrap the same struct
 * native_webview*, see controller_get_CoreWebView2), but going through
 * controller_get_native_handle keeps this correct even before
 * get_CoreWebView2 has ever been called. */
UINT32 WINAPI __wine_test_webview2loader_window_is_visible(ICoreWebView2Controller *controller)
{
    struct get_window_visible_params params;

    if (!controller) return 0;
    params.handle = controller_get_native_handle(controller);
    params.visible = FALSE;
    WEBVIEW2LOADER_UNIX_CALL(get_window_visible, &params);
    return params.visible;
}

/* Test-support-only exports (Plan 3 Task 3). */
BOOL WINAPI __wine_test_webview2loader_sync_window_geometry(ICoreWebView2Controller *controller,
                                                            const RECT *screen_bounds, BOOL visible)
{
    struct sync_window_geometry_params params;

    if (!controller || !screen_bounds) return FALSE;
    params.handle = controller_get_native_handle(controller);
    params.screen_bounds = *screen_bounds;
    params.visible = visible;
    params.success = FALSE;
    WEBVIEW2LOADER_UNIX_CALL(sync_window_geometry, &params);
    return params.success;
}

BOOL WINAPI __wine_test_webview2loader_get_window_geometry(ICoreWebView2Controller *controller, RECT *out_bounds)
{
    struct get_window_geometry_params params;

    if (!controller || !out_bounds) return FALSE;
    params.handle = controller_get_native_handle(controller);
    params.success = FALSE;
    WEBVIEW2LOADER_UNIX_CALL(get_window_geometry, &params);
    if (params.success) *out_bounds = params.screen_bounds;
    return params.success;
}
