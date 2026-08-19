#include <stdarg.h>
#include <stdlib.h>
#include <string.h>

#include <windef.h>
#include <winbase.h>
#include <winuser.h>
#include <wine/debug.h>

#include "webview2loader_private.h"

WINE_DEFAULT_DEBUG_CHANNEL(webview2loader);

/* Plan 3 Task 5: per-thread top-level window tracking, one registration per
 * thread, refcounted across however many controllers happen to share that
 * thread -- see the design spec's own Architecture section for why this is
 * observation-only (it cannot break Studio's own message loop, unlike
 * subclassing GWLP_WNDPROC, which must perfectly chain forever and conflicts
 * with anything else subclassing the same HWND).
 *
 * Two mechanisms are implemented, selected by TUXBLOX_WV2_WINDOW_SYNC:
 *
 *   unset / "winevent"  -- EVENT_OBJECT_SHOW/HIDE + EVENT_OBJECT_LOCATIONCHANGE
 *   "callwndproc"       -- the original WH_CALLWNDPROC hook
 *   "off"               -- no tracking at all
 *
 * Why the default moved off WH_CALLWNDPROC: win32u's process_message() has a
 * fast path for a same-thread SendMessage that is gated on there being *no*
 * WH_CALLWNDPROC/WH_CALLWNDPROCRET hook on the thread (dlls/win32u/message.c).
 * Installing one forfeits that path for every sent message on Studio's UI
 * thread, and each send then additionally runs call_message_hooks(), which
 * costs two wineserver round trips (start_hook_chain + finish_hook_chain)
 * even for the messages this hook ignores. Mouse input is the worst case:
 * win32u sends WM_SETCURSOR for every mouse message and WM_NCHITTEST per
 * hit-test, so simply moving the camera in Studio turned into a large number
 * of extra server round trips per second -- reported as a visible framerate
 * drop that lasted exactly as long as the camera kept moving.
 *
 * The WinEvent path costs nothing on that workload: win32u emits these three
 * events only from set_window_pos (dlls/win32u/window.c), i.e. on a real
 * move/resize/show/hide, and emits nothing at all for ordinary mouse
 * messages. Coverage is equivalent -- those emissions sit in the same
 * function that sends WM_WINDOWPOSCHANGED, and LOCATIONCHANGE fires exactly
 * when the geometry actually changed (WM_WINDOWPOSCHANGED also fires for
 * pure Z-order/activation changes, which this never needed to act on;
 * controller_push_geometry_to_native is idempotent either way).
 *
 * This depends on Wine calling the WinEvent proc synchronously on the thread
 * that generated the event (call_win_event_hook -> KeUserModeCallback,
 * dlls/win32u/hook.c), which keeps the callback on the same thread and at the
 * same point in the sequence as the old WH_CALLWNDPROC callback did. That is
 * true only for WINEVENT_INCONTEXT -- see the SetWinEventHook call below, where
 * assuming it held for WINEVENT_OUTOFCONTEXT too cost a real, observed bug. */

enum sync_mode
{
    SYNC_MODE_WINEVENT = 0,
    SYNC_MODE_CALLWNDPROC,
    SYNC_MODE_OFF,
};

/* Resolved once and cached: read on every track/untrack, and the environment
 * cannot change underneath a running process in a way that should retarget
 * already-installed hooks. */
static enum sync_mode get_sync_mode(void)
{
    static LONG cached = -1;
    LONG mode = InterlockedCompareExchange(&cached, -1, -1);
    char buf[32];
    DWORD len;

    if (mode >= 0) return (enum sync_mode)mode;

    /* GetEnvironmentVariableA, not getenv: this is PE-side code, and the
     * Win32 environment block is what Wine populates from the host
     * environment -- no dependency on CRT environment setup having run. */
    len = GetEnvironmentVariableA("TUXBLOX_WV2_WINDOW_SYNC", buf, sizeof(buf));
    if (len && len < sizeof(buf) && !strcmp(buf, "callwndproc")) mode = SYNC_MODE_CALLWNDPROC;
    else if (len && len < sizeof(buf) && !strcmp(buf, "off")) mode = SYNC_MODE_OFF;
    else mode = SYNC_MODE_WINEVENT;

    InterlockedExchange(&cached, mode);
    return (enum sync_mode)mode;
}

struct tracked_entry
{
    struct tracked_entry *next;
    HWND hwnd;
    window_sync_callback callback;
    void *user_data;
};

struct thread_hook
{
    struct thread_hook *next;
    DWORD thread_id;
    HHOOK cwp_hook;             /* SYNC_MODE_CALLWNDPROC only */
    HWINEVENTHOOK we_hooks[2];  /* SYNC_MODE_WINEVENT only: show/hide, then locationchange */
    LONG refcount;
    struct tracked_entry *entries;
};

static CRITICAL_SECTION hook_cs;
static CRITICAL_SECTION_DEBUG hook_cs_debug =
{
    0, 0, &hook_cs,
    { &hook_cs_debug.ProcessLocksList, &hook_cs_debug.ProcessLocksList },
      0, 0, { (DWORD_PTR)(__FILE__ ": hook_cs") }
};
static CRITICAL_SECTION hook_cs = { &hook_cs_debug, -1, 0, 0, 0, 0 };
static struct thread_hook *hooks;

/* Fires every tracked callback for cwp->hwnd on this thread. Snapshots
 * matches into a small fixed-size stack array under hook_cs, then invokes
 * them AFTER releasing the lock -- mirrors this DLL's existing
 * navigate_worker snapshot pattern (webview.c) for the same reason: a
 * callback (controller_push_geometry_to_native, ultimately issuing a
 * blocking unix call) must never run while hook_cs is held, or a
 * concurrent window_hook_track/untrack on another thread would deadlock
 * against it. Capped at 8 simultaneous entries for one HWND -- far beyond
 * any realistic number of controllers sharing one parent window; a real
 * overflow just means the 9th+ controller's geometry sync waits for the
 * next WM_WINDOWPOSCHANGED, not a crash or leak. */
#define MAX_SNAPSHOT 8

static void fire_tracked_callbacks(HWND hwnd)
{
    DWORD tid = GetCurrentThreadId();
    window_sync_callback cb_snapshot[MAX_SNAPSHOT];
    void *ud_snapshot[MAX_SNAPSHOT];
    unsigned int count = 0, i;
    struct thread_hook *th;
    struct tracked_entry *e;

    EnterCriticalSection(&hook_cs);
    for (th = hooks; th && th->thread_id != tid; th = th->next);
    if (th)
    {
        for (e = th->entries; e && count < MAX_SNAPSHOT; e = e->next)
        {
            /* IsChild as well as equality: Studio hides the Toolbox by hiding
             * the dock panel ABOVE the controller's parent, so the message
             * lands on an ancestor and an equality-only match would never fire
             * the sync that unmaps the webview. IsChild covers the whole
             * descendant chain, not just direct children. */
            if (e->hwnd == hwnd || IsChild(hwnd, e->hwnd))
            {
                cb_snapshot[count] = e->callback;
                ud_snapshot[count] = e->user_data;
                count++;
            }
        }
    }
    LeaveCriticalSection(&hook_cs);

    for (i = 0; i < count; i++) cb_snapshot[i](ud_snapshot[i]);
}

static LRESULT CALLBACK call_wnd_proc(int nCode, WPARAM wParam, LPARAM lParam)
{
    if (nCode == HC_ACTION)
    {
        CWPSTRUCT *cwp = (CWPSTRUCT *)lParam;
        /* WM_SHOWWINDOW as well: ShowWindow(SW_HIDE) on an ancestor does emit
         * WM_WINDOWPOSCHANGED, but a window hidden as part of its parent being
         * hidden gets only WM_SHOWWINDOW, and that is the playtest case. */
        if (cwp->message == WM_WINDOWPOSCHANGED || cwp->message == WM_SHOWWINDOW)
            fire_tracked_callbacks(cwp->hwnd);
    }
    return CallNextHookEx(NULL, nCode, wParam, lParam);
}

/* set_window_pos emits all three tracked events against the window object
 * itself (OBJID_WINDOW/CHILDID_SELF); anything else is a control's own
 * accessible child object, never this controller's parent moving. */
static void CALLBACK win_event_proc(HWINEVENTHOOK hook, DWORD event, HWND hwnd,
                                    LONG object_id, LONG child_id, DWORD event_thread, DWORD event_time)
{
    if (!hwnd || object_id != OBJID_WINDOW || child_id != CHILDID_SELF) return;
    fire_tracked_callbacks(hwnd);
}

/* Review fix (Important finding, post-Task-5): window_hook_track hands
 * back the thread id it discovered via GetWindowThreadProcessId(hwnd, NULL)
 * -- taken here, while hwnd is guaranteed still alive (that's how this
 * function found which thread to hook in the first place). Callers must
 * hang onto *tid_out and pass it back to window_hook_untrack instead of
 * letting untrack re-derive it from hwnd. Re-deriving at untrack time is
 * exactly the bug this fixes: GetWindowThreadProcessId returns 0 for an
 * already-destroyed HWND, which used to make window_hook_untrack silently
 * no-op -- leaving the tracked_entry (whose user_data is a pointer into the
 * caller's about-to-be-freed object, see controller.c's
 * controller_destroy_native) permanently in the registry. Since HWND
 * values get recycled by the OS after destruction, a later, unrelated
 * window created on the same thread could receive that same recycled
 * value; call_wnd_proc's cwp->hwnd match is purely by value with no
 * liveness check, so it would then fire the stale callback against freed
 * memory -- a real use-after-free, not just a leak. Capturing tid once,
 * at track time, means untrack never depends on hwnd still being valid to
 * find the right thread's registry. */
BOOL window_hook_track(HWND hwnd, window_sync_callback callback, void *user_data, DWORD *tid_out)
{
    DWORD tid;
    struct thread_hook *th;
    struct tracked_entry *entry;

    if (!hwnd || !callback) return FALSE;
    if (get_sync_mode() == SYNC_MODE_OFF) return FALSE;
    tid = GetWindowThreadProcessId(hwnd, NULL);
    if (!tid) return FALSE;

    if (!(entry = malloc(sizeof(*entry)))) return FALSE;
    entry->hwnd = hwnd;
    entry->callback = callback;
    entry->user_data = user_data;

    EnterCriticalSection(&hook_cs);
    for (th = hooks; th && th->thread_id != tid; th = th->next);
    if (!th)
    {
        if (!(th = malloc(sizeof(*th))))
        {
            LeaveCriticalSection(&hook_cs);
            free(entry);
            return FALSE;
        }
        th->thread_id = tid;
        th->refcount = 0;
        th->entries = NULL;
        th->cwp_hook = NULL;
        th->we_hooks[0] = th->we_hooks[1] = NULL;

        if (get_sync_mode() == SYNC_MODE_CALLWNDPROC)
        {
            th->cwp_hook = SetWindowsHookExW(WH_CALLWNDPROC, call_wnd_proc, NULL, tid);
        }
        else
        {
            /* Scoped to this process and this one thread, matching the old
             * hook's scope exactly -- a thread-specific registration only
             * bumps that thread's own queue hook count server-side. */
            /* WINEVENT_INCONTEXT, not WINEVENT_OUTOFCONTEXT -- a real bug fix,
             * verified against this fork's own wineserver source rather than
             * assumed.
             *
             * This file used to claim Wine "calls the WinEvent proc
             * synchronously on the thread that generated the event", which is
             * only true of the in-context path: server/hook.c's get_first_hook/
             * get_next_hook `return hook` (so win32u calls call_win_event_hook
             * inline) ONLY for WINEVENT_INCONTEXT, and otherwise falls through
             * to post_win_event(), which queues the event to the installing
             * thread's message queue instead. Out of context, the callback is
             * therefore neither synchronous nor ordered against the
             * set_window_pos that produced it.
             *
             * Measured cost of that: during a real playtest the webview was
             * never told to hide even once. Studio's own toolbox panel goes
             * invisible, but no sync ran to observe it, so a foreign X window
             * that Wine cannot hide on its own kept painting over the 3D
             * viewport. Forcing the WH_CALLWNDPROC mode below made the same
             * playtest emit the HIDDEN transition immediately -- same
             * visibility logic, only the delivery of the notification differed.
             *
             * The module handle is required, not optional: NtUserSetWinEventHook
             * fails with ERROR_HOOK_NEEDS_HMOD for an in-context hook with a
             * NULL HMODULE. It is then discarded again a few lines later for a
             * thread-local hook (`if (tid) inst = 0;`), so this costs nothing
             * and injects nothing -- the hook proc is already in this process. */
            th->we_hooks[0] = SetWinEventHook(EVENT_OBJECT_SHOW, EVENT_OBJECT_HIDE,
                                              webview2loader_instance, win_event_proc,
                                              GetCurrentProcessId(), tid, WINEVENT_INCONTEXT);
            th->we_hooks[1] = SetWinEventHook(EVENT_OBJECT_LOCATIONCHANGE, EVENT_OBJECT_LOCATIONCHANGE,
                                              webview2loader_instance, win_event_proc,
                                              GetCurrentProcessId(), tid, WINEVENT_INCONTEXT);
            if (!th->we_hooks[0] || !th->we_hooks[1])
            {
                /* All-or-nothing: a half-installed pair would silently miss
                 * either moves or show/hide rather than degrade predictably. */
                if (th->we_hooks[0]) UnhookWinEvent(th->we_hooks[0]);
                if (th->we_hooks[1]) UnhookWinEvent(th->we_hooks[1]);
                th->we_hooks[0] = th->we_hooks[1] = NULL;
            }
        }

        if (!th->cwp_hook && !th->we_hooks[0])
        {
            WARN("window-sync hook install failed for thread %lu, error %lu -- geometry sync for this "
                 "controller degrades to explicit put_Bounds/put_IsVisible calls only\n", tid, GetLastError());
            LeaveCriticalSection(&hook_cs);
            free(th);
            free(entry);
            return FALSE;
        }
        th->next = hooks;
        hooks = th;
    }
    entry->next = th->entries;
    th->entries = entry;
    th->refcount++;
    LeaveCriticalSection(&hook_cs);
    if (tid_out) *tid_out = tid;
    return TRUE;
}

/* tid is the value window_hook_track handed back via tid_out at install
 * time -- NOT re-derived from hwnd here (see window_hook_track's comment
 * above for why re-deriving was the bug). hwnd/callback/user_data are
 * still needed to identify which of possibly-several entries on that
 * thread to remove; they're never dereferenced, only compared by value. */
void window_hook_untrack(DWORD tid, HWND hwnd, window_sync_callback callback, void *user_data)
{
    struct thread_hook *th, **th_cur;
    struct tracked_entry **cur, *dead;

    if (!tid) return;

    EnterCriticalSection(&hook_cs);
    for (th_cur = &hooks; *th_cur && (*th_cur)->thread_id != tid; th_cur = &(*th_cur)->next);
    th = *th_cur;
    if (!th) { LeaveCriticalSection(&hook_cs); return; }

    for (cur = &th->entries; *cur; cur = &(*cur)->next)
    {
        if ((*cur)->hwnd == hwnd && (*cur)->callback == callback && (*cur)->user_data == user_data)
        {
            dead = *cur;
            *cur = dead->next;
            free(dead);
            th->refcount--;
            break;
        }
    }

    if (th->refcount <= 0)
    {
        *th_cur = th->next;
        LeaveCriticalSection(&hook_cs);
        /* Uninstall whichever mechanism this thread_hook was created with --
         * keyed off what is actually set, not off get_sync_mode(), so a
         * registration always tears down the way it was installed. */
        if (th->cwp_hook) UnhookWindowsHookEx(th->cwp_hook);
        if (th->we_hooks[0]) UnhookWinEvent(th->we_hooks[0]);
        if (th->we_hooks[1]) UnhookWinEvent(th->we_hooks[1]);
        free(th);
        return;
    }
    LeaveCriticalSection(&hook_cs);
}

/* Test-support-only exports (Plan 3 Task 5) -- let tests/webview2loader.c's
 * standalone hook test drive window_hook_track/untrack directly with a
 * plain counter callback, without needing a real controller/GTK/display at
 * all (design spec Testing Strategy item 2). Real production callers
 * (controller.c) call window_hook_track/untrack directly via normal C
 * linkage within the DLL -- these two exports exist purely so an external
 * test EXE (which only ever links against webview2loader's import lib, see
 * tests/webview2loader.c's own file-level comment on IID_* linkage) can
 * reach the same functions. */
BOOL WINAPI __wine_test_webview2loader_hook_track(HWND hwnd, window_sync_callback callback, void *user_data, DWORD *tid_out)
{
    return window_hook_track(hwnd, callback, user_data, tid_out);
}

void WINAPI __wine_test_webview2loader_hook_untrack(DWORD tid, HWND hwnd, window_sync_callback callback, void *user_data)
{
    window_hook_untrack(tid, hwnd, callback, user_data);
}

/* Review-fix regression test support (Important finding, post-Task-5):
 * returns how many tracked_entry structs are currently registered for
 * thread tid (0 if no thread_hook exists for tid at all). Lets a test
 * assert an entry was actually unlinked by window_hook_untrack -- even
 * after the tracked HWND has already been destroyed, which is exactly the
 * scenario the fix above addresses and which was previously unobservable
 * from outside this file (the old buggy behavior was a silent no-op, not
 * a visible failure). */
UINT WINAPI __wine_test_webview2loader_hook_entry_count(DWORD tid)
{
    struct thread_hook *th;
    struct tracked_entry *e;
    UINT count = 0;

    EnterCriticalSection(&hook_cs);
    for (th = hooks; th && th->thread_id != tid; th = th->next);
    if (th)
    {
        for (e = th->entries; e; e = e->next) count++;
    }
    LeaveCriticalSection(&hook_cs);
    return count;
}
