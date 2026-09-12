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

/* compat/webkitgtk/bundle/host/errorpage.c
 *
 * See errorpage.h for what this is and who calls it.
 *
 * Two rules hold everywhere in this file. Every value that reaches the page
 * from the network -- the URL, the GError message, a certificate's issuer --
 * goes through g_markup_escape_text() first. And the document never refers to
 * anything it does not carry: no stylesheet link, no webfont request, no
 * image, no script, because the page's whole job is to render when nothing can
 * be fetched.
 */

#include "errorpage.h"

#include "errorpage_fonts.h"
#include "errorpage_template.h"

#include <string.h>

/* Replaces every {{name}} slot in the page with value. The page is a template
 * rather than markup built up in here, so it can be opened and edited as a
 * real HTML file -- see host/errorpage.html. Deliberately not printf: the
 * stylesheet is full of percent signs. */
static void fill_slot(GString *page, const char *name, const char *value)
{
    char *slot = g_strconcat("{{", name, "}}", NULL);
    size_t slot_len = strlen(slot);
    size_t value_len = value ? strlen(value) : 0;
    char *at;
    size_t from = 0;

    while ((at = strstr(page->str + from, slot)))
    {
        size_t pos = at - page->str;

        g_string_erase(page, pos, slot_len);
        if (value_len)
            g_string_insert_len(page, pos, value, value_len);
        from = pos + value_len;
    }

    g_free(slot);
}

/* Drops the block between the IF_URL and END_URL markers, for a page with no
 * URL to offer. Leaving the markers in place when the block is kept is
 * harmless -- they are HTML comments. */
static void drop_url_block(GString *page)
{
    static const char open_marker[] = "<!--IF_URL-->";
    static const char close_marker[] = "<!--END_URL-->";
    char *start = strstr(page->str, open_marker);
    char *end = start ? strstr(start, close_marker) : NULL;

    if (!start || !end)
        return;

    g_string_erase(page, start - page->str,
                   (end - start) + (gssize)strlen(close_marker));
}

/* The host part of a URL, for the sentence that names what could not be
 * reached. Falls back to "Roblox" for a URL that will not parse -- the sentence
 * still has to read properly. */
static char *uri_host(const char *uri)
{
    GUri *parsed = uri ? g_uri_parse(uri, G_URI_FLAGS_NONE, NULL) : NULL;
    const char *host = parsed ? g_uri_get_host(parsed) : NULL;
    char *out = g_strdup(host && *host ? host : "Roblox");

    if (parsed)
        g_uri_unref(parsed);
    return out;
}

/* One row of the Technical details list. The value is escaped here so callers
 * can pass raw network data straight in. */
static void append_detail(GString *rows, const char *label, const char *value)
{
    char *safe = g_markup_escape_text(value ? value : "(none)", -1);

    g_string_append_printf(rows, "<dt>%s</dt><dd>%s</dd>", label, safe);
    g_free(safe);
}

/* Fills host/errorpage.html in. body_html and details_html are already escaped
 * by their callers; uri is escaped here because it lands in both an href
 * attribute and the page text.
 *
 * Slots are filled before the fonts so that nothing a caller supplies can
 * introduce a slot of its own -- a URL containing the literal text of a font
 * placeholder would otherwise have a 30 KB base64 blob substituted into it. */
static char *build_document(const char *kicker, const char *heading, const char *body_html,
                            const char *uri, const char *details_html)
{
    GString *page = g_string_new(errorpage_template);
    char *safe_uri = uri && *uri ? g_markup_escape_text(uri, -1) : NULL;

    /* No URL means nothing to go back to, so the address line and the button
     * both drop out rather than pointing at nowhere. A renderer that died
     * before its first navigation is the case that reaches this. */
    if (safe_uri)
        fill_slot(page, "URL", safe_uri);
    else
        drop_url_block(page);

    fill_slot(page, "KICKER", kicker);
    fill_slot(page, "HEADING", heading);
    fill_slot(page, "BODY", body_html);
    fill_slot(page, "DETAILS", details_html);

    fill_slot(page, "FONT_INTER_400", errorpage_font_inter_400);
    fill_slot(page, "FONT_INTER_600", errorpage_font_inter_600);
    fill_slot(page, "FONT_MONTSERRAT_700", errorpage_font_montserrat_700);

    g_free(safe_uri);
    return g_string_free(page, FALSE);
}

gboolean errorpage_wants_load_error(const GError *error)
{
    if (!error)
        return FALSE;

    /* A navigation this helper deliberately refused. on_decide_policy() calls
     * webkit_policy_decision_ignore() for every roblox-studio-auth: redirect,
     * which is how sign-in finishes -- reporting it as a failure would put an
     * error screen in front of a login that actually worked. */
    if (g_error_matches(error, WEBKIT_NETWORK_ERROR, WEBKIT_NETWORK_ERROR_CANCELLED))
        return FALSE;
    if (g_error_matches(error, WEBKIT_POLICY_ERROR,
                        WEBKIT_POLICY_ERROR_FRAME_LOAD_INTERRUPTED_BY_POLICY_CHANGE))
        return FALSE;

    return TRUE;
}

char *errorpage_for_load_error(const char *uri, const GError *error)
{
    char *host = uri_host(uri);
    const char *heading = "Can&rsquo;t reach this site";
    char *body;
    char *kicker;
    GString *rows = g_string_new(NULL);
    char *page;

    if (g_error_matches(error, WEBKIT_NETWORK_ERROR, WEBKIT_NETWORK_ERROR_UNKNOWN_PROTOCOL) ||
        g_error_matches(error, WEBKIT_POLICY_ERROR, WEBKIT_POLICY_ERROR_CANNOT_SHOW_URI))
    {
        heading = "TuxBlox can&rsquo;t open this link";
        body = g_strdup("Nothing on this system is registered to handle it. If this was a sign-in "
                        "link, reinstalling TuxBlox restores the handler.");
    }
    else if (g_error_matches(error, WEBKIT_NETWORK_ERROR, WEBKIT_NETWORK_ERROR_FILE_DOES_NOT_EXIST))
    {
        heading = "That page isn&rsquo;t there";
        body = g_markup_printf_escaped("<b>%s</b> answered, but the page has moved or been removed.",
                                       host);
    }
    else
    {
        body = g_markup_printf_escaped("TuxBlox couldn&rsquo;t connect to <b>%s</b>. Check that "
                                       "you&rsquo;re online, then try again.", host);
    }

    kicker = g_markup_printf_escaped("Network &middot; %s %d", g_quark_to_string(error->domain),
                                     error->code);

    append_detail(rows, "Domain", g_quark_to_string(error->domain));
    g_string_append_printf(rows, "<dt>Code</dt><dd>%d</dd>", error->code);
    append_detail(rows, "Message", error->message);
    append_detail(rows, "URL", uri);

    page = build_document(kicker, heading, body, uri, rows->str);

    g_string_free(rows, TRUE);
    g_free(kicker);
    g_free(body);
    g_free(host);
    return page;
}

/* The certificate problems GTlsCertificateFlags can report, in the order the
 * flags are defined. Joined with commas so a certificate that fails several
 * checks at once says so. */
static char *tls_flags_text(GTlsCertificateFlags flags)
{
    static const struct
    {
        GTlsCertificateFlags flag;
        const char *name;
    } known[] = {
        { G_TLS_CERTIFICATE_UNKNOWN_CA,    "G_TLS_CERTIFICATE_UNKNOWN_CA" },
        { G_TLS_CERTIFICATE_BAD_IDENTITY,  "G_TLS_CERTIFICATE_BAD_IDENTITY" },
        { G_TLS_CERTIFICATE_NOT_ACTIVATED, "G_TLS_CERTIFICATE_NOT_ACTIVATED" },
        { G_TLS_CERTIFICATE_EXPIRED,       "G_TLS_CERTIFICATE_EXPIRED" },
        { G_TLS_CERTIFICATE_REVOKED,       "G_TLS_CERTIFICATE_REVOKED" },
        { G_TLS_CERTIFICATE_INSECURE,      "G_TLS_CERTIFICATE_INSECURE" },
        { G_TLS_CERTIFICATE_GENERIC_ERROR, "G_TLS_CERTIFICATE_GENERIC_ERROR" },
    };
    GString *out = g_string_new(NULL);
    unsigned i;

    for (i = 0; i < G_N_ELEMENTS(known); i++)
    {
        if (!(flags & known[i].flag))
            continue;
        if (out->len)
            g_string_append(out, ", ");
        g_string_append(out, known[i].name);
    }

    if (!out->len)
        g_string_append(out, "(none reported)");
    return g_string_free(out, FALSE);
}

char *errorpage_for_tls_error(const char *uri, GTlsCertificateFlags flags)
{
    char *host = uri_host(uri);
    char *flag_names = tls_flags_text(flags);
    const char *heading = "This connection isn&rsquo;t secure";
    char *body;
    GString *rows = g_string_new(NULL);
    char *page;

    if (flags & G_TLS_CERTIFICATE_EXPIRED)
        body = g_markup_printf_escaped("The certificate for <b>%s</b> has expired. Please try again later.", host);
    else
        body = g_markup_printf_escaped("The certificate for <b>%s</b> couldn&rsquo;t be trusted. "
                                       "Please try again later.", host);

    append_detail(rows, "Host", host);
    append_detail(rows, "Flags", flag_names);
    append_detail(rows, "URL", uri);

    page = build_document("Certificate &middot; TLS verification failed", heading, body, uri,
                          rows->str);

    g_string_free(rows, TRUE);
    g_free(body);
    g_free(flag_names);
    g_free(host);
    return page;
}

char *errorpage_for_process_crash(const char *uri, WebKitWebProcessTerminationReason reason)
{
    const char *reason_name;
    const char *heading = "This page stopped responding";
    const char *body;
    GString *rows = g_string_new(NULL);
    char *page;

    switch (reason)
    {
    case WEBKIT_WEB_PROCESS_CRASHED:
        reason_name = "WEBKIT_WEB_PROCESS_CRASHED";
        body = "The WebKit process behind this window closed unexpectedly. Please refresh the page.";
        break;
    case WEBKIT_WEB_PROCESS_EXCEEDED_MEMORY_LIMIT:
        reason_name = "WEBKIT_WEB_PROCESS_EXCEEDED_MEMORY_LIMIT";
        heading = "This page ran out of memory";
        body = "The page ran out of maximum allocated memory. Please refresh the page.";
        break;
    case WEBKIT_WEB_PROCESS_TERMINATED_BY_API:
        reason_name = "WEBKIT_WEB_PROCESS_TERMINATED_BY_API";
        body = "This window was closed on request. You can refresh the page to bring it back.";
        break;
    default:
        reason_name = "unrecognised reason";
        body = "The WebKit process behind this window is gone. Please refresh the page.";
        break;
    }

    append_detail(rows, "Reason", reason_name);
    append_detail(rows, "Process", "WebKitWebProcess");
    append_detail(rows, "URL", uri);

    page = build_document("Renderer &middot; content process ended", heading, body, uri, rows->str);

    g_string_free(rows, TRUE);
    return page;
}
