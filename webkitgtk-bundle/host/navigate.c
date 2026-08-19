/* TuxBlox - Linux Compatibility Layer for the Roblox Engine
 * Copyright (C) 2026 TuxBlox Developers
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <https://www.gnu.org/licenses/>.
 */

/* webkitgtk-bundle/host/navigate.c
 *
 * Navigation, cookies, and the real roblox-studio-auth: OAuth redirect
 * handoff -- ported from ProtonSource/wine/dlls/webview2loader/unixlib.c as
 * of commit ac3634ea6 (see git show ac3634ea6:ProtonSource/wine/dlls/webview2loader/unixlib.c,
 * the last commit before Task 3's rewrite deleted that file's in-process
 * dlmopen/GTK-thread machinery). This is Task 6 of the
 * webview2loader-host-process plan
 * (docs/superpowers/plans/2026-08-14-webview2loader-host-process.md).
 *
 * Mapping from the old file (mechanical renames/removals per Task 6's
 * brief, same treatment Tasks 4/5 already applied -- no logic changes
 * beyond what's explicitly noted below):
 *   - on_load_changed, navigate_on_gtk_thread,
 *     disconnect_load_changed_on_gtk_thread -> navigate_and_wait, the
 *     void*data/gtk_thread_invoke_sync wrapper dropped the same way Task 4
 *     dropped it for create_webview_on_gtk_thread/destroy_webview_on_gtk_thread
 *     (this whole process's dispatch handler already runs on the same
 *     single thread the old code had to marshal onto separately).
 *   - delete_cookies_ctx_release, on_get_all_cookies_done,
 *     start_get_all_cookies_on_gtk_thread -> cookies_delete_all, same
 *     wrapper-removal treatment.
 *   - count_cookies_ctx_release, on_count_cookies_done,
 *     start_count_cookies_on_gtk_thread -> cookies_count, same treatment.
 *   - get_cookies_ctx_release, on_get_cookies_done,
 *     start_get_cookies_on_gtk_thread, copy_field_or_fail, fill_unix_cookie
 *     -> cookies_get + copy_field_or_fail + fill_wire_cookie, same
 *     treatment; fill_wire_cookie writes directly into struct wv2l_cookie
 *     (the wire format) instead of PE-side struct unix_cookie, since that
 *     PE-side struct isn't available/needed on this side of the IPC
 *     boundary -- see navigate.h's own comment on cookies_get.
 *   - xdg_open_handoff, on_decide_policy, known_roblox_schemes -> ported
 *     VERBATIM (same fork()+execvp() double-fork shape, same narrow
 *     prefix-only scheme allowlist, same real trace-verified behavior --
 *     see this task's own brief). The `g_signal_connect_data(nv->view,
 *     "decide-policy", ...)` call that connects on_decide_policy moves into
 *     webview.c's webview_create (Task 4's file, additive edit only --
 *     see that file's own updated comment), matching where it lived in the
 *     original (webview-creation time, alongside close-request).
 *
 * Real, load-bearing adaptation beyond p_-prefix-drop/real-header
 * substitution, worth calling out explicitly (unlike Tasks 4/5's port,
 * which needed no adaptation to the *wait* mechanism itself): the
 * original's bounded waits (unix_navigate_and_wait_impl's 30s,
 * unix_delete_all_cookies_impl/unix_count_cookies_impl/unix_get_cookies_impl's
 * 10s) used pthread_mutex_t/pthread_cond_t + pthread_cond_timedwait,
 * because that code ran on a genuinely separate OS thread ("gtk_thread")
 * from the PE-calling thread doing the waiting -- the GTK thread's own main
 * loop kept running independently while the calling thread blocked on the
 * condvar, and the GTK thread's later signal/callback woke it via
 * pthread_cond_signal.
 *
 * This host process has no such second thread: main.c's single GMainLoop
 * IS the thread that dispatches this IPC call in the first place (see
 * webview.c's own live_webviews comment: "there is no separate 'GTK
 * thread' to bridge to anymore, the whole helper process's main() *is*
 * that thread"). A real pthread_cond_timedwait here would block that one
 * and only thread -- but WebKit's own "load-changed" signal emission and
 * GAsyncReadyCallback completions are themselves delivered as events on
 * this same thread's GLib main context, so a genuinely blocked thread could
 * never actually process the very completion it's waiting for: every real
 * wait would silently degrade into a guaranteed full timeout, not a
 * mechanical no-op substitution. The correct, standard GLib idiom for
 * "synchronously wait for an async completion without blocking the thread
 * that has to process it" is recursing into a nested GMainLoop
 * (g_main_loop_new/g_main_loop_run/g_main_loop_quit) bounded by a
 * g_timeout_add_seconds source at the exact same values (30s/10s) the
 * original used -- used throughout this file below. This preserves the
 * original's real behavior/bound exactly; only the specific blocking
 * primitive changes, forced by the process's real (changed) threading
 * model, not a design choice.
 *
 * That same threading-model change also simplifies (never weakens) the
 * refcounted-heap-ctx pattern the original's three cookie functions
 * needed: GAsyncReadyCallback still has no synchronous cancel (a callback
 * that fires after a real timeout can still touch ctx), so heap allocation
 * + "whichever side finishes last frees it" refcounting is preserved
 * exactly, verbatim -- but the refcount itself no longer needs Windows'
 * InterlockedDecrement (a real interlocked primitive was required only
 * because the original's two ref-holders ran on two different OS threads
 * that could race); here both ref-holders are always the same single
 * thread, just two different points in time on it, so a plain `int`
 * decrement is correct and sufficient (same "no lock needed, single
 * serialized thread" reasoning webview.c's live_webviews registry and
 * geometry.c's xmove_call_count/geometry_debug_enabled already establish).
 */
#include "navigate.h"
#include "ipc.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <sys/wait.h>

/* --- UTF-16 wire buffer <-> UTF-8 conversion --- */

char *wire_uri_to_utf8(const uint16_t *wire_uri)
{
    /* NUL-terminated by the Wine-side sender (unixlib.c's
     * copy_wcs_to_wire_uri) before it ever reaches us -- len=-1 tells
     * g_utf16_to_utf8 to scan for that terminator itself rather than
     * needing an explicit length, exactly like the original's wcs_to_utf8
     * relied on wcslen() against a NUL-terminated WCHAR*. uint16_t and
     * GLib's gunichar2 are both plain 16-bit code units -- same
     * representation, no re-encoding needed for the cast itself. */
    return g_utf16_to_utf8((const gunichar2 *)wire_uri, -1, NULL, NULL, NULL);
}

/* --- Nested-wait survival guard ---
 *
 * Every bounded wait below recurses into a nested GMainLoop, and nv can be
 * destroyed and freed WHILE that loop runs. "Every handler runs serialized on
 * one thread, so no locking is needed" (webview.c's live_webviews comment, and
 * this file's own top comment) is true of concurrency but NOT of re-entrancy:
 * g_main_loop_run keeps dispatching every other source on the default context,
 * and watchdog.c's source calls webview_destroy() -> free(nv) directly from one
 * of them when the reparented-into parent window is destroyed. That is not a
 * hypothetical ordering -- the parent dying during a long login navigation is
 * exactly the sequence watchdog.c exists for, and a 30s navigate is the widest
 * window in this file.
 *
 * The IPC socket cannot deliver a second request mid-wait (unixlib.c holds one
 * global mutex across the whole round trip), so the watchdog is the only source
 * that can free nv here -- one is enough.
 *
 * webview_lookup() only compares nv's raw pointer VALUE against live_webviews[]
 * and never dereferences it, so it is safe to call with an already-freed nv --
 * the same property on_web_process_terminated's own guard relies on. A fresh
 * webview cannot have reused this address either: that needs a CREATE_WEBVIEW
 * request, which the same global mutex is holding off.
 *
 * Returns TRUE (and clears nv->active_wait_loop) if nv is still live; FALSE
 * means the caller must not touch nv, nv->view or nv->window again at all. */
static gboolean wait_ended_with_nv_alive(struct native_webview *nv, const char *what)
{
    if (webview_lookup((uint64_t)(uintptr_t)nv))
    {
        nv->active_wait_loop = NULL;
        return TRUE;
    }
    fprintf(stderr, "webview2loader-host: native_webview %p was destroyed while %s was waiting on "
                    "it -- leaving it alone and failing this call rather than writing through a "
                    "freed pointer\n", (void *)nv, what);
    return FALSE;
}

/* --- Navigate --- */

struct navigate_ctx
{
    GMainLoop *loop; /* set to NULL once the wait exits, so a "load-changed"
                       * emission that somehow raced past the disconnect
                       * below (it can't -- see navigate_and_wait's own
                       * comment -- but checked defensively, cheaply,
                       * exactly like the cookie ctx structs below do for a
                       * real, possible race) never calls g_main_loop_quit
                       * on an already-unref'd loop. */
    guint timeout_id;
    gboolean success;
    gboolean done;
    gulong handler_id;
};

static void on_load_changed(WebKitWebView *view, WebKitLoadEvent load_event, void *user_data)
{
    struct navigate_ctx *ctx = user_data;

    (void)view;
    if (load_event != WEBKIT_LOAD_FINISHED) return;

    ctx->success = TRUE;
    ctx->done = TRUE;
    if (ctx->loop) g_main_loop_quit(ctx->loop);
}

static gboolean on_navigate_timeout(gpointer data)
{
    struct navigate_ctx *ctx = data;

    ctx->timeout_id = 0; /* this source is about to be destroyed by GLib
                           * itself (G_SOURCE_REMOVE below) -- clearing this
                           * first means the g_source_remove() call after
                           * the wait won't try to remove an already-gone
                           * source. */
    if (ctx->loop) g_main_loop_quit(ctx->loop);
    return G_SOURCE_REMOVE;
}

void navigate_and_wait(struct native_webview *nv, const char *uri_utf8, gboolean *out_success, uint64_t *out_nav_id)
{
    struct navigate_ctx ctx = { 0 };

    *out_success = FALSE;
    *out_nav_id = 0;

    /* Task 7 UAF guard equivalent -- see webview.c's own webview_lookup
     * comment for why this is a plain NULL check rather than a re-derived
     * registry lookup: main.c's dispatch case already resolved `nv` before
     * calling here, same pattern Tasks 4/5 established. */
    if (!nv)
    {
        fprintf(stderr, "webview2loader-host: stale/destroyed native window handle -- failing Navigate\n");
        return;
    }

    ctx.handler_id = g_signal_connect_data(nv->view, "load-changed", (GCallback)on_load_changed, &ctx, NULL, 0);
    webkit_web_view_load_uri(nv->view, uri_utf8 ? uri_utf8 : "");

    /* Bounded wait, not indefinite: a real page can fail to ever fire
     * load-changed(FINISHED) (network failure, WebKit process crash). 30s
     * comfortably covers Studio's real login page per the original
     * investigation's own tracing (verbatim bound, unchanged) -- a stuck
     * navigation should surface as a timeout (*out_success stays FALSE),
     * not hang the Wine-side caller forever. See this file's own top
     * comment for why a nested GMainLoop, not pthread_cond_timedwait, is
     * the correct primitive here. */
    ctx.loop = g_main_loop_new(NULL, FALSE);
    /* Cosmetic web-process-terminated mitigation -- see webview.h's own
     * comment on active_wait_loop: lets on_web_process_terminated wake this
     * wait immediately (via g_main_loop_quit) if nv's WebProcess dies mid-
     * navigation, rather than leaving it to spin out the full 30s timeout
     * below for a load-changed emission that can now never arrive. Cleared
     * again right after the wait exits, before ctx becomes invalid, exactly
     * mirroring the "ctx.loop = NULL" defensive clear a few lines down. */
    nv->active_wait_loop = ctx.loop;
    ctx.timeout_id = g_timeout_add_seconds(30, on_navigate_timeout, &ctx);
    g_main_loop_run(ctx.loop);

    /* See wait_ended_with_nv_alive's own comment for the re-entrancy mechanism
     * this guards against. Deliberately skips the g_signal_handler_disconnect
     * below on the destroyed path: nv->view was torn down along with
     * nv->window, so the connection died with it and there is no live GObject
     * left to disconnect from. */
    if (!wait_ended_with_nv_alive(nv, "Navigate"))
    {
        if (ctx.timeout_id) g_source_remove(ctx.timeout_id);
        g_main_loop_unref(ctx.loop);
        *out_success = FALSE;
        *out_nav_id = 0;
        return;
    }

    if (ctx.timeout_id) g_source_remove(ctx.timeout_id);
    g_main_loop_unref(ctx.loop);
    ctx.loop = NULL;

    /* Always disconnect before touching ctx again or returning -- not just
     * on the success path -- because ctx is stack-local to this function:
     * once it returns, &ctx is dangling, but the signal connection stays
     * live on the WebKitWebView until explicitly disconnected. If a later
     * "load-changed" emission (a second Navigate() call, or -- the exact
     * scenario the original investigation's whole plan targeted -- the
     * page's own internal redirect right after a successful Studio login)
     * reached a still-connected handler pointing at a dangling ctx, that
     * would be a callback into freed/reused stack memory. g_signal_handler_
     * disconnect is synchronous and runs on this same (single, serialized)
     * thread, so once it returns, no further on_load_changed invocation
     * for this ctx can occur -- direct call here, no
     * gtk_thread_invoke_sync marshaling needed since there's no separate
     * thread left to marshal onto (same wrapper-removal Tasks 4/5 already
     * applied elsewhere). */
    if (ctx.handler_id) g_signal_handler_disconnect(nv->view, ctx.handler_id);

    *out_success = ctx.success;
    /* Unique-enough per call; real WebView2's own navigation IDs aren't
     * otherwise observable to us -- same rationale/value shape as the
     * original (&ctx there was its own stack-local counterpart). */
    *out_nav_id = (uint64_t)(uintptr_t)&ctx;
}

/* --- Cookies: DeleteAllCookies ---
 *
 * Deviation preserved from the original (Task 8 finding, not rediscovered
 * here -- see navigate.h's own comment): webkit_cookie_manager_
 * replace_cookies(mgr, NULL, NULL, NULL, NULL) ("empty list = delete all")
 * does NOT work -- WebKit's own implementation asserts on a NULL cookies
 * GList and silently does nothing (a non-fatal GLib CRITICAL, but zero
 * cookies actually deleted). Real fix, ported verbatim:
 * webkit_cookie_manager_get_all_cookies (async) -> _finish() for the real
 * GList<SoupCookie*> -> webkit_cookie_manager_delete_cookie() once per
 * cookie -> g_list_free_full()+soup_cookie_free() to release the list
 * (transfer-full per webkitgtk.org's own docs for get_all_cookies_finish).
 */
struct delete_cookies_ctx
{
    GMainLoop *loop; /* NULL once the wait exits -- see this file's own top
                       * comment and struct navigate_ctx's identical field
                       * for why. */
    guint timeout_id;
    gboolean done;
    /* Starts at 2: one ref for cookies_delete_all's own wait below, one for
     * the in-flight get_all_cookies async operation (released by
     * on_get_all_cookies_done once it actually runs). Whichever side
     * finishes touching ctx LAST frees it -- see this file's own top
     * comment for why this heap+refcount pattern (verbatim from the
     * original) is still required even though the wait mechanism itself
     * changed: GAsyncReadyCallback has no synchronous cancel, so a
     * callback that fires after a real timeout can still run (and touch
     * ctx) at ANY later point, even long after this function has already
     * given up and returned. */
    int refs;
};

static void delete_cookies_ctx_release(struct delete_cookies_ctx *ctx)
{
    if (--ctx->refs) return;
    free(ctx);
}

static void on_get_all_cookies_done(GObject *source, GAsyncResult *res, void *user_data)
{
    struct delete_cookies_ctx *ctx = user_data;
    /* GIO/WebKit async convention: source_object is the very object the
     * _async-style call was made on -- here, the WebKitCookieManager
     * itself. */
    WebKitCookieManager *mgr = (WebKitCookieManager *)source;
    GList *cookies = webkit_cookie_manager_get_all_cookies_finish(mgr, res, NULL);
    GList *l;

    for (l = cookies; l; l = l->next)
    {
        /* Fire-and-forget: no callback needed per-cookie -- delete_cookie's
         * cookie argument is caller-owned (webkitgtk.org: "the data is
         * owned by the caller of the method"), so passing l->data here
         * doesn't transfer ownership away from the g_list_free_full() call
         * below. */
        webkit_cookie_manager_delete_cookie(mgr, l->data, NULL, NULL, NULL);
    }
    if (cookies) g_list_free_full(cookies, (GDestroyNotify)soup_cookie_free);

    ctx->done = TRUE;
    if (ctx->loop) g_main_loop_quit(ctx->loop);

    delete_cookies_ctx_release(ctx); /* this callback's own ref */
}

static gboolean on_delete_cookies_timeout(gpointer data)
{
    struct delete_cookies_ctx *ctx = data;

    ctx->timeout_id = 0;
    if (ctx->loop) g_main_loop_quit(ctx->loop);
    return G_SOURCE_REMOVE;
}

gboolean cookies_delete_all(struct native_webview *nv)
{
    struct delete_cookies_ctx *ctx;
    WebKitNetworkSession *session;
    WebKitCookieManager *mgr;

    if (!nv)
    {
        fprintf(stderr, "webview2loader-host: stale/destroyed native window handle -- skipping "
                        "DeleteAllCookies\n");
        return FALSE;
    }
    if (!(ctx = calloc(1, sizeof(*ctx))))
    {
        fprintf(stderr, "webview2loader-host: calloc failed for DeleteAllCookies context -- out of "
                        "memory, failing without waiting\n");
        return FALSE;
    }
    ctx->refs = 2;

    /* WebKitCookieManager objects are owned by the WebKitNetworkSession (in
     * turn owned by the WebKitWebView), not by us -- fetched fresh here
     * rather than cached, same as the original. */
    session = webkit_web_view_get_network_session(nv->view);
    mgr = webkit_network_session_get_cookie_manager(session);
    webkit_cookie_manager_get_all_cookies(mgr, NULL, on_get_all_cookies_done, ctx);

    /* Bounded wait, not indefinite -- same rationale as navigate_and_wait's
     * own 30s bound (a stuck WebKit network process should surface as a
     * timeout, not hang the Wine-side caller forever). 10s here rather than
     * 30s (verbatim from the original): unlike a real page navigation,
     * cookie-store enumeration is local, in-process/IPC work, not a real
     * network round-trip, so it's expected to complete almost immediately
     * in the normal case. */
    ctx->loop = g_main_loop_new(NULL, FALSE);
    /* Cosmetic web-process-terminated mitigation -- see navigate_and_wait's
     * identical comment above and webview.h's own comment on
     * active_wait_loop. */
    nv->active_wait_loop = ctx->loop;
    ctx->timeout_id = g_timeout_add_seconds(10, on_delete_cookies_timeout, ctx);
    g_main_loop_run(ctx->loop);
    wait_ended_with_nv_alive(nv, "DeleteAllCookies");

    if (ctx->timeout_id) g_source_remove(ctx->timeout_id);
    g_main_loop_unref(ctx->loop);
    ctx->loop = NULL;

    delete_cookies_ctx_release(ctx); /* this function's own ref -- safe
                                       * unconditionally, even on a timeout,
                                       * same as the original. */
    return TRUE;
}

/* --- Cookies: count (test-support only, from here through cookies_get,
 * same as the original) --- */

struct count_cookies_ctx
{
    GMainLoop *loop; /* see struct delete_cookies_ctx's identical field */
    guint timeout_id;
    gboolean done;
    uint32_t count;
    int refs; /* same pattern as struct delete_cookies_ctx above */
};

static void count_cookies_ctx_release(struct count_cookies_ctx *ctx)
{
    if (--ctx->refs) return;
    free(ctx);
}

static void on_count_cookies_done(GObject *source, GAsyncResult *res, void *user_data)
{
    struct count_cookies_ctx *ctx = user_data;
    GList *cookies = webkit_cookie_manager_get_all_cookies_finish((WebKitCookieManager *)source, res, NULL);
    GList *l;
    uint32_t n = 0;

    for (l = cookies; l; l = l->next) n++;
    if (cookies) g_list_free_full(cookies, (GDestroyNotify)soup_cookie_free);

    ctx->count = n;
    ctx->done = TRUE;
    if (ctx->loop) g_main_loop_quit(ctx->loop);

    count_cookies_ctx_release(ctx); /* this callback's own ref */
}

static gboolean on_count_cookies_timeout(gpointer data)
{
    struct count_cookies_ctx *ctx = data;

    ctx->timeout_id = 0;
    if (ctx->loop) g_main_loop_quit(ctx->loop);
    return G_SOURCE_REMOVE;
}

gboolean cookies_count(struct native_webview *nv, uint32_t *out_count)
{
    struct count_cookies_ctx *ctx;
    WebKitNetworkSession *session;
    WebKitCookieManager *mgr;

    *out_count = 0;
    if (!nv)
    {
        fprintf(stderr, "webview2loader-host: stale/destroyed native window handle -- skipping "
                        "count_cookies\n");
        return FALSE;
    }
    if (!(ctx = calloc(1, sizeof(*ctx))))
    {
        fprintf(stderr, "webview2loader-host: calloc failed for count_cookies context -- out of "
                        "memory, failing without waiting\n");
        return FALSE;
    }
    ctx->refs = 2;

    session = webkit_web_view_get_network_session(nv->view);
    mgr = webkit_network_session_get_cookie_manager(session);
    webkit_cookie_manager_get_all_cookies(mgr, NULL, on_count_cookies_done, ctx);

    ctx->loop = g_main_loop_new(NULL, FALSE);
    /* Cosmetic web-process-terminated mitigation -- see navigate_and_wait's
     * identical comment above and webview.h's own comment on
     * active_wait_loop. */
    nv->active_wait_loop = ctx->loop;
    ctx->timeout_id = g_timeout_add_seconds(10, on_count_cookies_timeout, ctx);
    g_main_loop_run(ctx->loop);
    wait_ended_with_nv_alive(nv, "count_cookies");

    if (ctx->timeout_id) g_source_remove(ctx->timeout_id);
    g_main_loop_unref(ctx->loop);
    ctx->loop = NULL;

    /* Only trust ctx->count if the callback actually ran (ctx->done true)
     * -- on a genuine timeout it's still 0 from the calloc above, the
     * honest "don't know, treat as 0" answer rather than reading a count
     * field the callback may not have written yet. */
    if (ctx->done) *out_count = ctx->count;

    count_cookies_ctx_release(ctx);
    return TRUE;
}

/* --- Cookies: GetCookies ---
 *
 * Real ICoreWebView2CookieManager::GetCookies uri semantics (per the
 * original's own comment, verified against learn.microsoft.com's real
 * ICoreWebView2CookieManager reference): a non-empty uri filters to
 * cookies applicable to that URI; NULL or empty returns every cookie under
 * the profile. WebKit's own webkit_cookie_manager_get_cookies (uri-scoped)
 * vs. get_all_cookies (used above) map onto that split directly -- no
 * hand-rolled domain/path cookie-applicability matching needed, WebKit
 * already implements the standard algorithm underneath. */

/* Copies one UTF-8 field (src) into a fixed uint16_t dst[cap] wire buffer,
 * or fails rather than silently truncating (ported verbatim from the
 * original's copy_field_or_fail -- an earlier version there truncated
 * silently, which is memory-safe but hands Studio a corrupted cookie
 * *value* as if it were real data, worse than dropping the cookie
 * outright). Every UTF-8 encoding of a Unicode codepoint is at least as
 * many bytes as its UTF-16 encoding (1-3 UTF-8 bytes -> 1 UTF-16 unit for
 * BMP codepoints, 4 UTF-8 bytes -> a 2-unit UTF-16 surrogate pair) -- so if
 * the source's UTF-8 byte length already fits within the destination's
 * uint16_t capacity, the decoded UTF-16 string is GUARANTEED to fit too, no
 * truncation possible. When it doesn't fit that conservative check (rare
 * for any real cookie field given WV2L_COOKIE_*_MAX's own generous caps),
 * this fails closed rather than guess -- same as the original. */
static gboolean copy_field_or_fail(const char *src, uint16_t *dst, size_t cap, const char *field)
{
    size_t n = src ? strlen(src) : 0;
    gunichar2 *conv;
    glong written = 0;

    if (n >= cap)
    {
        fprintf(stderr, "webview2loader-host: cookie %s is %zu bytes, exceeding this build's %zu-uint16 "
                        "cap -- dropping this cookie rather than returning a truncated value\n",
                        field, n, cap - 1);
        dst[0] = 0;
        return FALSE;
    }

    conv = g_utf8_to_utf16(src ? src : "", -1, NULL, &written, NULL);
    if (!conv)
    {
        fprintf(stderr, "webview2loader-host: cookie %s failed UTF-8 -> UTF-16 conversion -- dropping "
                        "this cookie\n", field);
        dst[0] = 0;
        return FALSE;
    }
    /* written+1 (including the trailing NUL g_utf8_to_utf16 always writes,
     * per its own documented contract) is guaranteed <= cap by the
     * conservative byte-length check above. */
    memcpy(dst, conv, (size_t)(written + 1) * sizeof(uint16_t));
    g_free(conv);
    return TRUE;
}

/* Fills one struct wv2l_cookie (the wire format) from a real SoupCookie* --
 * ported verbatim from the original's fill_unix_cookie, retargeted at the
 * wire struct directly (see navigate.h's own comment on cookies_get for
 * why). Returns FALSE (leaving dst only partially filled -- caller must not
 * use it) if any string field doesn't fit its fixed buffer. */
static gboolean fill_wire_cookie(struct wv2l_cookie *dst, SoupCookie *cookie)
{
    GDateTime *expires;

    if (!copy_field_or_fail(soup_cookie_get_name(cookie), dst->name, WV2L_COOKIE_NAME_MAX, "name") ||
        !copy_field_or_fail(soup_cookie_get_value(cookie), dst->value, WV2L_COOKIE_VALUE_MAX, "value") ||
        !copy_field_or_fail(soup_cookie_get_domain(cookie), dst->domain, WV2L_COOKIE_DOMAIN_MAX, "domain") ||
        !copy_field_or_fail(soup_cookie_get_path(cookie), dst->path, WV2L_COOKIE_PATH_MAX, "path"))
        return FALSE;

    /* NULL expires == session cookie, real libsoup semantics (soup-cookie.c's
     * own doc comment) and exactly real ICoreWebView2Cookie::IsSession's own
     * signal -- -1.0/TRUE is real WebView2's own documented sentinel for
     * "this is a session cookie" (learn.microsoft.com's real
     * ICoreWebView2Cookie reference: "The default is -1.0, which means
     * cookies are session cookies by default."), not a placeholder. */
    if ((expires = soup_cookie_get_expires(cookie)))
    {
        dst->expires = (double)g_date_time_to_unix(expires);
        dst->is_session = FALSE;
    }
    else
    {
        dst->expires = -1.0;
        dst->is_session = TRUE;
    }

    dst->is_http_only = soup_cookie_get_http_only(cookie) ? TRUE : FALSE;
    dst->is_secure = soup_cookie_get_secure(cookie) ? TRUE : FALSE;
    /* SoupSameSitePolicy's 3 values are numerically identical to
     * COREWEBVIEW2_COOKIE_SAME_SITE_KIND's (both verified against their
     * real headers, per the original's own comment) -- passed straight
     * through, no translation table needed. */
    dst->same_site = (int32_t)soup_cookie_get_same_site_policy(cookie);
    return TRUE;
}

struct get_cookies_ctx
{
    GMainLoop *loop; /* see struct delete_cookies_ctx's identical field */
    guint timeout_id;
    gboolean done;
    gboolean filtered; /* mirrors the original's (ctx->uri_utf8 != NULL)
                         * check -- whether to call
                         * webkit_cookie_manager_get_cookies_finish
                         * (filtered) or webkit_cookie_manager_
                         * get_all_cookies_finish (unfiltered) in
                         * on_get_cookies_done. The original duplicated
                         * uri_utf8 itself into the ctx for this same
                         * purpose (freed in get_cookies_ctx_release); a
                         * plain flag suffices here since nothing else in
                         * this ctx needs the string's actual value once
                         * the get_cookies()/get_all_cookies() call itself
                         * has already been issued. */
    int refs; /* same "whichever side finishes last frees ctx" pattern as
               * struct delete_cookies_ctx above -- see this file's own top
               * comment for why this is still required unchanged. */

    uint32_t offset; /* in; first cookie of the requested page -- see
                       * on_get_cookies_done's own paging comment */

    /* out -- written only by on_get_cookies_done, read only by
     * cookies_get itself after ctx->done is observed true (see this
     * struct's own leading comment; no lock needed, both sides run on this
     * process's single serialized thread, just at different points in
     * time). */
    gboolean success;
    uint32_t count;
    uint32_t total;
    struct wv2l_cookie cookies[WV2L_MAX_COOKIES];
};

static void get_cookies_ctx_release(struct get_cookies_ctx *ctx)
{
    if (--ctx->refs) return;
    free(ctx);
}

static void on_get_cookies_done(GObject *source, GAsyncResult *res, void *user_data)
{
    struct get_cookies_ctx *ctx = user_data;
    GList *cookies, *l;
    uint32_t total = 0, n = 0, skip;
    gboolean success;

    if (ctx->filtered)
        cookies = webkit_cookie_manager_get_cookies_finish((WebKitCookieManager *)source, res, NULL);
    else
        cookies = webkit_cookie_manager_get_all_cookies_finish((WebKitCookieManager *)source, res, NULL);

    /* Count the real list length FIRST (a second pass, but a cheap one --
     * just pointer-chasing, no allocation) so the caller learns the true size
     * of the store, not just how much of it fits in one wire buffer. */
    for (l = cookies; l; l = l->next) total++;

    /* Paging, rather than the old "more than WV2L_MAX_COOKIES cookies => fail
     * the whole call". That cap was reachable in ordinary use: Roblox Studio's
     * own clearAllCookiesAndRunCallbackHelper calls GetCookies UNFILTERED,
     * which enumerates every cookie in the profile across every domain, and a
     * long-lived profile passes 128 easily -- at which point cookie clearing
     * failed before it could delete anything. The wire struct still carries at
     * most WV2L_MAX_COOKIES entries; the caller asks for the next window by
     * offset and reassembles the whole list on its side.
     *
     * Honest limitation: the store is enumerated fresh per page, so a cookie
     * added or removed between pages can be seen twice or missed. Bounded and
     * benign for the flow this exists to serve (deleting everything: a missed
     * cookie survives one round, a repeated one deletes once and then no-ops),
     * and the alternative -- holding a snapshot across calls -- would put real
     * cross-call state in this process for no proportionate gain. */
    success = TRUE;
    skip = ctx->offset;
    for (l = cookies; l; l = l->next)
    {
        if (skip) { skip--; continue; }
        if (n >= WV2L_MAX_COOKIES) break; /* rest of the store comes on the next page */
        /* fill_wire_cookie itself can still reject an individual
         * cookie whose field(s) don't fit -- that cookie is dropped
         * (loudly) rather than failing the whole call, same asymmetry
         * as the original: a single oversized field isn't something
         * clearAllCookiesAndRunCallbackHelper's enumerate-then-act
         * semantics can be silently wrong about the same way the
         * total-count cap used to be. */
        if (fill_wire_cookie(&ctx->cookies[n], l->data)) n++;
    }

    if (total > WV2L_MAX_COOKIES)
        fprintf(stderr, "webview2loader-host: cookie store holds %u cookies -- returning %u from "
                        "offset %u; caller pages for the rest\n",
                        (unsigned)total, (unsigned)n, (unsigned)ctx->offset);

    if (cookies) g_list_free_full(cookies, (GDestroyNotify)soup_cookie_free);

    ctx->success = success;
    ctx->count = n;
    ctx->total = total;
    ctx->done = TRUE;
    if (ctx->loop) g_main_loop_quit(ctx->loop);

    get_cookies_ctx_release(ctx); /* this callback's own ref */
}

static gboolean on_get_cookies_timeout(gpointer data)
{
    struct get_cookies_ctx *ctx = data;

    ctx->timeout_id = 0;
    if (ctx->loop) g_main_loop_quit(ctx->loop);
    return G_SOURCE_REMOVE;
}

gboolean cookies_get(struct native_webview *nv, const char *uri_utf8, struct wv2l_get_cookies_params *out)
{
    struct get_cookies_ctx *ctx;
    WebKitNetworkSession *session;
    WebKitCookieManager *mgr;
    const char *filter;

    out->success = FALSE;
    out->count = 0;
    out->total = 0;
    if (!nv)
    {
        fprintf(stderr, "webview2loader-host: stale/destroyed native window handle -- skipping "
                        "GetCookies\n");
        return FALSE;
    }
    /* Heap-allocated (not a stack local): this ctx embeds the same
     * WV2L_MAX_COOKIES-sized cookie array (~1.2MB) as the wire struct
     * itself -- same "too big for a thread stack, and needs to survive a
     * possible post-timeout callback anyway" reasoning as the original's
     * struct get_cookies_ctx. */
    if (!(ctx = calloc(1, sizeof(*ctx))))
    {
        fprintf(stderr, "webview2loader-host: calloc failed for GetCookies context -- out of memory, "
                        "failing without waiting\n");
        return FALSE;
    }
    ctx->refs = 2;
    ctx->offset = out->offset; /* caller-driven paging -- see on_get_cookies_done */

    /* NULL/empty uri => unfiltered, matching real GetCookies semantics --
     * see this section's own leading comment. */
    filter = (uri_utf8 && uri_utf8[0]) ? uri_utf8 : NULL;
    ctx->filtered = filter != NULL;

    session = webkit_web_view_get_network_session(nv->view);
    mgr = webkit_network_session_get_cookie_manager(session);
    if (filter)
        webkit_cookie_manager_get_cookies(mgr, filter, NULL, on_get_cookies_done, ctx);
    else
        webkit_cookie_manager_get_all_cookies(mgr, NULL, on_get_cookies_done, ctx);

    /* Bounded wait, not indefinite -- same rationale and same 10s bound
     * (verbatim) as cookies_delete_all/cookies_count above (local
     * cookie-store enumeration, not a real network round-trip). */
    ctx->loop = g_main_loop_new(NULL, FALSE);
    /* Cosmetic web-process-terminated mitigation -- see navigate_and_wait's
     * identical comment above and webview.h's own comment on
     * active_wait_loop. */
    nv->active_wait_loop = ctx->loop;
    ctx->timeout_id = g_timeout_add_seconds(10, on_get_cookies_timeout, ctx);
    g_main_loop_run(ctx->loop);
    wait_ended_with_nv_alive(nv, "GetCookies");

    if (ctx->timeout_id) g_source_remove(ctx->timeout_id);
    g_main_loop_unref(ctx->loop);
    ctx->loop = NULL;

    /* Only trust ctx->success/ctx->count/ctx->cookies if the callback
     * actually ran (ctx->done true) -- mirrors cookies_count's own
     * identical "don't read fields the callback may not have written yet"
     * guard above. This is also what makes it safe for on_get_cookies_done
     * to write straight into ctx at any time (even long after a real
     * timeout, per this file's own top comment): `out` (the caller's own
     * wire struct) is only ever written HERE, after this check -- never by
     * the callback itself. */
    if (ctx->done)
    {
        out->success = ctx->success;
        out->count = ctx->count;
        out->total = ctx->total;
        if (ctx->success) memcpy(out->cookies, ctx->cookies, ctx->count * sizeof(*ctx->cookies));
    }

    get_cookies_ctx_release(ctx); /* this function's own ref -- safe
                                    * unconditionally, same as
                                    * cookies_delete_all's own identical
                                    * release call. */
    return out->success;
}

/* --- Cookies: delete one specific cookie ---
 *
 * Real ICoreWebView2CookieManager::DeleteCookie, which Roblox Studio's own
 * clearAllCookiesAndRunCallbackHelper calls once per cookie after enumerating
 * them with GetCookies. It was an E_NOTIMPL stub, so every one of those calls
 * failed -- visible in a real Studio log as
 * "Failed DeleteCookie with error code '-2147467263'" (0x80004001 == E_NOTIMPL).
 * DeleteAllCookies was implemented all along, but Studio never calls it.
 *
 * Reconstructs a SoupCookie from the four fields the wire carries and hands it
 * to webkit_cookie_manager_delete_cookie. That is enough to match, and all four
 * are genuinely needed -- verified against real source rather than assumed:
 * WebKitCookieManager.cpp turns the SoupCookie into a WebCore::Cookie and calls
 * NetworkStorageSession::deleteCookie, which is a straight
 * soup_cookie_jar_delete_cookie; libsoup 3.6.5 looks the DOMAIN up in its hash,
 * then matches within it using soup_cookie_equal, which compares NAME, VALUE
 * and PATH. A name/domain/path-only delete would match nothing at all.
 *
 * Two consequences worth knowing, neither of which affects Studio's flow:
 *  - real WebView2 matches on name/domain/path and ignores value, so a caller
 *    that mutated the cookie's value (via put_Value) before deleting it would
 *    get a no-op here where Windows would delete. Studio's clear-cookies path
 *    passes back exactly the objects GetCookies handed it, unmodified.
 *  - a cookie whose fields exceeded the wire caps was already dropped by
 *    fill_wire_cookie during enumeration, so it is never offered for deletion
 *    in the first place -- consistent, if incomplete, in both directions.
 *
 * Fire-and-forget, like cookies_delete_all's own per-cookie deletes: the wire
 * struct has no out field, and webkit_cookie_manager_delete_cookie's completion
 * carries no information this side could act on. */
/* Mirror of cookies_delete_one below, for the write direction. See
 * wv2l_add_cookie_params in the protocol header for why Studio needs this.
 *
 * Unlike the delete path, expiry matters here: an auth cookie written as a
 * session cookie would be dropped the moment WebKit's session ends, and
 * .ROBLOSECURITY is long-lived. `expires` is carried as a unix timestamp on the
 * wire (0 == session cookie, which is what real WebView2 means by IsSession). */
gboolean cookies_add_or_update(struct native_webview *nv, const struct wv2l_cookie *wire)
{
    WebKitNetworkSession *session;
    WebKitCookieManager *mgr;
    SoupCookie *cookie;
    char *name, *value, *domain, *path;
    gboolean ok = FALSE;

    if (!nv)
    {
        fprintf(stderr, "webview2loader-host: stale/destroyed native window handle -- skipping "
                        "AddOrUpdateCookie\n");
        return FALSE;
    }

    name   = wire_uri_to_utf8(wire->name);
    value  = wire_uri_to_utf8(wire->value);
    domain = wire_uri_to_utf8(wire->domain);
    path   = wire_uri_to_utf8(wire->path);

    if (!name || !value || !domain || !path)
    {
        fprintf(stderr, "webview2loader-host: AddOrUpdateCookie: UTF-16 -> UTF-8 conversion failed for "
                        "one or more cookie fields -- not attempting the write\n");
        goto out;
    }

    if (!(cookie = soup_cookie_new(name, value, domain, path, -1)))
    {
        fprintf(stderr, "webview2loader-host: AddOrUpdateCookie: soup_cookie_new failed\n");
        goto out;
    }

    if (!wire->is_session && wire->expires > 0)
    {
        GDateTime *when = g_date_time_new_from_unix_utc((gint64)wire->expires);
        if (when)
        {
            soup_cookie_set_expires(cookie, when);
            g_date_time_unref(when);
        }
    }
    soup_cookie_set_secure(cookie, wire->is_secure ? TRUE : FALSE);
    soup_cookie_set_http_only(cookie, wire->is_http_only ? TRUE : FALSE);

    session = webkit_web_view_get_network_session(nv->view);
    mgr = webkit_network_session_get_cookie_manager(session);
    webkit_cookie_manager_add_cookie(mgr, cookie, NULL, NULL, NULL);
    /* Caller-owned, same ownership rule as delete_cookie below. */
    soup_cookie_free(cookie);
    ok = TRUE;
    fprintf(stderr, "webview2loader-host: AddOrUpdateCookie: set '%s' for domain '%s' on nv=%p\n",
            name, domain, (void *)nv);

out:
    g_free(name);
    g_free(value);
    g_free(domain);
    g_free(path);
    return ok;
}

gboolean cookies_delete_one(struct native_webview *nv, const struct wv2l_cookie *wire)
{
    WebKitNetworkSession *session;
    WebKitCookieManager *mgr;
    SoupCookie *cookie;
    char *name, *value, *domain, *path;
    gboolean ok = FALSE;

    if (!nv)
    {
        fprintf(stderr, "webview2loader-host: stale/destroyed native window handle -- skipping "
                        "DeleteCookie\n");
        return FALSE;
    }

    name   = wire_uri_to_utf8(wire->name);
    value  = wire_uri_to_utf8(wire->value);
    domain = wire_uri_to_utf8(wire->domain);
    path   = wire_uri_to_utf8(wire->path);

    /* soup_cookie_new g_return_val_if_fail's on a NULL name or value (and warns
     * on a NULL domain), so a conversion failure has to stop here rather than
     * be passed through. */
    if (!name || !value || !domain || !path)
    {
        fprintf(stderr, "webview2loader-host: DeleteCookie: UTF-16 -> UTF-8 conversion failed for one "
                        "or more cookie fields -- not attempting the delete\n");
        goto out;
    }

    /* max_age -1 == a session cookie. Irrelevant to the match (soup_cookie_equal
     * never looks at expiry) but it has to be something, and -1 is the one value
     * that invents no expiry date. */
    if (!(cookie = soup_cookie_new(name, value, domain, path, -1)))
    {
        fprintf(stderr, "webview2loader-host: DeleteCookie: soup_cookie_new failed\n");
        goto out;
    }

    session = webkit_web_view_get_network_session(nv->view);
    mgr = webkit_network_session_get_cookie_manager(session);
    webkit_cookie_manager_delete_cookie(mgr, cookie, NULL, NULL, NULL);
    /* delete_cookie's cookie argument is caller-owned (webkitgtk.org: "the data
     * is owned by the caller of the method"), same ownership rule
     * cookies_delete_all's own per-cookie deletes rely on -- so this side frees
     * it, and only after the call has taken what it needs from it. */
    soup_cookie_free(cookie);
    ok = TRUE;

out:
    g_free(name);
    g_free(value);
    g_free(domain);
    g_free(path);
    return ok;
}

/* Packs and sends a WV2L_EV_NAVIGATION_STARTING event. Returns 0 if the Wine
 * side received the whole frame, -1 otherwise (no event channel, peer gone,
 * short write) -- callers must treat -1 as "Studio was not told".
 *
 * A URI too long for the wire buffer is a failure, not a truncation: half an
 * OAuth URI silently delivered would strip the auth code and leave Studio
 * looking like it simply ignored the login, which is exactly the failure mode
 * this whole change exists to remove. Failing sends it down the xdg-open
 * fallback instead, which handles arbitrary lengths via the command line. */
static int event_send_navigation_starting(struct native_webview *nv, const char *uri_utf8)
{
    struct wv2l_ev_navigation_starting_params ev;
    glong written = 0;
    gunichar2 *utf16;
    int rc = -1;

    memset(&ev, 0, sizeof(ev));
    ev.handle = (uint64_t)(uintptr_t)nv;
    ev.is_redirect = 0; /* this fires from decide-policy on a real navigation
                          * action; WebKit's own redirect flag is not plumbed
                          * through yet and real WebView2 hosts tolerate FALSE */

    if (!(utf16 = g_utf8_to_utf16(uri_utf8, -1, NULL, &written, NULL)))
    {
        fprintf(stderr, "webview2loader-host: NavigationStarting: UTF-8 -> UTF-16 conversion failed\n");
        return -1;
    }
    if (written >= 0 && (size_t)written < WV2L_URI_MAX)
    {
        memcpy(ev.uri, utf16, (size_t)(written + 1) * sizeof(uint16_t));
        rc = ipc_send_event(WV2L_EV_NAVIGATION_STARTING, &ev, sizeof(ev));
    }
    else
    {
        fprintf(stderr, "webview2loader-host: NavigationStarting: URI is %ld UTF-16 units, over this "
                        "build's %d cap -- not truncating it into a useless auth code\n",
                written, (int)WV2L_URI_MAX);
    }
    g_free(utf16);
    return rc;
}

/* Page -> Studio web message, over the same one-way event channel
 * NavigationStarting uses. Fire-and-forget for the same reason: the helper's
 * main loop must never block waiting on the Wine side.
 *
 * Rejects an oversized payload rather than truncating it -- a cut-off JSON
 * document is not JSON, and Studio would fail to parse it in a way that looks
 * like a page bug rather than a transport limit. */
int event_send_web_message(struct native_webview *nv, const char *payload_utf8, const char *source_utf8,
                            int is_string)
{
    struct wv2l_ev_web_message_header hdr;
    gunichar2 *msg16 = NULL, *src16 = NULL;
    glong msg_len = 0, src_len = 0;
    int rc = -1;

    if (!nv || !payload_utf8) return -1;
    memset(&hdr, 0, sizeof(hdr));
    hdr.handle = (uint64_t)(uintptr_t)nv;
    hdr.is_string = is_string ? 1 : 0;

    if (!(msg16 = g_utf8_to_utf16(payload_utf8, -1, NULL, &msg_len, NULL)))
    {
        fprintf(stderr, "webview2loader-host: web message: UTF-8 -> UTF-16 conversion failed\n");
        goto done;
    }
    if (msg_len < 0 || (size_t)msg_len >= WV2L_WEB_MESSAGE_MAX)
    {
        fprintf(stderr, "webview2loader-host: web message is %ld UTF-16 units, over this build's %d "
                        "cap -- dropping rather than delivering a truncated payload\n",
                msg_len, (int)WV2L_WEB_MESSAGE_MAX);
        goto done;
    }
    hdr.message_len = (uint32_t)msg_len;

    /* Source is best-effort: a missing or oversized URI is not worth dropping a
     * message the Toolbox is waiting on -- it degrades to an empty string. */
    if (source_utf8 && (src16 = g_utf8_to_utf16(source_utf8, -1, NULL, &src_len, NULL)))
    {
        if (src_len >= 0 && (size_t)src_len < WV2L_URI_MAX)
            memcpy(hdr.source, src16, (size_t)(src_len + 1) * sizeof(uint16_t));
    }

    /* Header then payload, as two writes on the same stream -- safe for the
     * same reason ipc_send_event's own two writes are: this process is the only
     * writer and writes only from the main loop thread, so frames cannot
     * interleave. Sending only message_len units instead of a fixed 128 KB
     * buffer is what keeps the socket from filling; see the header's comment. */
    rc = ipc_send_event_payload(WV2L_EV_WEB_MESSAGE, &hdr, sizeof(hdr),
                                 msg16, (size_t)msg_len * sizeof(uint16_t));

done:
    g_free(msg16);
    g_free(src16);
    return rc;
}

/* --- OAuth redirect handoff ---
 *
 * Ported VERBATIM from the original (see this file's own top comment) --
 * real, committed, independently trace-verified logic (commit ac3634ea6),
 * not touched beyond the mechanical p_-prefix drop. */

/* Strips this bundle's own runtime environment before exec'ing xdg-open.
 *
 * This process inherits Wine's environment and then has unixlib.c's
 * set_webkit_relocation_env()/spawn_helper() point a dozen loader variables at
 * the TuxBlox WebKit bundle -- LD_LIBRARY_PATH at the gl-fallback llvmpipe
 * copy, GIO_EXTRA_MODULES, GSETTINGS_SCHEMA_DIR, XDG_DATA_DIRS, the GStreamer
 * plugin paths and (as of this change) FONTCONFIG_PATH plus the GDK_PIXBUF and
 * LIBGL variables, all pointed at the bundle's own
 * copies. All of that is correct for WebKit in THIS process and actively
 * hostile to whatever xdg-open launches: the user's real browser would be told
 * to load our software-only libEGL/libGL ahead of the system's, resolve GIO
 * modules and GSettings schemas out of our tree, and force
 * LIBGL_ALWAYS_SOFTWARE. Chromium- and Firefox-family browsers routinely fail
 * to start under exactly that -- which is what "Login via Browser does
 * nothing" looks like from the outside.
 *
 * unsetenv() only, never a hardcoded replacement value: the correct value for
 * the user's own session is whatever their session already had, and anything
 * this process could substitute would be a guess. Variables the bundle only
 * ever PREPENDS to (XDG_DATA_DIRS, GST_PLUGIN_SYSTEM_PATH_1_0,
 * LD_LIBRARY_PATH) lose the session's own entries along with ours; that is
 * still strictly better than handing the browser our loader paths, and the
 * defaults every one of them falls back to are the session defaults.
 *
 * Runs in the grandchild, after the second fork() and immediately before
 * execvp(), so nothing here can disturb this process's own environment. */
static void xdg_open_sanitize_env(void)
{
    static const char *const bundle_vars[] = {
        "LD_LIBRARY_PATH",
        "WEBKIT_EXEC_PATH", "WEBKIT_INJECTED_BUNDLE_PATH",
        "GIO_EXTRA_MODULES", "GBM_BACKENDS_PATH", "GSETTINGS_SCHEMA_DIR",
        "GST_PLUGIN_SCANNER", "GST_PLUGIN_SYSTEM_PATH", "GST_PLUGIN_SYSTEM_PATH_1_0",
        "XDG_DATA_DIRS",
        "FONTCONFIG_PATH", "GDK_PIXBUF_MODULE_FILE", "GDK_PIXBUF_MODULEDIR",
        "LIBGL_DRIVERS_PATH", "LIBGL_ALWAYS_SOFTWARE",
        "GDK_BACKEND", "GSK_RENDERER",
        /* Not loader paths, but they name THIS process's private IPC fds and
         * would be a lie in any other process's environment. */
        "WEBVIEW2LOADER_IPC_FD", "WEBVIEW2LOADER_EVENT_FD",
    };
    size_t i;

    for (i = 0; i < sizeof(bundle_vars) / sizeof(bundle_vars[0]); i++)
        unsetenv(bundle_vars[i]);
}

/* Handles a roblox-studio-auth:/roblox-studio:/roblox-player: navigation by
 * handing it off to the OS's own xdg-open, rather than letting WebKit try
 * (and fail, showing its own generic "URL can't be shown" error) to load an
 * external URI scheme it doesn't itself recognize -- letting this
 * codebase's own already-installed xdg-mime registrations
 * (install-handler.sh / launcher/src/desktop_integration.cpp --
 * x-scheme-handler/roblox-studio-auth, -studio, -player) complete the
 * hand-off to a fresh RobloxStudioBeta.exe, exactly reproducing the
 * OS-level custom-URI-protocol activation real, unmodified Roblox Studio
 * relies on (confirmed via a real Vinegar session log -- see the original
 * task's own brief). fork()+execvp() with an explicit argv array -- never
 * system()/a shell string -- so the URI's own &/=/? characters survive
 * completely intact rather than being shell-word-split/glob-expanded.
 *
 * Uses this codebase's own established double-fork pattern (see
 * dlls/ntdll/unix/process.c's __wine_unix_spawnvp, "in child"/
 * "in grandchild" comments): the outer child forks a grandchild that
 * execvp()s xdg-open and immediately _exit()s itself, so the grandchild
 * (the real, possibly long-lived xdg-open process) gets reparented to
 * init/systemd rather than staying a child of this process -- avoids
 * needing a persistent SIGCHLD handler or an indefinite wait to prevent a
 * zombie across what could be many logins over a long Studio session. The
 * outer child's own exit is waited on synchronously below, but since it
 * exits immediately after its own inner fork() call, this is a bounded,
 * near-instant wait, not a real block on xdg-open itself finishing. */
static void xdg_open_handoff(const char *uri)
{
    pid_t pid = fork();

    if (pid == 0)
    {
        /* in child */
        pid_t pid2 = fork();
        if (pid2 == 0)
        {
            /* in grandchild -- becomes the real, long-lived xdg-open
             * process once reparented away from this process. */
            char *argv[] = { (char *)"xdg-open", (char *)uri, NULL };
            xdg_open_sanitize_env();
            execvp("xdg-open", argv);
            _exit(127); /* only reached if execvp() itself failed */
        }
        _exit(0); /* child's only job was forking the grandchild -- exits
                   * immediately regardless of whether that inner fork
                   * itself succeeded, so the parent's waitpid below never
                   * blocks on xdg-open's own real runtime. */
    }
    if (pid > 0)
    {
        int status;
        if (waitpid(pid, &status, 0) < 0)
            fprintf(stderr, "webview2loader-host: waitpid on xdg-open handoff child failed for %s: %s\n",
                    uri, strerror(errno));
    }
    else
        fprintf(stderr, "webview2loader-host: fork() failed for xdg-open handoff of %s: %s\n",
                uri, strerror(errno));
}

/* Deliberately narrow, prefix-only allowlist -- NOT requiring a "://"
 * authority form. The real, observed Roblox OAuth redirect is single-slash
 * (roblox-studio-auth:/?code=...&state=..., confirmed via two independent
 * real captures -- see the original task's own brief); a strict "://" match
 * would silently miss the one URI this fix exists to handle. Matches
 * exactly this codebase's own existing xdg-mime scheme registrations,
 * nothing broader. */
static const char *const known_roblox_schemes[] = {
    "roblox-studio-auth:", "roblox-studio:", "roblox-player:",
};

/* WebKitWebView::decide-policy -- connected once per webview at creation
 * time (webview.c's webview_create, Task 4's file), not scoped to a single
 * navigate_and_wait() call. This has to be persistent for the whole
 * webview's lifetime: the roblox-studio-auth: redirect this exists to
 * handle is entirely browser-internal (WebKit's own OAuth flow navigating
 * there after the user submits credentials on the real Roblox login page),
 * never itself the direct target of a PE-side Navigate() call, so a
 * connection scoped to one navigate_and_wait() call would never even still
 * be alive when the redirect actually fires.
 *
 * Only WEBKIT_POLICY_DECISION_TYPE_NAVIGATION_ACTION is inspected, and only
 * the three known Roblox custom schemes within it are intercepted -- the
 * original investigation's own round-16 cautionary history (see
 * .superpowers/sdd/2026-08-13-webview2-window-docking-messaging/progress.md):
 * an earlier, reverted attempt denylisted too broadly and broke WebKit's own
 * internal about:blank init. Every other decision type
 * (NEW_WINDOW_ACTION, RESPONSE) and every non-matching scheme under
 * NAVIGATION_ACTION falls through to returning FALSE -- WebKit's own
 * documented default of proceeding with webkit_policy_decision_use(), i.e.
 * completely unmodified behavior for real http/https page loads and
 * everything else WebKit needs to load normally. */
gboolean on_decide_policy(WebKitWebView *view, WebKitPolicyDecision *decision,
                           WebKitPolicyDecisionType decision_type, void *user_data)
{
    WebKitNavigationAction *action;
    WebKitURIRequest *request;
    const char *uri;
    size_t i;

    (void)view;
    (void)user_data;

    if (decision_type != WEBKIT_POLICY_DECISION_TYPE_NAVIGATION_ACTION &&
        decision_type != WEBKIT_POLICY_DECISION_TYPE_NEW_WINDOW_ACTION)
        return FALSE;

    action = webkit_navigation_policy_decision_get_navigation_action((WebKitNavigationPolicyDecision *)decision);
    if (!action) return FALSE;
    request = webkit_navigation_action_get_request(action);
    if (!request) return FALSE;
    uri = webkit_uri_request_get_uri(request);
    if (!uri) return FALSE;

    /* "Login via Browser" on the Roblox authorize page opens its target in a new
     * window (target="_blank"/window.open), which WebKit reports as a SEPARATE
     * decision type from an ordinary navigation. Before this branch existed,
     * NEW_WINDOW_ACTION fell straight through to `return FALSE` -- WebKit's
     * default use(), which then emits WebKitWebView::create, which nothing in
     * this process connects, so the default handler returned NULL and the popup
     * was silently dropped. The button was a dead click: no browser, no error,
     * no log line.
     *
     * On Windows this is WebView2's NewWindowRequested event, and Studio's own
     * handler shells out to the system browser. Our shim accepts
     * add_NewWindowRequested (webview.c's webview_generic_add_event) but only
     * ever records the handler -- the event channel carries NavigationStarting
     * and nothing else -- so routing it to Studio is not available here.
     * xdg-open reaches the same destination Studio's Windows handler would.
     *
     * Deliberate divergence from WebView2's documented default (open a new
     * WebView2 window): this webview is XReparentWindow'd into one specific
     * Studio dialog and has no concept of a second window to open. Sending the
     * URI to the user's real browser is both what this particular button means
     * and strictly better than the silent drop it replaces. A known Roblox
     * scheme still falls through to the in-process NavigationStarting path
     * below -- those must NOT go to the browser, they are Studio's own
     * OAuth callback. */
    if (decision_type == WEBKIT_POLICY_DECISION_TYPE_NEW_WINDOW_ACTION &&
        strncmp(uri, "http://", 7) && strncmp(uri, "https://", 8))
    {
        /* Not http(s) and not handled below -- WebKit cannot open it in a new
         * window either, so suppress rather than let it error out visibly. */
        for (i = 0; i < G_N_ELEMENTS(known_roblox_schemes); i++)
            if (!strncmp(uri, known_roblox_schemes[i], strlen(known_roblox_schemes[i])))
                break;
        if (i == G_N_ELEMENTS(known_roblox_schemes))
        {
            fprintf(stderr, "webview2loader-host: suppressing new-window request for unsupported "
                            "scheme in %.64s...\n", uri);
            webkit_policy_decision_ignore(decision);
            return TRUE;
        }
    }
    else if (decision_type == WEBKIT_POLICY_DECISION_TYPE_NEW_WINDOW_ACTION)
    {
        fprintf(stderr, "webview2loader-host: new-window request for %.64s... -- handing to the "
                        "system browser via xdg-open\n", uri);
        webkit_policy_decision_ignore(decision);
        xdg_open_handoff(uri);
        return TRUE;
    }

    for (i = 0; i < G_N_ELEMENTS(known_roblox_schemes); i++)
    {
        if (!strncmp(uri, known_roblox_schemes[i], strlen(known_roblox_schemes[i])))
        {
            struct native_webview *nv = user_data;

            /* The load is suppressed either way -- WebKit cannot render these
             * schemes and would show its own generic "URL can't be shown"
             * error. What changed is where the URI goes afterwards. */
            webkit_policy_decision_ignore(decision);

            /* Preferred path: hand the URI to Studio's own NavigationStarting
             * handler, which is what happens on Windows. Studio registers that
             * handler before it ever navigates, and its login flow completes
             * from the code in this exact URI -- in-process, with no second
             * Studio and no OS round trip.
             *
             * Established by measurement: a probe build injected a real
             * window.chrome.webview shim and logged every postMessage the page
             * made. It made none across a full login, ruling out the
             * web-message channel and leaving NavigationStarting as the only
             * in-process route by which Studio can learn this URI. */
            if (nv && event_send_navigation_starting(nv, uri) == 0)
            {
                fprintf(stderr, "webview2loader-host: intercepting navigation to %.64s... -- delivered "
                                "to Studio's own NavigationStarting handler\n", uri);
                return TRUE; /* GDK_EVENT_STOP -- we handled this decision ourselves */
            }

            /* Fallback, unchanged from before the event channel existed: shell
             * out so the OS re-launches Studio with the URI on its command
             * line. Strictly worse (it starts a second Studio, and the
             * original has historically ignored the forwarded token), but it
             * is what shipped, so it stays as the path taken whenever the
             * event could not be delivered -- no event channel, a Wine side
             * that has gone away, a broken frame. Never both: reaching here
             * means Studio was NOT told, so there is nothing to double up. */
            fprintf(stderr, "webview2loader-host: intercepting navigation to %.64s... -- could not "
                            "reach Studio's NavigationStarting handler, falling back to xdg-open\n", uri);
            xdg_open_handoff(uri);
            return TRUE;
        }
    }
    return FALSE; /* not one of ours -- let WebKit's own default (use()) handle it */
}

/* --- WebKitWebView::web-process-terminated (cosmetic mitigation) ---
 *
 * 2026-08-15: a real, fully investigated (root-caused via coredumpctl,
 * reproduced 4 times) crash exists where WebKitGTK's own WebKitWebProcess
 * child -- spawned internally by libwebkitgtk-6.0.so.4 itself, never
 * directly forked/exec'd by this file -- can SIGSEGV during its OWN
 * shutdown, from a genuine race inside WebKitGTK's Skia GPU backend
 * colliding with NVIDIA's proprietary driver during GPU-context teardown
 * (two threads independently tearing down the same driver-global GL/EGL
 * state at once). Every frame in both racing stacks sits inside WebKitGTK,
 * glibc, or the NVIDIA driver -- confirmed NOT a bug in this file, this
 * process, or any other TuxBlox code. This is upstream WebKitGTK/NVIDIA's
 * own bug to fix; this handler does not attempt that (explicitly out of
 * scope -- see this task's own report at
 * .superpowers/sdd/2026-08-14-webview2loader-host-process/
 * webprocess-terminated-mitigation-report.md for the full writeup,
 * including why blanket coredump suppression was deliberately NOT added
 * here). What follows is a purely cosmetic mitigation: react to the real
 * WebKitWebView::web-process-terminated signal so this helper's own state
 * degrades honestly instead of assuming a dead content process is still
 * alive, without touching the actual upstream race.
 *
 * Real signal verified directly against this bundle's own built WebKitGTK
 * 2.52.5 (WEBKITGTK_VERSION in versions.env), not guessed: the installed
 * $webkitgtk-prefix/include/webkitgtk-6.0/webkit/WebKitWebView.h vtable
 * entry (`void (*web_process_terminated)(WebKitWebView*,
 * WebKitWebProcessTerminationReason)`) and the g_signal_new() call in this
 * exact version's own Source/WebKit/UIProcess/API/glib/WebKitWebView.cpp
 * (RUN_LAST, no accumulator, g_cclosure_marshal_VOID__ENUM, one
 * WEBKIT_TYPE_WEB_PROCESS_TERMINATION_REASON arg -- void return, unlike
 * decide-policy's gboolean) together fix this function's exact signature.
 * WebKitWebProcessTerminationReason's 3 real values (same header):
 * WEBKIT_WEB_PROCESS_CRASHED, WEBKIT_WEB_PROCESS_EXCEEDED_MEMORY_LIMIT,
 * WEBKIT_WEB_PROCESS_TERMINATED_BY_API. This project's own crash (a real
 * SIGSEGV) maps to WEBKIT_WEB_PROCESS_CRASHED.
 *
 * Deliberately does NOT call webkit_web_view_reload() to force a fresh
 * WebProcess -- weighed, not just skipped. Real evidence found both ways:
 * WebKitWebView.cpp's own WebKitNavigationClient::processDidTerminate
 * (Source/WebKit/UIProcess/API/glib/WebKitNavigationClient.cpp) returns
 * `true` (== WebCore's "handledByClient") unconditionally for Crash/
 * ExceededMemoryLimit/RequestedByClient, which permanently suppresses
 * WebPageProxy::dispatchProcessDidTerminate's own internal auto-reload
 * path for the whole GTK port -- confirming the GTK API's own design
 * deliberately hands this decision to the application, and that
 * webkit_web_view_reload() (or any later webkit_web_view_load_uri(), which
 * WebPageProxy::loadRequest already lazily relaunches a fresh WebProcess
 * for via launchProcess() when !hasRunningProcess() -- also verified
 * directly against this build's own WebPageProxy.cpp) IS a real, supported
 * way to recover. But WebKitGTK's own reference embedder (Tools/MiniBrowser)
 * does NOT call it from its webProcessTerminatedCallback either -- grepped
 * the whole MiniBrowser source; every real webkit_web_view_reload() call
 * there is wired to an explicit user-facing reload action/button, never
 * fired automatically on termination. Auto-reloading a Roblox Studio OAuth/
 * embedded webview out from under an in-progress flow the instant its
 * process dies carries real risk (reloading to a stale/blank state right
 * when the user least expects it) that this task's own brief doesn't ask
 * this fix to take on, and the reference embedder's own restraint here is
 * a real, on-point precedent for not forcing it. Today's real, understood
 * fallback -- a full Studio session restart -- stays exactly as it was;
 * this handler only makes the crash itself land more gracefully, not
 * silently invisible.
 *
 * Registry note: unlike webview_destroy, this handler must NEVER call
 * live_webview_unregister/free nv -- nv->window and nv->view (the
 * WebKitWebView GObject this very signal fired on) are both still alive;
 * only their underlying OS-level content *process* died. Removing nv from
 * the registry here would make every future webview_lookup(handle) for this
 * still-perfectly-valid handle spuriously fail, confusing every other
 * dispatch case in main.c for no real reason.
 *
 * UAF hardening (code review round 2, 2026-08-15): a normal Close()/
 * Release() on a webview calls webview_destroy() -> gtk_window_destroy(
 * nv->window) -> ... -> free(nv) -- and that same gtk_window_destroy() call
 * is itself what tears down that webview's own WebProcess (real teardown,
 * not a hypothetical). If THIS signal could fire asynchronously as a
 * consequence of that same teardown, AFTER webview_destroy() has already
 * run free(nv), user_data (nv) here would be a dangling pointer -- reading
 * nv->process_terminated/nv->active_wait_loop below would be a real
 * use-after-free, strictly worse than the bug this mitigation exists to
 * soften (a UAF here crashes this whole helper process, not just the
 * isolated WebKitWebProcess child).
 *
 * Checked against the real vendored WebKitGTK 2.52.5 source (not assumed):
 * this specific race is NOT reachable through the normal Close()/Release()
 * path, because the routing this signal depends on is severed
 * SYNCHRONOUSLY, strictly before gtk_window_destroy() returns (i.e. before
 * webview_destroy() ever reaches free(nv)) --
 *   1. Source/WebKit/UIProcess/API/gtk/WebKitWebViewBase.cpp,
 *      webkitWebViewBaseDispose() (webkit_web_view_base's own GObject
 *      dispose handler -- WebKitWebView's direct GTK parent type, per this
 *      build's own WEBKIT_DEFINE_TYPE(WebKitWebView, webkit_web_view,
 *      WEBKIT_TYPE_WEB_VIEW_BASE)): calls `webView->priv->pageProxy->
 *      close()` directly, synchronously, as part of dispose -- and GObject
 *      dispose/finalize for nv->view runs synchronously inside
 *      gtk_window_destroy()'s own widget-teardown call (nv->view's only ref
 *      is the one gtk_window_set_child sank into nv->window at creation
 *      time in webview.c -- nothing in this file takes a second ref -- so
 *      that ref hitting zero during gtk_window_destroy() drops it to zero
 *      and disposes/finalizes it then and there, not on some later main-loop
 *      iteration).
 *   2. Source/WebKit/UIProcess/WebPageProxy.cpp, WebPageProxy::close()
 *      (called by #1 above): line 1889, `m_navigationClient =
 *      makeUniqueRef<API::NavigationClient>()` -- replaces the real
 *      WebKitNavigationClient (whose processDidTerminate override is what
 *      calls webkitWebViewWebProcessTerminated() -> g_signal_emit(webView,
 *      signals[WEB_PROCESS_TERMINATED], ...), per WebKitNavigationClient.cpp)
 *      with a fresh, default API::NavigationClient, EARLY in close() --
 *      strictly before close()'s own later per-process teardown work runs.
 * Once that swap has happened, ANY later WebPageProxy::
 * dispatchProcessDidTerminate() for this page -- whether the crash already
 * happened, is happening concurrently, or hasn't happened yet -- calls
 * m_navigationClient->processDidTerminate() against the now-default
 * NavigationClient, never reaching webkitWebViewWebProcessTerminated()/
 * g_signal_emit() again for this WebKitWebView. So a real, on-point WebKit
 * source trace shows this signal cannot fire for THIS reason (this
 * webview's own destroy-triggered WebProcess teardown) after
 * webview_destroy() has moved past its own gtk_window_destroy() call --
 * i.e. never after free(nv).
 *
 * The registry check just below is added anyway, as cheap defense-in-depth
 * (not because the trace above found a real hole) -- it costs one
 * webview_lookup() call, matches this file's/webview.c's own established
 * Task 7 UAF-guard idiom exactly, and remains correct/harmless even if a
 * future WebKit version or a future change to this codebase's own object
 * lifetimes ever altered the synchronous ordering traced above. Safe by
 * construction even against a genuinely dangling nv: webview_lookup()
 * never dereferences its handle argument, only compares the raw pointer
 * VALUE against the live_webviews[] array (same reasoning that already
 * makes webview_lookup's own UAF guard safe elsewhere in this codebase). */
void on_web_process_terminated(WebKitWebView *view, WebKitWebProcessTerminationReason reason, void *user_data)
{
    struct native_webview *nv = user_data;
    const char *reason_str;

    switch (reason)
    {
    case WEBKIT_WEB_PROCESS_CRASHED:               reason_str = "CRASHED"; break;
    case WEBKIT_WEB_PROCESS_EXCEEDED_MEMORY_LIMIT:  reason_str = "EXCEEDED_MEMORY_LIMIT"; break;
    case WEBKIT_WEB_PROCESS_TERMINATED_BY_API:      reason_str = "TERMINATED_BY_API"; break;
    default:                                        reason_str = "UNKNOWN"; break;
    }

    fprintf(stderr, "webview2loader-host: WebKitWebView %p (native_webview %p) content process "
                    "terminated -- reason=%s. This is WebKitGTK's own internal WebKitWebProcess child, "
                    "not something this helper directly forked/exec'd; if reason=CRASHED, this is most "
                    "likely the known, understood WebKitGTK/NVIDIA Skia GPU-teardown race (see this "
                    "file's own top-of-section comment) -- degrading this webview's state gracefully "
                    "rather than assuming its content process is still alive.\n",
                    (void *)view, (void *)nv, reason_str);

    /* user_data is always nv, wired at connect time in webview.c's
     * webview_create -- but this is a real GObject signal callback that
     * could in principle be invoked with a NULL/garbage user_data by some
     * future connection this file doesn't control; never dereference on
     * faith. */
    if (!nv)
    {
        fprintf(stderr, "webview2loader-host: web-process-terminated fired with no native_webview "
                        "attached -- nothing to update, ignoring\n");
        return;
    }

    /* UAF guard (code review round 2) -- see this function's own top comment
     * for the full real-source trace showing this specific race is not
     * reachable today; kept as cheap defense-in-depth regardless, same
     * pattern webview_lookup()'s own doc comment already establishes
     * elsewhere in this codebase. webview_lookup() only compares nv's raw
     * pointer VALUE against the live_webviews[] array -- it never
     * dereferences nv itself, so this check is safe to run even if nv were
     * genuinely dangling, before anything below touches nv's fields for
     * real. */
    if (!webview_lookup((uint64_t)(uintptr_t)nv))
    {
        fprintf(stderr, "webview2loader-host: web-process-terminated fired for native_webview %p, which "
                        "is no longer in the live registry (already destroyed) -- ignoring rather than "
                        "touching it\n", (void *)nv);
        return;
    }

    nv->process_terminated = TRUE; /* diagnostic only -- see webview.h's own
                                     * comment on this field for why this is
                                     * never treated as a permanent failure
                                     * gate anywhere in this codebase. */

    /* See webview.h's own comment on active_wait_loop: wake any bounded
     * navigate_and_wait/cookies_* wait blocked on THIS webview's WebProcess
     * immediately, rather than leaving it to spin out its full 10s/30s
     * timeout for a load-changed/async cookie completion that can now never
     * arrive. Safe unconditionally -- every one of those wait functions
     * already treats an early g_main_loop_quit() (their own timeout path)
     * as "the wait ended without success", exactly the honest failure this
     * mitigation exists to produce, no extra special-casing needed here. */
    if (nv->active_wait_loop)
    {
        fprintf(stderr, "webview2loader-host: waking an in-flight wait on native_webview %p immediately "
                        "-- the WebProcess it was waiting on just terminated\n", (void *)nv);
        g_main_loop_quit(nv->active_wait_loop);
    }
}

/* --- ExecuteScript ---
 *
 * The blocker Studio named itself once the document-start script started
 * reaching the page: every Toolbox retry logged
 *
 *   Warning [FLog::StudioEmbeddedBrowserWebView2] executeJavaScript failed
 *   with error code '-2147467263'   (0x80004001 == E_NOTIMPL)
 *
 * Studio answers the page's messageBusEvent / internal:init handshake with
 * injected JavaScript, not with PostWebMessageAsJson, so with this stubbed the
 * page never got its reply and spun forever on fresh uuids.
 *
 * Same heap-ctx + refcount + bounded-nested-loop shape as the cookie calls
 * above, and for the same reason: GAsyncReadyCallback has no synchronous
 * cancel, so a completion that arrives after this function has already timed
 * out can still touch ctx at any later point. */
struct execute_script_ctx
{
    GMainLoop *loop; /* NULL once the wait exits -- see struct navigate_ctx's
                       * identical field */
    guint timeout_id;
    gboolean done;
    gboolean success;
    char *json; /* g_malloc'd; ownership passes to the caller on success */
    int refs;   /* see struct delete_cookies_ctx's own comment */
};

static void execute_script_ctx_release(struct execute_script_ctx *ctx)
{
    if (--ctx->refs) return;
    g_free(ctx->json);
    free(ctx);
}

static void on_execute_script_done(GObject *source, GAsyncResult *res, void *user_data)
{
    struct execute_script_ctx *ctx = user_data;
    GError *error = NULL;
    JSCValue *value = webkit_web_view_evaluate_javascript_finish((WebKitWebView *)source, res, &error);

    if (!value)
    {
        /* A script that throws lands here, and real WebView2 reports that as a
         * SUCCESSFUL ExecuteScript whose result is "null" -- the exception is
         * the page's business, not a failure of the call. Report it the same
         * way, and print the message here, where the launch log can see it:
         * "Studio's injected script threw" and "the bridge never answered" look
         * identical to Studio otherwise. */
        fprintf(stderr, "webview2loader-host: ExecuteScript: the page's evaluation failed -- %s\n",
                error && error->message ? error->message : "no detail");
        g_clear_error(&error);
        ctx->json = g_strdup("null");
    }
    else
    {
        /* jsc_value_to_json returns NULL for undefined, which most of Studio's
         * bridge calls evaluate to (they are statements, not expressions).
         * "null" is what real WebView2 hands the handler in that case. */
        ctx->json = jsc_value_to_json(value, 0);
        if (!ctx->json) ctx->json = g_strdup("null");
        g_object_unref(value);
    }

    ctx->success = TRUE;
    ctx->done = TRUE;
    if (ctx->loop) g_main_loop_quit(ctx->loop);

    execute_script_ctx_release(ctx); /* this callback's own ref */
}

static gboolean on_execute_script_timeout(gpointer data)
{
    struct execute_script_ctx *ctx = data;

    ctx->timeout_id = 0;
    if (ctx->loop) g_main_loop_quit(ctx->loop);
    return G_SOURCE_REMOVE;
}

gboolean execute_script_and_wait(struct native_webview *nv, const char *script_utf8, char **out_json)
{
    struct execute_script_ctx *ctx;
    gboolean ok;

    *out_json = NULL;
    if (!nv || !script_utf8)
    {
        fprintf(stderr, "webview2loader-host: stale/destroyed native window handle -- failing "
                        "ExecuteScript\n");
        return FALSE;
    }
    if (!(ctx = calloc(1, sizeof(*ctx))))
    {
        fprintf(stderr, "webview2loader-host: calloc failed for ExecuteScript context -- out of "
                        "memory, failing without waiting\n");
        return FALSE;
    }
    ctx->refs = 2;

    /* NULL world_name is the page's own main world, deliberately: Studio's
     * script talks to the page's bridge objects, which an isolated world could
     * not see -- the same reason webview_add_user_script installs Studio's
     * document-start script there. */
    webkit_web_view_evaluate_javascript(nv->view, script_utf8, -1, NULL, NULL, NULL,
                                         on_execute_script_done, ctx);

    /* 10s, matching the cookie calls rather than navigate_and_wait's 30s:
     * evaluating a script in an already-loaded page is local work, not a
     * network round trip. See this file's own top comment for why the wait is a
     * nested GMainLoop and not a blocking condvar. */
    ctx->loop = g_main_loop_new(NULL, FALSE);
    nv->active_wait_loop = ctx->loop;
    ctx->timeout_id = g_timeout_add_seconds(10, on_execute_script_timeout, ctx);
    g_main_loop_run(ctx->loop);
    wait_ended_with_nv_alive(nv, "ExecuteScript");

    if (ctx->timeout_id) g_source_remove(ctx->timeout_id);
    g_main_loop_unref(ctx->loop);
    ctx->loop = NULL;

    if ((ok = ctx->success))
    {
        /* Logged ONCE per webview, not per call: the Toolbox drives this
         * continuously, so a line per script would drown the log -- but with no
         * line at all, "Studio never called ExecuteScript" and "it called and it
         * worked" are indistinguishable, which is the exact ambiguity that made
         * this blocker take a session to find. One line settles that; failures
         * above still log every time. */
        if (!nv->logged_first_script)
        {
            nv->logged_first_script = TRUE;
            fprintf(stderr, "webview2loader-host: first ExecuteScript on nv=%p returned %s -- "
                            "script was: %.120s\n", (void *)nv, ctx->json, script_utf8);
        }
        *out_json = ctx->json;
        ctx->json = NULL; /* ownership handed to the caller */
    }
    else
        fprintf(stderr, "webview2loader-host: ExecuteScript did not complete within 10s on nv=%p\n",
                (void *)nv);

    execute_script_ctx_release(ctx); /* this function's own ref */
    return ok;
}
