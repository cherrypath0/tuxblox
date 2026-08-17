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
    /* Always append; never reorder or reuse a value -- same wire-compatibility
     * rule as enum wv2l_opcode. */
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
};

struct wv2l_get_window_geometry_params
{
    uint64_t handle;
    int32_t success; /* out */
    struct wv2l_rect screen_bounds; /* out */
};

#endif /* WEBVIEW2LOADER_IPC_PROTOCOL_H */
