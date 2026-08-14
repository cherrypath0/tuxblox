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

/* webkitgtk-bundle/host/main.c
 *
 * Entry point for webview2loader-host: a separate GTK4/WebKitGTK process
 * that Task 3's unixlib.c (Wine side) talks to over a socket, using the
 * wire protocol in webview2loader_ipc_protocol.h. This task only wires up
 * the dispatch loop skeleton -- WV2L_OP_INIT is handled for real (there's
 * nothing more to initialize yet beyond GTK itself, done in main() below),
 * every other opcode is a stub that consumes its request struct and replies
 * with an honest all-zero/failure response so the wire stays framed. Tasks
 * 4-6 replace each stub with the real ported handler.
 *
 * Deviation from the plan brief's example, found while actually compiling
 * this against the real GTK4 headers this bundle builds (build-in-container.sh
 * links against pkg-config's "gtk4", not a GTK3 gtk+-3.0): GTK4 removed
 * gtk_main()/gtk_main_quit() entirely (no declaration anywhere in
 * $PREFIX/include/gtk-4.0/gtk/gtkmain.h -- confirmed directly against the
 * installed headers, `-Werror` caught this immediately as
 * implicit-function-declaration) and changed gtk_init_check() to take no
 * arguments (it used to take argc/argv in GTK3; GTK4 apps get their command
 * line from GApplication instead, which this helper doesn't use). The
 * standard GTK4-era replacement for a bare, non-GApplication event loop is a
 * plain GLib GMainLoop, used below -- functionally identical to what
 * gtk_main()/gtk_main_quit() did, just GLib's own API instead of GTK's now
 * removed wrapper around it.
 */
#include <gtk/gtk.h>
#include <glib-unix.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/prctl.h>
#include <signal.h>
#include "ipc.h"
#include "webview2loader_ipc_protocol.h"

static int g_ipc_fd = -1;
static GMainLoop *g_loop = NULL;

/* Reads one opcode + its struct, dispatches, writes the struct back.
 * Returns FALSE (stopping the GSource / ending the process) once the
 * socket is gone -- matches unixlib.c's own "helper died" detection on
 * the other end (Task 3). */
static gboolean on_ipc_readable(gint fd, GIOCondition condition, gpointer user_data)
{
    uint32_t opcode;
    if (ipc_read_full(fd, &opcode, sizeof(opcode)) < 0) { g_main_loop_quit(g_loop); return G_SOURCE_REMOVE; }

    switch (opcode)
    {
    case WV2L_OP_INIT:
    {
        struct wv2l_init_params p;
        if (ipc_read_full(fd, &p, sizeof(p)) < 0) { g_main_loop_quit(g_loop); return G_SOURCE_REMOVE; }
        p.success = 1;
        if (ipc_write_full(fd, &p, sizeof(p)) < 0) { g_main_loop_quit(g_loop); return G_SOURCE_REMOVE; }
        return G_SOURCE_CONTINUE;
    }
    /* Every other opcode: read its struct (still need to consume the exact
     * byte count so the stream stays framed for whatever request comes
     * next), respond with an all-zero/failure struct. Tasks 4-6 replace
     * each case with the real ported handler; until then this is an
     * honest "not implemented yet", never a hang or a protocol
     * desync. */
    case WV2L_OP_CREATE_WEBVIEW:
    {
        struct wv2l_create_webview_params p;
        if (ipc_read_full(fd, &p, sizeof(p)) < 0) { g_main_loop_quit(g_loop); return G_SOURCE_REMOVE; }
        p.handle = 0;
        if (ipc_write_full(fd, &p, sizeof(p)) < 0) { g_main_loop_quit(g_loop); return G_SOURCE_REMOVE; }
        return G_SOURCE_CONTINUE;
    }
    default:
        fprintf(stderr, "webview2loader-host: unknown/unimplemented opcode %u\n", opcode);
        g_main_loop_quit(g_loop);
        return G_SOURCE_REMOVE;
    }
}

int main(int argc, char **argv)
{
    (void)argc;
    (void)argv;

    const char *fd_env = getenv("WEBVIEW2LOADER_IPC_FD");
    if (!fd_env) { fprintf(stderr, "webview2loader-host: WEBVIEW2LOADER_IPC_FD not set\n"); return 1; }
    g_ipc_fd = atoi(fd_env);

    /* If Wine dies without cleanly closing the socket, don't become an
     * orphan spinning forever -- see design spec's Lifecycle section. */
    prctl(PR_SET_PDEATHSIG, SIGTERM);

    if (!gtk_init_check())
    {
        fprintf(stderr, "webview2loader-host: gtk_init_check failed\n");
        return 1;
    }

    g_loop = g_main_loop_new(NULL, FALSE);
    g_unix_fd_add(g_ipc_fd, G_IO_IN, on_ipc_readable, NULL);
    g_main_loop_run(g_loop);
    g_main_loop_unref(g_loop);
    return 0;
}
