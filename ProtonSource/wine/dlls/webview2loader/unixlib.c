#if 0
#pragma makedep unix
#endif

#include "config.h"

/* Task 3 (2026-08-14-webview2-host-process plan): this file used to load
 * GTK4/WebKitGTK into an isolated dlmopen() namespace and run every
 * GTK/WebKit call on a raw pthread inside THIS process (see git history for
 * that implementation and its 18+ rounds of crash fixes -- ac3634ea6 is the
 * last commit before this rewrite). That's gone now: GTK/WebKit hosting has
 * moved into a separate process, webkitgtk-bundle/host's
 * `webview2loader-host` binary (built against real GTK4/WebKitGTK headers,
 * not hand-declared typeof() prototypes), spawned by spawn_helper() below
 * and talked to over a UNIX domain socket using the wire protocol in
 * webview2loader_ipc_protocol.h. Every unix_*_impl function below is now a
 * thin translator: build the matching `struct wv2l_*` from its PE-side
 * params, round-trip it via ipc_call(), translate the response back.
 *
 * This also means the TEB-unsafety that motivated GTK_THREAD_LOG (a raw,
 * non-Wine-created pthread calling into WARN/ERR/TRACE, which dereferences
 * NtCurrentTeb()) no longer applies -- everything in this file now runs on
 * an ordinary Wine-created thread (a PE-side worker thread blocked inside
 * its own WINE_UNIX_CALL), so plain WARN/ERR/TRACE from wine/debug.h is
 * safe to use directly.
 */

#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <unistd.h>

#include <ntstatus.h>
#define WIN32_NO_STATUS
#include <windef.h>
#include <winbase.h>
#include <winternl.h>

#include <wine/debug.h>
#include <wine/unixlib.h>

#include "unixlib.h"
#include "webview2loader_ipc_protocol.h"

WINE_DEFAULT_DEBUG_CHANNEL(webview2loader);

static int g_helper_fd = -1;
static pid_t g_helper_pid = -1;
static pthread_mutex_t g_ipc_mutex = PTHREAD_MUTEX_INITIALIZER;

/* host_has_usable_egl()/bundle_gl_fallback_dir(): Task 7 stubs. Task 7's
 * real job is deciding whether webview2loader-host should get a
 * bundle-relative LD_LIBRARY_PATH override (a Mesa/GL fallback for
 * environments where the host's own EGL isn't usable -- see spawn_helper's
 * own call site below and the salvaged investigation this is based on,
 * .superpowers/sdd/2026-08-13-webview2-window-docking-messaging/
 * lag-glvnd-report.md). Defined here (not left as bare prototypes) so this
 * translation unit links and runs standalone during Task 3, before Task 7
 * exists: host_has_usable_egl() always returning TRUE is a safe
 * placeholder -- it means "assume EGL is usable," so spawn_helper never
 * takes the LD_LIBRARY_PATH fallback branch until Task 7 replaces this with
 * a real dlopen("libEGL.so.1")+dlsym presence probe.
 * bundle_gl_fallback_dir() is unreachable while the above always returns
 * TRUE; its real bundle-relative path logic also lands in Task 7. */
static BOOL host_has_usable_egl(void)
{
    return TRUE;
}

static const char *bundle_gl_fallback_dir(void)
{
    return "";
}

/* Forks and execs webkitgtk-bundle/host's webview2loader-host binary,
 * connected to this process over a freshly created UNIX domain socketpair.
 * The child end is handed off via the WEBVIEW2LOADER_IPC_FD env var, always
 * fd 3 (dup2'd there before exec) so the contract is a constant, not
 * something that depends on fd-table state at fork time.
 *
 * Deviation from this task's own brief, found by checking Task 2's actual
 * deliverable rather than assuming the brief's illustrative path: the
 * brief's own Step 2 code sample used bundle_dir/bin/webview2loader-host,
 * but webkitgtk-bundle/package.sh (Task 2, already committed) copies the
 * built binary into libexec/ instead, alongside the pre-existing
 * gdk-pixbuf-query-loaders precedent -- package.sh's own comment there
 * explains bin/ is never tarred into the final relocatable bundle at all,
 * only libexec/ is. bundle_dir/bin/webview2loader-host would therefore
 * never exist in a real deployed bundle; bundle_dir/libexec/
 * webview2loader-host is the real, present-on-disk path. */
static BOOL spawn_helper(const char *bundle_dir)
{
    int sv[2];
    char helper_path[PATH_MAX];
    char fd_env[32];

    if (socketpair(AF_UNIX, SOCK_STREAM, 0, sv) != 0) return FALSE;

    g_helper_pid = fork();
    if (g_helper_pid < 0) { close(sv[0]); close(sv[1]); return FALSE; }

    if (g_helper_pid == 0)
    {
        /* Child: keep sv[1], drop sv[0]. dup2 onto a fixed fd (3) so the
         * env var passed to the child is a constant, not something that
         * depends on fd-table state -- simpler to reason about than
         * passing whatever number socketpair() happened to hand back. */
        close(sv[0]);
        dup2(sv[1], 3);
        if (sv[1] != 3) close(sv[1]);

        snprintf(fd_env, sizeof(fd_env), "3");
        setenv("WEBVIEW2LOADER_IPC_FD", fd_env, 1);
        if (!host_has_usable_egl())
        {
            /* execve's own envp is read at true process startup -- this is
             * exactly the case LD_LIBRARY_PATH works for, unlike setenv()
             * from inside an already-running process. See Task 7. */
            char ld_path[PATH_MAX];
            snprintf(ld_path, sizeof(ld_path), "%s", bundle_gl_fallback_dir());
            setenv("LD_LIBRARY_PATH", ld_path, 1);
        }

        snprintf(helper_path, sizeof(helper_path), "%s/libexec/webview2loader-host", bundle_dir);
        execl(helper_path, helper_path, (char *)NULL);
        _exit(127); /* only reached if execl itself failed */
    }

    /* Parent: keep sv[0], drop sv[1]. */
    close(sv[1]);
    g_helper_fd = sv[0];
    return TRUE;
}

/* Duplicated from webkitgtk-bundle/host/ipc.c's ipc_read_full/ipc_write_full
 * -- unixlib.c can't #include a .c file from webkitgtk-bundle/host across
 * the GPLv3/LGPLv2.1 license and build-system boundary this project
 * deliberately keeps clean (see CLAUDE.md's "What this is" section), so
 * this ~15-line implementation is duplicated here rather than shared. Keep
 * this in sync with ipc.c's copy if the framing logic ever changes --
 * ipc.h's own comment on the host side points back at this copy for the
 * same reason. */
static ssize_t ipc_read_full(int fd, void *buf, size_t len)
{
    size_t done = 0;
    while (done < len)
    {
        ssize_t n = read(fd, (char *)buf + done, len - done);
        if (n == 0) return -1; /* peer closed -- treat as failure, caller decides what that means */
        if (n < 0)
        {
            if (errno == EINTR) continue;
            return -1;
        }
        done += (size_t)n;
    }
    return (ssize_t)done;
}

static ssize_t ipc_write_full(int fd, const void *buf, size_t len)
{
    size_t done = 0;
    while (done < len)
    {
        ssize_t n = write(fd, (const char *)buf + done, len - done);
        if (n < 0)
        {
            if (errno == EINTR) continue;
            return -1;
        }
        done += (size_t)n;
    }
    return (ssize_t)done;
}

/* The one primitive every unix_*_impl function below calls: write the
 * opcode + request struct, block for the same-shape response, overwrite
 * *req_resp with it in place (matching how every existing unix_*_impl
 * already treats its params struct as in-place in/out). Returns FALSE on
 * any transport failure (helper never spawned, helper crashed, socket
 * closed, etc.) -- callers are responsible for translating that into
 * whatever failure convention their specific unix_*_impl already used
 * before this change (never a new crash/hang, matching this whole file's
 * established degrade-gracefully pattern). */
static BOOL ipc_call(enum wv2l_opcode op, void *req_resp, size_t size)
{
    uint32_t wire_op = (uint32_t)op;
    BOOL ok;

    pthread_mutex_lock(&g_ipc_mutex);
    ok = g_helper_fd >= 0
        && ipc_write_full(g_helper_fd, &wire_op, sizeof(wire_op)) == (ssize_t)sizeof(wire_op)
        && ipc_write_full(g_helper_fd, req_resp, size) == (ssize_t)size
        && ipc_read_full(g_helper_fd, req_resp, size) == (ssize_t)size;
    pthread_mutex_unlock(&g_ipc_mutex);
    return ok;
}

/* Copies a NUL-terminated WCHAR* into a fixed uint16_t[cap] wire buffer
 * (webview2loader_ipc_protocol.h's `uri` fields), truncating rather than
 * failing if it doesn't fit -- WCHAR and uint16_t are both plain 16-bit
 * code units here, see that header's own top comment ("truncated+bounds-
 * checked by unixlib.c before send"). src may be NULL (no uri / "all
 * cookies" for GetCookies), copied as an empty string. */
static void copy_wcs_to_wire_uri(uint16_t *dst, size_t cap, const WCHAR *src)
{
    size_t n = 0;

    if (src)
        while (n < cap - 1 && src[n]) { dst[n] = src[n]; n++; }
    dst[n] = 0;
}

/* Translates one wire-format cookie (struct wv2l_cookie, uint16_t string
 * fields) into the PE-facing struct unix_cookie (WCHAR string fields) --
 * both sides' string field caps are identical by construction
 * (webview2loader_ipc_protocol.h's WV2L_COOKIE_*_MAX mirror unixlib.h's
 * WEBVIEW2LOADER_COOKIE_*_MAX 1:1), so this is a plain per-field memcpy,
 * not a re-encoding. */
static void wire_cookie_to_unix(struct unix_cookie *dst, const struct wv2l_cookie *src)
{
    memcpy(dst->name, src->name, sizeof(dst->name));
    memcpy(dst->value, src->value, sizeof(dst->value));
    memcpy(dst->domain, src->domain, sizeof(dst->domain));
    memcpy(dst->path, src->path, sizeof(dst->path));
    dst->expires = src->expires;
    dst->same_site = src->same_site;
    dst->is_session = src->is_session ? TRUE : FALSE;
    dst->is_http_only = src->is_http_only ? TRUE : FALSE;
    dst->is_secure = src->is_secure ? TRUE : FALSE;
}

/* unix_init_impl must run its real init work (resolve TUXBLOX_WEBVIEW_DIR,
 * spawn_helper, a WV2L_OP_INIT round-trip) exactly once, even when called
 * concurrently from multiple PE threads (webview2loader_unix_init() is
 * called on every environment creation -- see main.c). The pre-rewrite
 * version of this function needed a hand-rolled three-state guard
 * (INIT_IDLE/INIT_RUNNING/INIT_DONE) specifically because its init body
 * contained a pthread_cond_wait that RELEASED its own guarding mutex for
 * the duration of the wait, opening a window where a second thread could
 * race through the same init work. Nothing in this rewrite's init path
 * (spawn_helper's fork(), or ipc_call's own blocking socket I/O under a
 * *different* mutex, g_ipc_mutex) ever releases a lock mid-init like that,
 * so a plain pthread_once suffices: its own contract (run the init
 * function exactly once; concurrent callers block until it completes) is
 * exactly the semantics that used to require three states to hand-roll. */
static pthread_once_t g_init_once = PTHREAD_ONCE_INIT;
static BOOL g_init_ok;

static void do_init_once(void)
{
    const char *dir;
    struct wv2l_init_params wire = { 0 };

    g_init_ok = FALSE;

    if (!(dir = getenv("TUXBLOX_WEBVIEW_DIR")) || !dir[0])
    {
        WARN("TUXBLOX_WEBVIEW_DIR not set -- not running under this repo's proton\n");
        return;
    }

    if (!spawn_helper(dir))
    {
        WARN("spawn_helper failed -- could not start webview2loader-host\n");
        return;
    }

    if (!ipc_call(WV2L_OP_INIT, &wire, sizeof(wire)) || !wire.success)
    {
        WARN("WV2L_OP_INIT round-trip failed -- webview2loader-host not responding correctly\n");
        return;
    }

    g_init_ok = TRUE;
}

static NTSTATUS unix_init_impl(void *args)
{
    struct init_params *params = args;

    pthread_once(&g_init_once, do_init_once);
    params->success = g_init_ok;
    return g_init_ok ? STATUS_SUCCESS : STATUS_NOT_SUPPORTED;
}

static NTSTATUS unix_create_webview_impl(void *args)
{
    struct create_webview_params *params = args;
    struct wv2l_create_webview_params wire = { .is_message_only = params->is_message_only };

    if (!ipc_call(WV2L_OP_CREATE_WEBVIEW, &wire, sizeof(wire)))
    {
        WARN("ipc_call failed -- helper not running, failing CreateWebview\n");
        params->handle = 0;
        return STATUS_NOT_SUPPORTED;
    }
    params->handle = wire.handle;
    return wire.handle ? STATUS_SUCCESS : STATUS_NOT_SUPPORTED;
}

static NTSTATUS unix_destroy_webview_impl(void *args)
{
    struct destroy_webview_params *params = args;
    struct wv2l_destroy_webview_params wire = { .handle = params->handle };

    if (!params->handle) return STATUS_SUCCESS;
    if (!ipc_call(WV2L_OP_DESTROY_WEBVIEW, &wire, sizeof(wire)))
        WARN("ipc_call failed -- helper not running, native webview leaked\n");
    return STATUS_SUCCESS;
}

static NTSTATUS unix_navigate_and_wait_impl(void *args)
{
    struct navigate_params *params = args;
    struct wv2l_navigate_params wire = { .handle = params->handle };

    params->is_success = FALSE;
    if (!params->handle) return STATUS_INVALID_HANDLE;

    copy_wcs_to_wire_uri(wire.uri, WV2L_URI_MAX, params->uri);

    if (!ipc_call(WV2L_OP_NAVIGATE_AND_WAIT, &wire, sizeof(wire)))
    {
        WARN("ipc_call failed -- helper not running, failing Navigate without waiting\n");
        return STATUS_NOT_SUPPORTED;
    }

    params->is_success = wire.is_success;
    params->navigation_id = wire.navigation_id;
    return STATUS_SUCCESS;
}

static NTSTATUS unix_delete_all_cookies_impl(void *args)
{
    struct delete_all_cookies_params *params = args;
    struct wv2l_delete_all_cookies_params wire = { .handle = params->handle };

    if (!params->handle) return STATUS_INVALID_HANDLE;
    if (!ipc_call(WV2L_OP_DELETE_ALL_COOKIES, &wire, sizeof(wire)))
    {
        WARN("ipc_call failed -- helper not running, failing DeleteAllCookies without waiting\n");
        return STATUS_NOT_SUPPORTED;
    }
    return STATUS_SUCCESS;
}

static NTSTATUS unix_count_cookies_impl(void *args)
{
    struct count_cookies_params *params = args;
    struct wv2l_count_cookies_params wire = { .handle = params->handle };

    params->count = 0;
    if (!params->handle) return STATUS_INVALID_HANDLE;
    if (!ipc_call(WV2L_OP_COUNT_COOKIES, &wire, sizeof(wire)))
    {
        WARN("ipc_call failed -- helper not running, failing count_cookies without waiting\n");
        return STATUS_NOT_SUPPORTED;
    }
    params->count = wire.count;
    return STATUS_SUCCESS;
}

static NTSTATUS unix_get_cookies_impl(void *args)
{
    struct get_cookies_params *params = args;
    struct wv2l_get_cookies_params *wire;
    UINT32 i, n;
    NTSTATUS status = STATUS_SUCCESS;

    params->success = FALSE;
    params->count = 0;
    if (!params->handle) return STATUS_INVALID_HANDLE;

    /* Heap-allocated, not a stack local: struct wv2l_get_cookies_params
     * embeds a WV2L_MAX_COOKIES-sized cookie array (~1.2MB) -- same "too
     * big for a thread stack" reasoning struct get_cookies_params's own
     * comment in unixlib.h already gives for the equivalent PE-side
     * struct. */
    if (!(wire = calloc(1, sizeof(*wire)))) return STATUS_NO_MEMORY;
    wire->handle = params->handle;
    copy_wcs_to_wire_uri(wire->uri, WV2L_URI_MAX, params->uri);

    if (!ipc_call(WV2L_OP_GET_COOKIES, wire, sizeof(*wire)))
    {
        WARN("ipc_call failed -- helper not running, failing GetCookies without waiting\n");
        status = STATUS_NOT_SUPPORTED;
        goto done;
    }

    params->success = wire->success;
    params->count = wire->count;
    if (wire->success)
    {
        n = wire->count < WEBVIEW2LOADER_MAX_COOKIES ? wire->count : WEBVIEW2LOADER_MAX_COOKIES;
        for (i = 0; i < n; i++)
            wire_cookie_to_unix(&params->cookies[i], &wire->cookies[i]);
    }

done:
    free(wire);
    return status;
}

static NTSTATUS unix_get_window_visible_impl(void *args)
{
    struct get_window_visible_params *params = args;
    struct wv2l_get_window_visible_params wire = { .handle = params->handle };

    if (!ipc_call(WV2L_OP_GET_WINDOW_VISIBLE, &wire, sizeof(wire)))
    {
        params->visible = FALSE;
        return STATUS_SUCCESS; /* matches this function's own existing
                                 * never-fatal convention -- a transport
                                 * failure degrades exactly like the
                                 * existing "invalid handle" case already
                                 * does today */
    }
    params->visible = wire.visible;
    return STATUS_SUCCESS;
}

static NTSTATUS unix_sync_window_geometry_impl(void *args)
{
    struct sync_window_geometry_params *params = args;
    struct wv2l_sync_window_geometry_params wire =
    {
        .handle = params->handle,
        .screen_bounds =
        {
            params->screen_bounds.left, params->screen_bounds.top,
            params->screen_bounds.right, params->screen_bounds.bottom,
        },
        .visible = params->visible,
        .parent_xid = params->parent_xid,
    };

    params->success = FALSE;
    if (!params->handle) return STATUS_INVALID_HANDLE;
    if (!ipc_call(WV2L_OP_SYNC_WINDOW_GEOMETRY, &wire, sizeof(wire)))
        WARN("ipc_call failed -- helper not running, geometry sync skipped\n");
    else
        params->success = wire.success;
    return STATUS_SUCCESS;
}

static NTSTATUS unix_get_window_geometry_impl(void *args)
{
    struct get_window_geometry_params *params = args;
    struct wv2l_get_window_geometry_params wire = { .handle = params->handle };

    params->success = FALSE;
    if (!params->handle) return STATUS_INVALID_HANDLE;
    if (!ipc_call(WV2L_OP_GET_WINDOW_GEOMETRY, &wire, sizeof(wire)))
    {
        WARN("ipc_call failed -- helper not running\n");
        return STATUS_SUCCESS;
    }

    params->success = wire.success;
    params->screen_bounds.left = wire.screen_bounds.left;
    params->screen_bounds.top = wire.screen_bounds.top;
    params->screen_bounds.right = wire.screen_bounds.right;
    params->screen_bounds.bottom = wire.screen_bounds.bottom;
    return STATUS_SUCCESS;
}

const unixlib_entry_t __wine_unix_call_funcs[] =
{
    unix_init_impl,
    unix_create_webview_impl,
    unix_destroy_webview_impl,
    unix_navigate_and_wait_impl,
    unix_delete_all_cookies_impl,
    unix_count_cookies_impl,
    unix_get_cookies_impl,
    unix_get_window_visible_impl,
    unix_sync_window_geometry_impl,
    unix_get_window_geometry_impl,
};
