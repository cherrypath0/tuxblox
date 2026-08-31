/*
 * Ordinals of ntdll's unix-interface exports
 *
 * Copyright (C) 2026 TuxBlox Developers
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301, USA
 */

#ifndef __WINE_WINE_NTDLLORDINALS_H
#define __WINE_WINE_NTDLLORDINALS_H

/* ntdll exports its unix interface by ordinal only, because a name beginning
 * with "__wine_" in ntdll's export table identifies Wine on its own. Anything
 * that used to resolve one of them by name uses these instead. They must stay
 * in sync with dlls/ntdll/ntdll.spec. */

#define NTDLL_ORDINAL_CTRL_ROUTINE                  8
#define NTDLL_ORDINAL_SYSCALL_DISPATCHER            1516
#define NTDLL_ORDINAL_UNIX_CALL_DISPATCHER          1517
#define NTDLL_ORDINAL_UNIX_CALL_DISPATCHER_ARM64EC  1518
#define NTDLL_ORDINAL_UNIXLIB_HANDLE                1519

#endif /* __WINE_WINE_NTDLLORDINALS_H */
