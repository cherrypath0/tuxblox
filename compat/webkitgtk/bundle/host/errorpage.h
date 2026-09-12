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

/* compat/webkitgtk/bundle/host/errorpage.h
 *
 * The TuxBlox error screen shown in place of WebKitGTK's own stock "Unable to
 * load page", styled to match tuxblox.net.
 *
 * Each builder returns one complete, standalone HTML document: the stylesheet
 * is inline, the two fonts are data: URIs from errorpage_fonts.h, the logo is
 * inline SVG, and there is no script. Nothing on the page needs the network,
 * which matters because a lost network is the most common reason it appears.
 *
 * Callers own the returned string and free it with g_free().
 */
#ifndef WV2L_HOST_ERRORPAGE_H
#define WV2L_HOST_ERRORPAGE_H

#include <gio/gio.h>
#include <glib.h>
#include <webkit/webkit.h>

/* Whether a WebKitWebView::load-failed error deserves an error screen at all.
 *
 * Cancelled and policy-interrupted loads are the normal, healthy result of
 * on_decide_policy() calling webkit_policy_decision_ignore() -- which is how
 * the roblox-studio-auth: hand-off is completed on every single sign-in. An
 * error screen there would replace a working login with a failure message, so
 * these are filtered out rather than reported. */
gboolean errorpage_wants_load_error(const GError *error);

/* WebKitWebView::load-failed -- DNS, connection and protocol failures. */
char *errorpage_for_load_error(const char *uri, const GError *error);

/* WebKitWebView::load-failed-with-tls-errors -- an untrusted certificate. */
char *errorpage_for_tls_error(const char *uri, GTlsCertificateFlags flags);

/* WebKitWebView::web-process-terminated -- the renderer died under us. */
char *errorpage_for_process_crash(const char *uri, WebKitWebProcessTerminationReason reason);

#endif /* WV2L_HOST_ERRORPAGE_H */
