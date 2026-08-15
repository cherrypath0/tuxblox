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

#include <dlfcn.h>
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

/* Task 7: real host-vs-bundled GL/EGL dispatch, salvaged from the blocked
 * attempt documented in .superpowers/sdd/2026-08-13-webview2-window-docking-
 * messaging/lag-glvnd-report.md. That attempt hit two real blockers, but
 * both were specific to the OLD architecture this file no longer has:
 *
 * - Blocker 1 (NVIDIA's proprietary EGL driver SIGSEGVs on its own internal
 *   dlopen() when loaded into an isolated dlmopen(LM_ID_NEWLM) namespace):
 *   moot now -- webview2loader-host is a genuinely separate process with its
 *   own default namespace, not a dlmopen namespace inside Wine's process.
 * - Blocker 2 (setenv("LD_LIBRARY_PATH", ...) from inside an already-running
 *   process doesn't retroactively affect glibc's already-cached dynamic-
 *   linker search path): moot now too -- spawn_helper() below sets
 *   LD_LIBRARY_PATH in the CHILD after fork() but BEFORE execl(), so it's
 *   read fresh at the new process's own startup, exactly the case the
 *   report's own "most promising remaining lead" identified as the fix
 *   (deciding host-vs-bundled before the process starts).
 *
 * host_has_usable_egl() runs here, in unixlib.c's own default namespace,
 * inside the PARENT (Wine's) process, strictly before fork() -- a plain
 * dlopen()+dlsym()+dlclose() probe, immediately closed, never loaded into
 * any isolated namespace and never left resident. This is explicitly NOT
 * the part the report's Blocker 1 crash was in (the report confirmed this
 * directly: one repro run had this exact probe present, one had it
 * hardcoded out entirely, and both crashed identically -- the crash was in
 * later real GTK/WebKit GL use inside the dlmopen namespace, not the probe
 * itself).
 *
 * Review fix (post-commit 61613652a): the "strictly before fork()" claim
 * above used to be aspirational, not actual -- spawn_helper() originally
 * called this function from inside the CHILD branch, after fork() had
 * already run. dlopen()/dlsym()/dlclose() are NOT fork-safe: they can hold
 * internal ld.so/malloc locks. Wine's own process is genuinely
 * multi-threaded, so if some other thread held that lock at the exact
 * instant fork() ran, the single-threaded child would inherit it
 * permanently locked, and the child's own post-fork dlopen() call would
 * deadlock forever -- hanging webview creation indefinitely, not just
 * picking the wrong GL path. Fixed: spawn_helper() now calls this function
 * in the PARENT, before fork(), and passes the result into the child via a
 * plain local BOOL (fork() copies the parent's already-computed stack
 * value, no further dlopen() involved) -- see spawn_helper()'s own call
 * site below. */
/* 2026-08-15 (software-only-llvmpipe plan): host_has_usable_egl()'s result is
 * no longer used to GATE the LD_LIBRARY_PATH decision in spawn_helper() below
 * -- see WV2L_ALWAYS_USE_BUNDLE_GL's own comment just above that call site for
 * the full reasoning (a confirmed, reproducible upstream WebKitGTK/NVIDIA
 * driver crash in the host-EGL path, not a change of opinion about this
 * function's own correctness). This function itself is left intact rather
 * than deleted: it's still a real, working probe of whether the host has a
 * usable libEGL.so.1, useful as a diagnostic and for a future person
 * revisiting hardware acceleration once the upstream WebKitGTK bug is fixed
 * (at which point this function is exactly what such a fix would want to
 * gate on again). Its return value is still computed in spawn_helper() below
 * (kept in a local, unused in the gating decision) purely so a future
 * WARN()-on-failure diagnostic path isn't silently lost either. */
static BOOL host_has_usable_egl(void)
{
    void *h = dlopen("libEGL.so.1", RTLD_LAZY);
    BOOL ok;
    if (!h)
    {
        WARN("dlopen(\"libEGL.so.1\") failed -- assuming no usable host EGL, falling back to bundle's own\n");
        return FALSE;
    }
    ok = dlsym(h, "eglGetDisplay") != NULL;
    if (!ok)
        WARN("dlopen(\"libEGL.so.1\") succeeded but dlsym(\"eglGetDisplay\") found nothing -- treating host EGL as unusable\n");
    dlclose(h);
    return ok;
}

/* Absolute path to this bundle's own relocated GL/EGL fallback libraries
 * (webkitgtk-bundle/package.sh moves libEGL/libGL/libGLESv1_CM/libGLESv2
 * (each with its full .so/.so.N/.so.N.N.N symlink chain) out of
 * lib/x86_64-linux-gnu/ into a gl-fallback/ subdirectory, specifically so
 * nothing else's RPATH ever resolves into it -- see that script's own
 * comment -- meaning these libraries are normally unreachable unless
 * LD_LIBRARY_PATH explicitly adds this directory, which is exactly what
 * spawn_helper()'s child branch below now always does -- see
 * WV2L_ALWAYS_USE_BUNDLE_GL's own comment for why this is no longer
 * conditional on host_has_usable_egl()). bundle_dir is the same
 * TUXBLOX_WEBVIEW_DIR value spawn_helper() already has -- not re-resolved
 * here.
 *
 * The returned pointer is a `static char[PATH_MAX]` -- NOT reentrant/
 * thread-safe, and overwritten by the next call. Safe today because every
 * call site copies the result out (snprintf) immediately, on a single
 * thread (spawn_helper()'s child branch, right after fork(), before any
 * other thread exists in that process) -- but this contract doesn't
 * survive being called from more than one place/thread without copying
 * first. */
static const char *bundle_gl_fallback_dir(const char *bundle_dir)
{
    static char path[PATH_MAX];
    snprintf(path, sizeof(path), "%s/lib/x86_64-linux-gnu/gl-fallback", bundle_dir);
    return path;
}

/* Part B (Task 7, controller-assigned gap found during Task 4's review):
 * ported verbatim from the pre-Task-3 unixlib.c's prepend_env(), deleted by
 * Task 3's rewrite as apparently dlmopen-specific. It isn't -- see
 * set_webkit_relocation_env() below. Prepends `value` onto whatever is
 * already in env var `name` (":"-joined, PATH-style) rather than
 * overwriting it outright, for vars a DIFFERENT in-process consumer might
 * already rely on. If `name` isn't set yet, this is equivalent to a plain
 * setenv(name, value, 1) (no leading ":" is emitted). */
static void prepend_env(const char *name, const char *value)
{
    const char *existing = getenv(name);
    char *joined;
    size_t len;

    if (!existing || !existing[0])
    {
        setenv(name, value, 1);
        return;
    }

    len = strlen(value) + 1 + strlen(existing) + 1;
    if (!(joined = malloc(len)))
    {
        WARN("out of memory prepending %s to %s -- leaving existing value in place\n", value, name);
        return;
    }
    snprintf(joined, len, "%s:%s", value, existing);
    setenv(name, joined, 1);
    free(joined);
}

/* Part B (Task 7): ported from the pre-Task-3 unixlib.c's
 * set_webkit_relocation_env(), deleted by Task 3's rewrite along with the
 * dlmopen/pthread machinery it used to run alongside. That deletion was
 * wrong -- this function isn't dlmopen-specific machinery, it sets
 * environment variables real WebKitGTK itself needs at runtime regardless
 * of how/where it's loaded (locating its own WebKitNetworkProcess/
 * WebKitWebProcess/WebKitGPUProcess helper binaries, GStreamer plugin/
 * scanner paths, GIO modules, GBM backends, GSettings schemas). Now called
 * from spawn_helper()'s CHILD branch, after fork() and before execl(): even
 * though the helper is a genuinely separate process now (not sharing Wine's
 * process environment directly), fork() duplicates the PARENT's (Wine's)
 * environment into the child before these setenv() calls run and execl()
 * inherits the result -- so the original "prepend, don't clobber the
 * inherited host/Wine value" reasoning for GST_PLUGIN_SYSTEM_PATH_1_0 and
 * XDG_DATA_DIRS still applies exactly as before and is preserved here, not
 * simplified to a plain setenv(). */
static void set_webkit_relocation_env(const char *dir)
{
    char path[PATH_MAX];

    snprintf(path, sizeof(path), "%s/libexec/webkitgtk-6.0", dir);
    setenv("WEBKIT_EXEC_PATH", path, 1);

    snprintf(path, sizeof(path), "%s/lib/webkitgtk-6.0/injected-bundle", dir);
    setenv("WEBKIT_INJECTED_BUNDLE_PATH", path, 1);

    snprintf(path, sizeof(path), "%s/lib/x86_64-linux-gnu/gio/modules", dir);
    setenv("GIO_EXTRA_MODULES", path, 1);

    snprintf(path, sizeof(path), "%s/lib/x86_64-linux-gnu/gbm", dir);
    setenv("GBM_BACKENDS_PATH", path, 1);

    snprintf(path, sizeof(path), "%s/libexec/gstreamer-1.0/gst-plugin-scanner", dir);
    setenv("GST_PLUGIN_SCANNER", path, 1);

    snprintf(path, sizeof(path), "%s/lib/x86_64-linux-gnu/gstreamer-1.0", dir);
    /* GStreamer 1.x reads GST_PLUGIN_SYSTEM_PATH_1_0 first and only falls
     * back to the unsuffixed GST_PLUGIN_SYSTEM_PATH when the _1_0-suffixed
     * one is unset. ProtonSource/proton sets GST_PLUGIN_SYSTEM_PATH_1_0
     * unconditionally for its own bundled GStreamer, so under this repo's
     * Proton the unsuffixed var alone is inert -- the bundle's own
     * gstreamer-1.0 plugins (built specifically for this bundle) would
     * never be found and WebKit's media pipeline would silently fall back
     * to Proton's differently-versioned GStreamer plugins instead. Set
     * both: the suffixed one to actually take effect here, the unsuffixed
     * one kept for any other GStreamer-based consumer that only checks the
     * unsuffixed name.
     *
     * This DLL's spawn_helper() forks a genuinely separate CHILD process
     * (unlike the pre-Task-3 version, which ran WebKit inside Wine's own
     * process) -- but fork() still duplicates Wine's own already-set
     * environment (including whatever Proton's own launcher set for its OWN
     * bundled GStreamer) into that child before these setenv()/prepend_env()
     * calls run, so the same collision this comment originally described
     * still applies: a plain setenv(..., 1) here would overwrite Proton's
     * own GST_PLUGIN_SYSTEM_PATH_1_0 outright in the child's environment.
     * Prepending instead of replacing keeps this bundle's plugins found
     * first by the helper's own WebKit while leaving Proton's own path
     * reachable afterward for anything else in the child's environment that
     * might consult it. GST_PLUGIN_SYSTEM_PATH (unsuffixed) has no such
     * known collision -- left as a plain setenv, out of scope for this
     * fix. */
    setenv("GST_PLUGIN_SYSTEM_PATH", path, 1);
    prepend_env("GST_PLUGIN_SYSTEM_PATH_1_0", path);

    snprintf(path, sizeof(path), "%s/share", dir);
    /* Same reasoning as GST_PLUGIN_SYSTEM_PATH_1_0 above -- a plain
     * setenv(..., 1) would replace XDG_DATA_DIRS outright in the child's
     * environment, dropping whatever the host/Wine's own inherited
     * environment already had there. Prepending keeps this bundle's share/
     * directory found first without discarding the rest. */
    prepend_env("XDG_DATA_DIRS", path);

    snprintf(path, sizeof(path), "%s/share/glib-2.0/schemas", dir);
    setenv("GSETTINGS_SCHEMA_DIR", path, 1);
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
/* 2026-08-15 (software-only-llvmpipe plan, repo owner's explicit live
 * decision): ALWAYS take the bundle's own software (llvmpipe) GL/EGL path in
 * spawn_helper() below, never the host's real driver, regardless of what
 * host_has_usable_egl() reports. This is not a portability fallback decision
 * any more -- it's a permanent crash workaround.
 *
 * Real, confirmed, reproducible evidence: WebKitWebProcess SIGSEGVs in a
 * SkiaGPUWorker thread deep inside libnvidia-eglcore.so during GPU-context
 * teardown, across 5 real coredumps in one session, all identical offsets.
 * Fully symbolized against an unstripped libwebkitgtk-6.0.so.4 matching the
 * deployed build's BuildID: two threads (SkiaGPUWorker's own TLS-destructor-
 * driven GrDirectContext teardown, and the main thread's own independent
 * exit()-driven EGL teardown) both tear down the same NVIDIA driver-global
 * GL/EGL state at once. This is a genuine upstream WebKitGTK Skia-backend/
 * NVIDIA-driver race -- zero TuxBlox frames in either racing stack -- only
 * reachable once the host's real EGL/GPU driver is actually in play, which is
 * exactly the branch this flag now permanently forecloses. See
 * .superpowers/sdd/2026-08-14-webview2loader-host-process/ for the full
 * crash investigation (progress.md's tail + the crash-investigation report
 * files there).
 *
 * Deliberately a named constant, not an inlined `if (1 || host_egl_ok)` or a
 * deleted conditional -- a future person revisiting hardware acceleration
 * (once the upstream WebKitGTK/NVIDIA bug is actually fixed upstream) should
 * be able to find this exact decision and flip it back to
 * `!host_egl_ok`-gated by reverting this one constant, not have to
 * reconstruct the whole host_has_usable_egl()-gating mechanism from git
 * history. host_has_usable_egl() itself is intentionally left in place (not
 * deleted) for exactly that future flip -- see its own comment. */
#define WV2L_ALWAYS_USE_BUNDLE_GL TRUE

static BOOL spawn_helper(const char *bundle_dir)
{
    int sv[2];
    char helper_path[PATH_MAX];
    char fd_env[32];
    BOOL host_egl_ok;

    if (socketpair(AF_UNIX, SOCK_STREAM, 0, sv) != 0) return FALSE;

    /* Review fix (post-commit 61613652a): must run here, in the PARENT,
     * strictly before fork() -- see host_has_usable_egl()'s own comment for
     * why calling it AFTER fork(), from inside the child, was a real bug
     * (dlopen()/dlsym()/dlclose() aren't fork-safe; a lock held by some
     * other thread of this genuinely multi-threaded Wine process at the
     * instant fork() runs would deadlock the child's own post-fork dlopen()
     * forever). The result is a plain BOOL on this function's own stack --
     * fork() copies it into the child along with everything else on the
     * stack, so the child below just reads the already-computed answer,
     * no further dlopen() call needed on that side at all.
     *
     * Still called unconditionally even though WV2L_ALWAYS_USE_BUNDLE_GL
     * means its result no longer gates anything below: this keeps its own
     * internal WARN()-on-failure diagnostics alive (useful signal about the
     * host's EGL state regardless of which GL path actually gets used), and
     * keeps the probe itself exercised/not bit-rotting for whenever a future
     * person flips WV2L_ALWAYS_USE_BUNDLE_GL back off. */
    host_egl_ok = host_has_usable_egl();

    g_helper_pid = fork();
    if (g_helper_pid < 0) { close(sv[0]); close(sv[1]); return FALSE; }

    if (g_helper_pid == 0)
    {
        /* Declared here, at the very top of this block before any statement,
         * not next to its own comment/use further down -- this whole file
         * (and Wine's own build, -Werror=declaration-after-statement) is C90,
         * which forbids mixed declarations and code. See this variable's own
         * assignment below for why it exists. */
        BOOL use_bundle_gl;

        /* Child: keep sv[1], drop sv[0]. dup2 onto a fixed fd (3) so the
         * env var passed to the child is a constant, not something that
         * depends on fd-table state -- simpler to reason about than
         * passing whatever number socketpair() happened to hand back. */
        close(sv[0]);
        dup2(sv[1], 3);
        if (sv[1] != 3) close(sv[1]);

        snprintf(fd_env, sizeof(fd_env), "3");
        setenv("WEBVIEW2LOADER_IPC_FD", fd_env, 1);

        /* Part B (Task 7): WebKitGTK's own runtime env vars (helper process
         * paths, GStreamer/GIO/GBM/GSettings lookup dirs) -- see
         * set_webkit_relocation_env()'s own comment for why this belongs
         * here and not in the deleted dlmopen-era init path. Must run
         * before execl() below, same as the LD_LIBRARY_PATH decision that
         * follows. */
        set_webkit_relocation_env(bundle_dir);

        /* WEBVIEW2LOADER_FORCE_GL_FALLBACK: test-only hook, originally added
         * alongside the PARENT-computed host_egl_ok above so the bundle's
         * own relocated gl-fallback/ llvmpipe copy could be exercised on a
         * machine whose host EGL was genuinely usable, without having to
         * uninstall glvnd to prove the fallback branch works. Now redundant
         * with WV2L_ALWAYS_USE_BUNDLE_GL (which already forces this branch
         * unconditionally) but left in the condition rather than removed --
         * still harmless, still a no-op when WV2L_ALWAYS_USE_BUNDLE_GL is
         * TRUE, and immediately useful again the moment a future person flips
         * that constant back to host-EGL-gated.
         *
         * use_bundle_gl assigned to a plain local first (rather than writing
         * the `#define` directly into the `if`) so this reads as an ordinary
         * runtime decision, not a `if (1 || ...)`-shaped dead-code puzzle for
         * whoever next edits this function -- declared at the top of this
         * block (see above) rather than here, to satisfy this file's C90
         * declaration-after-statement discipline. */
        use_bundle_gl = WV2L_ALWAYS_USE_BUNDLE_GL ||
                        getenv("WEBVIEW2LOADER_FORCE_GL_FALLBACK") != NULL ||
                        !host_egl_ok;
        if (use_bundle_gl)
        {
            /* execve's own envp is read at true process startup -- this is
             * exactly the case LD_LIBRARY_PATH works for, unlike setenv()
             * from inside an already-running process. See Task 7. */
            char ld_path[PATH_MAX];
            snprintf(ld_path, sizeof(ld_path), "%s", bundle_gl_fallback_dir(bundle_dir));
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
