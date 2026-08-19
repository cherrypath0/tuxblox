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

/* webkitgtk-bundle/host/webview.c
 *
 * Window creation/destruction and the live-webview registry -- ported from
 * ProtonSource/wine/dlls/webview2loader/unixlib.c as of commit ac3634ea6
 * (see git show ac3634ea6:ProtonSource/wine/dlls/webview2loader/unixlib.c),
 * the last commit before Task 3's rewrite deleted that file's in-process
 * dlmopen/GTK-thread machinery. This is Task 4 of the
 * webview2loader-host-process plan
 * (docs/superpowers/plans/2026-08-14-webview2loader-host-process.md).
 *
 * Mapping from the old file (all renames/removals are purely mechanical,
 * per Task 4's brief -- no logic changes beyond what's noted):
 *   - struct native_webview                     -> struct native_webview (webview.h), unchanged
 *   - live_webview_register/_unregister/_is_valid -> folded into webview_create/
 *     webview_destroy/webview_lookup below (same MAX_LIVE_WEBVIEWS array,
 *     extended so lookup-by-handle returns the pointer, not just a bool --
 *     needed now that handles cross a real IPC boundary as opaque uint64_t
 *     instead of already being a live in-process pointer)
 *   - on_close_request                          -> on_close_request, verbatim
 *   - create_webview_on_gtk_thread               -> webview_create, with the
 *     struct create_webview_ctx/void* data marshaling wrapper dropped (this
 *     runs directly as the dispatch handler's own body now, no
 *     gtk_thread_invoke_sync to bridge across)
 *   - destroy_webview_on_gtk_thread              -> webview_destroy, same
 *     wrapper-removal treatment
 *
 * Every p_-prefixed GTK function call from the old file (e.g.
 * p_gtk_window_new()) becomes a direct call against the real GTK4 headers
 * this file links against (gtk_window_new()) -- the old p_ indirection only
 * existed because that code was dlopen()'d at runtime with no real headers
 * available; that constraint doesn't apply here.
 *
 * Task 6 addition (additive only -- this file's Task 4 window creation/
 * destruction/registry logic is otherwise unchanged): the
 * WebKitWebView::decide-policy connection (real OAuth/xdg-open redirect
 * handoff, on_decide_policy -- implemented in navigate.c, Task 6's file)
 * is wired up in webview_create below, alongside the existing
 * "close-request" connection, exactly where it lived at webview-creation
 * time in the original source (see navigate.c's own comment on
 * on_decide_policy for why it must be connected here, not scoped to a
 * single Navigate() call).
 */
#include "webview.h"
#include "navigate.h"
#include "watchdog.h"
#include "geometry.h"
#include <gdk/x11/gdkx.h>
#include <stdio.h>
#include <stdlib.h>

/* Task 7 crash fix, round 14 (original unixlib.c comment, preserved
 * verbatim below on on_close_request itself): a small live-handle registry
 * is enough here -- unlike the original unixlib.c, every dispatch handler
 * in this process already runs serialized on the single GTK main-loop
 * thread (there is no separate "GTK thread" to bridge to anymore, the
 * whole helper process's main() *is* that thread), so no lock is needed
 * around this array: destroy unregisters (and only then frees) strictly
 * before any later-queued handler for the same handle can run. */
#define MAX_LIVE_WEBVIEWS 64
static struct native_webview *live_webviews[MAX_LIVE_WEBVIEWS];

static void live_webview_register(struct native_webview *nv)
{
    int i;
    for (i = 0; i < MAX_LIVE_WEBVIEWS; i++)
        if (!live_webviews[i]) { live_webviews[i] = nv; return; }
    fprintf(stderr, "webview2loader-host: live_webviews registry full (%d entries) -- "
                     "UAF guard won't cover handle %p\n", MAX_LIVE_WEBVIEWS, (void *)nv);
}

static void live_webview_unregister(struct native_webview *nv)
{
    int i;
    for (i = 0; i < MAX_LIVE_WEBVIEWS; i++)
        if (live_webviews[i] == nv) { live_webviews[i] = NULL; return; }
}

struct native_webview *webview_lookup(uint64_t handle)
{
    struct native_webview *nv = (struct native_webview *)(uintptr_t)handle;
    int i;

    if (!nv) return NULL;
    for (i = 0; i < MAX_LIVE_WEBVIEWS; i++)
        if (live_webviews[i] == nv) return nv;
    return NULL;
}

/* Task 7 crash fix, round 14: a real, reproducible double-destroy crash
 * (coredump, RAX == 0xaaaaaaaaaaaaaaaa -- GLib's own "gc-friendly" freed-
 * memory poison fill -- reading through nv->window inside GTK4's own
 * gtk_window_destroy(), called from this file's own webview_destroy)
 * surfaced only once these windows became genuinely X11/WM-backed (a
 * window that's not really window-manager-managed can't receive a real
 * close request from one either). GTK4's own documented default behavior
 * for GtkWindow::close-request is to destroy the window itself unless a
 * connected handler returns TRUE to stop that. Without a connected
 * handler, any window-manager-initiated close (the WM's own close button,
 * Alt+F4, etc.) could trigger GTK4 to self-destroy nv->window out from
 * under this file's own bookkeeping; a later, entirely normal
 * Close()/Release() on the same real WebView2 controller then calls this
 * file's own webview_destroy, which tries to destroy the same (already
 * GTK-internally-destroyed) widget a second time -- a genuine
 * double-destroy. Real WebView2 controllers are never independently
 * closable by the user or window manager at all (only via the real API's
 * own Close()), so unconditionally stopping this signal is the correct
 * semantic fix, not just a crash workaround. */
static gboolean on_close_request(GtkWindow *window, void *user_data)
{
    return TRUE; /* GDK_EVENT_STOP -- stop GTK4's own default handler,
                  * which would otherwise destroy the window itself. */
}

/* --- F-08 spike: does the page actually use window.chrome.webview? ---
 *
 * See webview_create's own call-site comment for why this exists. Diagnostic
 * only: nothing is forwarded to the Wine side, so no PE-side event plumbing,
 * no protocol change, and no behaviour change for anything that already works.
 *
 * The JS deliberately mirrors real WebView2's shape rather than inventing one,
 * so that if the answer is "yes, the page posts messages", this same script is
 * the starting point for the real bridge instead of throwaway code:
 * postMessage(string) and postMessage(object) are distinguished, because real
 * WebView2 surfaces those differently (TryGetWebMessageAsString returns the
 * string only for the former, while get_WebMessageAsJson always returns JSON).
 * addEventListener is accepted and recorded so a page that registers for
 * host->page messages doesn't throw; nothing dispatches to it yet. */
static const char *const WV2_MESSAGE_SHIM_JS =
    "(function(){\n"
    "  if (window.chrome && window.chrome.webview) return;\n"
    "  var listeners = [];\n"
    /* Posts a real object, not JSON text: WebKitGTK hands the handler a
     * JSCValue, so an object lets the host read the two fields straight off it
     * with jsc_value_object_get_property instead of pulling in a JSON parser. */
    "  function send(kind, payload) {\n"
    "    try {\n"
    "      window.webkit.messageHandlers.tuxblox.postMessage({kind: kind, payload: payload});\n"
    "    } catch (e) { /* handler not registered -- nothing to do */ }\n"
    "  }\n"
    "  window.chrome = window.chrome || {};\n"
    "  window.chrome.webview = {\n"
    "    postMessage: function(msg) {\n"
    "      send(typeof msg === 'string' ? 'string' : 'json',\n"
    "           typeof msg === 'string' ? msg : JSON.stringify(msg));\n"
    "    },\n"
    "    addEventListener: function(t, f) { if (t === 'message') listeners.push(f); },\n"
    "    removeEventListener: function(t, f) {\n"
    "      var i = listeners.indexOf(f); if (i >= 0) listeners.splice(i, 1);\n"
    "    }\n"
    "  };\n"
    /* Host -> page delivery, called by webview_post_web_message below.
     * Non-enumerable and underscore-named because it is our transport, not
     * part of the surface a page should discover. */
    "  Object.defineProperty(window, '__tuxblox_wv2_deliver', {\n"
    "    value: function(payload, isString) {\n"
    "      var data;\n"
    "      try { data = isString ? payload : JSON.parse(payload); }\n"
    "      catch (e) { data = payload; }\n"
    "      var ev = {data: data, source: window.chrome.webview};\n"
    "      for (var i = 0; i < listeners.length; i++) {\n"
    "        try { listeners[i](ev); } catch (e) {}\n"
    "      }\n"
    "      if (typeof window.chrome.webview.onmessage === 'function') {\n"
    "        try { window.chrome.webview.onmessage(ev); } catch (e) {}\n"
    "      }\n"
    "    }, enumerable: false, writable: false, configurable: false\n"
    "  });\n"
    "})();\n";

/* Page -> Studio. The shim posts {kind, payload}; this reads those two fields
 * off the JSCValue and forwards the payload over the event channel as a real
 * WebMessageReceived.
 *
 * Used to be diagnostic only (the "F-08 PROBE" line), which is what made the
 * Toolbox hang: the page reported loadprogress here and it went nowhere. The
 * log line is kept -- it is how the payloads were finally observed, and it
 * costs one fprintf per message. */
static void on_script_message(WebKitUserContentManager *manager, JSCValue *value, void *user_data)
{
    struct native_webview *nv = user_data;
    JSCValue *kind_v, *payload_v;
    char *kind = NULL, *payload = NULL;
    gboolean is_string;

    (void)manager;
    if (!value || !jsc_value_is_object(value))
    {
        fprintf(stderr, "webview2loader-host: postMessage did not carry the expected object -- "
                        "dropping\n");
        return;
    }

    kind_v = jsc_value_object_get_property(value, "kind");
    payload_v = jsc_value_object_get_property(value, "payload");
    if (kind_v) kind = jsc_value_to_string(kind_v);
    if (payload_v) payload = jsc_value_to_string(payload_v);

    if (payload)
    {
        is_string = (kind && !strcmp(kind, "string"));
        fprintf(stderr, "webview2loader-host: page called window.chrome.webview.postMessage on "
                        "nv=%p (%s) -- payload: %s\n",
                (void *)nv, is_string ? "string" : "json", payload);
        webview_send_web_message_event(nv, payload, is_string);
    }
    else fprintf(stderr, "webview2loader-host: postMessage had no payload field -- dropping\n");

    g_free(kind);
    g_free(payload);
    if (kind_v) g_object_unref(kind_v);
    if (payload_v) g_object_unref(payload_v);
}

void webview_send_web_message_event(struct native_webview *nv, const char *payload_utf8, gboolean is_string)
{
    const char *source;

    if (!nv) return;
    /* webkit_web_view_get_uri returns NULL before the first commit; the event
     * sender treats a NULL source as an empty string rather than dropping the
     * message, which matters because the page's earliest loadprogress messages
     * are exactly the ones the Toolbox is waiting for. */
    source = webkit_web_view_get_uri(nv->view);
    event_send_web_message(nv, payload_utf8, source, is_string ? 1 : 0);
}

/* Escapes `src` into a JS double-quoted string literal, including the quotes.
 * Hand-rolled rather than pulled from json-glib: this file already links only
 * what WebKitGTK itself guarantees, and one escaper is cheaper than a new
 * dependency in the bundle's build. Escapes the two structural characters plus
 * every C0 control, which is exactly what can terminate or corrupt a JS string
 * literal; everything else (including all non-ASCII UTF-8) passes through, since
 * the script is handed to WebKit as UTF-8 already. */
static char *js_string_literal(const char *src)
{
    GString *out = g_string_new("\"");
    const unsigned char *p;

    for (p = (const unsigned char *)src; *p; p++)
    {
        switch (*p)
        {
        case '"':  g_string_append(out, "\\\""); break;
        case '\\': g_string_append(out, "\\\\"); break;
        case '\n': g_string_append(out, "\\n"); break;
        case '\r': g_string_append(out, "\\r"); break;
        case '\t': g_string_append(out, "\\t"); break;
        default:
            if (*p < 0x20) g_string_append_printf(out, "\\u%04x", *p);
            else g_string_append_c(out, (char)*p);
            break;
        }
    }
    g_string_append_c(out, '"');
    return g_string_free(out, FALSE);
}

/* Studio -> page: hands `message_utf8` to the shim's own delivery entry point.
 *
 * The payload goes in as an escaped string literal, never concatenated raw --
 * this is page-adjacent data being placed into evaluated JavaScript, so an
 * unescaped quote would be an injection bug, not merely a parse error. */
gboolean webview_post_web_message(struct native_webview *nv, const char *message_utf8, gboolean is_string)
{
    char *literal;
    char *script;

    if (!nv || !message_utf8)
    {
        fprintf(stderr, "webview2loader-host: post_web_message on a stale/NULL webview -- ignoring\n");
        return FALSE;
    }

    literal = js_string_literal(message_utf8);
    script = g_strdup_printf(
        "if (window.__tuxblox_wv2_deliver) window.__tuxblox_wv2_deliver(%s, %s);",
        literal, is_string ? "true" : "false");
    g_free(literal);

    /* Logged on SUCCESS, not only on failure. Without this, "Studio replied and
     * we delivered it" and "Studio never replied at all" produce identical
     * logs -- which is exactly the ambiguity blocking the Toolbox, where the
     * page sends internal:init and waits for an answer. */
    fprintf(stderr, "webview2loader-host: Studio -> page message on nv=%p (%s): %.160s\n",
            (void *)nv, is_string ? "string" : "json", message_utf8);

    /* Fire-and-forget: real WebView2's PostWebMessage* has no completion, and a
     * page that throws inside its own listener is not our failure to report. */
    webkit_web_view_evaluate_javascript(nv->view, script, -1, NULL, NULL, NULL, NULL, NULL);
    g_free(script);
    return TRUE;
}

/* Take this window out of the window manager's hands entirely, before it is
 * ever mapped.
 *
 * It is a toplevel for only a moment: geometry_sync XReparentWindow's it into
 * Roblox Studio's own window as soon as the first put_Bounds arrives. But a WM
 * manages every toplevel it sees mapped, so in that moment KWin adds it to
 * _NET_CLIENT_LIST and gives it a task button -- an undecorated, contentless
 * entry next to Studio's real one. The reparent then pulls the window out from
 * under KWin without KWin dropping it from that list, so the entry is never
 * cleaned up when the window is destroyed either: the blank task button
 * outlives the whole session and only goes away when KWin restarts.
 *
 * override-redirect rather than _NET_WM_STATE_SKIP_TASKBAR: the state property
 * has to be set before the map (afterwards a WM only honors a client message,
 * and by then it has already stopped managing this window), but GDK writes its
 * own _NET_WM_STATE while mapping and wipes ours -- measured, the property
 * reads back absent. override-redirect is a window attribute, not a property;
 * GDK never touches it, and a WM ignores such windows completely.
 *
 * Costs nothing here: this window is only ever a child inside Studio's own
 * window, and X input focus sits on that Wine parent (also measured), not on
 * this surface, so there is no WM-assigned focus to lose.
 *
 * gtk_widget_realize creates the real X window without mapping it, which is
 * exactly the window this needs to run against. */
/* Default agent applied to every webview at creation.
 *
 * WebKitGTK otherwise reports its own Safari-shaped agent, which tells the
 * Toolbox page it is an ordinary browser -- so the page renders create.roblox.com's
 * consumer store (Sign Up button and all) inside the Studio panel instead of the
 * embedded Toolbox UI. Studio itself sets a WebView2 agent through
 * ICoreWebView2Settings2::put_UserAgent; this default exists so the page still
 * behaves correctly on the first load, before or without that call, and is
 * overwritten the moment Studio makes it.
 *
 * The "WebView2" token is the part that matters -- it is what the page keys on,
 * and it is taken from Studio's own template string. This is presenting the same
 * agent the official client presents on Windows so the unmodified client works
 * on Linux; it is not concealing anything about the user. */
#define WV2L_DEFAULT_USER_AGENT \
    "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) " \
    "Chrome/138.0.0.0 Safari/537.36 Edg/138.0.0.0 WebView2"

/* Returning TRUE from WebKitWebView::context-menu suppresses the menu entirely.
 * WebKitGTK has no "disable context menu" setting, so this signal is the
 * mechanism; it is connected once at creation and consults the flag each time
 * rather than being connected/disconnected as the setting changes. */
/* THREE arguments, not four. WebKitGTK 6.0's context-menu signal is
 * (WebKitWebView*, WebKitContextMenu*, WebKitHitTestResult*) -- the GdkEvent*
 * that the GTK3-era WebKit passed as a fourth argument is gone in GTK4, and
 * declaring it anyway shifted every parameter: user_data landed one slot past
 * the real arguments, so `nv` was read from uninitialised stack and the menu
 * appeared regardless of the setting. Verified against this bundle's own
 * installed WebKitWebView.h rather than assumed. */
static gboolean on_context_menu(WebKitWebView *view, WebKitContextMenu *menu,
                                 WebKitHitTestResult *hit, void *user_data)
{
    struct native_webview *nv = user_data;

    (void)view; (void)menu; (void)hit;
    return (nv && !nv->default_context_menus_enabled) ? TRUE : FALSE;
}

/* Applies the subset of ICoreWebView2Settings that WebKitGTK can actually
 * honour. See wv2l_apply_settings_params for why the rest are not faked here.
 *
 * Studio sets AreDefaultContextMenusEnabled=FALSE; until this existed the shim
 * stored that and did nothing, so right-clicking inside the Toolbox produced
 * WebKit's own Back/Forward/Stop/Reload menu, which real WebView2 never shows. */
gboolean webview_apply_settings(struct native_webview *nv, gboolean script_enabled,
                                 gboolean dev_tools_enabled, gboolean context_menus_enabled)
{
    WebKitSettings *settings;

    if (!nv)
    {
        fprintf(stderr, "webview2loader-host: apply_settings on a stale/NULL webview -- ignoring\n");
        return FALSE;
    }

    nv->default_context_menus_enabled = context_menus_enabled;

    settings = webkit_web_view_get_settings(nv->view);
    if (!settings) return FALSE;
    webkit_settings_set_enable_javascript(settings, script_enabled);
    webkit_settings_set_enable_developer_extras(settings, dev_tools_enabled);

    fprintf(stderr, "webview2loader-host: settings applied on nv=%p: script=%d devtools=%d "
                    "context_menus=%d\n", (void *)nv, script_enabled, dev_tools_enabled,
            context_menus_enabled);
    return TRUE;
}

gboolean webview_set_user_agent(struct native_webview *nv, const char *user_agent_utf8)
{
    WebKitSettings *settings;
    char *trimmed;
    const char *start;
    size_t len;

    if (!nv || !user_agent_utf8)
    {
        fprintf(stderr, "webview2loader-host: set_user_agent on a stale/NULL webview -- ignoring\n");
        return FALSE;
    }

    /* Studio hands this over with a LEADING SPACE (observed:
     * " RobloxStudio/WinInet RobloxApp/0.735...."). WebKit validates the value
     * as an HTTP header, where leading/trailing whitespace is not allowed, so
     * webkit_settings_set_user_agent rejected it outright with
     * "assertion 'isValidUserAgentHeaderValue' failed" and the agent silently
     * stayed at whatever it was.
     *
     * Trimmed rather than passed through: the surrounding whitespace carries no
     * meaning, and refusing an agent Studio deliberately set -- over a space --
     * would be worse than normalising it. */
    start = user_agent_utf8;
    while (*start == ' ' || *start == '\t') start++;
    len = strlen(start);
    while (len && (start[len - 1] == ' ' || start[len - 1] == '\t')) len--;

    if (!len)
    {
        fprintf(stderr, "webview2loader-host: refusing an empty user agent\n");
        return FALSE;
    }

    /* A control character cannot be trimmed away and would fail the same
     * validation, so reject rather than hand WebKit something it will refuse. */
    {
        size_t i;
        for (i = 0; i < len; i++)
        {
            if ((unsigned char)start[i] < 0x20 || (unsigned char)start[i] == 0x7f)
            {
                fprintf(stderr, "webview2loader-host: user agent contains a control character at "
                                "offset %zu -- refusing, WebKit would reject it anyway\n", i);
                return FALSE;
            }
        }
    }

    settings = webkit_web_view_get_settings(nv->view);
    if (!settings) return FALSE;

    trimmed = g_strndup(start, len);
    webkit_settings_set_user_agent(settings, trimmed);

    /* Read back rather than assuming: the previous version logged "user agent
     * set" unconditionally and said so even on the run where WebKit had
     * rejected the value, which is exactly the kind of false success that costs
     * an investigation round. */
    {
        const char *now = webkit_settings_get_user_agent(settings);
        gboolean ok = (now && !strcmp(now, trimmed));

        fprintf(stderr, "webview2loader-host: user agent %s on nv=%p: %s\n",
                ok ? "set" : "REJECTED BY WEBKIT", (void *)nv, trimmed);
        g_free(trimmed);
        return ok;
    }
}

gboolean webview_add_user_script(struct native_webview *nv, const char *script_utf8)
{
    WebKitUserContentManager *manager;
    WebKitUserScript *script;

    if (!nv || !script_utf8)
    {
        fprintf(stderr, "webview2loader-host: add_user_script on a stale/NULL webview -- ignoring\n");
        return FALSE;
    }

    manager = webkit_web_view_get_user_content_manager(nv->view);
    if (!manager)
    {
        fprintf(stderr, "webview2loader-host: no WebKitUserContentManager for nv=%p -- cannot install "
                        "Studio's document-start script\n", (void *)nv);
        return FALSE;
    }

    /* DOCUMENT_START and ALL_FRAMES for the same reasons as the bridge shim
     * below: the script has to run before any page script feature-detects what
     * it installs, and Roblox's pages use iframes. NULL world_name is the
     * page's own main script world -- an isolated world would hide Studio's
     * bridge from the very page that needs it. */
    script = webkit_user_script_new(script_utf8,
                                     WEBKIT_USER_CONTENT_INJECT_ALL_FRAMES,
                                     WEBKIT_USER_SCRIPT_INJECT_AT_DOCUMENT_START,
                                     NULL, NULL);
    webkit_user_content_manager_add_script(manager, script);
    webkit_user_script_unref(script);

    fprintf(stderr, "webview2loader-host: installed Studio document-start script (%zu bytes) on nv=%p\n",
            strlen(script_utf8), (void *)nv);
    return TRUE;
}

/* Installs the window.chrome.webview bridge shim on `nv`.
 *
 * Started life as the F-08 diagnostic probe and is now the actual transport for
 * both directions of the web-message channel -- page -> Studio through
 * on_script_message above, Studio -> page through the shim's own
 * __tuxblox_wv2_deliver entry point. */
static void webview_install_message_probe(struct native_webview *nv)
{
    WebKitUserContentManager *manager = webkit_web_view_get_user_content_manager(nv->view);
    WebKitUserScript *script;

    if (!manager)
    {
        fprintf(stderr, "webview2loader-host: no WebKitUserContentManager for nv=%p -- web-message "
                        "bridge not installed\n", (void *)nv);
        return;
    }

    /* NULL world_name == the page's own main script world, which is where
     * window.chrome has to exist for the page to see it. */
    if (!webkit_user_content_manager_register_script_message_handler(manager, "tuxblox", NULL))
    {
        fprintf(stderr, "webview2loader-host: could not register the 'tuxblox' script message "
                        "handler for nv=%p -- web-message bridge not installed\n", (void *)nv);
        return;
    }
    g_signal_connect_data(manager, "script-message-received::tuxblox",
                           (GCallback)on_script_message, nv, NULL, 0);

    /* DOCUMENT_START so window.chrome.webview exists before any page script
     * runs -- a page that feature-detects it at load time must see it. ALL
     * FRAMES because an OAuth flow can run in an iframe. */
    script = webkit_user_script_new(WV2_MESSAGE_SHIM_JS,
                                     WEBKIT_USER_CONTENT_INJECT_ALL_FRAMES,
                                     WEBKIT_USER_SCRIPT_INJECT_AT_DOCUMENT_START,
                                     NULL, NULL);
    webkit_user_content_manager_add_script(manager, script);
    webkit_user_script_unref(script);
    fprintf(stderr, "webview2loader-host: window.chrome.webview bridge installed for nv=%p\n",
            (void *)nv);
}

static void webview_unmanage_window(GtkWidget *window)
{
    GtkNative *native;
    GdkSurface *surface;
    GdkDisplay *gdisplay;
    Display *display;
    XSetWindowAttributes attrs;

    gtk_widget_realize(window);
    native = gtk_widget_get_native(window);
    surface = native ? gtk_native_get_surface(native) : NULL;
    if (!surface || !GDK_IS_X11_SURFACE(surface))
        return; /* no X window to mark (Wayland backend) -- nothing to do */

    gdisplay = gdk_surface_get_display(surface);
    if (!gdisplay || !GDK_IS_X11_DISPLAY(gdisplay))
        return;
/* Same GDK_DEPRECATED_IN_4_18 situation geometry.c documents at length for
 * gdk_x11_surface_get_xid/gdk_x11_display_get_xdisplay: no non-deprecated way
 * to reach a real X display or XID exists. */
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
    display = gdk_x11_display_get_xdisplay(gdisplay);
    if (display)
    {
        Window xid = gdk_x11_surface_get_xid(GDK_X11_SURFACE(surface));

        attrs.override_redirect = True;
        XChangeWindowAttributes(display, xid, CWOverrideRedirect, &attrs);
        fprintf(stderr, "webview2loader-host: xid=0x%lx set override-redirect before first map "
                        "-- window manager will not manage or list it\n", xid);
    }
#pragma GCC diagnostic pop
}

struct native_webview *webview_create(int is_message_only)
{
    struct native_webview *nv = calloc(1, sizeof(*nv));

    /* Code review fix (original unixlib.c): calloc() can fail under real
     * memory pressure -- dereferencing NULL two lines below would kill
     * this process's single GTK main-loop thread (and every future
     * dispatch on it). Returning NULL here is already a real, handled
     * failure path -- main.c's WV2L_OP_CREATE_WEBVIEW dispatch case
     * reports handle = 0, exactly the existing wire "failure" convention,
     * so simply not creating anything here is sufficient, no new
     * signaling needed. */
    if (!nv)
    {
        fprintf(stderr, "webview2loader-host: calloc failed for a new native_webview -- "
                         "out of memory, failing create\n");
        return NULL;
    }

    nv->window = gtk_window_new();
    /* Task 7 crash fix, round 13 (original unixlib.c comment): set before
     * the window is ever shown (below) so the window manager never draws
     * chrome on it even momentarily; unconditional (not gated on
     * is_message_only) since a message-only controller's window is never
     * shown at all, so this is a harmless no-op for that case rather than
     * something that needs its own branch. */
    gtk_window_set_decorated(GTK_WINDOW(nv->window), FALSE);
    /* Task 7 crash fix, round 14 -- see on_close_request's own comment
     * just above for the full crash evidence and reasoning. Connected
     * unconditionally (both is_message_only and real controllers can, in
     * principle, end up window-manager-visible/-addressable), before the
     * window is ever shown, so there's no window in existence yet that
     * could receive a close request before this handler is wired up. No
     * disconnect/cleanup needed -- the connection's lifetime is exactly
     * nv->window's own lifetime, torn down together by webview_destroy's
     * own gtk_window_destroy call. */
    g_signal_connect_data(nv->window, "close-request", (GCallback)on_close_request,
                           NULL, NULL, 0);
    nv->view = WEBKIT_WEB_VIEW(webkit_web_view_new());
    {
        /* Before anything navigates -- see WV2L_DEFAULT_USER_AGENT's comment. */
        WebKitSettings *st = webkit_web_view_get_settings(nv->view);
        if (st) webkit_settings_set_user_agent(st, WV2L_DEFAULT_USER_AGENT);
    }
    /* Real WebView2's default is TRUE; Studio turns it off explicitly, and that
     * put_ now reaches webview_apply_settings above. */
    nv->default_context_menus_enabled = TRUE;
    g_signal_connect_data(nv->view, "context-menu", (GCallback)on_context_menu, nv, NULL, 0);
    /* F-08 spike (diagnostic only -- no message is delivered to the Wine side
     * yet). Roblox Studio registers BOTH add_WebMessageReceived and
     * add_NavigationStarting before it navigates, and this shim implements
     * neither, so we do not know which one its login flow actually depends on.
     * Real WebView2 exposes window.chrome.webview.postMessage to the page; if
     * Studio's login page uses it to hand back the auth result, that is the
     * mechanism to build, and if the page never calls it then the redirect to
     * roblox-studio-auth: (i.e. NavigationStarting) is, and the whole message
     * bridge would be the wrong thing to build.
     *
     * This installs just enough to answer that: a document-start user script
     * defining window.chrome.webview.postMessage on top of WebKitGTK's own
     * webkit.messageHandlers channel, plus a handler that logs whatever
     * arrives. Cheap, reversible, and it makes the next login attempt decide
     * the design instead of a guess. */
    webview_install_message_probe(nv);
    /* Task 6 addition -- see this file's own top-of-file comment and
     * navigate.c's own comment on on_decide_policy for the full rationale.
     * Connected unconditionally, same reasoning as the close-request
     * connection just above -- even a message-only (HWND_MESSAGE/
     * CookieManager) controller has a real, live WebKitWebView that could
     * in principle navigate through this same OAuth flow. No disconnect/
     * cleanup needed -- the connection's lifetime is exactly nv->view's own
     * lifetime, torn down together by webview_destroy's own
     * gtk_window_destroy call (which destroys nv->view along with
     * nv->window, still its child at that point). */
    /* user_data is nv (was NULL): on_decide_policy now needs to know WHICH
     * webview a navigation belongs to, so it can name it in the
     * NavigationStarting event it sends to Studio. */
    g_signal_connect_data(nv->view, "decide-policy", (GCallback)on_decide_policy,
                           nv, NULL, 0);
    /* 2026-08-15 cosmetic mitigation for a real, understood WebKitGTK/NVIDIA
     * Skia GPU-teardown race in WebKitWebProcess's own shutdown path (a
     * genuine upstream WebKitGTK/NVIDIA bug -- not anything in this file or
     * this process -- see navigate.c's own comment on
     * on_web_process_terminated for the full real-signal/enum verification
     * against this bundle's actual built WebKitGTK 2.52.5 headers/source,
     * and .superpowers/sdd/2026-08-14-webview2loader-host-process/
     * webprocess-terminated-mitigation-report.md for the full writeup).
     * Connected unconditionally, same reasoning and same lifetime as the
     * decide-policy connection just above (torn down together by
     * webview_destroy's own gtk_window_destroy call, no separate
     * disconnect/cleanup needed) -- user_data is nv itself (unlike
     * decide-policy, which doesn't need it), so the handler can mark this
     * specific webview's state and wake any in-flight wait on it; see
     * webview.h's own comment on struct native_webview's process_terminated/
     * active_wait_loop fields. */
    g_signal_connect_data(nv->view, "web-process-terminated", (GCallback)on_web_process_terminated,
                           nv, NULL, 0);
    gtk_window_set_child(GTK_WINDOW(nv->window), GTK_WIDGET(nv->view));
    /* Plan 3 Task 2 (original unixlib.c comment): HWND_MESSAGE-parented
     * controllers (the CookieManager flow) still need a real, live
     * WebKitWebView -- Navigate/GetCookies/DeleteAllCookies all operate on
     * it -- but must never show a visible top-level window (real
     * WebView2's own HWND_MESSAGE semantics: the browser process runs,
     * there is simply no visible top-level HWND). */
    /* gtk_widget_show() (what the original unixlib.c called through its
     * p_gtk_widget_show function pointer) is deprecated as of GTK 4.10 in
     * favor of gtk_widget_set_visible() -- caught here, not in the old
     * file, because the old p_-prefixed indirection was just a hand-typed
     * function-pointer declaration with no deprecation annotation attached;
     * linking against the real header (which does carry the annotation)
     * turns this into a hard error under this build's -Werror. Using
     * gtk_widget_set_visible(widget, TRUE) rather than gtk_window_present()
     * is the literal behavioral equivalent of the old show() call (no
     * implicit focus-grab/present side effect), not just the first
     * deprecation-fix suggestion in the compiler note. */
    if (!is_message_only)
    {
        webview_unmanage_window(nv->window);
        gtk_widget_set_visible(nv->window, TRUE);
    }

    live_webview_register(nv); /* Task 7 UAF guard -- see webview_lookup's own comment above */
    return nv;
}

void webview_destroy(struct native_webview *nv)
{
    if (!nv) return;

    /* Task 7 UAF guard -- see webview_lookup's own comment above.
     * Unregister strictly before freeing, and before the GTK/WebKit call
     * below too: every other dispatch handler that checks this registry
     * runs serialized on this same GTK main-loop thread, so once this line
     * has executed, any such handler still queued behind this one for the
     * same (now-dying) handle will see it as no-longer-live. */
    live_webview_unregister(nv);
    /* If a bounded wait in navigate.c is currently blocked on this webview
     * (nested GMainLoop -- see wait_ended_with_nv_alive there), wake it now.
     * Its own guard already keeps it from touching nv after this point, so
     * this is purely about not making it sit out the rest of its 10s/30s
     * timeout waiting for a completion that can no longer arrive. Read before
     * the free below, obviously; g_main_loop_quit only sets a flag, so the
     * waiter unwinds when the current dispatch returns, not re-entrantly from
     * inside this call. */
    if (nv->active_wait_loop)
    {
        fprintf(stderr, "webview2loader-host: destroying native_webview %p with a wait in flight -- "
                        "waking it so it fails now instead of at its timeout\n", (void *)nv);
        g_main_loop_quit(nv->active_wait_loop);
        nv->active_wait_loop = NULL;
    }
    /* Crash fix -- see watchdog.h's own top-of-file comment: drop any live
     * watch this nv has on a reparented-into parent_xid before nv is freed
     * below, so a later DestroyNotify/ReparentNotify for that (by-then-
     * recycled) parent_xid value can never dereference a dangling nv
     * pointer. Also reached when watchdog.c itself is the one calling this
     * function (its own crash-mitigation teardown path) -- a harmless,
     * idempotent no-op there, since that path already cleared its own
     * watched[] entry for nv immediately before calling here. */
    watchdog_untrack(nv);
    /* Crash fix -- see geometry.h's own comment on geometry_unreparent for
     * the full real-coredump evidence and root-cause reasoning: a still-
     * X11-reparented nv->window can make GTK4's own gtk_window_destroy()
     * below hit the same fatal GDK internal-consistency assertion
     * watchdog.c defends against for an externally-triggered destroy --
     * except this trigger is entirely self-inflicted (a completely normal
     * Close()/Release() while still reparented, no external actor, no
     * second controller, no watchdog event involved at all). Restoring the
     * X11 hierarchy to what GDK's own bookkeeping still believes it is
     * (an ordinary top-level, effectively parented to the real root
     * window) before the real destroy call closes that gap. No-op (and
     * never fatal) if nv was never reparented in the first place. */
    geometry_unreparent(nv);
    /* Destroying nv->window tears down nv->view along with it (still its
     * child at this point), so this is the one native GTK/WebKit call
     * needed per webview. */
    gtk_window_destroy(GTK_WINDOW(nv->window));
    free(nv);
}
