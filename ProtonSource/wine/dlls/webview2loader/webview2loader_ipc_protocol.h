/* ProtonSource/wine/dlls/webview2loader/webview2loader_ipc_protocol.h
 *
 * Wire format for the socket boundary between unixlib.c (Wine process) and
 * webkitgtk-bundle/host's webview2loader-host (separate process). Only
 * stdint.h types -- no windef.h, no GTK/GLib/WebKit headers -- so this
 * single file compiles unmodified on both sides. Struct shapes mirror
 * unixlib.h's own struct *_params 1:1; see that file for the authoritative
 * field-level rationale (this header only re-states shape, not "why").
 *
 * Framing: every request is (uint32_t opcode, then sizeof(struct for that
 * opcode) raw bytes); every response is the SAME struct type, written back
 * with its own `out` fields populated. One request in flight at a time
 * (unixlib.c serializes with a mutex) -- see the design spec's IPC
 * Protocol section for why no request-IDs/multiplexing exists yet.
 */
#ifndef WEBVIEW2LOADER_IPC_PROTOCOL_H
#define WEBVIEW2LOADER_IPC_PROTOCOL_H

#include <stdint.h>

#define WV2L_URI_MAX 2048

enum wv2l_opcode
{
    WV2L_OP_INIT,
    WV2L_OP_CREATE_WEBVIEW,
    WV2L_OP_DESTROY_WEBVIEW,
    WV2L_OP_NAVIGATE_AND_WAIT,
    WV2L_OP_DELETE_ALL_COOKIES,
    WV2L_OP_COUNT_COOKIES,
    WV2L_OP_GET_COOKIES,
    WV2L_OP_GET_WINDOW_VISIBLE,
    WV2L_OP_SYNC_WINDOW_GEOMETRY,
    WV2L_OP_GET_WINDOW_GEOMETRY,
    WV2L_OP_DELETE_COOKIE,
    WV2L_OP_ADD_USER_SCRIPT,
    WV2L_OP_POST_WEB_MESSAGE,
    WV2L_OP_ADD_OR_UPDATE_COOKIE,
    WV2L_OP_SET_USER_AGENT,
    WV2L_OP_APPLY_SETTINGS,
    WV2L_OP_EXECUTE_SCRIPT,
    /* Always append; never reorder or reuse a value -- these are the wire
     * opcode numbers both already-built binaries agree on. */
};

/* --- Event channel (host -> Wine) ---
 *
 * The opcode channel above is strictly request/response, always initiated by
 * Wine, one in flight, serialised by unixlib.c's own g_ipc_mutex. That shape
 * cannot express "the helper has something to tell you", which real WebView2
 * events require -- so events get their OWN socketpair, created alongside the
 * request one in spawn_helper and handed to the child as fd 4 via
 * WEBVIEW2LOADER_EVENT_FD.
 *
 * A separate fd rather than multiplexing onto the existing one: the request
 * socket has no request IDs, so interleaving unsolicited frames on it would
 * mean redesigning the framing every working opcode depends on. A second fd
 * leaves all ten of them untouched, and lets the Wine side block on events
 * from a dedicated thread without ever holding the request mutex.
 *
 * Framing matches the request channel: uint32_t event type, then that type's
 * struct. Events are fire-and-forget -- the host never waits for a reply, so a
 * Wine side that is slow, busy, or gone can never stall the helper's main loop.
 */
enum wv2l_event
{
    WV2L_EV_NAVIGATION_STARTING,
    WV2L_EV_WEB_MESSAGE,
    /* Always append; never reorder or reuse a value -- same wire-compatibility
     * rule as enum wv2l_opcode. */
};

/* Real ICoreWebView2::WebMessageReceived, and its PostWebMessageAsJson/AsString
 * counterpart below.
 *
 * MEASURED, after a long detour. An earlier probe concluded the page never
 * calls window.chrome.webview.postMessage and that this channel was therefore
 * unnecessary. That probe was run against a page whose bridge had never been
 * bootstrapped, because AddScriptToExecuteOnDocumentCreated was discarding
 * Studio's init script (see wv2l_add_user_script_params). With the script
 * actually injected, a real Toolbox session immediately produces:
 *
 *   page called window.chrome.webview.postMessage --
 *       {"name":"loadprogress","data":"beforeInteractive"}
 *   page called window.chrome.webview.postMessage --
 *       {"name":"loadprogress","data":"afterInteractive"}
 *
 * so the page does use the channel, and the Toolbox waits on the other end of
 * it. The old conclusion was an artifact of the bug, not a property of the page.
 *
 * `is_string` distinguishes the two shapes real WebView2 surfaces differently:
 * postMessage(string) makes TryGetWebMessageAsString return that string, while
 * get_WebMessageAsJson always returns JSON. Carrying the flag lets the PE side
 * answer both correctly instead of guessing from the payload's syntax. */
#define WV2L_WEB_MESSAGE_MAX 65536

/* Sent as a FIXED HEADER followed by exactly message_len UTF-16 units, not as
 * one fixed-size struct.
 *
 * The struct used to embed the full WV2L_WEB_MESSAGE_MAX buffer, making every
 * event ~132 KB on the wire. A socketpair's buffer is ~208 KB, so two queued
 * messages filled it and the helper blocked in write() -- and since the only
 * thing draining that socket is the Wine side's event pump, which was itself
 * busy inside Studio's WebMessageReceived handler, that was a deadlock. Studio
 * then hung forever in CreateCoreWebView2EnvironmentWithOptions (same
 * g_ipc_mutex) and fell back to its non-webview Toolbox after ~70s.
 *
 * A real message is a few hundred bytes; paying 132 KB for it was never
 * justified. */
struct wv2l_ev_web_message_header
{
    uint64_t handle;
    int32_t is_string;               /* page used postMessage(string) */
    uint32_t message_len;            /* UTF-16 units that follow, excluding NUL */
    uint16_t source[WV2L_URI_MAX];   /* page URI, for get_Source */
};

/* Studio -> page. Delivered to the page's own
 * window.chrome.webview 'message' listeners by the helper. */
struct wv2l_post_web_message_params
{
    uint64_t handle;
    int32_t is_string;                       /* PostWebMessageAsString, not AsJson */
    uint16_t message[WV2L_WEB_MESSAGE_MAX];  /* in; NUL-terminated */
    int32_t success;                         /* out */
};

/* Real ICoreWebView2::NavigationStarting. Roblox Studio registers a handler for
 * this before it ever calls Navigate(), and its login flow depends on it: the
 * OAuth page finishes by navigating to roblox-studio-auth:/?code=..., and on
 * Windows Studio's handler intercepts exactly that, cancels it, and completes
 * the login in-process from the code in the URI.
 *
 * Established by measurement, not assumption: a probe build injected a real
 * window.chrome.webview shim and logged every postMessage the page made. It
 * made none, which rules out the web-message channel (the other candidate) and
 * leaves this one.
 *
 * No `cancel` field comes back, deliberately. The host has already suppressed
 * the navigation by the time it sends this (on_decide_policy calls
 * webkit_policy_decision_ignore for these schemes), so there is nothing left to
 * cancel and nothing to wait for -- which is what lets this be a one-way event
 * instead of a synchronous round trip the helper's main loop would have to
 * block on. */
struct wv2l_ev_navigation_starting_params
{
    uint64_t handle;            /* which webview this navigation belongs to */
    uint16_t uri[WV2L_URI_MAX]; /* NUL-terminated */
    int32_t is_redirect;        /* real WebView2 surfaces this on the args object */
};

/* Real ICoreWebView2::AddScriptToExecuteOnDocumentCreated.
 *
 * Studio calls this ("setInitScript" in its own log) right after creating a
 * controller and before navigating, for every webview it creates. Until this
 * opcode existed the PE side accepted the call, invented a script id, reported
 * S_OK -- and threw the JavaScript away without ever reading the parameter, so
 * the script never ran in the page.
 *
 * That is the confirmed cause of the Toolbox hang. A real Toolbox session shows
 * the navigation to https://create.roblox.com/store/models completing with
 * status 200 and NavigationCompleted firing normally, after which nothing else
 * happens for ~60s until Studio gives up and falls back to its non-webview
 * Toolbox: the page loads fine, then waits forever for the host bridge that
 * this script is supposed to install. The login dialog survived the same bug
 * only because its flow completes through NavigationStarting on the
 * roblox-studio-auth: redirect, which needs no injected script.
 *
 * It also means the earlier F-08 finding ("the page never calls
 * window.chrome.webview.postMessage, so the web-message channel is not needed")
 * was measured on a page whose bridge had never been bootstrapped, and cannot
 * be trusted as evidence about the message channel either way. */
#define WV2L_USER_SCRIPT_MAX 65536

struct wv2l_add_user_script_params
{
    uint64_t handle;
    uint16_t script[WV2L_USER_SCRIPT_MAX]; /* in; NUL-terminated UTF-16 */
    int32_t success;                       /* out */
};

/* Real ICoreWebView2::ExecuteScript.
 *
 * Named by Studio's own log, not guessed at. Once the document-start script
 * above actually reached the page, the Toolbox got as far as a clean 200 and
 * then produced, on every retry:
 *
 *   Warning [FLog::StudioEmbeddedBrowserWebView2] executeJavaScript failed
 *   with error code '-2147467263'
 *
 * 0x80004001 is E_NOTIMPL: ICoreWebView2::ExecuteScript was still a stub.
 * Studio drives the Toolbox through ExecuteScript rather than
 * PostWebMessageAsJson -- the page sends messageBusEvent / internal:init with a
 * uuid and waits for Studio to answer with injected JavaScript, so a stubbed
 * ExecuteScript leaves the page spinning forever and retrying with fresh uuids.
 *
 * Request and response share this one struct like every other opcode, so it
 * carries both buffers -- see this header's own top comment on framing. The
 * result is JSON because that is what real WebView2's completion handler
 * receives (resultObjectAsJson); the helper produces it with jsc_value_to_json.
 *
 * result_len is the UTF-16 unit count excluding the NUL, so the PE side never
 * has to trust an embedded terminator it did not write. */
#define WV2L_SCRIPT_RESULT_MAX 65536

struct wv2l_execute_script_params
{
    uint64_t handle;
    uint16_t script[WV2L_USER_SCRIPT_MAX]; /* in; NUL-terminated UTF-16 */
    int32_t success;                       /* out */
    uint32_t result_len;                   /* out; units in `result`, excl. NUL */
    uint16_t result[WV2L_SCRIPT_RESULT_MAX]; /* out; JSON, NUL-terminated */
};

struct wv2l_rect { int32_t left, top, right, bottom; };

struct wv2l_init_params { int32_t success; /* out */ };

struct wv2l_create_webview_params
{
    int32_t is_message_only; /* in */
    uint64_t handle;         /* out; 0 on failure */
};

struct wv2l_destroy_webview_params { uint64_t handle; /* in; no-op if 0 */ };

struct wv2l_navigate_params
{
    uint64_t handle;
    uint16_t uri[WV2L_URI_MAX]; /* in; NUL-terminated, truncated+bounds-
                                  * checked by unixlib.c before send -- see
                                  * this header's own top comment */
    int32_t is_success; /* out */
    uint64_t navigation_id; /* out */
};

struct wv2l_delete_all_cookies_params { uint64_t handle; };

struct wv2l_count_cookies_params
{
    uint64_t handle;
    uint32_t count; /* out */
};

#define WV2L_MAX_COOKIES 128
#define WV2L_COOKIE_NAME_MAX 256
#define WV2L_COOKIE_VALUE_MAX 4096
#define WV2L_COOKIE_DOMAIN_MAX 256
#define WV2L_COOKIE_PATH_MAX 512

struct wv2l_cookie
{
    uint16_t name[WV2L_COOKIE_NAME_MAX];
    uint16_t value[WV2L_COOKIE_VALUE_MAX];
    uint16_t domain[WV2L_COOKIE_DOMAIN_MAX];
    uint16_t path[WV2L_COOKIE_PATH_MAX];
    double expires;
    int32_t same_site;
    int32_t is_session;
    int32_t is_http_only;
    int32_t is_secure;
};

struct wv2l_get_cookies_params
{
    uint64_t handle;
    uint16_t uri[WV2L_URI_MAX]; /* in; empty = all cookies, see unixlib.h's
                                  * own get_cookies_params.uri comment */
    uint32_t offset; /* in; index of the first cookie to return, so a store
                       * larger than WV2L_MAX_COOKIES can be read across
                       * several calls instead of failing outright. The PE
                       * side drives the paging -- see cookie_manager.c's
                       * get_cookies_worker. */
    int32_t success; /* out */
    uint32_t count;  /* out; entries actually returned in this page, i.e.
                       * min(total - offset, WV2L_MAX_COOKIES) */
    uint32_t total;  /* out; cookies in the whole store/filter, independent of
                       * offset -- what the caller pages against */
    struct wv2l_cookie cookies[WV2L_MAX_COOKIES]; /* out; this page only */
};

/* Deleting one specific cookie. Carries name/value/domain/path because that is
 * exactly what libsoup matches on, verified against libsoup 3.6.5's own source
 * rather than assumed: soup_cookie_jar_delete_cookie looks the domain up in its
 * hash of domains, then walks that domain's list comparing with
 * soup_cookie_equal, which is `name && value && path` -- the VALUE is part of
 * the match. Sending only name/domain/path would silently delete nothing.
 * Everything else in struct wv2l_cookie (expires, same_site, flags) is ignored
 * by that comparison and left unset by the sender. */
struct wv2l_delete_cookie_params
{
    uint64_t handle;
    struct wv2l_cookie cookie; /* in; only name/value/domain/path are read */
};

/* Real ICoreWebView2CookieManager::AddOrUpdateCookie.
 *
 * This is how Studio authenticates a webview it did not itself log in through.
 * Its OAuth flow ends at roblox-studio-auth:/?code=..., which Studio redeems in
 * its OWN http stack -- the webview never sees a session cookie from that
 * exchange. For the Toolbox, Studio takes its RobloxStudioCookieManager lock
 * and writes .ROBLOSECURITY into the webview directly, immediately before
 * creating it (visible in a real session log).
 *
 * While CreateCookie/AddOrUpdateCookie were E_NOTIMPL, that write silently
 * failed and create.roblox.com/store/models served its logged-out page -- a
 * Sign Up button and a login form inside the Toolbox panel. Confirmed not to be
 * content negotiation: fetching that URL with WebKitGTK's user agent and with a
 * real WebView2 one returns byte-identical HTML. */
/* ICoreWebView2Settings2::put_UserAgent -> webkit_settings_set_user_agent.
 *
 * Studio sets a WebView2-identifying agent here (its own binary carries the
 * template "Mozilla/5.0 (Windows) WebView2 Edg/"), and the Toolbox page uses
 * that token to choose between its embedded UI and the ordinary consumer store.
 * Note this is a CLIENT-side decision: fetching the same URL with WebKitGTK's
 * agent and with a real WebView2 one returns byte-identical HTML, so the server
 * does not negotiate on it -- the page's own JavaScript branches after load. */
/* ICoreWebView2Settings -> the real WebKitSettings.
 *
 * The shim accepted every one of these properties and stored them, but nothing
 * ever reached WebKit -- so Studio setting AreDefaultContextMenusEnabled=FALSE
 * (as it does) had no effect and WebKit's own Back/Forward/Stop/Reload menu
 * appeared on right-click inside the Toolbox, which real WebView2 never shows.
 *
 * Only the properties with a genuine WebKit equivalent are carried. The rest
 * (status bar, built-in error page, zoom control, host objects) have no
 * counterpart in WebKitGTK and are still stored PE-side so get_* round-trips
 * honestly, rather than being faked here. */
struct wv2l_apply_settings_params
{
    uint64_t handle;
    int32_t is_script_enabled;
    int32_t are_dev_tools_enabled;
    int32_t are_default_context_menus_enabled;
    int32_t success; /* out */
};

struct wv2l_set_user_agent_params
{
    uint64_t handle;
    uint16_t user_agent[WV2L_URI_MAX]; /* in; NUL-terminated */
    int32_t success;                   /* out */
};

struct wv2l_add_cookie_params
{
    uint64_t handle;
    struct wv2l_cookie cookie; /* in */
    int32_t success;           /* out */
};

struct wv2l_get_window_visible_params
{
    uint64_t handle;
    int32_t visible; /* out */
};

struct wv2l_sync_window_geometry_params
{
    uint64_t handle;
    struct wv2l_rect screen_bounds;
    int32_t visible;
    uint64_t parent_xid;
    int32_t success; /* out */
    /* Diagnostic inputs for the visible decision, logged by the helper.
     *
     * Carried over the wire rather than logged PE-side because PE-side
     * MESSAGE()/wine_dbg_printf output does not reach the captured launch log
     * in this setup -- verified: this exact function ran (the helper received
     * the sync) while its MESSAGE line produced nothing. The helper's stderr is
     * the only channel observed to work reliably, so the inputs travel to it. */
    int32_t dbg_put_is_visible;
    int32_t dbg_parent_visible;
    int32_t dbg_parent_seen_visible;
};

struct wv2l_get_window_geometry_params
{
    uint64_t handle;
    int32_t success; /* out */
    struct wv2l_rect screen_bounds; /* out */
};

#endif /* WEBVIEW2LOADER_IPC_PROTOCOL_H */
