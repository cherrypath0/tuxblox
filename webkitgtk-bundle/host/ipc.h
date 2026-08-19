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

/* --- Event channel (host -> Wine) ---
 *
 * The second, independent socket described in webview2loader_ipc_protocol.h's
 * own "Event channel" comment. main() installs the fd once at startup from
 * WEBVIEW2LOADER_EVENT_FD; everything else just calls ipc_send_event.
 *
 * ipc_send_event writes one framed event (uint32_t type, then that type's
 * struct) and returns 0 on success, -1 if the event could not be delivered --
 * including the case where no event channel exists at all, which is normal
 * when running against an older Wine side that never created one. Callers MUST
 * treat -1 as "Wine did not get this" and fall back accordingly rather than
 * assuming delivery.
 *
 * Never blocks the caller on a reply: events are one-way by construction, so
 * this cannot stall the GTK main loop the way a synchronous round trip could. */
void ipc_set_event_fd(int fd);
int ipc_send_event(unsigned int type, const void *payload, size_t len);

/* Two-part event send, for an event whose payload is variable-length. See
 * ipc.c; used by the web-message event so it need not ship a fixed 128 KB
 * buffer per message. */
int ipc_send_event_payload(unsigned int type, const void *head, size_t head_len,
                            const void *tail, size_t tail_len);
int ipc_event_fd(void);
void ipc_close_event_fd(void);

#endif
