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

/* webkitgtk-bundle/host/ipc.c -- see ipc.h for the contract. */
#include "ipc.h"
#include <unistd.h>
#include <errno.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <sys/socket.h>

ssize_t ipc_read_full(int fd, void *buf, size_t len)
{
    size_t done = 0;
    while (done < len)
    {
        ssize_t n = read(fd, (char *)buf + done, len - done);
        if (n == 0) return -1; /* peer closed -- treat as failure, caller decides what that means */
        if (n < 0)
        {
            if (errno == EINTR) continue;
            return -1;
        }
        done += (size_t)n;
    }
    return (ssize_t)done;
}

ssize_t ipc_write_full(int fd, const void *buf, size_t len)
{
    size_t done = 0;
    while (done < len)
    {
        /* send(MSG_NOSIGNAL) rather than write(): if the Wine side has exited,
         * writing to this socket would raise SIGPIPE and kill this process
         * during what is otherwise an ordinary, recoverable "peer is gone".
         * Matches the same change made on unixlib.c's side of the boundary. */
        ssize_t n = send(fd, (const char *)buf + done, len - done, MSG_NOSIGNAL);
        if (n < 0)
        {
            if (errno == EINTR) continue;
            return -1;
        }
        done += (size_t)n;
    }
    return (ssize_t)done;
}

/* --- Event channel --- see ipc.h for the contract. */

static int g_event_fd = -1;

void ipc_set_event_fd(int fd)
{
    g_event_fd = fd;
}

int ipc_event_fd(void) { return g_event_fd; }
void ipc_close_event_fd(void) { g_event_fd = -1; }

/* Same framing as ipc_send_event, with the body split in two so a
 * variable-length event can avoid a fixed maximum-size buffer. */
int ipc_send_event_payload(unsigned int type, const void *head, size_t head_len,
                            const void *tail, size_t tail_len)
{
    if (ipc_send_event(type, head, head_len) != 0) return -1;
    if (tail_len && ipc_write_full(ipc_event_fd(), tail, tail_len) != (ssize_t)tail_len)
    {
        fprintf(stderr, "webview2loader-host: event payload could not be delivered (%s) -- the "
                        "stream is now unframed, closing the event channel\n", strerror(errno));
        ipc_close_event_fd();
        return -1;
    }
    return 0;
}

int ipc_send_event(unsigned int type, const void *payload, size_t len)
{
    uint32_t wire_type = (uint32_t)type;

    if (g_event_fd < 0) return -1; /* no channel (older Wine side) -- caller falls back */

    /* Type and payload go out as two writes on a SOCK_STREAM socket, which is
     * safe here for the same reason it is on the request channel: this process
     * is the only writer, and it only ever writes from the single main-loop
     * thread, so two frames can never interleave. */
    if (ipc_write_full(g_event_fd, &wire_type, sizeof(wire_type)) != (ssize_t)sizeof(wire_type) ||
        ipc_write_full(g_event_fd, payload, len) != (ssize_t)len)
    {
        fprintf(stderr, "webview2loader-host: event %u could not be delivered (%s) -- closing the "
                        "event channel; the caller falls back to its old behaviour\n",
                type, strerror(errno));
        /* Once a partial frame has gone out the stream is unframed and every
         * later event would be garbage, so the channel is finished. Closing
         * makes ipc_send_event fail fast (and honestly) from here on rather
         * than corrupting the Wine side's reader. */
        g_event_fd = -1;
        return -1;
    }
    return 0;
}
