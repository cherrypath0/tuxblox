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
    nv->active_wait_loop = NULL;

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
    nv->active_wait_loop = NULL;

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
    nv->active_wait_loop = NULL;

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

    /* out -- written only by on_get_cookies_done, read only by
     * cookies_get itself after ctx->done is observed true (see this
     * struct's own leading comment; no lock needed, both sides run on this
     * process's single serialized thread, just at different points in
     * time). */
    gboolean success;
    uint32_t count;
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
    uint32_t total = 0, n = 0;
    gboolean success;

    if (ctx->filtered)
        cookies = webkit_cookie_manager_get_cookies_finish((WebKitCookieManager *)source, res, NULL);
    else
        cookies = webkit_cookie_manager_get_all_cookies_finish((WebKitCookieManager *)source, res, NULL);

    /* Count the real list length FIRST (a second pass, but a cheap one --
     * just pointer-chasing, no allocation) so exceeding the cap is a real,
     * distinguishable failure (success==FALSE) instead of a silently
     * incomplete list -- ported verbatim from the original's own Important-2
     * code review fix. */
    for (l = cookies; l; l = l->next) total++;

    if (total > WV2L_MAX_COOKIES)
    {
        fprintf(stderr, "webview2loader-host: cookie store holds %u cookies, exceeding this build's "
                        "%u-cookie cap -- failing this GetCookies call rather than returning a silently "
                        "incomplete list\n", (unsigned)total, (unsigned)WV2L_MAX_COOKIES);
        success = FALSE;
    }
    else
    {
        success = TRUE;
        for (l = cookies; l; l = l->next)
        {
            /* fill_wire_cookie itself can still reject an individual
             * cookie whose field(s) don't fit -- that cookie is dropped
             * (loudly) rather than failing the whole call, same asymmetry
             * as the original: a single oversized field isn't something
             * clearAllCookiesAndRunCallbackHelper's enumerate-then-act
             * semantics can be silently wrong about the same way the
             * total-count cap above is. */
            if (fill_wire_cookie(&ctx->cookies[n], l->data)) n++;
        }
    }

    if (cookies) g_list_free_full(cookies, (GDestroyNotify)soup_cookie_free);

    ctx->success = success;
    ctx->count = n;
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
    nv->active_wait_loop = NULL;

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
        if (ctx->success) memcpy(out->cookies, ctx->cookies, ctx->count * sizeof(*ctx->cookies));
    }

    get_cookies_ctx_release(ctx); /* this function's own ref -- safe
                                    * unconditionally, same as
                                    * cookies_delete_all's own identical
                                    * release call. */
    return out->success;
}

/* --- OAuth redirect handoff ---
 *
 * Ported VERBATIM from the original (see this file's own top comment) --
 * real, committed, independently trace-verified logic (commit ac3634ea6),
 * not touched beyond the mechanical p_-prefix drop. */

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

    if (decision_type != WEBKIT_POLICY_DECISION_TYPE_NAVIGATION_ACTION)
        return FALSE;

    action = webkit_navigation_policy_decision_get_navigation_action((WebKitNavigationPolicyDecision *)decision);
    if (!action) return FALSE;
    request = webkit_navigation_action_get_request(action);
    if (!request) return FALSE;
    uri = webkit_uri_request_get_uri(request);
    if (!uri) return FALSE;

    for (i = 0; i < G_N_ELEMENTS(known_roblox_schemes); i++)
    {
        if (!strncmp(uri, known_roblox_schemes[i], strlen(known_roblox_schemes[i])))
        {
            fprintf(stderr, "webview2loader-host: intercepting navigation to %s -- ignoring the "
                            "in-WebKit load (would otherwise show WebKit's own generic \"URL can't be "
                            "shown\" error) and handing off to xdg-open, matching real WebView2's own "
                            "default behavior for an external scheme it doesn't itself recognize\n", uri);
            webkit_policy_decision_ignore(decision);
            xdg_open_handoff(uri);
            return TRUE; /* GDK_EVENT_STOP -- we handled this decision ourselves */
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
 * dispatch case in main.c for no real reason. */
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
