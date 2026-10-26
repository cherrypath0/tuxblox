/*
 * Unit test for the NT path casing rewrite
 *
 * Built and run by compat/tuxblox/tests/run-tests.sh, never by the Wine build.
 * It compiles the unit directly, so it lives beside it under the same licence.
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

#include <stdio.h>

#include "tuxblox_path_case.c"

static int failures;

static int same( const WCHAR *a, const WCHAR *b )
{
    while (*a && *a == *b) { a++; b++; }
    return *a == *b;
}

static void print_path( const WCHAR *s )
{
    for (; *s; s++) putchar( *s < 128 ? (char)*s : '?' );
}

static void expect_dir( const WCHAR *input, int under_windows, const WCHAR *want )
{
    WCHAR buffer[256];
    unsigned int i;

    for (i = 0; i < 255 && input[i]; i++) buffer[i] = input[i];
    buffer[i] = 0;

    tuxblox_canonical_dir_name( buffer, under_windows );
    if (same( buffer, want )) return;

    failures++;
    printf( "  FAIL: " );
    print_path( input );
    printf( " (under_windows=%d)\n     got: ", under_windows );
    print_path( buffer );
    printf( "\n    want: " );
    print_path( want );
    printf( "\n" );
}

static void expect( const WCHAR *input, const WCHAR *want )
{
    WCHAR buffer[256];
    unsigned int i;

    for (i = 0; i < 255 && input[i]; i++) buffer[i] = input[i];
    buffer[i] = 0;

    tuxblox_canonical_path_case( buffer );
    if (same( buffer, want )) return;

    failures++;
    printf( "  FAIL: " );
    print_path( input );
    printf( "\n     got: " );
    print_path( buffer );
    printf( "\n    want: " );
    print_path( want );
    printf( "\n" );
}

int main( void )
{
    /* The profile path, which is what the Roblox client writes into its log */
    expect( L"\\??\\C:\\users\\cherry\\AppData\\Local", L"\\??\\C:\\Users\\cherry\\AppData\\Local" );
    expect( L"\\??\\C:\\users", L"\\??\\C:\\Users" );

    /* System directories, including the two that are only canonical under windows\ */
    expect( L"\\??\\C:\\windows\\system32\\ntdll.dll", L"\\??\\C:\\Windows\\System32\\ntdll.dll" );
    expect( L"\\??\\C:\\windows\\syswow64\\ntdll.dll", L"\\??\\C:\\Windows\\SysWOW64\\ntdll.dll" );
    expect( L"\\??\\C:\\programdata\\Roblox", L"\\??\\C:\\ProgramData\\Roblox" );
    expect( L"\\??\\C:\\program files\\Roblox", L"\\??\\C:\\Program Files\\Roblox" );
    expect( L"\\??\\C:\\program files (x86)\\Roblox", L"\\??\\C:\\Program Files (x86)\\Roblox" );

    /* Already canonical, and a drive other than C */
    expect( L"\\??\\C:\\Users\\cherry", L"\\??\\C:\\Users\\cherry" );
    expect( L"\\??\\D:\\windows", L"\\??\\D:\\Windows" );

    /* Anchored: only the component directly under the drive root is rewritten */
    expect( L"\\??\\C:\\users\\cherry\\windows\\system32", L"\\??\\C:\\Users\\cherry\\windows\\system32" );
    expect( L"\\??\\C:\\games\\windows", L"\\??\\C:\\games\\windows" );

    /* system32 is canonical only directly under windows, never on its own */
    expect( L"\\??\\C:\\system32", L"\\??\\C:\\system32" );

    /* A longer name that merely starts with a known one must not match */
    expect( L"\\??\\C:\\usersdata\\x", L"\\??\\C:\\usersdata\\x" );
    expect( L"\\??\\C:\\windowsapps\\x", L"\\??\\C:\\windowsapps\\x" );

    /* Nothing to rewrite, and nothing to walk off the end of */
    expect( L"\\??\\C:\\", L"\\??\\C:\\" );
    expect( L"\\??\\C:", L"\\??\\C:" );
    expect( L"", L"" );

    /* Not a drive path at all */
    expect( L"\\??\\unix\\home\\cherry\\users", L"\\??\\unix\\home\\cherry\\users" );

    /* One listed name at a time, which is what a directory listing hands over */
    expect_dir( L"users", 0, L"Users" );
    expect_dir( L"windows", 0, L"Windows" );
    expect_dir( L"programdata", 0, L"ProgramData" );
    expect_dir( L"program files", 0, L"Program Files" );
    expect_dir( L"program files (x86)", 0, L"Program Files (x86)" );
    expect_dir( L"System32", 1, L"System32" );
    expect_dir( L"system32", 1, L"System32" );
    expect_dir( L"syswow64", 1, L"SysWOW64" );

    /* Each table applies only at its own level */
    expect_dir( L"system32", 0, L"system32" );
    expect_dir( L"users", 1, L"users" );

    /* Everything else is left exactly as the filesystem spelled it */
    expect_dir( L"games", 0, L"games" );
    expect_dir( L"usersdata", 0, L"usersdata" );
    expect_dir( L".", 0, L"." );
    expect_dir( L"..", 0, L".." );
    expect_dir( L"", 0, L"" );

    if (failures)
    {
        printf( "path case: %d failed\n", failures );
        return 1;
    }
    printf( "path case: all passed\n" );
    return 0;
}
