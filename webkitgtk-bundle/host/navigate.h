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

/* webkitgtk-bundle/host/navigate.h
 *
 * Navigation, cookies, and the real roblox-studio-auth: OAuth redirect
 * handoff -- ported from ProtonSource/wine/dlls/webview2loader/unixlib.c as
 * of commit ac3634ea6 (see git show ac3634ea6:ProtonSource/wine/dlls/webview2loader/unixlib.c).
 * This is Task 6 (the last "port real logic" task) of the
 * webview2loader-host-process plan
 * (docs/superpowers/plans/2026-08-14-webview2loader-host-process.md).
 *
 * gboolean stands in for the plan's "BOOL" return type, same substitution
 * Task 5 already established for geometry.h (Windows BOOL and GLib
 * gboolean are both plain `int` with identical TRUE(1)/FALSE(0) values;
 * this host process has no windef.h).
 */
#ifndef WV2L_HOST_NAVIGATE_H
#define WV2L_HOST_NAVIGATE_H

#include "webview.h"
#include "webview2loader_ipc_protocol.h"

/* Converts a wire uint16_t[] URI buffer -- already NUL-terminated by the
 * Wine-side sender (unixlib.c's copy_wcs_to_wire_uri, see
 * webview2loader_ipc_protocol.h's own top comment) -- into a newly
 * allocated UTF-8 string via GLib's g_utf16_to_utf8(). The original's
 * wcs_to_utf8 went through Wine-internal ntdll_wcstoumbs instead, not
 * available here (this host process is not Wine/PE code, see this file's
 * own top comment); GLib's own UTF-16->UTF-8 conversion is the mechanical
 * substitute the plan calls for. Returns an empty (not NULL) string for an
 * empty input, mirroring g_utf16_to_utf8's own behavior; NULL only on a
 * genuine conversion failure (malformed UTF-16 -- not expected in practice
 * since the Wine side only ever sends real Windows WCHAR strings, but
 * handled rather than assumed). Caller frees the result with g_free(), not
 * free(). */
char *wire_uri_to_utf8(const uint16_t *wire_uri);

/* Real page navigation -- ports on_load_changed/navigate_on_gtk_thread/
 * disconnect_load_changed_on_gtk_thread into one function, blocking until
 * WebKit's "load-changed" signal reports WEBKIT_LOAD_FINISHED or a bounded
 * 30s timeout elapses (verbatim bound from the original). See navigate.c's
 * own comment on why this uses a nested GMainLoop rather than the
 * original's cross-thread pthread_cond_timedwait -- this whole process is
 * a single GLib main-loop thread now (no separate "gtk_thread" left to
 * bridge to, see webview.c's own live_webviews comment for the same
 * observation), so a real blocking wait on this thread would deadlock the
 * very completion it's waiting for.
 *
 * uri_utf8 is navigated to as-is (never NULL -- see wire_uri_to_utf8's own
 * comment; may be empty, matching the original's own lack of a special
 * case for Navigate specifically, unlike GetCookies's optional filter).
 * nv must already be a resolved, live pointer (or NULL for a stale/
 * unregistered handle -- see webview_lookup), same calling convention
 * Tasks 4/5 established. Always sets *out_success and *out_nav_id, even on
 * a stale handle. */
void navigate_and_wait(struct native_webview *nv, const char *uri_utf8, gboolean *out_success, uint64_t *out_nav_id);

/* Real cookie-store deletion. webkit_cookie_manager_replace_cookies(mgr,
 * NULL, ...) does NOT work for "delete everything" -- see navigate.c's own
 * comment (a real, previously-found bug, ported verbatim from the
 * original's own Task 8 finding: that call silently no-ops with a GLib
 * CRITICAL instead of deleting anything). Real fix, ported as-is: enumerate
 * every cookie via webkit_cookie_manager_get_all_cookies, then delete each
 * one individually. Bounded ~10s wait (verbatim from the original), same
 * nested-GMainLoop mechanism as navigate_and_wait. Returns FALSE for a
 * stale/unregistered nv (NULL) or on allocation failure; TRUE otherwise
 * (matching the original's own "attempted" convention -- a real timeout
 * still returns TRUE, exactly like unix_delete_all_cookies_impl's own
 * STATUS_SUCCESS-even-on-timeout return). */
gboolean cookies_delete_all(struct native_webview *nv);

/* Real cookie count via the same get_all_cookies machinery, same bounded
 * ~10s wait. Returns FALSE for a stale/unregistered nv (NULL) or on
 * allocation failure; *out_count is always set (0 on failure/timeout,
 * matching the original's "don't know, treat as 0" convention -- only
 * trusted if the async callback actually completed in time). */
gboolean cookies_count(struct native_webview *nv, uint32_t *out_count);

/* Real cookie enumeration, optionally filtered to uri_utf8 (NULL/empty =
 * every cookie under the profile, matching real
 * ICoreWebView2CookieManager::GetCookies semantics -- see
 * learn.microsoft.com's reference, same as the original's own comment).
 * Writes directly into the wire-format out->success/out->count/out->cookies
 * fields -- no separate host-side cookie struct is needed since struct
 * wv2l_cookie's uint16_t fields already ARE the exact wire format this
 * function's UTF-8->UTF-16 conversion needs to produce (unlike the
 * original, which filled a WCHAR-based struct unix_cookie only available
 * PE-side). Returns FALSE for a stale/unregistered nv (NULL), an allocation
 * failure, or a real GetCookies failure (oversized store/timeout -- same
 * value as the resulting out->success); out->success/out->count are always
 * set either way. */
/* out->offset selects which page of the store to return; out->count is that
 * page's size and out->total is the whole store's, so the caller can page until
 * it has everything. See on_get_cookies_done's own paging comment for why a
 * store larger than WV2L_MAX_COOKIES no longer fails the call outright. */
gboolean cookies_get(struct native_webview *nv, const char *uri_utf8, struct wv2l_get_cookies_params *out);

/* Deletes the single cookie identified by wire's name/value/domain/path -- all
 * four are load-bearing, see this function's own comment in navigate.c for the
 * libsoup matching rule that makes value part of the key. */
gboolean cookies_delete_one(struct native_webview *nv, const struct wv2l_cookie *wire);

/* WebKitWebView::decide-policy handler -- the real roblox-studio-auth:/
 * roblox-studio:/roblox-player: OAuth redirect handoff (ported verbatim
 * from the original's on_decide_policy, see navigate.c's own comment).
 * Exposed here (not static) because it must be connected once per webview
 * at CREATION time, in webview.c's webview_create -- alongside its
 * existing "close-request" connection -- not scoped to a single
 * navigate_and_wait() call; see this function's own doc comment in
 * navigate.c for why. Matches the real WebKitWebView::decide-policy signal
 * signature exactly, so it can be passed straight to
 * g_signal_connect_data without a cast-through-GCallback wrapper beyond the
 * usual (GCallback) cast every signal connection already needs. */
gboolean on_decide_policy(WebKitWebView *view, WebKitPolicyDecision *decision,
                           WebKitPolicyDecisionType decision_type, void *user_data);

/* WebKitWebView::web-process-terminated handler -- cosmetic mitigation for a
 * real, understood WebKitGTK/NVIDIA Skia GPU-teardown race in
 * WebKitWebProcess's own shutdown path (see navigate.c's own comment on this
 * function for the full real-signal/enum verification against this bundle's
 * actual built WebKitGTK 2.52.5 headers/source, and
 * .superpowers/sdd/2026-08-14-webview2loader-host-process/
 * webprocess-terminated-mitigation-report.md for the full writeup). Exposed
 * here (not static), same reasoning as on_decide_policy above -- connected
 * once per webview at CREATION time, in webview.c's webview_create,
 * alongside close-request/decide-policy. Matches the real
 * WebKitWebView::web-process-terminated signal signature exactly (verified
 * against this build's own WebKitWebView.cpp g_signal_new call: RUN_LAST,
 * no accumulator, void return, one WebKitWebProcessTerminationReason arg --
 * g_cclosure_marshal_VOID__ENUM), so it can be passed straight to
 * g_signal_connect_data without a cast-through-GCallback wrapper beyond the
 * usual (GCallback) cast every signal connection already needs. user_data is
 * always the owning struct native_webview* (wired at connect time in
 * webview.c), unlike on_decide_policy which doesn't need it. */
void on_web_process_terminated(WebKitWebView *view, WebKitWebProcessTerminationReason reason, void *user_data);

/* Page -> Studio web message; see the definition in navigate.c. Returns 0 on
 * success, -1 if the channel is absent or the payload does not fit. */
int event_send_web_message(struct native_webview *nv, const char *payload_utf8, const char *source_utf8,
                            int is_string);

/* Writes one cookie into the webview's cookie jar (ICoreWebView2CookieManager::
 * AddOrUpdateCookie). See the definition in navigate.c. */
gboolean cookies_add_or_update(struct native_webview *nv, const struct wv2l_cookie *wire);

#endif /* WV2L_HOST_NAVIGATE_H */
