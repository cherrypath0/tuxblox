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

/* Shows one message box and reports which button was pressed.
 *
 * Windows draws a program's hard error itself, in a message box the program
 * expects the user to answer -- Roblox's says to press OK to collect its
 * support files. The compatibility layer sees that request on the side that
 * cannot draw anything, so it starts this instead: a Windows program, so the
 * box is the same one every other dialog goes through and looks like the rest
 * of TuxBlox rather than like whatever the desktop happens to ship.
 *
 *   tuxbloxdialog.exe <title> <text> [--ok-cancel]
 *
 * Exit code 0 means the affirmative button, 1 means anything else.
 */
#include <windows.h>

int WINAPI wWinMain( HINSTANCE inst, HINSTANCE prev, WCHAR *cmdline, int show )
{
    int argc = 0;
    WCHAR **argv = CommandLineToArgvW( GetCommandLineW(), &argc );
    UINT flags = MB_OK | MB_ICONERROR | MB_SETFOREGROUND | MB_TOPMOST;
    const WCHAR *title, *text;

    if (!argv || argc < 3) return 1;
    title = argv[1];
    text = argv[2];
    if (argc > 3 && !lstrcmpW( argv[3], L"--ok-cancel" )) flags = MB_OKCANCEL | MB_ICONERROR | MB_SETFOREGROUND | MB_TOPMOST;

    return MessageBoxW( NULL, text, title, flags ) == IDOK ? 0 : 1;
}
