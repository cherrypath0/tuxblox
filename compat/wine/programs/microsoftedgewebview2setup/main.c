/*
 * Stand-in for Microsoft's WebView2 runtime bootstrapper.
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

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <wchar.h>

#include "wine/debug.h"

WINE_DEFAULT_DEBUG_CHANNEL(webview2setup);

/* Roblox's installer ships Microsoft's WebView2 bootstrapper and runs it, which
 * spends over a minute installing an Edge runtime nothing here ever loads:
 * webview2loader is builtin and bridges to WebKitGTK instead. Standing in for
 * the bootstrapper skips that while still handing the caller the successful
 * child process it checks for -- blocking the launch outright instead stops the
 * installer on a "Module not found" dialog. */

/* WebView2's client GUID, the documented place an application looks to decide
 * whether the runtime is installed. */
static const WCHAR clients_key[] =
    L"Software\\Microsoft\\EdgeUpdate\\Clients\\{F3017226-FE2A-4295-8BDF-00C3A9A7E4C5}";

/* Matches what webview2loader reports from
 * GetAvailableCoreWebView2BrowserVersionString, so the registry and the API
 * cannot disagree about which runtime is present. */
static const WCHAR runtime_version[] = L"109.0.1518.140";

static void record_runtime(void)
{
    WCHAR existing[64];
    DWORD size = sizeof(existing), type;
    HKEY key;

    /* KEY_WOW64_32KEY: a real per-machine install records itself under the
     * 32-bit view, and that is where the documented check looks. */
    if (RegCreateKeyExW( HKEY_LOCAL_MACHINE, clients_key, 0, NULL, 0,
                         KEY_QUERY_VALUE | KEY_SET_VALUE | KEY_WOW64_32KEY,
                         NULL, &key, NULL ))
    {
        WINE_WARN( "could not open the EdgeUpdate client key\n" );
        return;
    }

    /* Leave a version that is already recorded alone. If the real runtime did
     * get installed, its own version is the truthful answer, and writing a
     * lower one over it would invite Edge's updater to reinstall. */
    if (!RegQueryValueExW( key, L"pv", NULL, &type, (BYTE *)existing, &size ) &&
        type == REG_SZ && size > sizeof(WCHAR) && existing[0])
    {
        WINE_TRACE( "runtime version %s already recorded, leaving it\n", wine_dbgstr_w(existing) );
        RegCloseKey( key );
        return;
    }

    if (RegSetValueExW( key, L"pv", 0, REG_SZ,
                        (const BYTE *)runtime_version, sizeof(runtime_version) ))
        WINE_WARN( "could not record the runtime version\n" );

    RegCloseKey( key );
}

int WINAPI wWinMain( HINSTANCE inst, HINSTANCE prev, LPWSTR cmdline, int show )
{
    WINE_TRACE( "stub: %s\n", wine_dbgstr_w(cmdline) );

    /* An uninstall must not leave the runtime recorded as present. */
    if (!wcsstr( cmdline, L"uninstall" ))
        record_runtime();

    return 0;
}
