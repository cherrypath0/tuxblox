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
        ssize_t n = write(fd, (const char *)buf + done, len - done);
        if (n < 0)
        {
            if (errno == EINTR) continue;
            return -1;
        }
        done += (size_t)n;
    }
    return (ssize_t)done;
}
