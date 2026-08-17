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
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <time.h>
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

/* Event channel (host -> here), the second socket described in
 * webview2loader_ipc_protocol.h's own "Event channel" comment. Separate from
 * g_helper_fd on purpose: a PE-side pump thread parks in a blocking read on
 * this fd for the whole session, and it must never hold g_ipc_mutex while
 * doing so or every ordinary request would block behind it. */
static int g_event_fd = -1;

/* Helper-restart state, all guarded by g_ipc_mutex.
 *
 * The helper used to be spawned exactly once, from a pthread_once. That made
 * a single helper crash permanent for the rest of the Wine process: the dead
 * socket stayed in g_helper_fd, the once-guard could never run the init body
 * again, and every later ipc_call failed forever -- so one abort in the host
 * (a real, observed WebKitGTK/GDK crash) took the whole WebView2 surface down
 * with it instead of costing one dialog. Real WebView2 recovers into a fresh
 * browser process; these three fields are what let this one do the same.
 *
 * g_helper_generation: bumped on every successful spawn, and folded into the
 *   top bits of every handle handed back to the PE side (see
 *   handle_tag_locked).
 *   Without it, a handle issued by a DEAD helper could be forwarded to a
 *   FRESH one and happen to match a genuinely-live native_webview there --
 *   the new helper allocates from the same allocator in the same binary, so
 *   an early allocation landing on a recycled address is plausible, not
 *   theoretical. The generation check rejects those before they are ever sent.
 * g_helper_unavailable: set only for failures that cannot improve by retrying
 *   (TUXBLOX_WEBVIEW_DIR unset -- not running under this repo's proton), so
 *   those don't fork a doomed child on every call.
 * g_last_spawn_secs: monotonic timestamp of the last spawn ATTEMPT, used for
 *   the cooldown below -- an unconditional respawn-on-demand would fork once
 *   per WebView2 call against a helper that dies instantly. */
static unsigned int g_helper_generation;
static BOOL g_helper_unavailable;
static time_t g_last_spawn_secs;

/* Minimum seconds between spawn attempts. Bounds the fork rate if the helper
 * dies immediately every time, while still allowing unlimited recovery across
 * a long Studio session (unlike a fixed attempt count, which would stop
 * recovering after N crashes on a session that legitimately runs for hours). */
#define WV2L_RESPAWN_COOLDOWN_SECS 2

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
    int ev[2];
    char helper_path[PATH_MAX];
    char fd_env[32];
    BOOL host_egl_ok;

    if (socketpair(AF_UNIX, SOCK_STREAM, 0, sv) != 0) return FALSE;
    /* Second socketpair for host->here events. Failing to create it is NOT
     * fatal: everything except event delivery works without one, and the
     * helper explicitly tolerates its absence (see main.c's own handling of a
     * missing WEBVIEW2LOADER_EVENT_FD), so degrade rather than refuse to
     * start the webview at all. */
    if (socketpair(AF_UNIX, SOCK_STREAM, 0, ev) != 0) { ev[0] = -1; ev[1] = -1; }

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

        /* Same fixed-fd contract as the request socket above, one number up:
         * a constant the helper can rely on rather than whatever socketpair
         * happened to return. Only set when the pair was actually created --
         * an unset WEBVIEW2LOADER_EVENT_FD is the helper's documented signal
         * that no event channel exists this run. */
        if (ev[1] >= 0)
        {
            close(ev[0]);
            dup2(ev[1], 4);
            if (ev[1] != 4) close(ev[1]);
            snprintf(fd_env, sizeof(fd_env), "4");
            setenv("WEBVIEW2LOADER_EVENT_FD", fd_env, 1);
        }
        else unsetenv("WEBVIEW2LOADER_EVENT_FD");

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
    if (ev[1] >= 0) close(ev[1]);
    if (ev[0] >= 0)
    {
        fcntl(ev[0], F_SETFD, FD_CLOEXEC); /* same rationale as sv[0] below */
        g_event_fd = ev[0];
    }
    /* CLOEXEC on our own end: nothing Wine exec's later (including a future
     * respawn's own child, or any process Studio itself launches) has any use
     * for this socket, and an inherited copy would keep the peer's read from
     * ever seeing EOF -- which is exactly the "Wine died" detection the helper
     * relies on in place of PR_SET_PDEATHSIG (see the removed-PDEATHSIG
     * comment in webkitgtk-bundle/host/main.c). */
    fcntl(sv[0], F_SETFD, FD_CLOEXEC);
    g_helper_fd = sv[0];
    return TRUE;
}

/* Closes this side of the socket and reaps the helper. Caller holds
 * g_ipc_mutex. Safe to call when nothing is running (both fields already
 * cleared) -- every failure path funnels through here rather than clearing
 * the fields itself, so there is one place that can leave a zombie and one
 * place to get it right.
 *
 * SIGKILL rather than SIGTERM, then a blocking waitpid: by the time this runs
 * the helper has already failed a request, so it is either dead (waitpid
 * returns immediately) or wedged badly enough that a polite shutdown is not
 * worth waiting on. It is our own direct child, so the wait is bounded. */
static void helper_teardown_locked(void)
{
    if (g_helper_fd >= 0)
    {
        close(g_helper_fd);
        g_helper_fd = -1;
    }
    if (g_event_fd >= 0)
    {
        /* shutdown() before close() so a pump thread currently parked in
         * read() on this fd wakes immediately with EOF instead of waiting for
         * the helper's own end to close. It would wake anyway once the child
         * dies below, but not until then, and the respawn should not have to
         * wait on that. */
        shutdown(g_event_fd, SHUT_RDWR);
        close(g_event_fd);
        g_event_fd = -1;
    }
    if (g_helper_pid > 0)
    {
        int status;
        kill(g_helper_pid, SIGKILL);
        while (waitpid(g_helper_pid, &status, 0) < 0 && errno == EINTR) { /* retry */ }
        g_helper_pid = -1;
    }
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
        /* send(MSG_NOSIGNAL) rather than write(): writing to a socket whose
         * peer has exited raises SIGPIPE, and a dead helper is now an expected,
         * recoverable state rather than an impossible one. Wine does set
         * SIGPIPE to SIG_IGN process-wide (dlls/ntdll/unix/server.c), so this
         * is belt-and-braces -- but it makes the helper-death path depend on
         * this file rather than on a global signal disposition set elsewhere,
         * and a harness that drives this code outside Wine proved the
         * difference is real: the same write races between ECONNRESET and
         * EPIPE depending on how far the peer's teardown has progressed. */
        ssize_t n = send(fd, (const char *)buf + done, len - done, MSG_NOSIGNAL);
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
static BOOL ipc_call_locked(enum wv2l_opcode op, void *req_resp, size_t size);

/* Makes sure a live helper is on the other end of g_helper_fd, spawning (or
 * respawning) one if not. Caller holds g_ipc_mutex. Returns FALSE without
 * having spawned anything if the helper is permanently unavailable, or if the
 * cooldown since the last attempt hasn't elapsed.
 *
 * The WV2L_OP_INIT round-trip is part of "spawned successfully": a child that
 * exec'd but can't answer its very first request is not a working helper, and
 * treating it as one would just move the failure to the next real call. */
static BOOL ensure_helper_locked(void)
{
    struct wv2l_init_params wire;
    struct timespec now;
    const char *dir;

    if (g_helper_fd >= 0) return TRUE;
    if (g_helper_unavailable) return FALSE;

    if (!(dir = getenv("TUXBLOX_WEBVIEW_DIR")) || !dir[0])
    {
        WARN("TUXBLOX_WEBVIEW_DIR not set -- not running under this repo's proton\n");
        g_helper_unavailable = TRUE; /* cannot improve by retrying */
        return FALSE;
    }

    /* Cooldown. CLOCK_MONOTONIC so a wall-clock jump (NTP, suspend/resume)
     * can't make this wait for hours or fire continuously. */
    if (clock_gettime(CLOCK_MONOTONIC, &now) == 0)
    {
        if (g_last_spawn_secs && now.tv_sec - g_last_spawn_secs < WV2L_RESPAWN_COOLDOWN_SECS)
            return FALSE;
        g_last_spawn_secs = now.tv_sec;
    }

    /* Clears a previous generation's dead fd/zombie before forking a new
     * child -- also what keeps the new child from inheriting the old socket. */
    helper_teardown_locked();

    if (!spawn_helper(dir))
    {
        WARN("spawn_helper failed -- could not start webview2loader-host\n");
        return FALSE;
    }

    memset(&wire, 0, sizeof(wire));
    if (!ipc_call_locked(WV2L_OP_INIT, &wire, sizeof(wire)) || !wire.success)
    {
        WARN("WV2L_OP_INIT round-trip failed -- webview2loader-host not responding correctly\n");
        helper_teardown_locked();
        return FALSE;
    }

    /* Only now is this a helper whose handles are worth honouring. Bumping
     * AFTER a confirmed-working INIT means a failed spawn never burns a
     * generation, so handles from the last GOOD helper stay distinguishable
     * from handles the next good one will issue. */
    g_helper_generation++;
    if (g_helper_pid > 0)
        WARN("webview2loader-host running as pid %d (generation %u)\n",
             (int)g_helper_pid, g_helper_generation);
    return TRUE;
}

/* The raw round-trip: caller holds g_ipc_mutex and has already ensured a live
 * helper. Tears the helper down on any transport failure so the NEXT call
 * respawns rather than writing into a socket whose peer is gone. */
static BOOL ipc_call_locked(enum wv2l_opcode op, void *req_resp, size_t size)
{
    uint32_t wire_op = (uint32_t)op;
    BOOL ok;

    ok = g_helper_fd >= 0
        && ipc_write_full(g_helper_fd, &wire_op, sizeof(wire_op)) == (ssize_t)sizeof(wire_op)
        && ipc_write_full(g_helper_fd, req_resp, size) == (ssize_t)size
        && ipc_read_full(g_helper_fd, req_resp, size) == (ssize_t)size;

    if (!ok && g_helper_fd >= 0)
    {
        /* A partial write or a short read also lands here: once framing is
         * broken there is no way to resynchronise a stream protocol with no
         * request IDs, so the connection is finished either way. */
        WARN("IPC round-trip for opcode %u failed (%s) -- tearing down the helper; "
             "the next call will respawn it\n", (unsigned)op, strerror(errno));
        helper_teardown_locked();
    }
    return ok;
}

/* --- Handle generation tagging ---
 *
 * x86_64 user-space pointers occupy the low 47 bits, so the top 16 are free to
 * carry the generation the handle was issued in. See g_helper_generation's own
 * comment for why a bare pointer must not be forwarded across a respawn.
 *
 * Both directions run with g_ipc_mutex held, in the same critical section as
 * the send/receive they belong to. That matters: validating a handle and then
 * writing it in a later critical section would leave a window for another
 * thread's failure to respawn the helper in between, which is exactly the
 * stale-handle-into-a-fresh-helper case this exists to prevent. */
#define WV2L_HANDLE_PTR_MASK 0x0000ffffffffffffULL
#define WV2L_HANDLE_GEN_SHIFT 48

static UINT64 handle_tag_locked(uint64_t raw)
{
    if (!raw) return 0; /* 0 is the wire's own "failed" value -- never tag it */
    return (UINT64)((raw & WV2L_HANDLE_PTR_MASK)
                    | ((uint64_t)(g_helper_generation & 0xffff) << WV2L_HANDLE_GEN_SHIFT));
}

/* Returns the raw helper-side handle, or 0 if it was issued by a helper that
 * is no longer running -- callers already treat 0 as the invalid handle. */
static uint64_t handle_untag_locked(UINT64 tagged)
{
    unsigned int gen;

    if (!tagged) return 0;
    gen = (unsigned int)(tagged >> WV2L_HANDLE_GEN_SHIFT);
    if (gen != (g_helper_generation & 0xffff))
    {
        WARN("handle %s came from an earlier webview2loader-host (generation %u, now %u) -- "
             "rejecting rather than forwarding it to the current one\n",
             wine_dbgstr_longlong(tagged), gen, g_helper_generation & 0xffff);
        return 0;
    }
    return (uint64_t)(tagged & WV2L_HANDLE_PTR_MASK);
}

/* The two public entry points below both take g_ipc_mutex, ensure a live
 * helper, and send. Neither RETRIES the request after a respawn, deliberately:
 * these opcodes are not all idempotent, and every handle the caller is holding
 * belongs to the dead helper's generation anyway. The failing call fails; the
 * caller creates a fresh controller and that one works.
 *
 * There is no handle-less variant besides ipc_call_create -- WV2L_OP_INIT is
 * issued from inside ensure_helper_locked (which already holds the lock, so it
 * calls ipc_call_locked directly), and every other opcode carries a handle.
 *
 * ipc_call_handle: handle_field points at the `handle` member of the caller's
 * own wire struct, holding the PE-side TAGGED value on entry; this swaps in the
 * raw helper-side value before sending. Passing the field by pointer rather
 * than assuming it sits at offset 0 keeps this honest even though, today,
 * every handle-bearing wire struct does start with it. */
static BOOL ipc_call_handle(enum wv2l_opcode op, void *req_resp, size_t size, uint64_t *handle_field)
{
    BOOL ok = FALSE;
    uint64_t raw;

    pthread_mutex_lock(&g_ipc_mutex);
    if (ensure_helper_locked() && (raw = handle_untag_locked((UINT64)*handle_field)) != 0)
    {
        *handle_field = raw;
        ok = ipc_call_locked(op, req_resp, size);
    }
    pthread_mutex_unlock(&g_ipc_mutex);
    return ok;
}

/* CREATE_WEBVIEW's response handle has to be tagged with the generation of the
 * helper that actually answered, read in the same critical section as the call
 * -- reading g_helper_generation afterwards could pick up a newer one if
 * another thread's failure respawned in between, mislabelling the handle as
 * belonging to a helper that has never seen it. */
static BOOL ipc_call_create(struct wv2l_create_webview_params *wire, UINT64 *tagged_out)
{
    BOOL ok;

    pthread_mutex_lock(&g_ipc_mutex);
    ok = ensure_helper_locked() && ipc_call_locked(WV2L_OP_CREATE_WEBVIEW, wire, sizeof(*wire));
    *tagged_out = ok ? handle_tag_locked(wire->handle) : 0;
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

/* webview2loader_unix_init() runs on every environment creation (see main.c),
 * possibly from several PE threads at once. This used to be a pthread_once
 * around the whole spawn -- correct for "exactly once", wrong for a helper
 * that can die and need replacing (see g_helper_generation's own comment).
 * g_ipc_mutex now serves both purposes: it already serialises every request,
 * and holding it across the spawn gives concurrent callers the same
 * block-until-the-first-one-finishes behaviour pthread_once provided, while
 * still allowing a later call to spawn a replacement. */
static NTSTATUS unix_init_impl(void *args)
{
    struct init_params *params = args;
    BOOL ok;

    pthread_mutex_lock(&g_ipc_mutex);
    ok = ensure_helper_locked();
    pthread_mutex_unlock(&g_ipc_mutex);

    params->success = ok;
    return ok ? STATUS_SUCCESS : STATUS_NOT_SUPPORTED;
}

static NTSTATUS unix_create_webview_impl(void *args)
{
    struct create_webview_params *params = args;
    struct wv2l_create_webview_params wire = { .is_message_only = params->is_message_only };
    UINT64 tagged = 0;

    if (!ipc_call_create(&wire, &tagged))
    {
        WARN("ipc_call failed -- helper not running, failing CreateWebview\n");
        params->handle = 0;
        return STATUS_NOT_SUPPORTED;
    }
    params->handle = tagged;
    return tagged ? STATUS_SUCCESS : STATUS_NOT_SUPPORTED;
}

static NTSTATUS unix_destroy_webview_impl(void *args)
{
    struct destroy_webview_params *params = args;
    struct wv2l_destroy_webview_params wire = { .handle = params->handle };

    if (!params->handle) return STATUS_SUCCESS;
    /* A handle from a dead helper is also rejected here, and that is the
     * right answer: the helper that owned that native_webview is gone, so
     * there is nothing left to destroy. Not a leak. */
    if (!ipc_call_handle(WV2L_OP_DESTROY_WEBVIEW, &wire, sizeof(wire), &wire.handle))
        WARN("destroy_webview did not reach a live helper -- nothing to release on that side\n");
    return STATUS_SUCCESS;
}

static NTSTATUS unix_navigate_and_wait_impl(void *args)
{
    struct navigate_params *params = args;
    struct wv2l_navigate_params wire = { .handle = params->handle };

    params->is_success = FALSE;
    if (!params->handle) return STATUS_INVALID_HANDLE;

    copy_wcs_to_wire_uri(wire.uri, WV2L_URI_MAX, params->uri);

    if (!ipc_call_handle(WV2L_OP_NAVIGATE_AND_WAIT, &wire, sizeof(wire), &wire.handle))
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
    if (!ipc_call_handle(WV2L_OP_DELETE_ALL_COOKIES, &wire, sizeof(wire), &wire.handle))
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
    if (!ipc_call_handle(WV2L_OP_COUNT_COOKIES, &wire, sizeof(wire), &wire.handle))
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
    wire->offset = params->offset;
    copy_wcs_to_wire_uri(wire->uri, WV2L_URI_MAX, params->uri);

    if (!ipc_call_handle(WV2L_OP_GET_COOKIES, wire, sizeof(*wire), &wire->handle))
    {
        WARN("ipc_call failed -- helper not running, failing GetCookies without waiting\n");
        status = STATUS_NOT_SUPPORTED;
        goto done;
    }

    params->success = wire->success;
    params->count = wire->count;
    params->total = wire->total;
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

/* Mirror of wire_cookie_to_unix for the outbound direction -- only the four
 * fields libsoup actually matches on are sent; see struct delete_cookie_params's
 * own comment in unixlib.h. Both sides' caps are identical by construction
 * (WV2L_COOKIE_*_MAX mirror WEBVIEW2LOADER_COOKIE_*_MAX 1:1), so this is a plain
 * per-field memcpy, not a re-encoding. */
static void unix_cookie_to_wire(struct wv2l_cookie *dst, const struct unix_cookie *src)
{
    memcpy(dst->name, src->name, sizeof(dst->name));
    memcpy(dst->value, src->value, sizeof(dst->value));
    memcpy(dst->domain, src->domain, sizeof(dst->domain));
    memcpy(dst->path, src->path, sizeof(dst->path));
}

static NTSTATUS unix_delete_cookie_impl(void *args)
{
    struct delete_cookie_params *params = args;
    struct wv2l_delete_cookie_params *wire;
    NTSTATUS status = STATUS_SUCCESS;

    if (!params->handle) return STATUS_INVALID_HANDLE;

    /* Heap, not a stack local: struct wv2l_delete_cookie_params embeds a whole
     * struct wv2l_cookie (~9.6KB of fixed UTF-16 buffers) -- same "too big for
     * a thread stack" reasoning unix_get_cookies_impl already applies to its own
     * much larger wire struct. */
    if (!(wire = calloc(1, sizeof(*wire)))) return STATUS_NO_MEMORY;
    wire->handle = params->handle;
    unix_cookie_to_wire(&wire->cookie, &params->cookie);

    if (!ipc_call_handle(WV2L_OP_DELETE_COOKIE, wire, sizeof(*wire), &wire->handle))
    {
        WARN("ipc_call failed -- helper not running, failing DeleteCookie\n");
        status = STATUS_NOT_SUPPORTED;
    }

    free(wire);
    return status;
}

/* Blocks until the helper sends an event, then hands exactly one back to the
 * PE-side pump thread. Returns STATUS_NOT_SUPPORTED when there is no event
 * channel or it has gone away, which the pump treats as "stop pumping".
 *
 * Deliberately does NOT take g_ipc_mutex. It spends nearly all of its time
 * blocked in read(), and holding the request mutex for that would deadlock
 * every ordinary WebView2 call in the process. The fd is snapshotted once up
 * front instead: helper_teardown_locked shutdown()s the socket before closing
 * it, so a pump parked here wakes with EOF rather than hanging until the child
 * happens to die.
 *
 * Residual race, stated rather than hidden: between the snapshot and the
 * read(), a teardown could close this fd and something else could open a new
 * one that reuses the number, in which case this read would target the wrong
 * fd. Closing it properly needs refcounting the fd across an unbounded
 * blocking read; the exposure here is one syscall wide, only reachable during
 * a helper respawn, and the worst case is one bogus/failed event read that
 * ends the pump -- which the PE side already has to handle. */
static NTSTATUS unix_wait_event_impl(void *args)
{
    struct wait_event_params *params = args;
    struct wv2l_ev_navigation_starting_params wire;
    uint32_t wire_type;
    int fd;

    pthread_mutex_lock(&g_ipc_mutex);
    fd = g_event_fd;
    pthread_mutex_unlock(&g_ipc_mutex);
    if (fd < 0) return STATUS_NOT_SUPPORTED;

    if (ipc_read_full(fd, &wire_type, sizeof(wire_type)) != (ssize_t)sizeof(wire_type))
        return STATUS_NOT_SUPPORTED;

    switch (wire_type)
    {
    case WV2L_EV_NAVIGATION_STARTING:
        if (ipc_read_full(fd, &wire, sizeof(wire)) != (ssize_t)sizeof(wire))
            return STATUS_NOT_SUPPORTED;
        params->type = WEBVIEW2LOADER_EVENT_NAVIGATION_STARTING;
        /* Tagged on the way out for the same reason create_webview tags its
         * result: the PE side only ever holds tagged handles, so an event
         * naming a raw pointer would never match the controller it belongs to. */
        pthread_mutex_lock(&g_ipc_mutex);
        params->handle = handle_tag_locked(wire.handle);
        pthread_mutex_unlock(&g_ipc_mutex);
        params->is_redirect = wire.is_redirect ? TRUE : FALSE;
        memcpy(params->uri, wire.uri, sizeof(params->uri));
        params->uri[WEBVIEW2LOADER_URI_MAX - 1] = 0; /* never trust the peer's terminator */
        return STATUS_SUCCESS;

    default:
        /* An event type this build does not know cannot be skipped, because
         * its length is unknown -- the stream is unframed from here on. Ending
         * the pump is the only honest response. */
        WARN("unknown event type %u on the event channel -- stopping the pump\n", wire_type);
        return STATUS_NOT_SUPPORTED;
    }
}

static NTSTATUS unix_get_window_visible_impl(void *args)
{
    struct get_window_visible_params *params = args;
    struct wv2l_get_window_visible_params wire = { .handle = params->handle };

    if (!ipc_call_handle(WV2L_OP_GET_WINDOW_VISIBLE, &wire, sizeof(wire), &wire.handle))
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
    if (!ipc_call_handle(WV2L_OP_SYNC_WINDOW_GEOMETRY, &wire, sizeof(wire), &wire.handle))
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
    if (!ipc_call_handle(WV2L_OP_GET_WINDOW_GEOMETRY, &wire, sizeof(wire), &wire.handle))
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
    unix_delete_cookie_impl,
    unix_wait_event_impl,
};
