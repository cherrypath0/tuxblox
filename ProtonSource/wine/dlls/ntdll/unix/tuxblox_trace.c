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
    ULONG64 sp = get_syscall_caller_sp();
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

    for (line = buf; line && *line && count < capacity; line = next)
    {
        ULONG64 start, end;
        char *path, *perms;

        if ((next = strchr( line, '\n' ))) *next++ = 0;
        start = strtoull( line, &path, 16 );
        if (*path != '-') continue;
        end = strtoull( path + 1, &perms, 16 );
        while (*perms == ' ') perms++;
        if (strlen( perms ) < 4 || perms[2] != 'x') continue;
        if (!(path = strchr( perms, '/' ))) continue;

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

    TRACE_(tuxblox)( "STACK-BEGIN sp=0x%llx\n", (unsigned long long)sp );
    for (slot = 0; slot < 4096 && shown < 96; slot++)
    {
        ULONG64 value = ((const ULONG64 *)(ULONG_PTR)sp)[slot];

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

/* The instructions around whatever called for the process to die.
 *
 * The module is packed on disk and only decrypted in memory, so the bytes that
 * matter exist nowhere else. Dumping them here and disassembling offline is
 * the only way to see the branch that chose to terminate. */
static void dump_code_around( ULONG64 addr )
{
    const unsigned char *code;
    char line[3 * 16 + 1];
    ULONG64 start;
    unsigned int i, j;

    if (!addr) return;
    start = (addr - 0x400) & ~(ULONG64)0xf;
    code = (const unsigned char *)(ULONG_PTR)start;

    TRACE_(tuxblox)( "CODE-BEGIN around=0x%llx from=0x%llx\n",
                     (unsigned long long)addr, (unsigned long long)start );
    for (i = 0; i < 0x500; i += 16)
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
