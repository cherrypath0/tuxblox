/*
 * TuxBlox - Linux Compatibility Layer for the Roblox Engine
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

/* compat/webkitgtk/bundle/useragent-verify.c
 *
 * Checks that patch 5 in compat/webkitgtk/src/README-TUXBLOX-PATCHES.md is
 * present and still correctly bounded in the WebKitGTK built into the
 * webkitgtk-prefix volume.
 *
 * Upstream WebKit allows at most one "/" per User-Agent product, which rejects
 * the Roblox Player's own agent outright and silently leaves the agent at
 * WebKit's default -- so every request from the webview told Roblox's servers it
 * was an ordinary desktop browser. The patch widens the grammar; this checks:
 *
 *   1. The Player's real agent is accepted (the patch is present).
 *   2. A malformed agent is still rejected (the patch did not turn the
 *      validator into "accept anything", which would be a different bug).
 *   3. Studio's agent is still accepted (it always was -- a regression guard).
 *
 * Given a URL it also loads it with the Player's agent, so a server at the far
 * end can report the agent that genuinely reached the wire. That is the part a
 * read-back from WebKitSettings cannot prove on its own, and it is where the
 * original bug actually bit: verify-useragent.sh runs a throwaway local server
 * and asserts on what arrived, for the document and for two subresources.
 *
 * Re-run this against the webkitgtk-prefix volume with:
 *
 *   compat/webkitgtk/bundle/verify-useragent.sh
 */

#include <string.h>
#include <gtk/gtk.h>
#include <webkit/webkit.h>

/* Observed in a real session log, 2026-10-01. The two slashes are the point. */
#define PLAYER_AGENT "RobloxNewBrowser Roblox/WinInetRobloxApp/0.740.0.7400927 " \
                     "(GlobalDist; RobloxDirectDownload) GAMEPADNAVIGATION"
#define STUDIO_AGENT "RobloxStudio/WinInet RobloxApp/0.735.0.7350614 " \
                     "(GlobalDist; RobloxDirectDownload)"
/* An unclosed comment, so still invalid however many slashes are allowed. */
#define BAD_AGENT    "Roblox/WinInet RobloxApp/(bad"

static gboolean agent_sticks(const char *agent)
{
    WebKitSettings *settings = webkit_settings_new();
    const char *now;
    gboolean ok;

    webkit_settings_set_user_agent(settings, agent);
    now = webkit_settings_get_user_agent(settings);
    ok = (now && !strcmp(now, agent));
    g_object_unref(settings);
    return ok;
}

static gboolean check(const char *what, const char *agent, gboolean want)
{
    gboolean got = agent_sticks(agent);

    g_print("%-6s %-8s %s\n", got == want ? "ok" : "FAIL",
            got ? "accepted" : "rejected", what);
    return got == want;
}

static gboolean quit_now(gpointer loop)
{
    g_main_loop_quit(loop);
    return G_SOURCE_REMOVE;
}

/* Loads the URL so the server at the far end can report what it received. */
static void load_with_player_agent(const char *url)
{
    GtkWidget *window = gtk_window_new();
    WebKitWebView *view = WEBKIT_WEB_VIEW(webkit_web_view_new());
    GMainLoop *loop = g_main_loop_new(NULL, FALSE);

    webkit_settings_set_user_agent(webkit_web_view_get_settings(view), PLAYER_AGENT);
    gtk_window_set_child(GTK_WINDOW(window), GTK_WIDGET(view));
    gtk_window_present(GTK_WINDOW(window));
    webkit_web_view_load_uri(view, url);

    g_timeout_add_seconds(8, quit_now, loop);
    g_main_loop_run(loop);
    g_main_loop_unref(loop);
}

int main(int argc, char **argv)
{
    gboolean ok = TRUE;

    gtk_init();

    ok &= check("the Player's agent is accepted", PLAYER_AGENT, TRUE);
    ok &= check("a malformed agent is still rejected", BAD_AGENT, FALSE);
    ok &= check("Studio's agent is accepted", STUDIO_AGENT, TRUE);

    if (!ok)
    {
        g_printerr("useragent-verify: patch 5 is missing or mis-scoped -- see "
                   "compat/webkitgtk/src/README-TUXBLOX-PATCHES.md\n");
        return 1;
    }

    if (argc > 1) load_with_player_agent(argv[1]);
    return 0;
}
