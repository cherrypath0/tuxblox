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

#include <errno.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/syscall.h>
#include <linux/perf_event.h>
#include <linux/hw_breakpoint.h>
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
WINE_DECLARE_DEBUG_CHANNEL(seh);

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

/* Per-call logging is the expensive half of this tracer: a write to stderr for
 * every system call, which is slow enough to change what is being watched.
 * Roblox's anti-tamper layer times itself with thousands of rdtsc sites, and
 * under the full trace it takes a path it never takes otherwise -- so the
 * flood has to be separable from the rest. With TUXBLOX_QUIET=1 the named
 * surfaces, the exit code and the tallies are still recorded; only the
 * per-call lines go away. */
static BOOL trace_calls_enabled(void)
{
    static int quiet = -1;

    if (quiet == -1)
    {
        const char *v = getenv( "TUXBLOX_QUIET" );
        quiet = (v && *v && *v != '0') ? 1 : 0;
    }
    return !quiet;
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
/* True for the first few self-directed context restores, which is where the
 * anti-tamper layer resumes after a deliberate fault. Bounded because these
 * run in the thousands once its exception machinery gets going, and only the
 * first handful say anything. */
BOOL tuxblox_trace_resume_dumps(void)
{
    static int done;

    if (!tuxblox_trace_enabled()) return FALSE;
    if (done >= 4) return FALSE;
    done++;
    return TRUE;
}


/* Dump code bytes at an address, for a caller that has just been handed one
 * worth looking at. The anti-tamper layer's own code is decrypted only in
 * memory, so there is no other way to read it. */
void tuxblox_trace_code( const char *tag, ULONG_PTR addr, unsigned int len )
{
    unsigned char buf[32];
    char line[3 * sizeof(buf) + 1];
    unsigned int i, n;
    SIZE_T got;

    if (!tuxblox_trace_enabled() || !addr) return;
    if (len > sizeof(buf)) len = sizeof(buf);

    /* The address comes from a register the program chose, so it need not be
     * readable at all -- and this runs where a fault cannot be caught, so an
     * unguarded read here takes the process down. Read it the way the debugger
     * paths do, which reports a short read instead of raising. */
    got = virtual_uninterrupted_read_memory( (const void *)addr, buf, len );
    if (!got)
    {
        TRACE_(tuxblox)( "CODE %s 0x%llx unreadable\n", tag, (unsigned long long)addr );
        return;
    }
    for (i = n = 0; i < got; i++) n += snprintf( line + n, sizeof(line) - n, "%02x ", buf[i] );
    TRACE_(tuxblox)( "CODE %s 0x%llx %s\n", tag, (unsigned long long)addr, line );
}


/* A control transfer into the thread's own stack, reported once.
 *
 * The anti-tamper layer reaches such an address twice per run: once on purpose
 * (a far jump into 32-bit mode, which lands at zero) and once at 0x51c080,
 * where it is not yet known whether the layer went there deliberately or was
 * sent there by something this build got wrong. The two look identical from
 * the fault alone, and the difference is in the instruction that transferred
 * control -- which lives in the layer's own section, decrypted only in memory.
 *
 * So print the three things that separate them: what is about to run at the
 * faulting address, the top of the stack (whose first slot is the return
 * address if a call put us here), and the bytes ending at that return address,
 * which are the calling instruction itself.
 *
 * Gated on TUXBLOX_DIAG rather than the trace channel: the tracer changes what
 * the layer does, and this has to be readable from an otherwise ordinary run.
 */
static void diag_hex( const char *tag, ULONG_PTR addr, unsigned int len )
{
    unsigned char buf[256];
    char line[3 * sizeof(buf) + 1];
    unsigned int i, n;
    SIZE_T got;

    if (!addr) return;
    if (len > sizeof(buf)) len = sizeof(buf);
    if (!(got = virtual_uninterrupted_read_memory( (const void *)addr, buf, len )))
    {
        ERR_(seh)( "DIAG %s 0x%llx unreadable\n", tag, (unsigned long long)addr );
        return;
    }
    for (i = n = 0; i < got; i++) n += snprintf( line + n, sizeof(line) - n, "%02x ", buf[i] );
    ERR_(seh)( "DIAG %s 0x%llx %s\n", tag, (unsigned long long)addr, line );
}

/* The last raw system calls the layer issued, with the stack pointer each was
 * made on.
 *
 * Its inner loop repeats the same three call sites, so the stack pointer at a
 * given site is the same on every pass -- until it is not. A pass where it
 * moves by eight is the imbalance that later sends a `ret` through a slot that
 * holds data, and this is the cheapest way to see which pass that is. Written
 * from a signal handler, so it is an array store and nothing else.
 */
#define DIAG_RING 4096
static __thread struct { ULONG64 rip, rsp, rax; } diag_ring[DIAG_RING];
static __thread unsigned int diag_ring_pos;
static int diag_enabled_state = -1;
/* One counter across every diagnostic below, so a fixup, a resume and an
 * exception can be put in order against each other. The system-call count
 * cannot do that: several of these happen between two system calls. */
static LONG diag_seq;

static BOOL diag_enabled(void)
{
    if (diag_enabled_state == -1)
    {
        const char *v = getenv( "TUXBLOX_DIAG" );
        diag_enabled_state = (v && *v && *v != '0') ? 1 : 0;
    }
    return diag_enabled_state == 1;
}

static void diag_bp_arm(void);

BOOL tuxblox_diag_enabled(void)
{
    return diag_enabled();
}

/* Every system call the process makes, with the answer it got.
 *
 * The raw-syscall ring above says which calls the layer issued and on what
 * stack, but not what came back, and what comes back is what it branches on.
 * A check that fails is a call that answered differently from Windows, so the
 * status -- and the information class the call asked about, which is the first
 * or second argument of every Query and Set -- has to be in the record too.
 *
 * Sized to hold a whole run rather than the last few hundred. The divergence
 * is at the end, but what a call at the end is doing usually depends on one
 * near the beginning -- which handle is being closed depends on what opened it
 * -- and a ring that stops short cannot answer that. Matches the raw ring, so
 * the two can be read against each other for the whole run.
 */
#define DIAG_CALLS 4096
static __thread struct { UINT id; ULONG64 arg0, arg1, arg2, arg3, ret; } diag_calls[DIAG_CALLS];
static __thread unsigned int diag_calls_pos;
static __thread struct { UINT id; ULONG64 arg0, arg1, arg2, arg3; } diag_call_pending;

void tuxblox_diag_note_call( UINT id, const ULONG_PTR *args, ULONG len )
{
    if (!diag_enabled()) return;
    diag_call_pending.id   = id;
    diag_call_pending.arg0 = len > 0 ? args[0] : 0;
    diag_call_pending.arg1 = len > sizeof(ULONG_PTR) ? args[1] : 0;
    /* the length and the out-pointer: a query's answer usually turns on these */
    diag_call_pending.arg2 = len > 2 * sizeof(ULONG_PTR) ? args[2] : 0;
    diag_call_pending.arg3 = len > 3 * sizeof(ULONG_PTR) ? args[3] : 0;
}

void tuxblox_diag_note_callret( UINT id, ULONG_PTR retval )
{
    unsigned int at;

    if (!diag_enabled()) return;
    at = diag_calls_pos++ % DIAG_CALLS;
    diag_calls[at].id   = id;
    diag_calls[at].ret  = retval;
    /* only trust the arguments if they belong to this call */
    diag_calls[at].arg0 = diag_call_pending.id == id ? diag_call_pending.arg0 : 0;
    diag_calls[at].arg1 = diag_call_pending.id == id ? diag_call_pending.arg1 : 0;
    diag_calls[at].arg2 = diag_call_pending.id == id ? diag_call_pending.arg2 : 0;
    diag_calls[at].arg3 = diag_call_pending.id == id ? diag_call_pending.arg3 : 0;
}

/* Every named section the program tries to open, and whether it was there.
 *
 * The layer opens a section, queries it, and then reads a pointer out of one
 * of its own globals. If the open fails there is nothing to store, so the name
 * it asked for and the answer it got are the two things worth seeing.
 */
/* One query, with the first few words of whatever it answered.
 *
 * The layer compares what the loader says about a section against what the
 * kernel says about the mapping it actually has. Which fields those are is the
 * question, so the buffer is shown raw rather than interpreted.
 */
void tuxblox_diag_note_query( const char *what, unsigned int class, ULONG64 addr,
                              const void *buffer, unsigned int status )
{
    char line[256];
    unsigned int i, n = 0;
    ULONG64 words[6] = { 0 };

    if (!diag_enabled()) return;
    if (buffer && !status)
    {
        virtual_uninterrupted_read_memory( buffer, words, sizeof(words) );
        for (i = 0; i < 6; i++)
            n += snprintf( line + n, sizeof(line) - n, "%016llx ", (unsigned long long)words[i] );
    }
    else line[0] = 0;
    ERR_(seh)( "DIAG %s class=%u addr=0x%llx -> %08x  %s\n", what, class,
               (unsigned long long)addr, status, line );
}

void tuxblox_diag_note_open_section( const OBJECT_ATTRIBUTES *attr, unsigned int status )
{
    char name[256];
    unsigned int i, len = 0;

    if (!diag_enabled()) return;
    if (attr && attr->ObjectName && attr->ObjectName->Buffer)
    {
        len = attr->ObjectName->Length / sizeof(WCHAR);
        if (len > sizeof(name) - 1) len = sizeof(name) - 1;
        for (i = 0; i < len; i++)
        {
            WCHAR c = attr->ObjectName->Buffer[i];
            name[i] = (c >= 0x20 && c < 0x7f) ? (char)c : '?';
        }
    }
    name[len] = 0;
    ERR_(seh)( "DIAG NtOpenSection \"%s\" -> %08x\n", name, status );
}

/* Where the Player sleeps out a whole timeout and then calls it a wait.
 *
 * The run ends on a thirty second gap in which the thread issues no system call
 * at all, prints "Wait timeout expired" and aborts. Every wait in sync.c is
 * traced except NtDelayExecution, which when it is not alertable is a plain
 * select() -- so the gap is a Sleep, and nothing in any trace says where it is.
 *
 * Long delays only, with the Windows stack above the call, because the call
 * site is the only thing that leads to the check the layer makes when it wakes.
 */
void tuxblox_diag_note_delay( BOOLEAN alertable, const LARGE_INTEGER *timeout )
{
    ULONG64 sp, pc;
    LONGLONG when;
    unsigned int i;

    static unsigned int dumped;

    if (!diag_enabled() || !timeout) return;
    when = timeout->QuadPart;
    /* a millisecond, in the 100ns units the timeout is counted in, either sign.
     * A wait spun out of short sleeps looks like no wait at all otherwise. */
    if (when > -10000 && when < 10000) return;

    sp = get_syscall_caller_sp();
    pc = get_syscall_caller_pc();
    ERR_(seh)( "DIAG NtDelayExecution alertable=%u timeout=%lld pc=0x%llx sp=0x%llx\n",
               alertable, (long long)when, (unsigned long long)pc, (unsigned long long)sp );

    /* the stack above the call, which is what leads to the check it makes when
     * it wakes -- only for the long ones, and only a few times */
    if (when > -10000000 && when < 10000000) return;
    if (dumped++ >= 4) return;
    diag_hex( "delay-pc-64", (ULONG_PTR)pc - 64, 128 );

    for (i = 0; i < 64; i++)
    {
        ULONG64 slot;

        if (!virtual_uninterrupted_read_memory( (const char *)(ULONG_PTR)sp + i * 8,
                                                &slot, sizeof(slot) ))
            continue;
        ERR_(seh)( "DIAG delay sp+%#x = 0x%llx\n", i * 8, (unsigned long long)slot );
        /* a return address into a loaded image: show the instruction that
         * called, and what it does with the answer when it comes back */
        if ((slot > 0x6ffff0000000ull && slot < 0x700000000000ull) ||
            (slot > 0x7ff600000000ull && slot < 0x800000000000ull))
        {
            diag_hex( "delay-ret-64", (ULONG_PTR)slot - 64, 64 );
            diag_hex( "delay-ret+0", (ULONG_PTR)slot, 128 );
        }
    }
}

void tuxblox_diag_note_syscall( ULONG64 rip, ULONG64 rsp, ULONG64 rax )
{
    static unsigned int shown;
    unsigned int i;

    if (!diag_enabled()) return;
    diag_bp_arm();
    tuxblox_diag_dump_ldr();
    tuxblox_diag_dump_keys( rip, rsp );
    tuxblox_diag_xpage_arm();
    tuxblox_diag_wpage_arm();
    /* The SIGSYS path resumes the caller at rip + 0xb, which is the shape of
     * ntdll's own stub. The layer issues its system calls from code it
     * generates itself, so what its stubs look like decides whether that
     * resume address is an instruction boundary at all. */
    if (shown < 6 && rip < 0x6fffffc00000ull)
    {
        shown++;
        diag_hex( "stub", (ULONG_PTR)rip - 16, 32 );
    }
    /* ntdll's stub pushes nothing before the syscall, so rsp here is the
     * caller's rsp less the return address the call pushed: 8 mod 16 whenever
     * the caller kept to the ABI, 0 when it did not. That makes every system
     * call a free alignment sample, and the point where it flips is where the
     * layer's eight-byte offset begins. */
    {
        static int last_parity = -1;
        int parity = (int)(rsp % 16);

        if (parity != last_parity)
        {
            ERR_(seh)( "DIAG parity now %d at call %u rip=0x%llx rsp=0x%llx\n",
                       parity, diag_ring_pos, (unsigned long long)rip,
                       (unsigned long long)rsp );
            last_parity = parity;
        }
    }
    i = diag_ring_pos++ % DIAG_RING;
    diag_ring[i].rip = rip;
    diag_ring[i].rsp = rsp;
    diag_ring[i].rax = rax;
}

/* Single-step the last stretch of the run.
 *
 * The failure is a `ret` to a slot that holds data, and nothing short of the
 * instruction stream says which return it was. The stretch is short and
 * identical on every run, so it can be stepped: arm the trap flag on the way
 * back from a chosen system call, record (rip, rsp) per instruction without
 * ever telling the program a single-step happened, and print the tail when the
 * fault lands.
 *
 * TUXBLOX_DIAG_STEP=<n> arms it after the n-th raw system call; take n from the
 * "total=" figure an unstepped TUXBLOX_DIAG run prints. Stepping is visible to
 * a program that reads its own EFLAGS, so this is a diagnostic of last resort,
 * not something to leave on.
 */
#define DIAG_STEPS (1 << 19)
/* rcx and rax as well as the program counter: the branches that decide the
 * failing path are `test ecx,ecx`, and the only way to know which way one went
 * is to have the value it tested. */
static struct { ULONG64 rip, rsp, rcx, rax; } diag_steps[DIAG_STEPS];
static unsigned int diag_step_pos, diag_step_start, diag_step_limit;
static ULONG64 diag_step_stop_rsp, diag_step_stop_lo, diag_step_stop_hi, diag_step_below;
static unsigned int diag_step_hold, diag_step_held;
static unsigned int diag_step_cap = 4 * 1024 * 1024;
static ULONG diag_step_tid = (ULONG)-1, diag_step_owner;
static ULONG64 diag_step_module_base;
static ULONG diag_step_reject[16];
static unsigned int diag_step_rejects;
static ULONG64 module_base_of( ULONG64 addr, char *name, size_t namelen );

/* Where the layer's image starts, asked of the loader rather than of
 * /proc/self/maps: its code pages are anonymous, not file-backed, so the maps
 * cannot name them. Watch offsets are relative to this. */
static ULONG64 layer_image_base( ULONG64 rip )
{
    LIST_ENTRY *head, *cur;
    unsigned int n = 0;

    if (!peb || !peb->LdrData) return 0;
    head = &peb->LdrData->InLoadOrderModuleList;
    for (cur = head->Flink; cur && cur != head && n < 128; cur = cur->Flink, n++)
    {
        LDR_DATA_TABLE_ENTRY *m = (LDR_DATA_TABLE_ENTRY *)cur;  /* InLoadOrderLinks at off 0 */
        ULONG64 base = (ULONG64)(ULONG_PTR)m->DllBase;
        const WCHAR *name = m->BaseDllName.Buffer;
        SIZE_T len = m->BaseDllName.Length / sizeof(WCHAR), i;
        BOOL is_roblox = FALSE;

        if (rip < base || rip >= base + m->SizeOfImage) continue;
        /* ntdll and the rest are in this list too, and stepping one of them
         * spends the whole trace on a thread nobody is looking at. */
        for (i = 0; name && i + 6 <= len; i++)
        {
            static const WCHAR robloxW[] = {'r','o','b','l','o','x'};
            SIZE_T j;

            for (j = 0; j < 6; j++)
            {
                WCHAR c = name[i + j];

                if (c >= 'A' && c <= 'Z') c += 'a' - 'A';
                if (c != robloxW[j]) break;
            }
            if (j == 6) { is_roblox = TRUE; break; }
        }
        if (is_roblox) return base;
        return 0;
    }
    return 0;
}
/* Where the layer's image starts, without needing an address inside it first.
 *
 * The layer is RobloxPlayerBeta.dll, not the executable of the same name, so
 * matching "roblox" alone finds the wrong module half the time. Asked of the
 * loader rather than of /proc/self/maps, for the reason above: the layer's
 * code pages are anonymous and the maps cannot name them.
 */
static ULONG64 roblox_dll_base(void)
{
    LIST_ENTRY *head, *cur;
    unsigned int n = 0;

    if (!peb || !peb->LdrData) return 0;
    head = &peb->LdrData->InLoadOrderModuleList;
    for (cur = head->Flink; cur && cur != head && n < 128; cur = cur->Flink, n++)
    {
        LDR_DATA_TABLE_ENTRY *m = (LDR_DATA_TABLE_ENTRY *)cur;  /* InLoadOrderLinks at off 0 */
        const WCHAR *name = m->BaseDllName.Buffer;
        SIZE_T len = m->BaseDllName.Length / sizeof(WCHAR), i;

        if (!name || len < 10) continue;                        /* "roblox" + ".dll" */
        if (name[len - 4] != '.' ||
            (name[len - 3] | 0x20) != 'd' ||
            (name[len - 2] | 0x20) != 'l' ||
            (name[len - 1] | 0x20) != 'l') continue;

        for (i = 0; i + 6 <= len; i++)
        {
            static const WCHAR robloxW[] = {'r','o','b','l','o','x'};
            SIZE_T j;

            for (j = 0; j < 6; j++)
                if ((name[i + j] | 0x20) != robloxW[j]) break;
            if (j == 6) return (ULONG64)(ULONG_PTR)m->DllBase;
        }
    }
    return 0;
}

/* Read straight from the signal paths, so an ordinary run pays one load and a
 * predictable branch rather than a call into this file. */
BOOL tuxblox_diag_stepping;

BOOL tuxblox_diag_step_arm(void)
{
    static int start = -1;
    unsigned int i;

    if (!diag_enabled()) return FALSE;
    if (start == -1)
    {
        const char *v = getenv( "TUXBLOX_DIAG_STEP" );

        start = v ? atoi( v ) : 0;
        diag_step_start = start;
        if ((v = getenv( "TUXBLOX_DIAG_STEP_MAX" ))) diag_step_limit = atoi( v );
        if ((v = getenv( "TUXBLOX_DIAG_STEP_STOPRSP" ))) diag_step_stop_rsp = strtoull( v, NULL, 16 );
        if ((v = getenv( "TUXBLOX_DIAG_STEP_STOPLO" ))) diag_step_stop_lo = strtoull( v, NULL, 16 );
        if ((v = getenv( "TUXBLOX_DIAG_STEP_STOPHI" ))) diag_step_stop_hi = strtoull( v, NULL, 16 );
        if ((v = getenv( "TUXBLOX_DIAG_STEP_BELOW" ))) diag_step_below = strtoull( v, NULL, 16 );
        if ((v = getenv( "TUXBLOX_DIAG_STEP_HOLD" ))) diag_step_hold = atoi( v );
        if ((v = getenv( "TUXBLOX_DIAG_STEP_CAP" ))) diag_step_cap = strtoul( v, NULL, 0 );
    }
    /* Armed once only. Re-arming after each system call was tried and is not
     * usable: the layer clears the trap flag itself -- `pushfq; and qword
     * [rsp],~0x100; popfq`, right after its hypervisor probe -- and putting it
     * back changes what the layer does (the run ends 0x80004004 instead). So a
     * trace covers from the arming point to the layer's next such clear. */
    if (!diag_step_start || tuxblox_diag_stepping || diag_ring_pos < diag_step_start) return FALSE;

    /* The raw-syscall ring and this whole diagnostic are process-wide, but the
     * trap flag is not: arming on whichever thread happens to reach the count
     * first single-steps a thread nobody is looking at, and -- at the wrong
     * moment -- one the layer is watching, which changes the run. Pin the
     * thread. TUXBLOX_DIAG_STEP_TID=<hex>, the id as it appears in the log. */
    if (diag_step_tid == (ULONG)-1)
    {
        const char *v = getenv( "TUXBLOX_DIAG_STEP_TID" );
        diag_step_tid = v ? strtoul( v, NULL, 16 ) : 0;
    }
    if (diag_step_tid && GetCurrentThreadId() != diag_step_tid) return FALSE;

    /* Several threads reach the arming count, and stepping one nobody is
     * looking at is what every earlier trace really measured. The layer issues
     * its raw system calls from its own image, so the thread whose last one
     * came from there is the only one worth arming -- and its base is what the
     * watch offsets are relative to. */
    /* Threads that turned out not to be the layer's are remembered, so the
     * next raw system call does not arm the same one again. */
    for (i = 0; i < diag_step_rejects; i++)
        if (diag_step_reject[i] == GetCurrentThreadId()) return FALSE;

    tuxblox_diag_stepping = TRUE;
    diag_step_owner = GetCurrentThreadId();
    ERR_(seh)( "DIAG step armed at raw syscall %u on thread %04x\n",
               diag_ring_pos, (unsigned int)diag_step_owner );
    return TRUE;
}

/* Every SIGTRAP the stepping thread sees, for the first few after arming.
 * A trace that records nothing needs to say whether no trap arrived or whether
 * something consumed it before the recorder ran. */
static unsigned int diag_bp_count;

void tuxblox_diag_note_trap( unsigned int trapno, int si_code, ULONG64 rip, ULONG64 rsp )
{
    static unsigned int shown;

    /* Also when breakpoints are armed and nothing is stepping: a breakpoint
     * that never reports is either an instruction that never ran or a trap
     * that arrived in a shape the handler does not match, and only the trap
     * itself tells the two apart. */
    if (!tuxblox_diag_stepping || GetCurrentThreadId() != diag_step_owner)
    {
        if (!diag_bp_count || !diag_enabled()) return;
    }
    if (shown >= 8) return;
    shown++;
    ERR_(seh)( "DIAG trap #%u trapno=%u si_code=%d rip=0x%llx rsp=0x%llx\n",
               shown, trapno, si_code, (unsigned long long)rip, (unsigned long long)rsp );
}

/* The thread the tracer is stepping, if it is running at all. */
BOOL tuxblox_diag_step_this_thread(void)
{
    return tuxblox_diag_stepping && GetCurrentThreadId() == diag_step_owner;
}

/* Keep the trap flag out of what the program can read, and put it back for
 * the processor.
 *
 * The layer reads its own flags all through its code -- `pushfq; and qword
 * [rsp],~0x100; popfq` appears at every probe site -- so a trace that leaves
 * the bit visible is a trace of the layer noticing it. Hardware keeps
 * stepping; every value handed to the program has the bit taken out.
 */
ULONG tuxblox_diag_step_hide_flags( ULONG eflags )
{
    return tuxblox_diag_step_this_thread() ? eflags & ~0x100 : eflags;
}

ULONG tuxblox_diag_step_show_flags( ULONG eflags )
{
    return tuxblox_diag_step_this_thread() ? eflags | 0x100 : eflags;
}

/* The whole register file at chosen addresses inside the traced window.
 *
 * The step ring holds only the last half million instructions and a run is
 * longer than that, by an amount that varies between runs. The instructions
 * worth reading are known by address, so watch for them instead of hoping they
 * land inside the ring. TUXBLOX_DIAG_STEP_REGS=<hex rip>[,<hex rip>...].
 */
#define DIAG_STEP_WATCH_MAX 8
static ULONG64 diag_step_watch[DIAG_STEP_WATCH_MAX];
static unsigned int diag_step_watch_count;
static int diag_step_watch_init;
static ULONG64 diag_step_stop;
static void diag_dump_steps(void);
static void diag_dump_steps_around( unsigned int at, unsigned int before, unsigned int after );

/* Where the module containing an address begins.
 *
 * Image bases move between runs, so a watch address given as an absolute
 * number only works until something else changes the layout. Anything smaller
 * than 4 GB is taken as an offset into the module the trace starts in, which
 * is the layer's own, and resolved once. */
static ULONG64 module_base_of( ULONG64 addr, char *name, size_t namelen )
{
    char line[512], want[256] = "", path[256];
    unsigned long long start, end, best = 0;
    FILE *maps;

    if (!(maps = fopen( "/proc/self/maps", "r" ))) return 0;
    while (fgets( line, sizeof(line), maps ))
    {
        path[0] = 0;
        if (sscanf( line, "%llx-%llx %*s %*s %*s %*s %255s", &start, &end, path ) < 2) continue;
        if (addr >= start && addr < end) { strcpy( want, path ); break; }
    }
    if (want[0])
    {
        if (name && namelen)
        {
            size_t n = strlen( want );

            if (n > namelen - 1) n = namelen - 1;
            memcpy( name, want, n );
            name[n] = 0;
        }
        rewind( maps );
        while (fgets( line, sizeof(line), maps ))
        {
            path[0] = 0;
            if (sscanf( line, "%llx-%llx %*s %*s %*s %*s %255s", &start, &end, path ) < 2) continue;
            if (!strcmp( path, want ) && (!best || start < best)) best = start;
        }
    }
    fclose( maps );
    return best;
}

BOOL tuxblox_diag_step_watch_hit( ULONG64 rip )
{
    unsigned int i;

    if (!tuxblox_diag_stepping) return FALSE;
    if (!diag_step_watch_init)
    {
        const char *v = getenv( "TUXBLOX_DIAG_STEP_REGS" );

        const char *stop = getenv( "TUXBLOX_DIAG_STEP_STOP" );

        diag_step_watch_init = 1;
        while (v && *v && diag_step_watch_count < DIAG_STEP_WATCH_MAX)
        {
            char *end;

            diag_step_watch[diag_step_watch_count++] = strtoull( v, &end, 16 );
            v = (*end == ',') ? end + 1 : end;
        }
        if (stop && *stop) diag_step_stop = strtoull( stop, NULL, 16 );
    }
    /* A run is longer than the ring and its length varies, so the interesting
     * stretch is not reliably in it. Stopping at a chosen address leaves the
     * ring holding exactly the instructions that led up to it. */
    if (diag_step_stop && rip == (diag_step_stop < 0x100000000ull ? diag_step_stop + diag_step_module_base : diag_step_stop))
    {
        ERR_(seh)( "DIAG step stop at 0x%llx\n", (unsigned long long)rip );
        diag_dump_steps();
        tuxblox_diag_stepping = FALSE;
        return TRUE;
    }
    for (i = 0; i < diag_step_watch_count; i++)
    {
        ULONG64 want = diag_step_watch[i];

        if (want < 0x100000000ull) want += diag_step_module_base;
        if (want == rip) return TRUE;
    }
    return FALSE;
}

void tuxblox_diag_step_watch_regs( ULONG64 rip, const ULONG64 *regs )
{
    static const char * const names[16] = { "rax", "rbx", "rcx", "rdx", "rsi", "rdi", "rbp", "rsp",
                                            "r8", "r9", "r10", "r11", "r12", "r13", "r14", "r15" };
    char line[512];
    unsigned int i, n = 0;

    for (i = 0; i < 16; i++)
        n += snprintf( line + n, sizeof(line) - n, "%s=%llx ", names[i], (unsigned long long)regs[i] );
    ERR_(seh)( "DIAG watch rip=0x%llx %s\n", (unsigned long long)rip, line );
}

BOOL tuxblox_diag_step_record( ULONG64 rip, ULONG64 rsp, ULONG64 rcx, ULONG64 rax )
{
    if (!tuxblox_diag_stepping) return FALSE;
    /* Only the thread that armed it. Another thread reaching here is stepping
     * for no reason and its steps would be interleaved into the same ring. */
    if (GetCurrentThreadId() != diag_step_owner) return FALSE;
    /* A `pushfq` puts the live flags on the program's own stack, where the
     * next instruction reads them. Take the trap flag back off the value it
     * has just pushed. */
    if (diag_step_pos)
    {
        ULONG64 prev = diag_steps[(diag_step_pos - 1) % DIAG_STEPS].rip, flags;
        unsigned char op;

        if (virtual_uninterrupted_read_memory( (const char *)(ULONG_PTR)prev, &op, 1 ) && op == 0x9c &&
            virtual_uninterrupted_read_memory( (const char *)(ULONG_PTR)rsp, &flags, sizeof(flags) ) &&
            (flags & 0x100))
        {
            flags &= ~(ULONG64)0x100;
            virtual_uninterrupted_write_memory( (char *)(ULONG_PTR)rsp, &flags, sizeof(flags) );
        }
    }
    if (!diag_step_pos)
    {
        char name[256] = "";

        diag_step_module_base = layer_image_base( rip );
        if (!diag_step_module_base) module_base_of( rip, name, sizeof(name) );
        if (!diag_step_module_base)
        {
            /* not the layer's thread: stop, remember it, and let the next raw
             * system call arm a different one. The trap is still swallowed. */
            ERR_(seh)( "DIAG step thread %04x is at %s, disarming\n",
                       (unsigned int)diag_step_owner, name[0] ? name : "(anon)" );
            if (diag_step_rejects < ARRAY_SIZE(diag_step_reject))
                diag_step_reject[diag_step_rejects++] = diag_step_owner;
            diag_step_module_base = 0;
            tuxblox_diag_stepping = FALSE;
            return TRUE;
        }
        ERR_(seh)( "DIAG first step rip=0x%llx rsp=0x%llx image base 0x%llx\n",
                   (unsigned long long)rip, (unsigned long long)rsp,
                   (unsigned long long)diag_step_module_base );
    }
    /* A run that never reaches the fault must not step forever.
     * TUXBLOX_DIAG_STEP_CAP raises the ceiling for a long window; stepping runs
     * at tens of microseconds an instruction, so raising it costs real time. */
    if (diag_step_pos >= diag_step_cap)
    {
        tuxblox_diag_stepping = FALSE;
        return FALSE;
    }
    diag_steps[diag_step_pos % DIAG_STEPS].rip = rip;
    diag_steps[diag_step_pos % DIAG_STEPS].rsp = rsp;
    diag_steps[diag_step_pos % DIAG_STEPS].rcx = rcx;
    diag_steps[diag_step_pos % DIAG_STEPS].rax = rax;
    diag_step_pos++;

    /* Stop and print once the stack pointer has stayed below a watermark for a
     * chosen number of observed instructions.
     * TUXBLOX_DIAG_STEP_BELOW=<hex> TUXBLOX_DIAG_STEP_HOLD=<n>.
     *
     * This is what separates the frame losing eight bytes from an ordinary
     * `pushf`, which looks identical for the three instructions it lasts. The
     * loss is permanent -- afterwards the pointer never comes back -- so the
     * count grows without bound after it and never exceeds a handful before.
     * Only an observed value at or above the watermark resets it, so a trace
     * that runs in bursts still accumulates correctly across the gaps.
     */
    /* How far the trace has actually got, and how long it has been below the
     * watermark. Without this a run that fires nothing is indistinguishable
     * from a run where stepping barely ran at all. */
    if (diag_step_pos % 200000 == 0)
        ERR_(seh)( "DIAG step progress total=%u held=%u rip=0x%llx rsp=0x%llx\n",
                   diag_step_pos, diag_step_held,
                   (unsigned long long)rip, (unsigned long long)rsp );

    if (diag_step_below)
    {
        if (rsp < diag_step_below) diag_step_held++;
        else diag_step_held = 0;

        if (diag_step_hold && diag_step_held >= diag_step_hold)
        {
            unsigned int back = diag_step_held < diag_step_pos ? diag_step_held : diag_step_pos;

            ERR_(seh)( "DIAG step held below 0x%llx for %u instructions, rip=0x%llx\n",
                       (unsigned long long)diag_step_below, diag_step_held,
                       (unsigned long long)rip );
            /* the transition is exactly diag_step_held records back */
            diag_dump_steps_around( diag_step_pos - back, 12, 28 );
            diag_dump_steps();
            tuxblox_diag_stepping = FALSE;
            diag_step_start = 0;
            return TRUE;
        }
    }

    /* Stop and print when the stack pointer reaches a chosen value.
     * TUXBLOX_DIAG_STEP_STOPRSP=<hex>. An address cannot catch this: the
     * question is where the stack pointer stops coming back, and the code that
     * takes it there is not known in advance -- which is the whole point of
     * looking. The value is.
     *
     * STOPLO/STOPHI narrow it to a range of the layer's own image, because a
     * stack pointer on its own is ambiguous: the value the frame drops to when
     * the bytes go missing is also the value it passes through on any ordinary
     * call, and the two are told apart by whether the code running is the
     * function that lost them or the one it called.
     */
    if (diag_step_stop_rsp && rsp == diag_step_stop_rsp &&
        (!diag_step_stop_hi ||
         (rip >= diag_step_stop_lo + diag_step_module_base &&
          rip <= diag_step_stop_hi + diag_step_module_base)))
    {
        ERR_(seh)( "DIAG step stop at rsp=0x%llx rip=0x%llx\n",
                   (unsigned long long)rsp, (unsigned long long)rip );
        diag_dump_steps();
        tuxblox_diag_stepping = FALSE;
        diag_step_start = 0;
        return TRUE;
    }

    /* Stop and print after a chosen number of instructions.
     * TUXBLOX_DIAG_STEP_MAX=<n>. The other two ways out -- reaching a chosen
     * address, or the run faulting -- both depend on the run getting somewhere,
     * and neither fires when the layer clears the trap flag first or the run
     * ends on a stack overflow, which reaches no handler. Counting always
     * fires. */
    if (diag_step_limit && diag_step_pos >= diag_step_limit)
    {
        ERR_(seh)( "DIAG step limit %u reached\n", diag_step_limit );
        diag_dump_steps();
        tuxblox_diag_stepping = FALSE;
        diag_step_start = 0;          /* and do not arm again */
        return TRUE;
    }
    return TRUE;
}

/* Everything from here to the end of the block reads an x86-64 CONTEXT by
 * name or dumps 64-bit addresses. What this file diagnoses is 64-bit -- the
 * Player is a 64-bit process and the fault being chased is in its 64-bit
 * unwinder -- so the 32-bit build gets stubs rather than a second copy
 * written against the i386 register names.
 */
#ifdef __x86_64__

/* The whole register file at a fault or a resume.
 *
 * The layer builds the offsets it reads the TEB at while it runs -- a constant,
 * an xmm register and a rotate -- so the bytes at the faulting address do not
 * say which field is being read. With the registers at that moment each offset
 * can be worked out by hand, which is the only way to name those reads: none of
 * them is a system call, so nothing else in this file can see them.
 */
static void diag_regs( const char *tag, const CONTEXT *context )
{
    ERR_(seh)( "DIAG %s rax=%016llx rbx=%016llx rcx=%016llx rdx=%016llx\n", tag,
               (unsigned long long)context->Rax, (unsigned long long)context->Rbx,
               (unsigned long long)context->Rcx, (unsigned long long)context->Rdx );
    ERR_(seh)( "DIAG %s rsi=%016llx rdi=%016llx rbp=%016llx rsp=%016llx\n", tag,
               (unsigned long long)context->Rsi, (unsigned long long)context->Rdi,
               (unsigned long long)context->Rbp, (unsigned long long)context->Rsp );
    ERR_(seh)( "DIAG %s r8 =%016llx r9 =%016llx r10=%016llx r11=%016llx\n", tag,
               (unsigned long long)context->R8, (unsigned long long)context->R9,
               (unsigned long long)context->R10, (unsigned long long)context->R11 );
    ERR_(seh)( "DIAG %s r12=%016llx r13=%016llx r14=%016llx r15=%016llx\n", tag,
               (unsigned long long)context->R12, (unsigned long long)context->R13,
               (unsigned long long)context->R14, (unsigned long long)context->R15 );
    ERR_(seh)( "DIAG %s rip=%016llx eflags=%08x cs=%04x ss=%04x ds=%04x es=%04x fs=%04x gs=%04x\n", tag,
               (unsigned long long)context->Rip, (unsigned int)context->EFlags,
               context->SegCs, context->SegSs, context->SegDs, context->SegEs,
               context->SegFs, context->SegGs );
    ERR_(seh)( "DIAG %s xmm0=%016llx%016llx xmm1=%016llx%016llx\n", tag,
               (unsigned long long)context->Xmm0.High, (unsigned long long)context->Xmm0.Low,
               (unsigned long long)context->Xmm1.High, (unsigned long long)context->Xmm1.Low );
    ERR_(seh)( "DIAG %s xmm2=%016llx%016llx xmm3=%016llx%016llx\n", tag,
               (unsigned long long)context->Xmm2.High, (unsigned long long)context->Xmm2.Low,
               (unsigned long long)context->Xmm3.High, (unsigned long long)context->Xmm3.Low );
}

/* The stretch around one recorded instruction, rather than the tail.
 *
 * What is wanted when a watermark fires is the moment the stack pointer went
 * below it, which may be a hundred thousand instructions back -- printing a
 * tail that long to reach it is unusable. */
static void diag_dump_steps_around( unsigned int at, unsigned int before, unsigned int after )
{
    unsigned int n = diag_step_pos < DIAG_STEPS ? diag_step_pos : DIAG_STEPS;
    unsigned int first = at > before ? at - before : 0, i;

    if (!diag_step_pos) return;
    ERR_(seh)( "DIAG steps around %u (total=%u):\n", at, diag_step_pos );
    for (i = first; i < at + after && i < diag_step_pos; i++)
    {
        unsigned int slot = i % DIAG_STEPS;

        if (diag_step_pos > n && i < diag_step_pos - n) continue;   /* wrapped away */
        ERR_(seh)( "DIAG at[%u] rip=0x%llx rsp=0x%llx rcx=0x%llx rax=0x%llx\n", i,
                   (unsigned long long)diag_steps[slot].rip, (unsigned long long)diag_steps[slot].rsp,
                   (unsigned long long)diag_steps[slot].rcx, (unsigned long long)diag_steps[slot].rax );
    }
}

static void diag_dump_steps(void)
{
    unsigned int n = diag_step_pos < DIAG_STEPS ? diag_step_pos : DIAG_STEPS, i;
    const char *tail;

    if (!diag_step_pos) return;
    /* A full ring is half a million lines, which is why printing it is opt-in.
     * TUXBLOX_DIAG_STEP_TAIL=<n> asks for the last n instead, which is what a
     * question about one short stretch actually wants. */
    if ((tail = getenv( "TUXBLOX_DIAG_STEP_TAIL" )))
    {
        unsigned int want = atoi( tail );

        if (want && want < n) n = want;
    }
    ERR_(seh)( "DIAG steps total=%u, last %u:\n", diag_step_pos, n );
    if (getenv( "TUXBLOX_DIAG_QUIETSTEPS" )) return;
    for (i = 0; i < n; i++)
    {
        unsigned int at = (diag_step_pos - n + i) % DIAG_STEPS;

        ERR_(seh)( "DIAG step[-%u] rip=0x%llx rsp=0x%llx rcx=0x%llx rax=0x%llx\n", n - i,
                   (unsigned long long)diag_steps[at].rip, (unsigned long long)diag_steps[at].rsp,
                   (unsigned long long)diag_steps[at].rcx, (unsigned long long)diag_steps[at].rax );
    }
}

static void diag_dump_ring(void)
{
    unsigned int n = diag_ring_pos < DIAG_RING ? diag_ring_pos : DIAG_RING, i;

    unsigned int m = diag_calls_pos < DIAG_CALLS ? diag_calls_pos : DIAG_CALLS;

    ERR_(seh)( "DIAG calls total=%u\n", diag_calls_pos );
    for (i = 0; i < m; i++)
    {
        unsigned int at = (diag_calls_pos - m + i) % DIAG_CALLS;
        const char *name = ntdll_syscall_name( diag_calls[at].id );

        ERR_(seh)( "DIAG call[-%u] %s id=%04x arg0=0x%llx arg1=0x%llx arg2=0x%llx arg3=0x%llx -> %08x\n",
                   m - i, name ? name : "?", diag_calls[at].id,
                   (unsigned long long)diag_calls[at].arg0,
                   (unsigned long long)diag_calls[at].arg1,
                   (unsigned long long)diag_calls[at].arg2,
                   (unsigned long long)diag_calls[at].arg3,
                   (unsigned int)diag_calls[at].ret );
    }

    ERR_(seh)( "DIAG syscall total=%u\n", diag_ring_pos );
    for (i = 0; i < n; i++)
    {
        unsigned int at = (diag_ring_pos - n + i) % DIAG_RING;

        ERR_(seh)( "DIAG syscall[-%u] rip=0x%llx rsp=0x%llx rax=0x%llx\n", n - i,
                   (unsigned long long)diag_ring[at].rip, (unsigned long long)diag_ring[at].rsp,
                   (unsigned long long)diag_ring[at].rax );
    }
}


/* Every exception the target raises, with the code at the faulting address and
 * at whatever return addresses are on top of its stack.
 *
 * Windows' x86-64 unwinder checks whether a frame's pc sits in a function's
 * epilogue and, if it does, simulates the rest of the epilogue instead of
 * applying the unwind codes -- which would otherwise undo stack adjustments the
 * epilogue has already made. Wine's has no such check. Reading the code around
 * each frame's pc is the only way to tell whether that is what is happening
 * here, since the layer's section is decrypted nowhere but in memory.
 */
/* Where each NtContinue resumes.
 *
 * A handler-driven unwind ends here, and this is the stack pointer the program
 * gets back. Comparing it with the stack pointer the program had before the
 * exception is what says whether the frame came back whole.
 */
void tuxblox_diag_continue( const CONTEXT *context )
{
    static unsigned int seen;

    if (!diag_enabled() || !context || seen >= 64) return;
    seen++;
    ERR_(seh)( "DIAG continue s=%d n=%u rip=0x%llx rsp=0x%llx rbp=0x%llx rax=0x%llx flags=%#x\n",
               (int)InterlockedIncrement( &diag_seq ), diag_ring_pos,
               (unsigned long long)context->Rip, (unsigned long long)context->Rsp,
               (unsigned long long)context->Rbp, (unsigned long long)context->Rax,
               (unsigned int)context->ContextFlags );
    /* Only for a resume into the target itself: what it is about to run, and
     * what is on top of the stack it is being given back. */
    if (context->Rip > 0x6ffffc000000ull && context->Rip < 0x6fffffc00000ull)
    {
        ULONG64 slot = 0;
        unsigned int i;

        diag_regs( "continue", context );
        diag_hex( "resume-at", (ULONG_PTR)context->Rip, 256 );
        for (i = 0; i < 4; i++)
        {
            if (!virtual_uninterrupted_read_memory( (const char *)context->Rsp + i * 8, &slot, sizeof(slot) ))
                continue;
            ERR_(seh)( "DIAG resume stack+%#x 0x%llx = 0x%llx\n", i * 8,
                       (unsigned long long)(context->Rsp + i * 8), (unsigned long long)slot );
        }
    }
}


void tuxblox_diag_exception( const EXCEPTION_RECORD *rec, const CONTEXT *context )
{
    static unsigned int seen;
    unsigned int i;

    if (!diag_enabled() || seen >= 12) return;
    seen++;

    ERR_(seh)( "DIAG exception s=%d %#x at 0x%llx rsp=0x%llx rbp=0x%llx\n",
               (int)InterlockedIncrement( &diag_seq ), (unsigned int)rec->ExceptionCode,
               (unsigned long long)context->Rip, (unsigned long long)context->Rsp,
               (unsigned long long)context->Rbp );
    diag_regs( "exc", context );
    /* Which system calls led here. The layer computes its call numbers rather
     * than loading them as constants, so the number it actually used is only
     * visible from the ring. */
    /* The run ends on either of these, varying between runs, so both have to
     * dump or half the runs say nothing. */
    if (rec->ExceptionCode == STATUS_ACCESS_VIOLATION ||
        rec->ExceptionCode == STATUS_STACK_OVERFLOW)
    {
        diag_dump_ring();
        diag_dump_steps();
    }
    diag_hex( "at-rip", (ULONG_PTR)context->Rip, 128 );
    diag_hex( "before-rip", (ULONG_PTR)context->Rip - 64, 64 );
    /* TUXBLOX_DIAG_HEX=<layer offset>[:<len>][,...] dumps a place in the
     * layer's own image at every exception. The tables it calls through are at
     * fixed offsets and hold nothing readable from outside the run. */
    {
        const char *v = getenv( "TUXBLOX_DIAG_HEX" );
        ULONG64 base = roblox_dll_base();

        while (v && *v && base)
        {
            char *end;
            ULONG64 off = strtoull( v, &end, 16 );
            unsigned int n = 64;

            if (end == v) break;
            v = end;
            if (*v == ':') { n = strtoul( v + 1, &end, 0 ); v = end; }
            if (*v == ',') v++;
            diag_hex( "hex", (ULONG_PTR)(base + off), n );
        }
    }
    for (i = 0; i < 24; i++)
    {
        ULONG64 slot = 0;

        if (!virtual_uninterrupted_read_memory( (const char *)context->Rsp + i * 8, &slot, sizeof(slot) ))
            continue;
        ERR_(seh)( "DIAG exc stack+%#x 0x%llx = 0x%llx\n", i * 8,
                   (unsigned long long)(context->Rsp + i * 8), (unsigned long long)slot );
        /* a return address on the stack: show the call that pushed it and what
         * follows, which is what an epilogue check would be looking at */
        if (slot > 0x6ffff0000000ull && slot < 0x700000000000ull)
        {
            diag_hex( "ret-144", (ULONG_PTR)slot - 144, 144 );
            diag_hex( "ret+0", (ULONG_PTR)slot, 144 );
        }
    }
}


/* Write the main module's mapped image out, decrypted, as it stands right now.
 *
 * Every byte of the Player's 100 MB .text is encrypted on disk -- entropy 7.2
 * from the entry point to the last section -- so the protection layer's own
 * code cannot be read from the file at all. It is readable in memory once the
 * layer has unpacked itself, and until now this file has only ever looked at
 * it 144 bytes at a time around an address already known to be interesting.
 * A whole-image dump turns "which answer did it dislike" from a guess into
 * something that can be read.
 *
 * Reads go through /proc/self/mem rather than straight through the pointer:
 * pages the layer left unmapped or no-access come back as a short read instead
 * of killing the process, and nothing about the mapping is disturbed.
 *
 * TUXBLOX_DIAG_DUMP=<path>. The dump is the program's own code -- keep it out
 * of the repository.
 */
void tuxblox_diag_dump_image( const char *why )
{
    static int done;
    const char *path = getenv( "TUXBLOX_DIAG_DUMP" );
    char maps_path[512], line[512], page[0x1000];
    FILE *maps;
    int mem, out, idx;
    unsigned long long total = 0;

    if (!path || !*path || done) return;
    done = 1;

    snprintf( maps_path, sizeof(maps_path), "%s.maps", path );
    if (!(maps = fopen( "/proc/self/maps", "r" ))) return;
    if ((mem = open( "/proc/self/mem", O_RDONLY )) == -1) { fclose( maps ); return; }
    if ((out = open( path, O_WRONLY | O_CREAT | O_TRUNC, 0600 )) == -1) { close( mem ); fclose( maps ); return; }
    if ((idx = open( maps_path, O_WRONLY | O_CREAT | O_TRUNC, 0600 )) == -1)
    { close( out ); close( mem ); fclose( maps ); return; }

    while (fgets( line, sizeof(line), maps ))
    {
        unsigned long long start, end, off;
        char perms[8];
        char note[600];
        int n;

        if (sscanf( line, "%llx-%llx %7s", &start, &end, perms ) != 3) continue;
        if (perms[0] != 'r' || perms[2] != 'x') continue;

        /* Every executable region, written back to back, with an index giving
         * each one's address. The offset in the dump is what turns a runtime
         * address from a log into a byte in the file. */
        n = snprintf( note, sizeof(note), "%016llx-%016llx %s dumpoff=%016llx %s",
                      start, end, perms, total, strchr( line, '/' ) ? strchr( line, '/' ) : "\n" );
        if (n > 0) { ssize_t w = write( idx, note, n ); (void)w; }

        for (off = start; off < end; off += sizeof(page))
        {
            if (pread( mem, page, sizeof(page), off ) != (ssize_t)sizeof(page))
                memset( page, 0, sizeof(page) );
            if (write( out, page, sizeof(page) ) != (ssize_t)sizeof(page)) goto done_dump;
            total += sizeof(page);
        }
    }
done_dump:
    close( idx );
    close( out );
    close( mem );
    fclose( maps );
    ERR_(seh)( "DIAG exec dump (%s): %llu bytes -> %s, index -> %s\n",
               why, total, path, maps_path );
}


void tuxblox_diag_stack_exec( const EXCEPTION_RECORD *rec, const CONTEXT *context )
{
    static int enabled = -1;
    static int done;
    const char *base = NtCurrentTeb()->Tib.StackBase;
    const char *limit = NtCurrentTeb()->DeallocationStack;
    ULONG_PTR addr, ret = 0;
    unsigned int i;

    if (enabled == -1)
    {
        const char *v = getenv( "TUXBLOX_DIAG" );
        enabled = (v && *v && *v != '0') ? 1 : 0;
    }
    if (!enabled || done >= 4) return;
    if (rec->ExceptionCode != EXCEPTION_ACCESS_VIOLATION || rec->NumberParameters < 2) return;

    addr = rec->ExceptionInformation[1];
    if (addr != (ULONG_PTR)rec->ExceptionAddress) return;      /* not an execute fault */
    if ((const char *)addr < limit || (const char *)addr >= base) return;  /* not our stack */
    done++;

    tuxblox_diag_dump_image( "execute on own stack" );
    ERR_(seh)( "DIAG execute on own stack: rip=0x%llx rsp=0x%llx rbp=0x%llx stack=%p-%p\n",
         (unsigned long long)context->Rip, (unsigned long long)context->Rsp,
         (unsigned long long)context->Rbp, limit, base );
    diag_dump_ring();
    diag_dump_steps();
    diag_hex( "target", addr, 64 );
    diag_hex( "target-64", addr - 64, 64 );
    virtual_uninterrupted_read_memory( (const void *)context->Rsp, &ret, sizeof(ret) );
    /* Slots below the stack pointer as well as above: a ret leaves the address
     * it popped one slot below, which is what separates "called here" from
     * "returned here" -- and the two have very different causes. */
    for (i = 0; i < 48; i++)
    {
        ULONG_PTR at = context->Rsp - 0x100 + i * 8;
        ULONG64 slot = 0;

        if (!virtual_uninterrupted_read_memory( (const void *)at, &slot, sizeof(slot) )) continue;
        ERR_(seh)( "DIAG stack%+d 0x%llx = 0x%llx\n", (int)(i * 8) - 0x100,
             (unsigned long long)at, (unsigned long long)slot );
    }
    /* If a call brought us here, the instruction that made it ends where the
     * pushed return address points, and the code after it is what runs on the
     * way back. */
    if (ret > 64)
    {
        diag_hex( "caller", ret - 64, 64 );
        diag_hex( "return", ret, 32 );
    }
}


#else  /* __x86_64__ */

static void diag_dump_steps(void) { }
static void diag_dump_steps_around( unsigned int at, unsigned int before, unsigned int after ) { }
void tuxblox_diag_continue( const CONTEXT *context ) { }
void tuxblox_diag_exception( const EXCEPTION_RECORD *rec, const CONTEXT *context ) { }
void tuxblox_diag_stack_exec( const EXCEPTION_RECORD *rec, const CONTEXT *context ) { }
void tuxblox_diag_dump_image( const char *why ) { }

#endif  /* __x86_64__ */

/* Watch one stack slot across system calls.
 *
 * The layer's crash is a `ret` to an address that was written over a saved
 * return address. The addresses are the same on every run, so the cheapest way
 * to name the write is to read the slot after every system call and report the
 * first call it changes across -- which brackets the write between two calls
 * even when the write itself is not a system call's doing.
 *
 * Armed with TUXBLOX_DIAG_WATCH=<hex address>. An address below 4 GB is an
 * offset into the layer's image, the same as a breakpoint's, which is how a
 * slot in the layer's own tables gets named at all -- the image moves between
 * runs. Arming it turns on the syscall trace hook, which is otherwise off;
 * nothing is printed per call, so the cost is the hook itself rather than the
 * tracer's stderr writes.
 */
#define DIAG_WATCH_MAX 4
static ULONG_PTR diag_watch[DIAG_WATCH_MAX];
static char diag_watch_resolved[DIAG_WATCH_MAX];
static unsigned int diag_watch_count;

static void diag_watch_init(void)
{
    static int done;
    const char *v;

    if (done) return;
    done = 1;
    if (!(v = getenv( "TUXBLOX_DIAG_WATCH" ))) return;
    while (*v && diag_watch_count < DIAG_WATCH_MAX)
    {
        char *end;
        ULONG64 parsed = strtoull( v, &end, 16 );

        if (end == v) break;
        if (parsed)
        {
            diag_watch_resolved[diag_watch_count] = (parsed >= 0x100000000ull);
            diag_watch[diag_watch_count++] = (ULONG_PTR)parsed;
        }
        v = (*end == ',') ? end + 1 : end;
    }
}

BOOL tuxblox_diag_watch_enabled(void)
{
    diag_watch_init();
    return diag_watch_count != 0;
}

void tuxblox_diag_watch_sysret( unsigned int id )
{
    static __thread ULONG64 last[DIAG_WATCH_MAX];
    static __thread unsigned int seen;
    const char *name = NULL;
    unsigned int i;

    diag_watch_init();
    for (i = 0; i < diag_watch_count; i++)
    {
        ULONG_PTR addr = diag_watch[i];
        ULONG64 now = 0;

        if (!diag_watch_resolved[i])
        {
            ULONG64 base = roblox_dll_base();

            if (!base) continue;
            diag_watch[i] = addr = addr + base;
            diag_watch_resolved[i] = 1;
            ERR_(seh)( "DIAG watch resolved to 0x%llx\n", (unsigned long long)addr );
        }
        if ((const char *)addr >= (const char *)NtCurrentTeb()->Tib.StackLimit &&
            (const char *)addr + 8 <= (const char *)NtCurrentTeb()->Tib.StackBase)
            now = *(volatile ULONG64 *)addr;
        /* Anywhere else -- a slot in one of the layer's own tables -- is read
         * the safe way, so an address that is not mapped yet costs a failed
         * read rather than the process. */
        else if (virtual_uninterrupted_read_memory( (const void *)addr, &now, sizeof(now) ) != sizeof(now))
            continue;
        if ((seen & (1u << i)) && now == last[i]) continue;
        if (!name) name = ntdll_syscall_name( id );
        /* n= is the raw-system-call count, so a change here can be ordered
         * against the syscall ring and against the other watched slots. */
        ERR_(seh)( "DIAG watch n=%u 0x%llx = 0x%llx (was 0x%llx) after %s from 0x%llx\n",
                   diag_ring_pos, (unsigned long long)addr, (unsigned long long)now,
                   (unsigned long long)last[i], name ? name : "?",
                   (unsigned long long)get_syscall_caller_pc() );
        last[i] = now;
        seen |= 1u << i;
    }
}

/* Page-granularity execution trace.
 *
 * Every other way of watching this layer run has been closed by the layer
 * itself: a planted int3 is read back before the byte is executed, hardware
 * breakpoints need the debug registers it uses for its own control flow, and
 * single-stepping is visible in r11 after any system call, which it tests and
 * answers by poisoning its own frame pointer.
 *
 * This watches at page granularity instead. Exactly one of the layer's code
 * pages is executable at a time; running off it faults, which reports where
 * execution went and what the stack pointer was, and the page it went to is
 * made executable in its place. Nothing in the program's own memory is
 * modified and no processor debug facility is used, so none of the checks
 * above can see it.
 *
 * It is coarse -- one event per page crossed, not per instruction -- but for
 * the question it is aimed at that is enough: it says which 4 KB page the
 * stack pointer changed inside, which is a page to disassemble rather than
 * three megabytes.
 *
 * The cost is a fault per crossing. That is affordable here for a measured
 * reason: this program already takes 194 million SIGSEGVs from the misaligned
 * SSE fixup during a normal start-up and does not object, so a fault handler
 * in its path is not itself something it reacts to.
 *
 * TUXBLOX_DIAG_XPAGE=1 turns it on, TUXBLOX_DIAG_XPAGE_AT=<n> starts it after
 * the n-th raw system call, TUXBLOX_DIAG_XPAGE_MAX=<n> stops after n events.
 */
static ULONG64 xpage_lo, xpage_hi;
/* A window of live pages rather than one. An instruction may straddle a page
 * boundary, and fetching its tail off a page with no execute right faults with
 * the *next* page as the address while the instruction pointer is still on the
 * previous one -- so one live page can never complete it. Four is enough for a
 * straddle plus the page a call returns to, and still narrow enough that every
 * real transition is seen. */
#define XPAGE_LIVE 4
static ULONG64 xpage_live_set[XPAGE_LIVE];
static unsigned int xpage_live_pos;
/* TUXBLOX_DIAG_XPAGE_LIVE narrows the window. Two is the smallest that can
 * still complete an instruction straddling a page boundary, and it records
 * every hop between two pages that four would absorb -- which is the
 * difference between seeing a path and seeing the pages it stayed in. */
static unsigned int xpage_live_n = XPAGE_LIVE;
static unsigned int xpage_events, xpage_max, xpage_start, xpage_start_seq;
/* One offset to photograph the registers at. The last page both the passing and
 * the failing traversal of a handler share is the place to read what they are
 * about to branch on; the page trace alone cannot say. */
static ULONG64 xpage_regs_at;
static int xpage_parsed, xpage_on;

/* Only the executable pages, and only their execute bit.
 *
 * The first version of this took the whole image down to PROT_READ, which also
 * stripped write permission from its data sections: the layer's next write to
 * its own data faulted, this handler declined it because the address was not
 * the instruction pointer, and the program was handed a genuine access
 * violation about ten system calls after arming. The ranges and their other
 * permissions come from /proc/self/maps rather than being assumed. */
#define XPAGE_RANGES 64
static struct { ULONG64 lo, hi; int prot; } xpage_range[XPAGE_RANGES];
static unsigned int xpage_ranges;

static int xpage_prot_of( ULONG64 page )
{
    unsigned int i;

    for (i = 0; i < xpage_ranges; i++)
        if (page >= xpage_range[i].lo && page < xpage_range[i].hi) return xpage_range[i].prot;
    return -1;
}

static int xpage_setprot( ULONG64 page, int exec )
{
    int prot = xpage_prot_of( page );

    if (prot < 0) return -1;                       /* not one of ours */
    if (exec) prot |= PROT_EXEC;
    else prot &= ~PROT_EXEC;
    return mprotect( (void *)(ULONG_PTR)page, page_size, prot );
}

/* The executable ranges of the layer's image, with the permissions they
 * already have. */
static void xpage_scan_ranges( ULONG64 lo, ULONG64 hi )
{
    char line[512];
    FILE *maps;

    xpage_ranges = 0;
    if (!(maps = fopen( "/proc/self/maps", "r" ))) return;
    while (fgets( line, sizeof(line), maps ) && xpage_ranges < XPAGE_RANGES)
    {
        unsigned long long start, end;
        char perms[8];
        int prot = 0;

        if (sscanf( line, "%llx-%llx %7s", &start, &end, perms ) != 3) continue;
        if (perms[2] != 'x') continue;             /* only executable ranges */
        if (end <= lo || start >= hi) continue;    /* only the layer's image */
        if (perms[0] == 'r') prot |= PROT_READ;
        if (perms[1] == 'w') prot |= PROT_WRITE;
        prot |= PROT_EXEC;
        if (start < lo) start = lo;
        if (end > hi) end = hi;
        xpage_range[xpage_ranges].lo = start;
        xpage_range[xpage_ranges].hi = end;
        xpage_range[xpage_ranges].prot = prot;
        xpage_ranges++;
    }
    fclose( maps );
}

/* Take execution rights off the whole image, keeping the page the thread is
 * on so it can carry on from here. */
static void xpage_arm_now( ULONG64 rip )
{
    ULONG64 p;
    unsigned int off = 0;

    unsigned int i, pages = 0;

    xpage_scan_ranges( xpage_lo, xpage_hi );
    memset( xpage_live_set, 0, sizeof(xpage_live_set) );
    xpage_live_set[0] = rip & ~(ULONG64)(page_size - 1);
    xpage_live_pos = 1;
    for (i = 0; i < xpage_ranges; i++)
        for (p = xpage_range[i].lo; p < xpage_range[i].hi; p += page_size)
        {
            pages++;
            if (p == xpage_live_set[0]) continue;
            if (xpage_setprot( p, 0 )) off++;
        }
    xpage_on = 1;
    ERR_(seh)( "DIAG xpage armed over %u executable ranges, %u pages, live 0x%llx, %u refused\n",
               xpage_ranges, pages, (unsigned long long)xpage_live_set[0], off );
}

/* Every loader entry, with the fields the layer's module walk reads.
 *
 * It iterates a list by a pointer at +0x10 and then requires a non-NULL
 * pointer at +0x60 and a non-zero 16-bit value at +0x58 of each element. On
 * an LDR_DATA_TABLE_ENTRY those are BaseDllName.Buffer and BaseDllName.Length
 * if the pointer is the entry itself, or Flags and the hash links if it is the
 * InMemoryOrderLinks field -- so the raw bytes are logged rather than a
 * guess at which. TUXBLOX_DIAG_LDR=1.
 */
/* The layer's obfuscation keys.
 *
 * Every computed jump in the flattened function is `base + f(immediate, keys)`
 * where the immediate is baked into the instruction and the keys are slots in
 * the function's own frame that do not change for the life of the run. Read
 * them once and the control flow stops being computed: every target can be
 * worked out by arithmetic instead of by watching the program run.
 *
 * The frame pointer is not passed to this hook, but the function's unwind data
 * fixes it -- rbp = rsp + 0x80 -- so a system call issued from inside the
 * function gives it away. TUXBLOX_DIAG_KEYS=<n> dumps at the n-th raw call.
 */
void tuxblox_diag_dump_keys( ULONG64 rip, ULONG64 rsp )
{
    static const unsigned int slots[] = {
        0x318, 0x520, 0x720, 0x780, 0x790, 0x7c8, 0xa68, 0xa98, 0xb40, 0xb58,
        0xb60, 0xd8c, 0xe08, 0xf70, 0x1390, 0x1558, 0x1a40, 0x1a78, 0x1ab0,
        0x1b30, 0x1eb0, 0x2070, 0x21c0, 0x22b0, 0x2aa0, 0x2ab0, 0x2b18, 0x2b40,
        0x2cf0, 0x2d00, 0x3210, 0x3420, 0x30b0
    };
    static int done;
    ULONG64 base, rbp;
    const char *v;
    unsigned int i;

    if (!diag_enabled() || done || !(v = getenv( "TUXBLOX_DIAG_KEYS" ))) return;
    if (diag_ring_pos < (unsigned int)atoi( v )) return;
    if (!(base = roblox_dll_base())) return;
    /* only a call from inside the flattened function, where rbp is known */
    if (rip < base + 0xb791a0 || rip > base + 0xeb0264) return;

    rbp = rsp + 0x80;
    done = 1;
    ERR_(seh)( "DIAG keys rip=layer+0x%llx rsp=0x%llx rbp=0x%llx\n",
               (unsigned long long)(rip - base), (unsigned long long)rsp,
               (unsigned long long)rbp );
    for (i = 0; i < ARRAY_SIZE(slots); i++)
    {
        ULONG64 val = 0;

        if (virtual_uninterrupted_read_memory( (const void *)(ULONG_PTR)(rbp + slots[i]),
                                               &val, sizeof(val) ) == sizeof(val))
            ERR_(seh)( "DIAG key [rbp+0x%04x] = %016llx\n", slots[i],
                       (unsigned long long)val );
    }
}

void tuxblox_diag_dump_ldr( void )
{
    static int done;
    LIST_ENTRY *head, *cur;
    unsigned int n = 0;
    const char *v;

    if (!diag_enabled() || done || !(v = getenv( "TUXBLOX_DIAG_LDR" ))) return;
    /* Late enough that the process has finished loading. The first raw system
     * call of a process has only its own image in the list, which is not the
     * question. TUXBLOX_DIAG_LDR=<n> is that threshold. */
    if (diag_ring_pos < (unsigned int)atoi( v )) return;
    if (!peb || !peb->LdrData) return;

    head = &peb->LdrData->InMemoryOrderModuleList;
    for (cur = head->Flink; cur && cur != head && n < 128; cur = cur->Flink, n++)
    {
        /* the entry starts one list link before the in-memory-order link */
        char *e = (char *)cur - 0x10;
        USHORT base_len = *(USHORT *)(e + 0x58);
        void  *base_buf = *(void **)(e + 0x60);
        ULONG  at68     = *(ULONG *)(e + 0x68);
        void  *at70     = *(void **)(e + 0x70);
        const WCHAR *nm = (const WCHAR *)base_buf;
        char name[128];
        unsigned int i;

        for (i = 0; i + 1 < sizeof(name) && nm && i < base_len / 2; i++) name[i] = (char)nm[i];
        name[nm ? i : 0] = 0;

        ERR_(seh)( "DIAG ldr[%u] entry=%p base=%p +0x58=%u +0x60=%p +0x68=%08x +0x70=%p %s%s\n",
                   n, e, *(void **)(e + 0x30), base_len, base_buf,
                   (unsigned int)at68, at70, name,
                   (!base_buf || !base_len) ? "  <<< EMPTY at +0x58/+0x60" : "" );
    }
    /* The first raw system call of a process happens before its modules are
     * in the list, so an empty walk is too early rather than an answer: leave
     * it un-done and look again at the next one. */
    if (!n) return;
    done = 1;
    ERR_(seh)( "DIAG ldr: %u entries\n", n );
}

/* A page of the layer's own data with write permission taken off, so that
 * every store into it reports where it came from.
 *
 * "What writes this table" is not answerable by reading the layer: its base
 * pointers are all computed, and the one place a table's address appears as a
 * rip-relative constant is an opaque predicate that never uses it as an
 * address. Named by TUXBLOX_DIAG_WPAGE as a comma-separated list of offsets
 * into the layer's image. Rearmed at each system call, so a routine that
 * writes a run of bytes reports its first store rather than all of them --
 * which is the code site, which is the question.
 */
#define WPAGE_MAX 4
static ULONG64 wpage_addr[WPAGE_MAX];
static int wpage_prot[WPAGE_MAX];
static char wpage_armed[WPAGE_MAX];
static unsigned int wpage_count, wpage_hits, wpage_hit_max;
static int wpage_parsed;

/* the permissions the page already has, read rather than assumed */
static int wpage_prot_of( ULONG64 page )
{
    char line[512];
    FILE *maps;
    int prot = -1;

    if (!(maps = fopen( "/proc/self/maps", "r" ))) return -1;
    while (fgets( line, sizeof(line), maps ))
    {
        unsigned long long start, end;
        char perm[8];

        if (sscanf( line, "%llx-%llx %7s", &start, &end, perm ) != 3) continue;
        if (page < start || page >= end) continue;
        prot = 0;
        if (perm[0] == 'r') prot |= PROT_READ;
        if (perm[1] == 'w') prot |= PROT_WRITE;
        if (perm[2] == 'x') prot |= PROT_EXEC;
        break;
    }
    fclose( maps );
    return prot;
}

void tuxblox_diag_wpage_arm( void )
{
    ULONG64 base;
    unsigned int i;

    if (!diag_enabled()) return;
    if (!wpage_parsed)
    {
        const char *v = getenv( "TUXBLOX_DIAG_WPAGE" );

        wpage_parsed = 1;
        while (v && *v && wpage_count < WPAGE_MAX)
        {
            char *end;
            ULONG64 off = strtoull( v, &end, 16 );

            if (end == v) break;
            wpage_addr[wpage_count++] = off;
            v = (*end == ',') ? end + 1 : end;
        }
        if ((v = getenv( "TUXBLOX_DIAG_WPAGE_MAX" ))) wpage_hit_max = atoi( v );
        else wpage_hit_max = 64;
    }
    if (!wpage_count || wpage_hits >= wpage_hit_max) return;
    if (!(base = roblox_dll_base())) return;

    for (i = 0; i < wpage_count; i++)
    {
        ULONG64 page;

        if (wpage_armed[i]) continue;
        if (wpage_addr[i] < 0x100000000ull) wpage_addr[i] += base;
        page = wpage_addr[i] & ~(ULONG64)(page_size - 1);
        if (!wpage_prot[i] && (wpage_prot[i] = wpage_prot_of( page )) <= 0)
        {
            wpage_prot[i] = 0;                    /* not mapped yet, look again */
            continue;
        }
        if (mprotect( (void *)(ULONG_PTR)page, page_size, wpage_prot[i] & ~PROT_WRITE )) continue;
        wpage_armed[i] = 1;
        ERR_(seh)( "DIAG wpage armed at 0x%llx (page 0x%llx, prot %d)\n",
                   (unsigned long long)wpage_addr[i], (unsigned long long)page, wpage_prot[i] );
    }
}

/* Returns TRUE when the fault was one this made and the store can be retried. */
BOOL tuxblox_diag_wpage_fault( ULONG64 addr, ULONG64 rip, const ULONG64 *regs, ULONG kind )
{
    unsigned int i;

    if (!(kind & 1)) return FALSE;                /* reads are not the question */
    for (i = 0; i < wpage_count; i++)
    {
        ULONG64 page = wpage_addr[i] & ~(ULONG64)(page_size - 1), base;

        if (!wpage_armed[i] || (addr & ~(ULONG64)(page_size - 1)) != page) continue;
        mprotect( (void *)(ULONG_PTR)page, page_size, wpage_prot[i] );
        wpage_armed[i] = 0;
        wpage_hits++;
        base = roblox_dll_base();
        if (base && rip > base && rip < base + 0x10000000ull)
            ERR_(seh)( "DIAG wpage write to layer+0x%llx from layer+0x%llx  rcx=%llx rdx=%llx r8=%llx r9=%llx r10=%llx r11=%llx\n",
                       (unsigned long long)(addr - base), (unsigned long long)(rip - base),
                       (unsigned long long)regs[2], (unsigned long long)regs[3],
                       (unsigned long long)regs[8], (unsigned long long)regs[9],
                       (unsigned long long)regs[10], (unsigned long long)regs[11] );
        else
            ERR_(seh)( "DIAG wpage write to 0x%llx from 0x%llx (outside the layer)\n",
                       (unsigned long long)addr, (unsigned long long)rip );
        return TRUE;
    }
    return FALSE;
}

void tuxblox_diag_xpage_arm( void )
{
    ULONG64 base;

    if (!diag_enabled() || xpage_on) return;
    if (!xpage_parsed)
    {
        const char *v = getenv( "TUXBLOX_DIAG_XPAGE" );

        xpage_parsed = 1;
        if (!v || !*v) return;
        if ((v = getenv( "TUXBLOX_DIAG_XPAGE_AT" ))) xpage_start = atoi( v );
        /* The raw system-call count is not the number anything else in the log
         * is indexed by. TUXBLOX_DIAG_XPAGE_SEQ arms at a trace record instead,
         * which is how a window worth tracing is actually identified. */
        if ((v = getenv( "TUXBLOX_DIAG_XPAGE_SEQ" ))) xpage_start_seq = atoi( v );
        if ((v = getenv( "TUXBLOX_DIAG_XPAGE_REGS_AT" ))) xpage_regs_at = strtoull( v, NULL, 16 );
        if ((v = getenv( "TUXBLOX_DIAG_XPAGE_LIVE" )))
        {
            xpage_live_n = atoi( v );
            if (xpage_live_n < 2) xpage_live_n = 2;
            if (xpage_live_n > XPAGE_LIVE) xpage_live_n = XPAGE_LIVE;
        }
        if ((v = getenv( "TUXBLOX_DIAG_XPAGE_MAX" ))) xpage_max = atoi( v );
        else xpage_max = 200000;
    }
    if (!xpage_max) return;                       /* not asked for */
    if (diag_ring_pos < xpage_start) return;
    if (xpage_start_seq && (unsigned int)trace_seq < xpage_start_seq) return;
    if (!(base = roblox_dll_base())) return;

    /* only the executable part, read from the loader rather than guessed */
    xpage_lo = base;
    xpage_hi = base + 0x1490000;
    xpage_arm_now( get_syscall_caller_pc() );
}

/* Returns TRUE when the fault was one this made, and execution can carry on. */
BOOL tuxblox_diag_xpage_fault( ULONG64 addr, ULONG64 rip, ULONG64 rsp, ULONG kind,
                               const ULONG64 *regs )
{
    ULONG64 page, evict;
    unsigned int i;

    if (!xpage_on || kind != 8) return FALSE;     /* 8 is an execute fault */
    if (addr < xpage_lo || addr >= xpage_hi) return FALSE;

    /* the page that could not be fetched from, which is not always the one the
     * instruction pointer is on */
    page = addr & ~(ULONG64)(page_size - 1);
    for (i = 0; i < xpage_live_n; i++) if (xpage_live_set[i] == page) return TRUE;
    if (xpage_setprot( page, 1 )) return FALSE;   /* not ours, let it through */

    evict = xpage_live_set[xpage_live_pos % xpage_live_n];
    if (evict && evict != page) xpage_setprot( evict, 0 );
    xpage_live_set[xpage_live_pos % xpage_live_n] = page;
    xpage_live_pos++;
    {
        /* xpage_lo is the layer base the arming already resolved; asking the
         * view tree again from inside a page fault comes back empty */
        ULONG64 ibase = (rip >= xpage_lo && rip < xpage_hi) ? xpage_lo : 0;

        /* the base moves run to run, so the offset is the only form of the
         * address that can be compared between them */
        if (xpage_events < xpage_max)
            ERR_(seh)( "DIAG xpage[%u] rip=0x%llx rva=+0x%llx rsp=0x%llx\n", xpage_events,
                       (unsigned long long)rip,
                       (unsigned long long)(ibase ? rip - ibase : 0),
                       (unsigned long long)rsp );
        if (xpage_regs_at && regs && ibase && rip - ibase == xpage_regs_at)
        {
            static const char * const names[16] = { "rax","rbx","rcx","rdx","rsi","rdi","rbp","rsp",
                                                    "r8","r9","r10","r11","r12","r13","r14","r15" };
            char line[512];
            unsigned int k, n = 0;

            for (k = 0; k < 16; k++)
                n += snprintf( line + n, sizeof(line) - n, "%s=%llx ", names[k],
                               (unsigned long long)regs[k] );
            ERR_(seh)( "DIAG xpage-regs[%u] +0x%llx %s\n", xpage_events,
                       (unsigned long long)xpage_regs_at, line );
        }
    }
    if (++xpage_events >= xpage_max)
    {
        ULONG64 p;
        unsigned int i;

        ERR_(seh)( "DIAG xpage limit %u reached, releasing\n", xpage_max );
        for (i = 0; i < xpage_ranges; i++)
            for (p = xpage_range[i].lo; p < xpage_range[i].hi; p += page_size)
                xpage_setprot( p, 1 );
        xpage_on = 0;
    }
    return TRUE;
}

/* Breakpoints, for questions a fault counter cannot answer.
 *
 * The layer computes almost every address it uses, so reading its code says
 * little about which branches actually run. An `int3` planted at a chosen
 * address answers that directly: it reports the registers execution reaches it
 * with, puts the original byte back and carries on. The trap is swallowed in
 * the handler, so the program is never told it happened -- which matters here,
 * because the layer raises debug traps of its own and watches how they are
 * delivered.
 *
 * Named by TUXBLOX_DIAG_BP as a comma-separated list. An address below 4 GB is
 * an offset into the layer's own image, which is the only way to name one of
 * its instructions that survives the image moving between runs.
 *
 * An entry may carry "@<delta>", which adds that many bytes to the stack
 * pointer when the address is reached. That is not a fix for anything: it is
 * how a theory about a stack imbalance gets tested, by correcting it at one
 * named instruction and watching what the program does afterwards.
 *
 * Each address reports up to TUXBLOX_DIAG_BP_MAX times, eight by default. The
 * interesting questions are about a call the layer makes more than once with
 * different arguments, and a breakpoint that fires once cannot tell those
 * apart.
 */
/* Enough to ask "which of these ever runs" of a whole census in one run
 * rather than eight at a time, which is the question this facility is
 * actually used for. */
#define DIAG_BP_MAX 160

static ULONG64 diag_bp_addr[DIAG_BP_MAX];
static LONG64 diag_bp_rsp_delta[DIAG_BP_MAX];
static unsigned char diag_bp_orig[DIAG_BP_MAX], diag_bp_want[DIAG_BP_MAX];
static char diag_bp_armed[DIAG_BP_MAX], diag_bp_resolved[DIAG_BP_MAX];
static unsigned int diag_bp_hits[DIAG_BP_MAX], diag_bp_races[DIAG_BP_MAX];
static ULONG diag_bp_rearm_tid[DIAG_BP_MAX];
static unsigned int diag_bp_pending, diag_bp_max_hits;
static int diag_bp_parsed;

/* Breakpoint states. A hit leaves the address WAITING rather than ARMED: the
 * byte has to stay out of the way until the instruction under it has run, and
 * the first system call the same thread makes is proof that it has. */
#define DIAG_BP_PENDING 0
#define DIAG_BP_ARMED   1
#define DIAG_BP_RETIRED 2
#define DIAG_BP_WAITING 3

/* Arming has to wait for the layer to decrypt the code being watched, and
 * nothing says when that has happened. Each address may therefore be given the
 * byte expected to be there -- "0x6ffffdd7bf39=4d" -- and arming is retried
 * from the busiest hooks until what is there matches. Without a byte, it is
 * armed at the first opportunity.
 */
static void diag_bp_arm(void)
{
    unsigned int i;

    if (!diag_bp_parsed)
    {
        const char *v = getenv( "TUXBLOX_DIAG_BP" );

        diag_bp_parsed = 1;
        while (v && *v && diag_bp_count < DIAG_BP_MAX)
        {
            ULONG64 addr;
            char *end;

            /* hex, with or without the 0x -- offsets are written that way
             * everywhere else, and a bare one used to consume nothing and
             * spin here for the life of the process. */
            addr = strtoull( v, &end, 16 );
            if (end == v)
            {
                ERR_(seh)( "DIAG bp: cannot read an address at \"%s\", giving up on the rest\n", v );
                break;
            }
            v = end;
            if (*v == '=') diag_bp_want[diag_bp_count] = (unsigned char)strtoul( v + 1, &end, 16 );
            v = end;
            if (*v == '@') diag_bp_rsp_delta[diag_bp_count] = strtoll( v + 1, &end, 0 );
            v = end;
            if (*v == ',') v++;
            if (addr) diag_bp_addr[diag_bp_count++] = addr;
        }
        diag_bp_pending = diag_bp_count;
        v = getenv( "TUXBLOX_DIAG_BP_MAX" );
        diag_bp_max_hits = v ? atoi( v ) : 8;
    }
    if (!diag_bp_pending) return;

    for (i = 0; i < diag_bp_count; i++)
    {
        unsigned char cur;

        /* A hit thread that has reached a system call is past the instruction
         * the breakpoint sat on, so the byte can go back. */
        if (diag_bp_armed[i] == DIAG_BP_WAITING)
        {
            if (GetCurrentThreadId() != diag_bp_rearm_tid[i]) continue;
            diag_bp_armed[i] = DIAG_BP_PENDING;
        }
        if (diag_bp_armed[i]) continue;

        /* An offset only becomes an address once the layer is loaded. */
        if (!diag_bp_resolved[i])
        {
            ULONG64 base;

            if (diag_bp_addr[i] >= 0x100000000ull) diag_bp_resolved[i] = 1;
            else if (!(base = roblox_dll_base())) continue;
            else
            {
                diag_bp_addr[i] += base;
                diag_bp_resolved[i] = 1;
                ERR_(seh)( "DIAG bp resolved to 0x%llx\n", (unsigned long long)diag_bp_addr[i] );
            }
        }
        if (virtual_uninterrupted_read_memory( (const void *)(ULONG_PTR)diag_bp_addr[i], &cur, 1 ) != 1)
            continue;
        if (diag_bp_want[i] && cur != diag_bp_want[i]) continue;
        if (virtual_patch_code_byte( (void *)(ULONG_PTR)diag_bp_addr[i], 0xcc )) continue;

        diag_bp_orig[i] = cur;
        diag_bp_armed[i] = DIAG_BP_ARMED;
        diag_bp_pending--;
        if (!diag_bp_hits[i])
            ERR_(seh)( "DIAG bp armed at 0x%llx, was 0x%02x\n",
                       (unsigned long long)diag_bp_addr[i], cur );
    }
}

/* Putting a breakpoint's byte back, one instruction after it was hit.
 *
 * A hit leaves the address WAITING: the byte has to stay out of the way until
 * the instruction under it has run. Proof that it has used to be the thread's
 * next system call -- but the layer's hot loops make none, so a breakpoint
 * inside one fired exactly once and every count taken from such a loop was
 * wrong by construction. The trap flag is set for that one instruction instead
 * and the byte goes back on the way out of the step.
 *
 * Stepping is visible to a program that reads its own EFLAGS, so this trades a
 * one-instruction window of that for counts that mean something. */
static __thread int diag_bp_rearm_step;

BOOL tuxblox_diag_bp_step_pending(void)
{
    return diag_bp_rearm_step != 0;
}

BOOL tuxblox_diag_bp_step_rearm(void)
{
    if (!diag_bp_rearm_step) return FALSE;
    diag_bp_rearm_step = 0;
    diag_bp_arm();
    return TRUE;
}

/* Frame slots to read at a breakpoint, from TUXBLOX_DIAG_BP_SLOTS.
 *
 * The layer's flattened functions keep their dispatcher key in a frame slot --
 * 0x22b0 in one, 0x540 in another -- and the arithmetic that turns a key into a
 * jump target reads more slots beside it. Those are run constants, so the graph
 * cannot be walked without them, and nothing else here can address the stack:
 * the hex dump is relative to the layer's base and the watch samples far too
 * rarely. Written "950,22b0" for rbp-relative, "rsp:20" for rsp-relative.
 */
#define DIAG_BP_SLOT_MAX 12

static LONG64 diag_bp_slot_off[DIAG_BP_SLOT_MAX];
static char diag_bp_slot_rsp[DIAG_BP_SLOT_MAX];
static unsigned int diag_bp_slot_count;
static int diag_bp_slots_parsed;

static void diag_bp_slots_parse(void)
{
    const char *v;

    if (diag_bp_slots_parsed) return;
    diag_bp_slots_parsed = 1;
    if (!(v = getenv( "TUXBLOX_DIAG_BP_SLOTS" ))) return;
    while (*v && diag_bp_slot_count < DIAG_BP_SLOT_MAX)
    {
        char *end;
        int on_rsp = 0;

        if (!strncmp( v, "rsp:", 4 )) { on_rsp = 1; v += 4; }
        else if (!strncmp( v, "rbp:", 4 )) v += 4;
        diag_bp_slot_off[diag_bp_slot_count] = strtoll( v, &end, 16 );
        if (end == v)
        {
            ERR_(seh)( "DIAG bp slots: cannot read an offset at \"%s\", giving up on the rest\n", v );
            break;
        }
        diag_bp_slot_rsp[diag_bp_slot_count++] = (char)on_rsp;
        v = end;
        if (*v == ',') v++;
    }
}

/* Appends the slot values to a breakpoint's line. Reads are uninterrupted and
 * bounded, so an unmapped slot prints as ? rather than taking the process down. */
static unsigned int diag_bp_slots_print( char *line, unsigned int n, unsigned int size,
                                         const ULONG64 *regs )
{
    unsigned int k;

    for (k = 0; k < diag_bp_slot_count; k++)
    {
        LONG64 off = diag_bp_slot_off[k];
        ULONG64 addr = regs[diag_bp_slot_rsp[k] ? 7 : 6] + off;
        const char *reg = diag_bp_slot_rsp[k] ? "rsp" : "rbp";
        unsigned long long mag = (unsigned long long)(off < 0 ? -off : off);
        char sign = (off < 0) ? '-' : '+';
        ULONG64 val;

        if (virtual_uninterrupted_read_memory( (const void *)(ULONG_PTR)addr, &val, sizeof(val) )
            == sizeof(val))
            n += snprintf( line + n, size - n, " [%s%c%llx]=%llx", reg, sign, mag,
                           (unsigned long long)val );
        else
            n += snprintf( line + n, size - n, " [%s%c%llx]=?", reg, sign, mag );
    }
    return n;
}

/* Frame slots to WRITE at a breakpoint, from TUXBLOX_DIAG_BP_POKE.
 *
 * The layer decides its route from values it computed itself, so the only way to
 * ask "would it take the other route if this value were different" is to change
 * the value and watch. This writes one, the way the @<delta> suffix corrects the
 * stack pointer: a research tool, not a fix for anything.
 *
 * Written "990=36a1fadc" for rbp-relative, "rsp:20=0" for rsp-relative. Values are
 * hex and written as eight bytes. Signal-handler safe -- fixed statics, no
 * allocation, and a bounded write that reports rather than faulting.
 */
#define DIAG_BP_POKE_MAX 8

static LONG64 diag_bp_poke_off[DIAG_BP_POKE_MAX];
static ULONG64 diag_bp_poke_val[DIAG_BP_POKE_MAX];
static char diag_bp_poke_rsp[DIAG_BP_POKE_MAX];
static unsigned int diag_bp_poke_count;
static int diag_bp_poke_parsed;

static void diag_bp_poke_parse(void)
{
    const char *v;

    if (diag_bp_poke_parsed) return;
    diag_bp_poke_parsed = 1;
    if (!(v = getenv( "TUXBLOX_DIAG_BP_POKE" ))) return;
    while (*v && diag_bp_poke_count < DIAG_BP_POKE_MAX)
    {
        char *end;
        int on_rsp = 0;

        if (!strncmp( v, "rsp:", 4 )) { on_rsp = 1; v += 4; }
        else if (!strncmp( v, "rbp:", 4 )) v += 4;
        diag_bp_poke_off[diag_bp_poke_count] = strtoll( v, &end, 16 );
        if (end == v || *end != '=')
        {
            ERR_(seh)( "DIAG bp poke: expected <offset>=<value> at \"%s\", giving up on the rest\n", v );
            break;
        }
        v = end + 1;
        diag_bp_poke_val[diag_bp_poke_count] = strtoull( v, &end, 16 );
        if (end == v)
        {
            ERR_(seh)( "DIAG bp poke: cannot read a value at \"%s\", giving up on the rest\n", v );
            break;
        }
        diag_bp_poke_rsp[diag_bp_poke_count++] = (char)on_rsp;
        v = end;
        if (*v == ',') v++;
    }
}

/* Applies the pokes at a hit, and says what it did -- a poke that silently failed
 * would read exactly like the value not mattering. */
static void diag_bp_poke_apply( const ULONG64 *regs )
{
    unsigned int k;

    for (k = 0; k < diag_bp_poke_count; k++)
    {
        ULONG64 base = diag_bp_poke_rsp[k] ? regs[7] : regs[6];
        ULONG64 addr = base + diag_bp_poke_off[k];
        ULONG64 was = 0;
        BOOL read_ok = virtual_uninterrupted_read_memory( (const void *)(ULONG_PTR)addr,
                                                          &was, sizeof(was) ) == sizeof(was);

        if (virtual_uninterrupted_write_memory( (void *)(ULONG_PTR)addr,
                                                &diag_bp_poke_val[k], sizeof(diag_bp_poke_val[k]) ))
        {
            ERR_(seh)( "DIAG bp poke FAILED [%s%+lld] = 0x%llx\n",
                       diag_bp_poke_rsp[k] ? "rsp" : "rbp", (long long)diag_bp_poke_off[k],
                       (unsigned long long)diag_bp_poke_val[k] );
            continue;
        }
        ERR_(seh)( "DIAG bp poke [%s%+lld] 0x%llx -> 0x%llx\n",
                   diag_bp_poke_rsp[k] ? "rsp" : "rbp", (long long)diag_bp_poke_off[k],
                   read_ok ? (unsigned long long)was : 0ull,
                   (unsigned long long)diag_bp_poke_val[k] );
    }
}

/* Registers to SET at a breakpoint, from TUXBLOX_DIAG_BP_SETREG.
 *
 * Poking a frame slot changes every later read of it, which for a slot the layer
 * reads hundreds of times derails the run instead of steering it. Setting the
 * register at the one instruction that consumes it changes exactly one use.
 *
 * Written "r14=36a1fadc,rax=0". rsp is refused -- the @<delta> suffix owns it.
 */
#define DIAG_BP_SETREG_MAX 8

static const char * const diag_reg_names[16] = { "rax", "rbx", "rcx", "rdx", "rsi", "rdi", "rbp", "rsp",
                                                 "r8", "r9", "r10", "r11", "r12", "r13", "r14", "r15" };
static unsigned int diag_bp_setreg_idx[DIAG_BP_SETREG_MAX];
static ULONG64 diag_bp_setreg_val[DIAG_BP_SETREG_MAX];
static unsigned int diag_bp_setreg_count;
static int diag_bp_setreg_parsed;

static void diag_bp_setreg_parse(void)
{
    const char *v;

    if (diag_bp_setreg_parsed) return;
    diag_bp_setreg_parsed = 1;
    if (!(v = getenv( "TUXBLOX_DIAG_BP_SETREG" ))) return;
    while (*v && diag_bp_setreg_count < DIAG_BP_SETREG_MAX)
    {
        unsigned int r;
        char *end;

        for (r = 0; r < 16; r++)
        {
            size_t len = strlen( diag_reg_names[r] );
            if (!strncmp( v, diag_reg_names[r], len ) && v[len] == '=') break;
        }
        if (r == 16 || r == 7)
        {
            ERR_(seh)( "DIAG bp setreg: not a settable register at \"%s\" (rsp is the @delta suffix's)\n", v );
            break;
        }
        v += strlen( diag_reg_names[r] ) + 1;
        diag_bp_setreg_val[diag_bp_setreg_count] = strtoull( v, &end, 16 );
        if (end == v)
        {
            ERR_(seh)( "DIAG bp setreg: cannot read a value at \"%s\"\n", v );
            break;
        }
        diag_bp_setreg_idx[diag_bp_setreg_count++] = r;
        v = end;
        if (*v == ',') v++;
    }
}

static void diag_bp_setreg_apply( ULONG64 *regs )
{
    unsigned int k;

    for (k = 0; k < diag_bp_setreg_count; k++)
    {
        unsigned int r = diag_bp_setreg_idx[k];

        ERR_(seh)( "DIAG bp setreg %s 0x%llx -> 0x%llx\n", diag_reg_names[r],
                   (unsigned long long)regs[r], (unsigned long long)diag_bp_setreg_val[k] );
        regs[r] = diag_bp_setreg_val[k];
    }
}

/* Arm stepping when a breakpoint is reached, rather than after a count of
 * system calls.
 *
 * The count is not reproducible: the layer's path through itself varies between
 * runs, and thread ids do too, so the same number lands hundreds of calls apart.
 * An address does not. TUXBLOX_DIAG_STEP_AT=<layer offset>, which must also be
 * listed in TUXBLOX_DIAG_BP so that there is a breakpoint to arm at.
 *
 * "<offset>#<n>" arms at the n-th time that address is reached rather than the
 * first, for a per-item loop whose one interesting pass is somewhere in the
 * middle -- the module walk fails on its twenty-first module, and stepping the
 * first twenty is both useless and slow.
 */
static ULONG64 diag_step_at;
static unsigned int diag_step_at_want = 1, diag_step_at_seen;
static int diag_step_at_parsed;

static void diag_step_at_check( ULONG64 addr )
{
    if (!diag_step_at_parsed)
    {
        const char *v = getenv( "TUXBLOX_DIAG_STEP_AT" );
        ULONG64 base = roblox_dll_base();
        char *end;

        diag_step_at_parsed = 1;
        if (v)
        {
            diag_step_at = strtoull( v, &end, 16 );
            if (*end == '#') diag_step_at_want = strtoul( end + 1, NULL, 0 );
        }
        if (diag_step_at && diag_step_at < 0x100000000ull && base) diag_step_at += base;
    }
    if (!diag_step_at || addr != diag_step_at || tuxblox_diag_stepping) return;
    if (++diag_step_at_seen < diag_step_at_want) return;
    tuxblox_diag_stepping = TRUE;
    diag_step_owner = GetCurrentThreadId();
    ERR_(seh)( "DIAG step armed at 0x%llx on thread %04x\n",
               (unsigned long long)addr, (unsigned int)diag_step_owner );
}

BOOL tuxblox_diag_bp_hit( ULONG64 rip, ULONG64 *regs, LONG64 *rsp_delta )
{
    static const char * const names[16] = { "rax", "rbx", "rcx", "rdx", "rsi", "rdi", "rbp", "rsp",
                                            "r8", "r9", "r10", "r11", "r12", "r13", "r14", "r15" };
    unsigned int i;

    diag_bp_slots_parse();
    diag_bp_poke_parse();
    diag_bp_setreg_parse();

    for (i = 0; i < diag_bp_count; i++)
    {
        char line[1024];
        unsigned int k, n = 0;

        if (!diag_bp_resolved[i] || diag_bp_addr[i] != rip - 1) continue;
        if (rsp_delta) *rsp_delta = diag_bp_rsp_delta[i];
        diag_bp_poke_apply( regs );
        diag_bp_setreg_apply( regs );
        if (diag_bp_rsp_delta[i])
            ERR_(seh)( "DIAG bp rsp 0x%llx %+lld -> 0x%llx\n", (unsigned long long)regs[7],
                       (long long)diag_bp_rsp_delta[i],
                       (unsigned long long)(regs[7] + diag_bp_rsp_delta[i]) );

        /* A second thread can reach the same int3 between the trap that
         * disarmed it and the original byte going back, and by the time its
         * signal arrives the breakpoint is no longer armed. The trap is still
         * ours, and handing it to the program is far worse than losing one
         * reading: the layer sees a breakpoint it never planted and stops.
         * Swallowed unless the byte underneath was itself an int3, in which
         * case it really is the program's. */
        if (diag_bp_armed[i] != DIAG_BP_ARMED)
        {
            if (diag_bp_orig[i] == 0xcc) return FALSE;
            diag_bp_races[i]++;
            return TRUE;
        }

        for (k = 0; k < 16; k++)
            n += snprintf( line + n, sizeof(line) - n, "%s=%llx ", names[k],
                           (unsigned long long)regs[k] );

        /* The top of the stack as well as the registers. At a function's first
         * instruction [rsp] is the return address, which is the only thing
         * that says which of the layer's many call sites this one came from --
         * and with the layer's calls all computed, that is the question. Shown
         * as an offset into the layer where it lands in it. */
        {
            ULONG64 stack[4], base = roblox_dll_base();

            if (virtual_uninterrupted_read_memory( (const void *)(ULONG_PTR)regs[7],
                                                   stack, sizeof(stack) ) == sizeof(stack))
            {
                n += snprintf( line + n, sizeof(line) - n, " stack:" );
                for (k = 0; k < 4; k++)
                {
                    if (base && stack[k] > base && stack[k] < base + 0x10000000ull)
                        n += snprintf( line + n, sizeof(line) - n, " layer+%llx",
                                       (unsigned long long)(stack[k] - base) );
                    else
                        n += snprintf( line + n, sizeof(line) - n, " %llx",
                                       (unsigned long long)stack[k] );
                }
            }
        }
        /* The layer finds modules by hashing their base name, so at the sites
         * that matter rdx is a UTF-16 name and rcx its length in characters.
         * Printing it turns a line of hex into something readable. */
        if (regs[2] > 0x10000 && regs[1] && regs[1] < 128)
        {
            WCHAR buf[128];
            unsigned int len = (unsigned int)regs[1];

            if (virtual_uninterrupted_read_memory( (const void *)(ULONG_PTR)regs[2],
                                                   buf, len * sizeof(WCHAR) ) == len * sizeof(WCHAR))
            {
                char name[128];

                for (k = 0; k < len; k++) name[k] = (buf[k] < 0x80) ? (char)buf[k] : '?';
                name[len] = 0;
                n += snprintf( line + n, sizeof(line) - n, " name=\"%s\"", name );
            }
        }
        n = diag_bp_slots_print( line, n, sizeof(line), regs );
        ERR_(seh)( "DIAG bp hit #%u (%u raced) 0x%llx %s\n", diag_bp_hits[i] + 1,
                   diag_bp_races[i], (unsigned long long)diag_bp_addr[i], line );
        virtual_patch_code_byte( (void *)(ULONG_PTR)diag_bp_addr[i], diag_bp_orig[i] );
        diag_step_at_check( diag_bp_addr[i] );

        if (++diag_bp_hits[i] >= diag_bp_max_hits) diag_bp_armed[i] = DIAG_BP_RETIRED;
        else
        {
            diag_bp_rearm_tid[i] = GetCurrentThreadId();
            diag_bp_armed[i] = DIAG_BP_WAITING;
            diag_bp_pending++;
            diag_bp_rearm_step = 1;   /* step one instruction, then put it back */
        }
        return TRUE;
    }
    return FALSE;
}

/* Whether each breakpoint was still where it was put.
 *
 * The layer rewrites and re-protects its own code as it runs, so a page holding
 * one of these can be replaced wholesale after it was armed. The 0xcc goes with
 * it and nothing notices: the tool still says "armed", the breakpoint never
 * fires, and the run is read as proof that the instruction never executed. That
 * conclusion is only safe for an address whose byte survived to the end, so the
 * survivors are named here and a negative from any other address is void.
 */
void tuxblox_diag_bp_report(void)
{
    unsigned int i;

    for (i = 0; i < diag_bp_count; i++)
    {
        unsigned char cur = 0;
        const char *state;

        if (!diag_bp_resolved[i]) continue;
        switch (diag_bp_armed[i])
        {
        case DIAG_BP_ARMED:   state = "armed";   break;
        case DIAG_BP_RETIRED: state = "retired"; break;
        case DIAG_BP_WAITING: state = "waiting"; break;
        default:              state = "pending"; break;
        }
        if (virtual_uninterrupted_read_memory( (const void *)(ULONG_PTR)diag_bp_addr[i], &cur, 1 ) != 1)
            ERR_(seh)( "DIAG bp end 0x%llx %s hits=%u UNREADABLE -- a negative here proves nothing\n",
                       (unsigned long long)diag_bp_addr[i], state, diag_bp_hits[i] );
        else if (diag_bp_armed[i] == DIAG_BP_ARMED && cur != 0xcc)
            ERR_(seh)( "DIAG bp end 0x%llx %s hits=%u byte=0x%02x ERASED -- the page was rewritten, "
                       "a negative here proves nothing\n",
                       (unsigned long long)diag_bp_addr[i], state, diag_bp_hits[i], cur );
        else
            ERR_(seh)( "DIAG bp end 0x%llx %s hits=%u byte=0x%02x survived\n",
                       (unsigned long long)diag_bp_addr[i], state, diag_bp_hits[i], cur );
    }
}

/* Which instruction is faulting, not just how many of them there are.
 *
 * Only the moves have an unaligned twin the same length as themselves --
 * movaps becomes movups, movdqa becomes movdqu -- so only they could be
 * rewritten where they sit and made to stop faulting. The arithmetic forms
 * that take a 128-bit memory operand, paddd and pxor and the rest, have no
 * unaligned spelling at all. The share of the two, and the number of distinct
 * addresses the faults come from, together say whether rewriting them is worth
 * anything. Gated on TUXBLOX_DIAG with the rest of this.
 */
#define DIAG_ALIGN_SITES 4096

static unsigned int diag_align_op[256];
static unsigned int diag_align_undecoded, diag_align_movable, diag_align_nsites;
/* Where the misaligned accesses point, and by how much.
 *
 * The question these answer is whether the layer is reading data that Windows
 * would have placed 16-byte aligned. A residue histogram piled entirely on 8
 * means a systematic half-alignment on our side -- a frame or an allocation --
 * and is fixable at the source; a spread across 1..15 means the layer really is
 * reading at arbitrary offsets and the faults are inherent. */
static unsigned int diag_align_residue[16];
/* Mirrors the fault counter so the summary can also be printed at exit; a run
 * that faults fewer than a million times never reaches the periodic report. */
static unsigned int diag_align_seen;
extern unsigned int align_rewrites_done, align_rewrites_refused;
enum { ALIGN_RGN_STACK, ALIGN_RGN_IMAGE, ALIGN_RGN_OTHER, ALIGN_RGN_COUNT };
static unsigned int diag_align_region[ALIGN_RGN_COUNT];
static const char * const diag_align_region_name[ALIGN_RGN_COUNT] = { "stack", "image", "other" };

static unsigned int diag_align_classify( ULONG64 addr )
{
    const TEB *teb = NtCurrentTeb();

    if (addr >= (ULONG64)(ULONG_PTR)teb->Tib.StackLimit
        && addr < (ULONG64)(ULONG_PTR)teb->Tib.StackBase) return ALIGN_RGN_STACK;
    if (virtual_is_image_address( (const void *)(ULONG_PTR)addr )) return ALIGN_RGN_IMAGE;
    return ALIGN_RGN_OTHER;
}
static ULONG64 diag_align_site[DIAG_ALIGN_SITES];

/* The opcode of a 0f-escaped instruction, past its prefixes. 0 if it is not one. */
static unsigned char diag_align_opcode( const unsigned char *p, unsigned int len )
{
    unsigned int i = 0;

    while (i < len)
    {
        unsigned char b = p[i];

        if (b == 0x66 || b == 0xf2 || b == 0xf3 || b == 0xf0 || b == 0x67 ||
            b == 0x2e || b == 0x36 || b == 0x3e || b == 0x26 || b == 0x64 || b == 0x65)
            i++;
        else break;
    }
    if (i < len && p[i] >= 0x40 && p[i] <= 0x4f) i++;
    if (i + 1 >= len || p[i] != 0x0f) return 0;
    return p[i + 1];
}

/* Remembers an address once. The table holds 4096 of them; if it fills, the
 * count stops rising, which is an answer of its own.
 */
static void diag_align_note_site( ULONG64 rip )
{
    unsigned int h = (unsigned int)((rip * 0x9e3779b1u) >> 8) & (DIAG_ALIGN_SITES - 1);
    unsigned int k;

    for (k = 0; k < DIAG_ALIGN_SITES; k++)
    {
        unsigned int i = (h + k) & (DIAG_ALIGN_SITES - 1);

        if (diag_align_site[i] == rip) return;
        if (!diag_align_site[i])
        {
            diag_align_site[i] = rip;
            diag_align_nsites++;
            return;
        }
    }
}

static void diag_align_report( unsigned int seen )
{
    char line[512];
    unsigned int i, n = 0;

    for (i = 0; i < 256; i++)
        if (diag_align_op[i] && n < sizeof(line) - 24)
            n += snprintf( line + n, sizeof(line) - n, "0f%02x=%u ", i, diag_align_op[i] );
    if (!n) line[0] = 0;

    {
        char rline[256], gline[128];
        unsigned int k, rn = 0, gn = 0;

        for (k = 0; k < 16; k++)
            if (diag_align_residue[k])
                rn += snprintf( rline + rn, sizeof(rline) - rn, "+%u=%u ", k, diag_align_residue[k] );
        for (k = 0; k < ALIGN_RGN_COUNT; k++)
            gn += snprintf( gline + gn, sizeof(gline) - gn, "%s=%u ",
                            diag_align_region_name[k], diag_align_region[k] );
        ERR_(seh)( "DIAG align where: %s| %s| rewritten=%u refused=%u\n",
                   rn ? rline : "", gline, align_rewrites_done, align_rewrites_refused );
    }
    ERR_(seh)( "DIAG align mix: %u faults, %u movable (%u%%), %u distinct sites, %u undecoded | %s\n",
               seen, diag_align_movable, seen ? diag_align_movable * 100 / seen : 0,
               diag_align_nsites, diag_align_undecoded, line );
}

/* Every misaligned-SSE fixup, and every misaligned fault the fixup declined.
 *
 * The fixup is this build's own code -- it decodes the instruction, performs it
 * and steps Rip past it -- so a mis-decode does not report an error, it silently
 * resumes the program somewhere it should not be. Counting what it sees is the
 * first thing to know before trusting it. Gated on TUXBLOX_DIAG.
 */
void tuxblox_diag_align( ULONG64 rip, ULONG64 rsp, ULONG64 rbp, ULONG64 addr, BOOL handled,
                         const ULONG64 *regs )
{
    static int enabled = -1, log_all = -1;
    static unsigned int seen, declined, dump_at;
    /* One address to keep an eye on, and the last value seen there. The layer
     * dies reading a pointer out of one of its own globals that is still zero;
     * whether it is ever anything else is the first thing to know, and reading
     * it costs nothing next to the fault that got us here. */
    static ULONG64 watch, watch_val;
    static int watch_seen;
    static ULONG64 stack_at;
    static int stack_at_done;

    unsigned char buf[16];
    char line[3 * sizeof(buf) + 1];
    unsigned int i, n;
    unsigned char op;
    SIZE_T got;

    if (enabled == -1)
    {
        const char *v = getenv( "TUXBLOX_DIAG" );
        enabled = (v && *v && *v != '0') ? 1 : 0;
        log_all = getenv( "TUXBLOX_DIAG_ALIGN_ALL" ) ? 1 : 0;
        /* Which fault to photograph the decrypted code at. Two million lands in
         * the middle of the hashing loop, which is where it was first wanted;
         * a small number catches the layer's first misaligned frame instead,
         * seconds into a run rather than minutes. */
        v = getenv( "TUXBLOX_DIAG_DUMP_AT" );
        dump_at = v ? atoi( v ) : 2000000;
        v = getenv( "TUXBLOX_DIAG_WATCH" );
        watch = v ? strtoull( v, NULL, 0 ) : 0;
        /* Which site to photograph the stack at, as an offset in its image.
         * The first-eight dump catches the start of a run; this catches one
         * named instruction wherever in the run it faults. */
        v = getenv( "TUXBLOX_DIAG_ALIGN_STACK_AT" );
        stack_at = v ? strtoull( v, NULL, 16 ) : 0;
        /* The layer's own code is only readable once it has decrypted itself,
         * and by the first of these faults it has. */
        diag_bp_arm();
    }
    if (!enabled) return;
    seen++;
    diag_align_seen = seen;
    if (!handled) declined++;

    /* Reading the instruction on every one of these costs, and that is accepted
     * here: the sample is for proportions, not for timings. */
    got = virtual_uninterrupted_read_memory( (const void *)(ULONG_PTR)rip, buf, sizeof(buf) );
    op = diag_align_opcode( buf, (unsigned int)got );
    if (watch && !(seen % 256))
    {
        ULONG64 now = 0;

        if (virtual_uninterrupted_read_memory( (const void *)(ULONG_PTR)watch, &now, sizeof(now) )
            == sizeof(now) && (!watch_seen || now != watch_val))
        {
            ERR_(seh)( "DIAG watch 0x%llx = 0x%llx at fault %u\n",
                       (unsigned long long)watch, (unsigned long long)now, seen );
            watch_val = now;
            watch_seen = 1;
        }
    }
    if (addr)
    {
        diag_align_residue[addr & 15]++;
        diag_align_region[diag_align_classify( addr )]++;
    }
    if (op) diag_align_op[op]++;
    else diag_align_undecoded++;
    if (op == 0x28 || op == 0x29 || op == 0x6f || op == 0x7f) diag_align_movable++;
    diag_align_note_site( rip );
    /* The Player makes these by the million, so the rate is worth having on its
     * own, and nothing on this path may call getenv. */
    if (!(seen % 1000000))
    {
        static const char * const names[16] = { "rax", "rbx", "rcx", "rdx", "rsi", "rdi", "rbp", "rsp",
                                                "r8", "r9", "r10", "r11", "r12", "r13", "r14", "r15" };
        char regline[512];
        unsigned int k, m = 0;

        for (k = 0; k < 16; k++)
            m += snprintf( regline + m, sizeof(regline) - m, "%s=%llx ", names[k],
                           (unsigned long long)regs[k] );
        ERR_(seh)( "DIAG align %u million so far, %u declined, rip=0x%llx %s\n",
                   seen / 1000000, declined, (unsigned long long)rip, regline );
        diag_align_report( seen );
    }
    /* The layer only decrypts its own code in memory, so a fixup is the moment
     * it can be read. Writes nothing unless TUXBLOX_DIAG_DUMP names a path. */
    if (seen == dump_at) tuxblox_diag_dump_image( "alignment scan" );
    if (stack_at && !stack_at_done)
    {
        ULONG_PTR base = virtual_get_image_base( (const void *)(ULONG_PTR)rip );
        unsigned int k;

        if (base && rip - base == stack_at)
        {
            stack_at_done = 1;
            ERR_(seh)( "DIAG align-at +0x%llx fault #%u rsp=0x%llx (rsp%%16=%u) rbp=0x%llx\n",
                       (unsigned long long)stack_at, seen, (unsigned long long)rsp,
                       (unsigned int)(rsp & 15), (unsigned long long)rbp );
            diag_hex( "at-code", (ULONG_PTR)rip, 64 );
            for (k = 0; k < 6; k++) diag_hex( "at-stack", (ULONG_PTR)rsp + k * 256, 256 );
        }
    }
    if (!log_all && seen > 64 && handled) return;

    for (i = n = 0; i < got; i++) n += snprintf( line + n, sizeof(line) - n, "%02x ", buf[i] );
    ERR_(seh)( "DIAG align s=%d %s #%u n=%u rip=0x%llx rsp=0x%llx rbp=0x%llx addr=0x%llx +%u %s %s\n",
               (int)InterlockedIncrement( &diag_seq ), handled ? "fixed" : "DECLINED",
               seen, diag_ring_pos, (unsigned long long)rip, (unsigned long long)rsp,
               (unsigned long long)rbp, (unsigned long long)addr, (unsigned int)(addr & 15),
               addr ? diag_align_region_name[diag_align_classify( addr )] : "-",
               got ? line : "unreadable" );
    /* the image base moves run to run, so the offset within it is the only
     * form of the address worth comparing between runs */
    {
        ULONG_PTR base = virtual_get_image_base( (const void *)(ULONG_PTR)rip );

        if (base) ERR_(seh)( "DIAG align-rva #%u rip=+0x%llx base=0x%llx\n",
                             seen, (unsigned long long)(rip - base), (unsigned long long)base );
    }

    /* A faulting spill says the frame is eight bytes out; what it does not say
     * is who put it there. The first faults of a run are the ones close enough
     * to the origin to be worth the bytes, so they get enough code to reach the
     * function's epilogue -- which gives the frame size, and with it the slot
     * holding the return address -- and the stack that return address is in. */
    if (seen <= 8)
    {
        unsigned int k;

        diag_hex( "align-code", (ULONG_PTR)rip, 96 );
        /* Far enough to hold several of the layer's frames, which run about
         * 0xc0 bytes each: the offset is inherited from a caller, so one frame
         * never names where it started. */
        for (k = 0; k < 4; k++)
            diag_hex( "align-stack", (ULONG_PTR)rsp + k * 256, 256 );
    }
}


void tuxblox_trace_record_us( const char *surface, const UNICODE_STRING *detail )
{
    char buf[1024];
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
/* Return addresses still sitting on the stack when the process is killed.
 *
 * Roblox's anti-tamper layer decides, then calls NtTerminateProcess, and it
 * writes no log of its own -- so the only record of how it got there is the
 * stack it is standing on. Every 8-byte slot above the caller's stack pointer
 * that lands in an executable mapping is reported as module+offset, which is
 * enough to find the deciding code in a disassembler even though the module
 * is packed and its addresses move between runs.
 *
 * Values that merely look like code addresses are reported too; this is a
 * diagnostic, not an unwinder. */
struct code_range
{
    ULONG64 start, end, base;
    const char *name;
};

static void dump_stack_return_addresses(void)
{
    struct code_range *ranges;
    unsigned int count = 0, capacity = 512, shown = 0;
    char *buf, *line, *next;
    size_t cap = 512 * 1024, len = 0;
    ULONG64 sp = get_syscall_caller_sp(), sp_end = 0;
    ssize_t n;
    int fd;
    unsigned int i, slot;

    if (!sp) return;
    if (!(buf = malloc( cap ))) return;
    if (!(ranges = malloc( capacity * sizeof(*ranges) ))) { free( buf ); return; }

    if ((fd = open( "/proc/self/maps", O_RDONLY )) == -1) goto done;
    while (len < cap - 1 && (n = read( fd, buf + len, cap - 1 - len )) > 0) len += n;
    close( fd );
    buf[len] = 0;

    for (line = buf; line && *line; line = next)
    {
        ULONG64 start, end;
        char *path, *perms;

        if ((next = strchr( line, '\n' ))) *next++ = 0;
        start = strtoull( line, &path, 16 );
        if (*path != '-') continue;
        end = strtoull( path + 1, &perms, 16 );
        /* remember where the stack we are about to walk actually ends */
        if (start <= sp && sp < end) sp_end = end;
        while (*perms == ' ') perms++;
        if (strlen( perms ) < 4 || perms[2] != 'x') continue;
        if (!(path = strchr( perms, '/' ))) continue;
        if (count == capacity) continue;

        ranges[count].start = start;
        ranges[count].end   = end;
        ranges[count].base  = start;
        ranges[count].name  = path;
        /* the module's base is the lowest mapping of the same file */
        for (i = 0; i < count; i++)
        {
            if (strcmp( ranges[i].name, path )) continue;
            if (ranges[i].base < ranges[count].base) ranges[count].base = ranges[i].base;
            else ranges[i].base = ranges[count].base;
        }
        count++;
    }

    TRACE_(tuxblox)( "STACK-BEGIN sp=0x%llx end=0x%llx\n",
                     (unsigned long long)sp, (unsigned long long)sp_end );
    /* The layer switches stacks, so sp can sit within this window of the
     * mapping's end. Reading past it faults, and because this runs inside
     * NtTerminateProcess the fault turns a terminate into a bogus
     * STATUS_ACCESS_VIOLATION return and the layer runs on into garbage. */
    for (slot = 0; slot < 4096 && shown < 96; slot++)
    {
        ULONG64 value;

        if (!sp_end || sp + slot * 8 + 8 > sp_end) break;
        value = ((const ULONG64 *)(ULONG_PTR)sp)[slot];

        for (i = 0; i < count; i++)
        {
            const char *base_name;

            if (value < ranges[i].start || value >= ranges[i].end) continue;
            base_name = strrchr( ranges[i].name, '/' );
            TRACE_(tuxblox)( "STACK +0x%04x 0x%llx  %s+0x%llx\n",
                             slot * 8, (unsigned long long)value,
                             base_name ? base_name + 1 : ranges[i].name,
                             (unsigned long long)(value - ranges[i].base) );
            shown++;
            break;
        }
    }
    TRACE_(tuxblox)( "STACK-END\n" );

done:
    free( ranges );
    free( buf );
}

/* The mapping that contains addr, so the dumps below can stop at its edge
 * instead of faulting past it. A fault here is not survivable in any useful
 * sense: these run inside NtTerminateProcess, where the syscall dispatcher
 * turns it into a STATUS_ACCESS_VIOLATION return and the caller carries on
 * as though the process had refused to die. */
static int mapping_range( ULONG64 addr, ULONG64 *range_start, ULONG64 *range_end )
{
    size_t cap = 512 * 1024, len = 0;
    char *buf, *line, *next, *p;
    ssize_t n;
    int fd, found = 0;

    if (!(buf = malloc( cap ))) return 0;
    if ((fd = open( "/proc/self/maps", O_RDONLY )) == -1) { free( buf ); return 0; }
    while (len < cap - 1 && (n = read( fd, buf + len, cap - 1 - len )) > 0) len += n;
    close( fd );
    buf[len] = 0;

    for (line = buf; line && *line && !found; line = next)
    {
        ULONG64 start, end;

        if ((next = strchr( line, '\n' ))) *next++ = 0;
        start = strtoull( line, &p, 16 );
        if (*p != '-') continue;
        end = strtoull( p + 1, &p, 16 );
        if (addr < start || addr >= end) continue;
        *range_start = start;
        *range_end = end;
        found = 1;
    }
    free( buf );
    return found;
}

/* The instructions around whatever called for the process to die.
 *
 * The module is packed on disk and only decrypted in memory, so the bytes that
 * matter exist nowhere else. Dumping them here and disassembling offline is
 * the only way to see the branch that chose to terminate. */
static void dump_code_around( ULONG64 addr )
{
    const unsigned char *code;
    char line[3 * 16 + 1];
    ULONG64 start, low, high;
    unsigned int i, j;

    if (!addr) return;
    if (!mapping_range( addr, &low, &high )) return;
    start = (addr - 0x400) & ~(ULONG64)0xf;
    if (start < low) start = low;
    code = (const unsigned char *)(ULONG_PTR)start;

    TRACE_(tuxblox)( "CODE-BEGIN around=0x%llx from=0x%llx\n",
                     (unsigned long long)addr, (unsigned long long)start );
    for (i = 0; i < 0x500 && start + i + 16 <= high; i += 16)
    {
        for (j = 0; j < 16; j++) snprintf( line + j * 3, 4, "%02x ", code[i + j] );
        TRACE_(tuxblox)( "CODE 0x%llx %s\n", (unsigned long long)(start + i), line );
    }
    TRACE_(tuxblox)( "CODE-END\n" );
}

/* Every system call that comes back with something other than success.
 *
 * Roblox's anti-tamper layer issues raw syscalls and keeps no log, so the only
 * way to see what it was told is to record it here. Both the wrapper path and
 * a bare `syscall` instruction funnel through the same dispatcher, so this
 * reaches its calls as well as ordinary ones. The caller's address is included
 * because the interesting failures are the ones from inside its own module,
 * not from Wine's own start-up probing. */
/* Arguments of every system call, recorded on the way in. Paired with the
 * return below, this is the complete record of what the anti-tamper layer asked
 * for and what it was told -- including the calls that succeed, which a
 * failures-only log misses entirely. The caller's address is included so its
 * own calls can be told apart from Wine's from the log alone. */
static void arm_watch(void);

void tuxblox_trace_syscall_args( unsigned int id, const ULONG_PTR *args, ULONG len )
{
    const char *name;

    if (!tuxblox_trace_enabled()) return;

    arm_watch();  /* once, guarded; arm the field watchpoints early */

    if (!trace_calls_enabled()) return;

    name = ntdll_syscall_name( id );
    len /= sizeof(ULONG_PTR);
    TRACE_(tuxblox)( "CALL name=%s rip=0x%llx a0=0x%llx a1=0x%llx a2=0x%llx a3=0x%llx\n",
                     name ? name : "?", (unsigned long long)get_syscall_caller_pc(),
                     (unsigned long long)(len > 0 ? args[0] : 0),
                     (unsigned long long)(len > 1 ? args[1] : 0),
                     (unsigned long long)(len > 2 ? args[2] : 0),
                     (unsigned long long)(len > 3 ? args[3] : 0) );
}

void tuxblox_trace_sysret( unsigned int id, ULONG_PTR retval )
{
    const char *name;

    if (!tuxblox_trace_enabled()) return;
    if (!trace_calls_enabled()) return;

    name = ntdll_syscall_name( id );
    TRACE_(tuxblox)( "RET  syscall=%u name=%s ret=0x%08x rip=0x%llx\n",
                     id, name ? name : "?", (unsigned int)retval,
                     (unsigned long long)get_syscall_caller_pc() );
}

/* Installing and removing the instrumentation callback -- the hook Windows
 * runs on every return from the kernel to user mode. Roblox's anti-tamper layer
 * installs one, and code that installs a hook generally checks that it ran. */
void tuxblox_trace_instrumentation( void *old, void *callback )
{
    if (!tuxblox_trace_enabled()) return;
    TRACE_(tuxblox)( "INSTR old=%p new=%p rip=0x%llx\n", old, callback,
                     (unsigned long long)get_syscall_caller_pc() );
}

/* Sampling read/write watchpoints on candidate userspace-fingerprint fields, to
 * see which ones Byfron actually reads (and from where) before terminating.
 * Sampling captures the caller RIP, so Byfron's reads are told apart from Wine's
 * own accesses -- essential here since these fields are Wine-internal. Armed from
 * inside the process. PEB is at the fixed 0x7ffd0000, main-thread TEB 0x7ffc0000.
 *
 * 64-bit only. The target is the 64-bit Roblox Player, and the addresses this
 * works with -- Wine's image band, sample IPs, register values recovered from a
 * perf record -- are all 64-bit whatever the build. Compiling it for i386 would
 * mean truncating every one of them, so it is left out there entirely. */
#ifdef __x86_64__

#define NWATCH 4
#define WATCH_DATA_PAGES 512 /* power of two; large so REGS_USER records (~160B)
                              * don't overflow before Byfron's late export reads */
static unsigned long long watch_addr[NWATCH];
static const char *watch_name[NWATCH];
static int watch_fd[NWATCH] = { -1, -1, -1, -1 };
static void *watch_buf[NWATCH];

static void arm_watch(void)
{
    static LONG done;
    unsigned int i;
    long pgsz = sysconf( _SC_PAGESIZE );
    LDR_DATA_TABLE_ENTRY *mod = NULL;
    LIST_ENTRY *head, *cur;

    if (done) return;
    if (!peb || !peb->ProcessParameters || !peb->LdrData) return;
    if (!image_is_target()) return;
    /* Off unless asked for. These are the CPU's four debug registers, and the
     * target wants them too: Roblox's anti-tamper layer sets its own hardware
     * breakpoints through NtSetContextThread, and with all four taken the
     * kernel refuses that with ENOSPC, which the server turns into
     * STATUS_DISK_FULL. Leaving them armed made every traced run measure the
     * tracer instead of the program. */
    if (!getenv( "TUXBLOX_WATCH" )) return;

    /* Find a builtin DLL entry -- one whose DllBase is in Wine's 0x6…… band, the
     * value that differs from Windows. Watch its fields to see whether Byfron reads
     * the base value, the size, the name string, or just walks the list links.
     * The list is empty until the loader populates it, so retry (don't set done)
     * until a builtin appears. */
    head = &peb->LdrData->InLoadOrderModuleList;
    {
        unsigned int n = 0;
        for (cur = head->Flink; cur && cur != head && n < 64; cur = cur->Flink, n++)
        {
            LDR_DATA_TABLE_ENTRY *m = (LDR_DATA_TABLE_ENTRY *)cur; /* InLoadOrderLinks at off 0 */
            if ((ULONG_PTR)m->DllBase >= 0x600000000000ull && (ULONG_PTR)m->DllBase < 0x700000000000ull) { mod = m; break; }
        }
    }
    if (!mod) return;   /* loader hasn't populated the list yet -- retry next syscall */

    if (InterlockedCompareExchange( &done, 1, 0 )) return;

    /* Byfron uses DllBase only as a pointer (proven: it reads [base+0x3c] e_lfanew
     * and forms the NT-headers pointer, never band-checks the value). So watch the
     * PE fields it parses through that pointer instead. NT = base + e_lfanew;
     * PE32+ OptionalHeader begins at NT+0x18. LEN_4 on the low 4 bytes of each
     * field -- an 8-byte read still overlaps and triggers. */
    {
        unsigned char *img = (unsigned char *)mod->DllBase;
        unsigned int e_lfanew = *(unsigned int *)(img + 0x3c);
        unsigned char *nt = img + e_lfanew;
        unsigned int exp_rva = *(unsigned int *)(nt + 0x88);   /* DataDirectory[0].VA */
        unsigned char *ed = img + exp_rva;                     /* IMAGE_EXPORT_DIRECTORY */

        /* Byfron reaches the export directory (reads AddressOfFunctions at ed+0x1c).
         * Watch the fields that would betray Wine: Base (ordinal base 1500 vs
         * Windows' low base), NumberOfFunctions / NumberOfNames (Wine 1699/1686 vs
         * Windows 2517/2516), and AddressOfNames (name enumeration, positive
         * control). */
        watch_addr[0] = (ULONG_PTR)(ed + 0x10);  watch_name[0] = "Export.Base(ordbase)";
        watch_addr[1] = (ULONG_PTR)(ed + 0x14);  watch_name[1] = "Export.NumberOfFunctions";
        watch_addr[2] = (ULONG_PTR)(ed + 0x18);  watch_name[2] = "Export.NumberOfNames";
        watch_addr[3] = (ULONG_PTR)(ed + 0x20);  watch_name[3] = "Export.AddressOfNames";
        TRACE_(tuxblox)( "WATCH module DllBase=%p e_lfanew=%#x exp_rva=%#x ed=%p Base=%u NumFunc=%u NumNames=%u\n",
                         mod->DllBase, e_lfanew, exp_rva, ed,
                         *(unsigned int *)(ed + 0x10), *(unsigned int *)(ed + 0x14), *(unsigned int *)(ed + 0x18) );
    }

    for (i = 0; i < NWATCH; i++)
    {
        struct perf_event_attr attr;
        if (!watch_addr[i]) continue;
        memset( &attr, 0, sizeof(attr) );
        attr.type          = PERF_TYPE_BREAKPOINT;
        attr.size          = sizeof(attr);
        attr.bp_type       = HW_BREAKPOINT_R | HW_BREAKPOINT_W;
        attr.bp_addr       = watch_addr[i];
        attr.bp_len        = HW_BREAKPOINT_LEN_4;
        attr.sample_period = 1;
        attr.sample_type   = PERF_SAMPLE_IP | PERF_SAMPLE_REGS_USER;
        /* GP regs: AX,BX,CX,DX,SI,DI,BP,SP,IP (bits 0-8) + R8..R15 (bits 16-23),
         * so a search's target-name pointer, live in some register, is captured. */
        attr.sample_regs_user = 0x00ff01ffULL;
        attr.exclude_kernel = 1;
        attr.exclude_hv     = 1;
        watch_fd[i] = syscall( __NR_perf_event_open, &attr, 0, -1, -1, 0 );
        if (watch_fd[i] == -1) { TRACE_(tuxblox)( "WATCH %s arm FAILED errno=%d\n", watch_name[i], errno ); continue; }
        watch_buf[i] = mmap( NULL, (1 + WATCH_DATA_PAGES) * pgsz, PROT_READ | PROT_WRITE, MAP_SHARED, watch_fd[i], 0 );
        if (watch_buf[i] == MAP_FAILED) watch_buf[i] = NULL;
    }
}

/* Load the target image's executable ranges so a sample IP can be attributed. */
struct exec_range { unsigned long long start, end, base; };
static unsigned int load_target_ranges( struct exec_range *r, unsigned int max )
{
    char buf[256*1024]; int fd; ssize_t n=0, len=0; char *line, *next; unsigned int cnt=0;
    if ((fd = open( "/proc/self/maps", O_RDONLY )) == -1) return 0;
    while ((size_t)len < sizeof(buf)-1 && (n = read( fd, buf+len, sizeof(buf)-1-len )) > 0) len += n;
    close( fd ); buf[len] = 0;
    for (line = buf; line && *line && cnt < max; line = next)
    {
        unsigned long long s, e; char *p, *perms;
        if ((next = strchr( line, '\n' ))) *next++ = 0;
        if (!strstr( line, "RobloxPlayerBeta" ) && !strstr( line, "robloxplayerbeta" )) continue;
        s = strtoull( line, &p, 16 ); if (*p != '-') continue;
        e = strtoull( p+1, &perms, 16 ); while (*perms==' ') perms++;
        if (strlen(perms) < 4 || perms[2] != 'x') continue;
        r[cnt].start = s; r[cnt].end = e; r[cnt].base = s; cnt++;
    }
    return cnt;
}

/* All readable user mappings (Byfron is packed -- its target strings live in
 * anonymous unpacked memory, not the file-backed RobloxPlayerBeta range), for
 * safely bounding register-value dereferences. Skip the kernel/vsyscall region. */
static unsigned int load_readable_ranges( struct exec_range *r, unsigned int max )
{
    char buf[512*1024]; int fd; ssize_t n=0, len=0; char *line, *next; unsigned int cnt=0;
    if ((fd = open( "/proc/self/maps", O_RDONLY )) == -1) return 0;
    while ((size_t)len < sizeof(buf)-1 && (n = read( fd, buf+len, sizeof(buf)-1-len )) > 0) len += n;
    close( fd ); buf[len] = 0;
    for (line = buf; line && *line && cnt < max; line = next)
    {
        unsigned long long s, e; char *p, *perms;
        if ((next = strchr( line, '\n' ))) *next++ = 0;
        s = strtoull( line, &p, 16 ); if (*p != '-') continue;
        e = strtoull( p+1, &perms, 16 ); while (*perms==' ') perms++;
        if (strlen(perms) < 4 || perms[0] != 'r') continue;
        if (s >= 0x800000000000ULL) continue;   /* skip vsyscall/kernel */
        r[cnt].start = s; r[cnt].end = e; r[cnt].base = s; cnt++;
    }
    return cnt;
}

/* Collected candidate strings that Byfron held in a register at an export-search
 * read. Filtered offline against the real export name sets -- noise that isn't an
 * ntdll export name is discarded there; a hit present in Windows' ntdll but not
 * Wine's is a name Byfron looks up and cannot find. */
#define NAMES_MAX 8192
static char names_pool[NAMES_MAX][64];
static unsigned int names_cnt;

static void add_candidate_name( const char *s )
{
    unsigned int i, k;
    for (i = 0; i < names_cnt; i++) if (!strcmp( names_pool[i], s )) return;
    if (names_cnt < NAMES_MAX)
    {
        for (k = 0; k < 63 && s[k]; k++) names_pool[names_cnt][k] = s[k];
        names_pool[names_cnt][k] = 0; names_cnt++;
    }
}

static int range_readable( unsigned long long v, const struct exec_range *rr, unsigned int nrr )
{
    unsigned int j;
    for (j = 0; j < nrr; j++) if (v >= rr[j].start && v < rr[j].end) return 1;
    return 0;
}

static void scan_reg_for_name( unsigned long long v, const struct exec_range *rr, unsigned int nrr )
{
    unsigned int j, k; const char *p;
    for (j = 0; j < nrr; j++)
        if (v >= rr[j].start && v < rr[j].end)
        {
            unsigned long long room = rr[j].end - v;
            char tmp[64]; if (room > 63) room = 63;
            p = (const char *)v;
            for (k = 0; k < room; k++)
            {
                char c = p[k];
                if (c == 0) break;
                if (!((c>='A'&&c<='Z')||(c>='a'&&c<='z')||(c>='0'&&c<='9')||c=='_'||c=='@')) return;
                tmp[k] = c;
            }
            if (k < 4 || k >= room) return;        /* too short, or not NUL-terminated in range */
            if (!((tmp[0]>='A'&&tmp[0]<='Z')||(tmp[0]>='a'&&tmp[0]<='z')||tmp[0]=='_')) return;
            tmp[k] = 0;
            add_candidate_name( tmp );
            return;
        }
}

/* Dump the in-memory (unpacked) code around a read IP so the instructions that
 * consume the watched field can be disassembled offline. Byfron is packed on
 * disk, so only the live bytes are meaningful. Emit [ip-16, ip+48) as hex,
 * clamped to the module's executable range, with the marker offset noted. */
static void dump_code_at( const char *tag, unsigned long long ip,
                          const struct exec_range *r )
{
    unsigned long long from = ip - 16, to = ip + 48;
    char hex[3 * 96 + 1]; unsigned int k = 0; unsigned long long a;

    if (from < r->start) from = r->start;
    if (to > r->end) to = r->end;
    if (to - from > 96) to = from + 96;
    for (a = from; a < to; a++)
        k += snprintf( hex + k, sizeof(hex) - k, "%02x ", *(volatile unsigned char *)a );
    TRACE_(tuxblox)( "WATCH-CODE %s +0x%llx marker@+0x%llx bytes: %s\n",
                     tag, from - r->base, ip - from, hex );
}

static void dump_watch(void)
{
    struct exec_range ranges[64], rranges[400];
    unsigned int nr = load_target_ranges( ranges, 64 ), nrr = load_readable_ranges( rranges, 400 ), i, j;
    long pgsz = sysconf( _SC_PAGESIZE );
    unsigned long long rbp_seen[128]; unsigned int rbp_seen_n = 0;

    for (i = 0; i < NWATCH; i++)
    {
        struct perf_event_mmap_page *meta;
        unsigned long long head, tail, count = 0, byfron = 0, shown = 0;
        unsigned long long uniq[16]; const struct exec_range *uniq_r[16]; unsigned int nuniq = 0, u;
        unsigned char *data;

        if (!watch_buf[i]) { TRACE_(tuxblox)( "WATCH %s addr=0x%llx unavailable\n", watch_name[i], watch_addr[i] ); continue; }
        meta = watch_buf[i];
        head = meta->data_head; __sync_synchronize();
        tail = meta->data_tail;
        data = (unsigned char *)watch_buf[i] + pgsz;
        while (tail < head)
        {
            struct perf_event_header *h = (struct perf_event_header *)(data + (tail % (WATCH_DATA_PAGES * pgsz)));
            if (h->size == 0) break;
            if (h->type == PERF_RECORD_SAMPLE)
            {
                unsigned char *rec = (unsigned char *)h + sizeof(*h);
                unsigned long long ip = *(unsigned long long *)rec;
                count++;
                for (j = 0; j < nr; j++)
                    if (ip >= ranges[j].start && ip < ranges[j].end)
                    {
                        /* layout: IP(8), abi(8), regs[17](8 each) -- see sample_regs_user mask */
                        unsigned long long *regs = (unsigned long long *)(rec + 16);
                        unsigned int g;
                        byfron++;
                        if (shown < 6) { TRACE_(tuxblox)( "WATCH %s READ-BY RobloxPlayerBeta.dll+0x%llx\n", watch_name[i], ip - ranges[j].base ); shown++; }
                        for (u = 0; u < nuniq; u++) if (uniq[u] == ip) break;
                        if (u == nuniq && nuniq < 16) { uniq[nuniq] = ip; uniq_r[nuniq] = &ranges[j]; nuniq++; }
                        for (g = 0; g < 17; g++) scan_reg_for_name( regs[g], rranges, nrr );
                        /* Byfron's VM keeps working values in an rbp-relative frame,
                         * so the target key pointer is stored there, not in a GP reg.
                         * Scan the frame once per unique rbp (regs[6]) for pointers
                         * that resolve to name strings anywhere readable. */
                        {
                            unsigned long long rbp = regs[6]; unsigned int s; int seen = 0;
                            for (s = 0; s < rbp_seen_n; s++) if (rbp_seen[s] == rbp) { seen = 1; break; }
                            if (!seen && rbp_seen_n < 128 && range_readable( rbp, rranges, nrr ))
                            {
                                long long off;
                                rbp_seen[rbp_seen_n++] = rbp;
                                /* pointers stored in the frame [rbp-0x1000, rbp+0x3000] */
                                for (off = -0x1000; off < 0x3000; off += 8)
                                {
                                    unsigned long long slot = rbp + off;
                                    if (!range_readable( slot, rranges, nrr )) continue;
                                    scan_reg_for_name( *(unsigned long long *)slot, rranges, nrr );
                                }
                                /* target keys copied INLINE into the frame as bytes */
                                {
                                    unsigned long long a = rbp - 0x1000, end = rbp + 0x3000;
                                    char tmp[64]; unsigned int kk = 0;
                                    for (; a < end; a++)
                                    {
                                        char c; if (!range_readable( a, rranges, nrr )) { kk = 0; continue; }
                                        c = *(volatile char *)a;
                                        if ((c>='A'&&c<='Z')||(c>='a'&&c<='z')||(c>='0'&&c<='9')||c=='_'||c=='@')
                                        { if (kk < 63) tmp[kk++] = c; }
                                        else { if (kk >= 4) { tmp[kk] = 0; if ((tmp[0]>='A'&&tmp[0]<='Z')||(tmp[0]>='a'&&tmp[0]<='z')||tmp[0]=='_') add_candidate_name( tmp ); } kk = 0; }
                                    }
                                }
                            }
                        }
                        break;
                    }
            }
            tail += h->size;
        }
        TRACE_(tuxblox)( "WATCH %s addr=0x%llx samples=%llu byfron=%llu unique=%u\n",
                         watch_name[i], watch_addr[i], count, byfron, nuniq );
        /* dump the code at each distinct read site of this field */
        for (u = 0; u < nuniq; u++)
            dump_code_at( watch_name[i], uniq[u], uniq_r[u] );
    }

    /* candidate export-name strings Byfron held in a register during a search;
     * intersect offline with the real ntdll export sets to find missing lookups */
    for (i = 0; i < names_cnt; i++)
        TRACE_(tuxblox)( "WATCH-NAME %s\n", names_pool[i] );
}

#else  /* __x86_64__ */

static void arm_watch(void) { }
static void dump_watch(void) { }

#endif  /* __x86_64__ */

void tuxblox_trace_exit( LONG exit_code, const char *how )
{
    if (!tuxblox_trace_enabled()) return;

    tuxblox_diag_bp_report();
    if (diag_align_seen) diag_align_report( diag_align_seen );
    dump_watch();
    dump_maps();
    dump_stack_return_addresses();
    dump_code_around( get_syscall_caller_pc() );
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
 * the last few instructions before the process actually terminates.
 *
 * exit_code == 0 is skipped entirely: a clean exit is never what the
 * launcher's crash popup needs to explain, and WebView2 spawns several
 * short-lived helper processes (renderer, GPU, network service, ...) that
 * routinely exit 0 mid-session -- each one otherwise leaves its own marker
 * line in the log. The launcher's findRealExitCodeInLog() takes the *last*
 * marker line unconditionally, so on an ordinary user-initiated close, a
 * trailing "=0" from one of those helpers could still land in the log after
 * Proton's own wrapper exit code came back non-zero for unrelated reasons,
 * producing a nonsensical "Roblox Error ... Exit Code: 0 (OK)" popup for a
 * clean shutdown. Not logging 0 means the last line in the log is always an
 * actual non-zero exit worth showing, if there is one. */
void tuxblox_report_real_exit_code( LONG exit_code )
{
    char buf[64];
    int len;

    if (!image_is_target()) return;
    if (exit_code == 0) return;

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

/* Which information classes the program actually asks for.
 *
 * The measured divergences in the token and process surfaces are many, and
 * answering them all is expensive -- TokenAccessInformation alone is 1808
 * bytes of nested structure, and infoprobe's capture differs on 6304 lines.
 * This says which of them are worth answering: it records every class asked,
 * the size asked for and the answer given, so the classes the program never
 * touches can be left alone. TUXBLOX_DIAG_TOKEN=1.
 */
static int diag_class_state = -1;

static BOOL diag_class_enabled(void)
{
    if (diag_class_state == -1)
    {
        const char *v = getenv( "TUXBLOX_DIAG_TOKEN" );
        diag_class_state = (v && *v && *v != '0') ? 1 : 0;
    }
    return diag_class_state == 1;
}

void tuxblox_diag_class( const char *surface, unsigned int class, ULONG length,
                         ULONG len, unsigned int status )
{
    /* One line per class per size, not one per call: a class read twice --
     * once for the length, once for the data -- is the normal shape and would
     * otherwise bury the classes asked only once. */
    static struct { const char *surface; unsigned int class; ULONG length; } seen[4096];
    static unsigned int seen_count;
    unsigned int i;

    if (!diag_class_enabled()) return;
    for (i = 0; i < seen_count; i++)
        if (seen[i].surface == surface && seen[i].class == class && seen[i].length == length) return;
    if (seen_count == ARRAY_SIZE(seen)) return;
    seen[seen_count].surface = surface;
    seen[seen_count].class = class;
    seen[seen_count].length = length;
    seen_count++;

    ERR_(seh)( "tuxblox: ask s=%d %s class=%u length=%u len=%u status=%08x\n",
               (int)InterlockedIncrement( &diag_seq ), surface, class,
               (unsigned int)length, (unsigned int)len, status );
}
