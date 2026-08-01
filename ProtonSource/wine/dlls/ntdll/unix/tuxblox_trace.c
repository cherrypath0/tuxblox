/*
 * TuxBlox fingerprint tracer
 *
 * Diagnostic instrumentation for the Roblox Player exit-187 investigation.
 * Records which fingerprint-relevant Nt* calls the Player makes and which
 * PE-side code called them. It observes only -- no value returned to the
 * caller is altered anywhere in this facility.
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

#include "config.h"

#include <fcntl.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "ntstatus.h"
#define WIN32_NO_STATUS
#include "windef.h"
#include "winternl.h"
#include "unix_private.h"
#include "wine/debug.h"

WINE_DECLARE_DEBUG_CHANNEL(tuxblox);

/* -1 = not yet determined, 0 = off, 1 = on. Resolved once and cached: the
 * choke points sit on hot paths (NtQuerySystemInformation especially) and
 * must not pay a string comparison per call. */
static int trace_state = -1;

/* Compared lowercase against the tail of the image path. Written out as a
 * WCHAR array rather than an L"" literal so this stays independent of the
 * unixlib's wide-string helpers. */
static const WCHAR target_exe[] =
    {'r','o','b','l','o','x','p','l','a','y','e','r','b','e','t','a','.','e','x','e'};

static BOOL image_is_target(void)
{
    const UNICODE_STRING *image;
    SIZE_T path_len, i;
    const WCHAR *tail;

    if (!peb || !peb->ProcessParameters) return FALSE;
    image = &peb->ProcessParameters->ImagePathName;
    if (!image->Buffer) return FALSE;

    path_len = image->Length / sizeof(WCHAR);
    if (path_len < ARRAY_SIZE(target_exe)) return FALSE;
    tail = image->Buffer + path_len - ARRAY_SIZE(target_exe);

    for (i = 0; i < ARRAY_SIZE(target_exe); i++)
    {
        WCHAR c = tail[i];
        if (c >= 'A' && c <= 'Z') c += 'a' - 'A';
        if (c != target_exe[i]) return FALSE;
    }
    return TRUE;
}

BOOL tuxblox_trace_enabled(void)
{
    if (trace_state < 0)
    {
        /* Called before the PEB exists -- answer no, but do not cache it,
         * or every later call inherits this early answer. */
        if (!peb || !peb->ProcessParameters) return FALSE;
        trace_state = (TRACE_ON(tuxblox) && image_is_target()) ? 1 : 0;
    }
    return trace_state == 1;
}

/* One-shot /proc/self/maps dump. Symbolization is deliberately offline:
 * records carry raw addresses, and this map is what lets the analysis script
 * turn them into (module, RVA). Reading the whole file before emitting keeps
 * Wine's per-line trace prefix from landing in the middle of a maps line. */
static void dump_maps_once(void)
{
    static LONG done;
    char *buf, *line, *next;
    size_t cap = 512 * 1024, len = 0;
    ssize_t n;
    int fd;

    if (InterlockedCompareExchange( &done, 1, 0 )) return;
    if (!(buf = malloc( cap ))) return;
    if ((fd = open( "/proc/self/maps", O_RDONLY )) == -1)
    {
        free( buf );
        return;
    }
    while (len < cap - 1 && (n = read( fd, buf + len, cap - 1 - len )) > 0) len += n;
    close( fd );
    buf[len] = 0;

    for (line = buf; line && *line; line = next)
    {
        if ((next = strchr( line, '\n' ))) *next++ = 0;
        TRACE_(tuxblox)( "MAPS %s\n", line );
    }
    free( buf );
}

static LONG trace_seq;

void tuxblox_trace_record( const char *surface, const char *detail )
{
    if (!tuxblox_trace_enabled()) return;
    dump_maps_once();

    /* Note %llx, not %I64x: unix-side Wine debug output is formatted by
     * libc vsnprintf, which does not understand the MS-style specifier. */
    TRACE_(tuxblox)( "REC seq=%d tid=%04x surface=%s rip=0x%llx detail=%s\n",
                     (int)InterlockedIncrement( &trace_seq ),
                     (unsigned int)(ULONG_PTR)NtCurrentTeb()->ClientId.UniqueThread,
                     surface, (unsigned long long)get_syscall_caller_pc(),
                     detail ? detail : "" );
}

/* UNICODE_STRING convenience wrapper -- most fingerprint-relevant details
 * (registry paths, object names) arrive in this form. Truncates rather than
 * allocating: these run on hot paths and the leading path component is the
 * discriminating part. */
void tuxblox_trace_record_us( const char *surface, const UNICODE_STRING *detail )
{
    char buf[256];
    SIZE_T i, len;

    if (!tuxblox_trace_enabled()) return;

    if (!detail || !detail->Buffer) return tuxblox_trace_record( surface, "" );

    len = detail->Length / sizeof(WCHAR);
    if (len > sizeof(buf) - 1) len = sizeof(buf) - 1;
    for (i = 0; i < len; i++)
    {
        WCHAR c = detail->Buffer[i];
        buf[i] = (c >= 0x20 && c < 0x7f) ? (char)c : '?';
    }
    buf[len] = 0;
    tuxblox_trace_record( surface, buf );
}

/* The single most important record in the trace: it is what distinguishes a
 * deliberate Hyperion termination from a Wine bug in its load path. */
void tuxblox_trace_exit( LONG exit_code, const char *how )
{
    if (!tuxblox_trace_enabled()) return;

    tuxblox_trace_tally_dump();
    TRACE_(tuxblox)( "EXIT code=%d how=%s last_seq=%d rip=0x%llx\n",
                     (int)exit_code, how, (int)trace_seq,
                     (unsigned long long)get_syscall_caller_pc() );
}

/* Per-syscall-number counters. Counters only, never per-call output: the
 * point is to detect whether Hyperion bypasses the Win32 layer entirely,
 * and per-call logging here would perturb timing badly enough to change the
 * behavior being measured (see the item 6 WebView2 findings). */
#define TUXBLOX_MAX_SYSCALL 2048
static LONG syscall_tally[TUXBLOX_MAX_SYSCALL];

void tuxblox_trace_tally( unsigned int syscall_id )
{
    if (!tuxblox_trace_enabled()) return;
    if (syscall_id < TUXBLOX_MAX_SYSCALL) InterlockedIncrement( &syscall_tally[syscall_id] );
}

void tuxblox_trace_tally_dump(void)
{
    unsigned int i;

    if (!tuxblox_trace_enabled()) return;
    for (i = 0; i < TUXBLOX_MAX_SYSCALL; i++)
        if (syscall_tally[i])
            TRACE_(tuxblox)( "TALLY syscall=%u count=%d\n", i, (int)syscall_tally[i] );
}
