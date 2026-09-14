/*
 * Client/Server Runtime Subsystem
 *
 * Copyright 2026 TuxBlox Developers
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

/*
 * Every Windows session runs csrss.exe, and software that looks at the process
 * list notices when it is absent.  Wine has never needed one because the work
 * it does on Windows is spread across the Unix side and win32u here, so this
 * holds the name and nothing else: it starts, waits, and exits when the session
 * ends.
 */

#include <windows.h>

int WINAPI wWinMain( HINSTANCE inst, HINSTANCE prev, WCHAR *cmdline, int show )
{
    /* Nothing to do but hold the name. Sleeping alertably rather than looping
     * keeps the process off the scheduler until the session ends it. */
    for (;;) SleepEx( INFINITE, TRUE );
}
