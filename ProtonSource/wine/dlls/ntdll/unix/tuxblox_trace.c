/*
 * TuxBlox fingerprint tracer
 *
 * Diagnostic instrumentation originally built for the Roblox Player exit-187
 * investigation (item 6/8/13), later extended to cover Roblox Studio and its
 * WebView2 helper process for item 6's still-open login-hang investigation.
 * Records which fingerprint-relevant Nt* calls the target process makes and
 * which PE-side code called them. It observes only -- no value returned to
 * the caller is altered anywhere in this facility.
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
#include <stdio.h>
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

/* Compared lowercase against the tail of the image path. Written out as
 * WCHAR arrays rather than L"" literals so this stays independent of the
 * unixlib's wide-string helpers.
 *
 * Three targets, spanning both investigations this tracer has been used for:
 * robloxplayerbeta.exe (item 6/8/13's original exit-187 work) and
 * robloxstudiobeta.exe + msedgewebview2.exe (item 6's still-open WebView2
 * login-hang: that failure lives entirely in Studio and its WebView2 helper
 * process, never in Player, so the original Player-only check would record
 * nothing for it). */
static const WCHAR target_player[] =
    {'r','o','b','l','o','x','p','l','a','y','e','r','b','e','t','a','.','e','x','e'};
static const WCHAR target_studio[] =
    {'r','o','b','l','o','x','s','t','u','d','i','o','b','e','t','a','.','e','x','e'};
static const WCHAR target_webview[] =
    {'m','s','e','d','g','e','w','e','b','v','i','e','w','2','.','e','x','e'};

static const struct { const WCHAR *name; SIZE_T len; } trace_targets[] =
{
    { target_player,  ARRAY_SIZE(target_player) },
    { target_studio,  ARRAY_SIZE(target_studio) },
    { target_webview, ARRAY_SIZE(target_webview) },
};

static BOOL image_is_target(void)
{
    const UNICODE_STRING *image;
    SIZE_T path_len, i, t;

    if (!peb || !peb->ProcessParameters) return FALSE;
    image = &peb->ProcessParameters->ImagePathName;
    if (!image->Buffer) return FALSE;

    path_len = image->Length / sizeof(WCHAR);

    for (t = 0; t < ARRAY_SIZE(trace_targets); t++)
    {
        const WCHAR *name = trace_targets[t].name;
        SIZE_T len = trace_targets[t].len;
        const WCHAR *tail;
        BOOL match = TRUE;

        if (path_len < len) continue;
        tail = image->Buffer + path_len - len;

        for (i = 0; i < len; i++)
        {
            WCHAR c = tail[i];
            if (c >= 'A' && c <= 'Z') c += 'a' - 'A';
            if (c != name[i]) { match = FALSE; break; }
        }
        if (match) return TRUE;
    }
    return FALSE;
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

/* Emit the current /proc/self/maps. Called twice per traced process: once
 * at the first record, and again at exit.
 *
 * The first-hit dump is taken at process startup, when only a few dozen
 * mappings exist -- Wine's PE modules, where essentially every interesting
 * caller lives, are mapped later. It is therefore useless for symbolizing
 * most records, and is kept only as a fallback for a process that dies
 * without reaching NtTerminateProcess. The exit dump is the complete one,
 * and the analyser prefers the last block it sees. */
static void dump_maps(void)
{
    char *buf, *line, *next;
    size_t cap = 512 * 1024, len = 0;
    ssize_t n;
    int fd;

    if (!(buf = malloc( cap ))) return;
    if ((fd = open( "/proc/self/maps", O_RDONLY )) == -1)
    {
        free( buf );
        return;
    }
    while (len < cap - 1 && (n = read( fd, buf + len, cap - 1 - len )) > 0) len += n;
    close( fd );
    buf[len] = 0;

    TRACE_(tuxblox)( "MAPS-BEGIN\n" );
    for (line = buf; line && *line; line = next)
    {
        if ((next = strchr( line, '\n' ))) *next++ = 0;
        TRACE_(tuxblox)( "MAPS %s\n", line );
    }
    free( buf );
}

static void dump_maps_once(void)
{
    static LONG done;

    if (InterlockedCompareExchange( &done, 1, 0 )) return;
    dump_maps();
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

    dump_maps();
    tuxblox_trace_tally_dump();
    TRACE_(tuxblox)( "EXIT code=%d how=%s last_seq=%d rip=0x%llx\n",
                     (int)exit_code, how, (int)trace_seq,
                     (unsigned long long)get_syscall_caller_pc() );
}

/* Always-on, independent of the opt-in tuxblox_trace_enabled() tracer above
 * (which needs WINEDEBUG=+tuxblox and has real per-call overhead, so it stays
 * off by default -- see launch.sh). The OS-level process exit status Proton
 * and the launcher see is truncated to a single unsigned byte (0-255), which
 * loses real information: e.g. Hyperion terminates Roblox with exit_code
 * -2147467260 (0x80004004, E_ABORT), not anything in the 0-255 range. This
 * writes that real, non-truncated value to this process's own stderr --
 * which Proton and the launcher already capture into their own log files --
 * as a single greppable marker line, so the crash-report popup can show the
 * real underlying code instead of just the truncated one.
 *
 * Scoped to the same three targets as the tracer via image_is_target(): only
 * Player/Studio/WebView2's own real exit code is meaningful to surface here,
 * not every internal Wine helper process's (services.exe, explorer.exe,
 * etc., which call NtTerminateProcess on themselves constantly as part of
 * ordinary operation). Uses a raw write(), not stdio, since this can run in
 * the last few instructions before the process actually terminates. */
void tuxblox_report_real_exit_code( LONG exit_code )
{
    char buf[64];
    int len;

    if (!image_is_target()) return;

    len = snprintf( buf, sizeof(buf), "TUXBLOX_REAL_EXIT_CODE=%d\n", (int)exit_code );
    if (len > 0) write( 2, buf, (size_t)len );
}


/* Per-syscall-number counters. Counters only, never per-call output: the
 * point is to detect whether Hyperion bypasses the Win32 layer entirely,
 * and per-call logging here would perturb timing badly enough to change the
 * behavior being measured (see the item 6 WebView2 findings). */
/* Wine encodes frame->syscall_id as the raw syscall register value: bits
 * 12-13 select the syscall table, the low 12 bits are the number within it
 * (see signal_x86_64.c, "syscall table number" / "syscall number"). So
 * win32u syscalls arrive as 0x1000+n, not 0..n, and the maximum encodable
 * id is 0x3FFF. Size for the whole space -- a smaller bound silently drops
 * every win32u syscall, which would read as "never touched" rather than
 * "not counted". 16384 LONGs is 64KB of BSS, paid once. */
#define TUXBLOX_MAX_SYSCALL 16384
static LONG syscall_tally[TUXBLOX_MAX_SYSCALL];

void tuxblox_trace_tally( unsigned int syscall_id )
{
    if (!tuxblox_trace_enabled()) return;
    if (syscall_id < TUXBLOX_MAX_SYSCALL) InterlockedIncrement( &syscall_tally[syscall_id] );
}

void tuxblox_trace_tally_dump(void)
{
    unsigned int i;
    const char *name;

    if (!tuxblox_trace_enabled()) return;
    for (i = 0; i < TUXBLOX_MAX_SYSCALL; i++)
    {
        if (!syscall_tally[i]) continue;
        name = ntdll_syscall_name( i );
        if (name)
            TRACE_(tuxblox)( "TALLY syscall=%u (%s) count=%d\n", i, name, (int)syscall_tally[i] );
        else
            TRACE_(tuxblox)( "TALLY syscall=%u count=%d\n", i, (int)syscall_tally[i] );
    }
}
