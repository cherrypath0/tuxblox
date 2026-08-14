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
    /* Always append; never reorder or reuse a value -- these are the wire
     * opcode numbers both already-built binaries agree on. */
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
    int32_t success; /* out */
    uint32_t count;  /* out */
    struct wv2l_cookie cookies[WV2L_MAX_COOKIES]; /* out */
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
