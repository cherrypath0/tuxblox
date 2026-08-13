#include <stdarg.h>
#include <stdlib.h>

#include <windef.h>
#include <winbase.h>
#include <winuser.h>
#include <wine/debug.h>

#include "webview2loader_private.h"

WINE_DEFAULT_DEBUG_CHANNEL(webview2loader);

/* Plan 3 Task 5: WH_CALLWNDPROC local hook, one per thread, refcounted
 * across however many controllers happen to share that thread -- see the
 * design spec's own Architecture section for why WH_CALLWNDPROC
 * specifically (observation-only, cannot break Studio's own message
 * loop -- unlike subclassing GWLP_WNDPROC, which must perfectly chain
 * forever and conflicts with anything else subclassing the same HWND). */

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
    HHOOK hook;
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

static LRESULT CALLBACK call_wnd_proc(int nCode, WPARAM wParam, LPARAM lParam)
{
    if (nCode == HC_ACTION)
    {
        CWPSTRUCT *cwp = (CWPSTRUCT *)lParam;
        if (cwp->message == WM_WINDOWPOSCHANGED)
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
                    if (e->hwnd == cwp->hwnd)
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
    }
    return CallNextHookEx(NULL, nCode, wParam, lParam);
}

BOOL window_hook_track(HWND hwnd, window_sync_callback callback, void *user_data)
{
    DWORD tid;
    struct thread_hook *th;
    struct tracked_entry *entry;

    if (!hwnd || !callback) return FALSE;
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
        th->hook = SetWindowsHookExW(WH_CALLWNDPROC, call_wnd_proc, NULL, tid);
        if (!th->hook)
        {
            WARN("SetWindowsHookExW failed for thread %lu, error %lu -- geometry sync for this "
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
    return TRUE;
}

void window_hook_untrack(HWND hwnd, window_sync_callback callback, void *user_data)
{
    DWORD tid = hwnd ? GetWindowThreadProcessId(hwnd, NULL) : 0;
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
        UnhookWindowsHookEx(th->hook);
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
BOOL WINAPI __wine_test_webview2loader_hook_track(HWND hwnd, window_sync_callback callback, void *user_data)
{
    return window_hook_track(hwnd, callback, user_data);
}

void WINAPI __wine_test_webview2loader_hook_untrack(HWND hwnd, window_sync_callback callback, void *user_data)
{
    window_hook_untrack(hwnd, callback, user_data);
}
