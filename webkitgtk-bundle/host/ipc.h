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

/* webkitgtk-bundle/host/ipc.h
 *
 * Socket framing helpers for webview2loader-host's side of the IPC boundary
 * described in webview2loader_ipc_protocol.h. Task 3's unixlib.c client
 * implements matching functions with this exact signature/error convention
 * on the Wine side -- keep the two in sync if this ever changes.
 */
#ifndef WV2L_HOST_IPC_H
#define WV2L_HOST_IPC_H
#include <stddef.h>
#include <sys/types.h>

/* Loops over read()/write() until len bytes are moved or a real error/EOF
 * occurs. Returns len on full success, -1 otherwise (errno set by the
 * underlying syscall, or set to 0 with a 0 return value from the
 * underlying call meaning a clean EOF -- callers only need "did the full
 * transfer succeed", not the exact syscall-level reason). */
ssize_t ipc_read_full(int fd, void *buf, size_t len);
ssize_t ipc_write_full(int fd, const void *buf, size_t len);

#endif
