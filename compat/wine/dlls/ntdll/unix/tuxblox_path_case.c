/*
 * NT path casing
 *
 * Wine names a path's components after the directories on disk, which are
 * lower case, so paths handed to the application read C:\users\... where
 * Windows reads C:\Users\... The prefix cannot be renamed -- a case folding
 * filesystem is needed to open capitalised names, and without one Wine matches
 * a missed path by lower casing the whole request -- so the casing is corrected
 * on the way out instead. Every rewrite here keeps the length it replaces.
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

#if 0
#pragma makedep unix
#endif

#ifndef WINE_UNICODE_CHAR_DEFINED
typedef unsigned short WCHAR;
#define WINE_UNICODE_CHAR_DEFINED
#endif

#ifndef ARRAY_SIZE
#define ARRAY_SIZE(x) (sizeof(x) / sizeof((x)[0]))
#endif

struct name_case
{
    const char *lower;
    const char *canonical;
};

/* Longer names first, so "program files (x86)" is not taken for "program files" */
static const struct name_case root_names[] =
{
    { "program files (x86)", "Program Files (x86)" },
    { "program files",       "Program Files" },
    { "programdata",         "ProgramData" },
    { "windows",             "Windows" },
    { "users",               "Users" },
};

static const struct name_case windows_names[] =
{
    { "system32", "System32" },
    { "syswow64", "SysWOW64" },
};

static WCHAR to_lower( WCHAR c )
{
    return (c >= 'A' && c <= 'Z') ? c + ('a' - 'A') : c;
}

static unsigned int ascii_length( const char *name )
{
    unsigned int len = 0;

    while (name[len]) len++;
    return len;
}

/* The name must run to the end of the component, so "usersdata" is not "users" */
static int component_matches( const WCHAR *path, const char *name )
{
    unsigned int i;

    for (i = 0; name[i]; i++)
        if (to_lower( path[i] ) != (WCHAR)name[i]) return 0;
    return !path[i] || path[i] == '\\';
}

static void write_name( WCHAR *path, const char *name )
{
    unsigned int i;

    for (i = 0; name[i]; i++) path[i] = (WCHAR)name[i];
}

static void canonical_component( WCHAR *path, const struct name_case *names, unsigned int count )
{
    unsigned int i;

    for (i = 0; i < count; i++)
    {
        if (!component_matches( path, names[i].lower )) continue;
        write_name( path, names[i].canonical );
        return;
    }
}

/* Rewrites one name from a directory listing, which arrives without the path it sits in */
void tuxblox_canonical_dir_name( WCHAR *name, int under_windows )
{
    if (!name) return;

    if (under_windows) canonical_component( name, windows_names, ARRAY_SIZE(windows_names) );
    else canonical_component( name, root_names, ARRAY_SIZE(root_names) );
}

/* Rewrites the system directories of an NT path in place, leaving everything else alone */
void tuxblox_canonical_path_case( WCHAR *path )
{
    unsigned int i;
    WCHAR *p;

    if (!path) return;

    /* Only a drive path, \??\X:\, names the directories this corrects */
    for (i = 0; i < 7; i++) if (!path[i]) return;
    if (path[0] != '\\' || path[1] != '?' || path[2] != '?' || path[3] != '\\') return;
    if (path[5] != ':' || path[6] != '\\') return;

    p = path + 7;
    if (!component_matches( p, "windows" ))
    {
        canonical_component( p, root_names, ARRAY_SIZE(root_names) );
        return;
    }

    /* system32 and syswow64 are canonical under windows and nowhere else */
    write_name( p, "Windows" );
    p += ascii_length( "windows" );
    if (*p != '\\') return;
    canonical_component( p + 1, windows_names, ARRAY_SIZE(windows_names) );
}
