/*
 * System information APIs
 *
 * Copyright 1996-1998 Marcus Meissner
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
#include <string.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <sys/wait.h>
#include <poll.h>
#include <signal.h>
#include <errno.h>
#include <assert.h>
#include <sys/time.h>
#include <time.h>
#include <dirent.h>
#ifdef HAVE_SYS_PARAM_H
# include <sys/param.h>
#endif
#ifdef HAVE_SYS_SYSCTL_H
# include <sys/sysctl.h>
#endif
#ifdef HAVE_SYS_UTSNAME_H
# include <sys/utsname.h>
#endif
#ifdef HAVE_MACHINE_CPU_H
# include <machine/cpu.h>
#endif
#ifdef HAVE_SYS_RANDOM_H
# include <sys/random.h>
#endif
#ifdef HAVE_SYS_RESOURCE_H
# include <sys/resource.h>
#endif
#ifdef HAVE_SYS_AUXV_H
# include <sys/auxv.h>
#endif
#ifdef __APPLE__
# include <CoreFoundation/CoreFoundation.h>
# include <IOKit/IOKitLib.h>
# include <IOKit/ps/IOPSKeys.h>
# include <IOKit/ps/IOPowerSources.h>
# include <mach/mach.h>
# include <mach/machine.h>
# include <mach/mach_init.h>
# include <mach/mach_host.h>
# include <mach/vm_map.h>
#endif

#if defined(HAVE_LIBHWLOC)
# include <hwloc.h>
#endif

#include "ntstatus.h"
#define WIN32_NO_STATUS
#include "windef.h"
#include "winternl.h"
#include "ddk/wdm.h"
#include "wine/asm.h"
#include "unix_private.h"
#include "wine/debug.h"

WINE_DEFAULT_DEBUG_CHANNEL(ntdll);

#pragma pack(push,1)

struct smbios_prologue
{
    BYTE calling_method;
    BYTE major_version;
    BYTE minor_version;
    BYTE revision;
    DWORD length;
};

struct smbios_header
{
    BYTE type;
    BYTE length;
    WORD handle;
};

struct smbios_bios
{
    struct smbios_header hdr;
    BYTE vendor;
    BYTE version;
    WORD start;
    BYTE date;
    BYTE size;
    UINT64 characteristics;
    BYTE characteristics_ext[2];
    BYTE system_bios_major_release;
    BYTE system_bios_minor_release;
    BYTE ec_firmware_major_release;
    BYTE ec_firmware_minor_release;
};

struct smbios_system
{
    struct smbios_header hdr;
    BYTE vendor;
    BYTE product;
    BYTE version;
    BYTE serial;
    BYTE uuid[16];
    BYTE wake_up_type;
    BYTE sku_number;
    BYTE family;
};

struct smbios_board
{
    struct smbios_header hdr;
    BYTE vendor;
    BYTE product;
    BYTE version;
    BYTE serial;
    BYTE asset_tag;
    BYTE feature_flags;
    BYTE location;
    WORD chassis_handle;
    BYTE board_type;
    BYTE num_contained_handles;
};

struct smbios_chassis
{
    struct smbios_header hdr;
    BYTE vendor;
    BYTE type;
    BYTE version;
    BYTE serial;
    BYTE asset_tag;
    BYTE boot_state;
    BYTE power_supply_state;
    BYTE thermal_state;
    BYTE security_status;
    DWORD oem_defined;
    BYTE height;
    BYTE num_power_cords;
    BYTE num_contained_elements;
    BYTE contained_element_rec_length;
};

struct smbios_processor
{
    struct smbios_header hdr;
    BYTE socket;
    BYTE type;
    BYTE family;
    BYTE vendor;
    ULONGLONG id;
    BYTE version;
    BYTE voltage;
    WORD clock;
    WORD max_speed;
    WORD cur_speed;
    BYTE status;
    BYTE upgrade;
    WORD l1cache;
    WORD l2cache;
    WORD l3cache;
    BYTE serial;
    BYTE asset_tag;
    BYTE part_number;
    BYTE core_count;
    BYTE core_enabled;
    BYTE thread_count;
    WORD characteristics;
    WORD family2;
    WORD core_count2;
    WORD core_enabled2;
    WORD thread_count2;
};

struct smbios_boot_info
{
    struct smbios_header hdr;
    BYTE reserved[6];
    BYTE boot_status[10];
};

struct smbios_processor_specific_block
{
    BYTE length;
    BYTE processor_type;
    BYTE data[];
};

struct smbios_processor_additional_info
{
    struct smbios_header hdr;
    WORD ref_handle;
    struct smbios_processor_specific_block info_block;
};

struct smbios_wine_core_id_regs_arm64
{
    WORD num_regs;
    struct smbios_wine_id_reg_value_arm64
    {
        WORD reg;
        UINT64 value;
    } regs[];
};

#pragma pack(pop)

enum smbios_type
{
    SMBIOS_TYPE_BIOS = 0,
    SMBIOS_TYPE_SYSTEM = 1,
    SMBIOS_TYPE_BASEBOARD = 2,
    SMBIOS_TYPE_CHASSIS = 3,
    SMBIOS_TYPE_PROCESSOR = 4,
    SMBIOS_TYPE_BOOTINFO = 32,
    SMBIOS_TYPE_PROCESSOR_ADDITIONAL_INFO = 44,
    SMBIOS_TYPE_END = 127
};

#define SMBIOS_MAJOR_VERSION 3
#define SMBIOS_MINOR_VERSION 0

/* Firmware table providers */
#define ACPI 0x41435049
#define FIRM 0x4649524D
#define RSMB 0x52534D42

static char cpu_name[49];
static char cpu_vendor[13];
static USHORT cpu_level, cpu_revision;
static ULONGLONG cpu_id;
static ULONGLONG cpu_features_bitmap[2];
static ULONG *performance_cores;
static unsigned int performance_cores_capacity = 0;
static SYSTEM_LOGICAL_PROCESSOR_INFORMATION *logical_proc_info;
static unsigned int logical_proc_info_len, logical_proc_info_alloc_len;
static SYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX *logical_proc_info_ex;
static unsigned int logical_proc_info_ex_size, logical_proc_info_ex_alloc_size;
static ULONG_PTR system_cpu_mask;

static pthread_mutex_t timezone_mutex = PTHREAD_MUTEX_INITIALIZER;

static const char default_tzinfo_dir[] = "/usr/share/zoneinfo";
static const WCHAR Time_ZonesW[] = { '\\','R','e','g','i','s','t','r','y','\\',
    'M','a','c','h','i','n','e','\\',
    'S','o','f','t','w','a','r','e','\\',
    'M','i','c','r','o','s','o','f','t','\\',
    'W','i','n','d','o','w','s',' ','N','T','\\',
    'C','u','r','r','e','n','t','V','e','r','s','i','o','n','\\',
    'T','i','m','e',' ','Z','o','n','e','s',0 };
static struct
{
    struct cpu_topology_override mapping;
    ULONG_PTR siblings_mask[MAXIMUM_PROCESSORS];
}
cpu_override;

/*******************************************************************************
 * Architecture specific feature detection for CPUs
 *
 * This a set of mutually exclusive #if define()s each providing its
 * own functions to be called from init_cpu_info().
 */
#if defined(__i386__) || defined(__x86_64__)

static int next_xstate_offset( int off, UINT64 compaction_mask, int feature_idx )
{
    const UINT64 feature_mask = (UINT64)1 << feature_idx;

    if (!compaction_mask)
        return user_shared_data->XState.Features[feature_idx + 1].Offset - sizeof(XSAVE_FORMAT);

    if (compaction_mask & feature_mask) off += user_shared_data->XState.Features[feature_idx].Size;
    if (user_shared_data->XState.AlignedFeatures & (feature_mask << 1)) off = (off + 63) & ~63;
    return off;
}

unsigned int xstate_get_size( UINT64 compaction_mask, UINT64 mask )
{
    unsigned int i;
    int off;

    mask >>= 2;
    off = sizeof(XSAVE_AREA_HEADER);
    i = 2;
    while (mask)
    {
        if (mask == 1) return off + user_shared_data->XState.Features[i].Size;
        off = next_xstate_offset( off, compaction_mask, i );
        mask >>= 1;
        ++i;
    }
    return off;
}

void copy_xstate( XSAVE_AREA_HEADER *dst, XSAVE_AREA_HEADER *src, UINT64 mask )
{
    unsigned int i;
    int src_off, dst_off;
    UINT64 extended_features = user_shared_data->XState.EnabledFeatures & ~(UINT64)3;

    mask &= extended_features & src->Mask;
    if (src->CompactionMask) mask &= src->CompactionMask;
    if (dst->CompactionMask) mask &= dst->CompactionMask;
    dst->Mask = (dst->Mask & ~extended_features) | mask;
    mask >>= 2;
    src_off = dst_off = sizeof(XSAVE_AREA_HEADER);
    i = 2;
    while (1)
    {
        if (mask & 1) memcpy( (char *)dst + dst_off, (char *)src + src_off,
                              user_shared_data->XState.Features[i].Size );
        if (!(mask >>= 1)) break;
        src_off = next_xstate_offset( src_off, src->CompactionMask, i );
        dst_off = next_xstate_offset( dst_off, dst->CompactionMask, i );
        ++i;
    }
}

static inline void do_cpuid( unsigned int ax, unsigned int cx, unsigned int *p )
{
    __asm__ ( "cpuid" : "=a" (p[0]), "=b" (p[1]), "=c" (p[2]), "=d" (p[3]) : "a" (ax), "c" (cx) );
}

static inline UINT64 do_xgetbv( unsigned int cx )
{
    UINT low, high;
    __asm__( "xgetbv" : "=a" (low), "=d" (high) : "c" (cx) );
    return low | ((UINT64)high << 32);
}

/* Detect if a SSE2 processor is capable of Denormals Are Zero (DAZ) mode.
 *
 * This function assumes you have already checked for SSE2/FXSAVE support. */
static inline BOOL have_sse_daz_mode(void)
{
#ifdef __i386__
    /* Intel says we need a zeroed 16-byte aligned buffer */
    char buffer[512 + 16];
    XSAVE_FORMAT *state = (XSAVE_FORMAT *)(((ULONG_PTR)buffer + 15) & ~15);
    memset(buffer, 0, sizeof(buffer));

    __asm__ __volatile__( "fxsave %0" : "=m" (*state) : "m" (*state) );

    return (state->MxCsr_Mask & (1 << 6)) >> 6;
#else /* all x86_64 processors include SSE2 with DAZ mode */
    return TRUE;
#endif
}

static void init_cpu_model(void)
{
    unsigned int regs[4];

    do_cpuid( 0x00000000, 0, regs );  /* get standard cpuid level and vendor name */
    memcpy( cpu_vendor, &regs[1], sizeof(unsigned int) );
    memcpy( cpu_vendor + 4, &regs[3], sizeof(unsigned int) );
    memcpy( cpu_vendor + 8, &regs[2], sizeof(unsigned int) );

    do_cpuid( 0x00000001, 0, regs ); /* get cpu features */
    cpu_id = regs[0] | ((ULONGLONG)regs[3] << 32);
    cpu_level = ((regs[0] >> 8) & 0xf) + ((regs[0] >> 20) & 0xff); /* family */
    cpu_revision  = ((regs[0] >> 16) & 0xf) << 12; /* extended model */
    cpu_revision |= ((regs[0] >> 4 ) & 0xf) << 8;  /* model    */
    cpu_revision |= regs[0] & 0xf;                 /* stepping */

    do_cpuid( 0x80000000, 0, regs );  /* get vendor cpuid level */
    if (regs[0] >= 0x80000004)
    {
        char *p = cpu_name;

        do_cpuid( 0x80000002, 0, (unsigned int *)p );
        p += sizeof(regs);
        do_cpuid( 0x80000003, 0, (unsigned int *)p );
        p += sizeof(regs);
        do_cpuid( 0x80000004, 0, (unsigned int *)p );
        p += sizeof(regs);
        *p = 0;
    }
}

static ULONGLONG get_cpu_features(void)
{
    const BOOLEAN *pf = user_shared_data->ProcessorFeatures;
    ULONGLONG features;

    /* feature bits are derived from KF_* flags and Geoff Chappell's documentation */

    if (native_machine == IMAGE_FILE_MACHINE_AMD64)
    {
        features = 0x20013dfe; /* tsc | vme | cmov | pge | pse | mtrr | cx8 | mmx | pat | fxsr | sep | sse | sse2 | nx */
        if (pf[PF_RDRAND_INSTRUCTION_AVAILABLE]) features |= 0x100000000; /* rdrand */
        if (pf[PF_XSAVE_ENABLED])                features |= 0x00800000;  /* xstate */
        if (pf[PF_COMPARE_EXCHANGE128])          features |= 0x00100000;  /* cx16 */
        if (pf[PF_SSE3_INSTRUCTIONS_AVAILABLE])  features |= 0x00080000;  /* sse3 */
        if (pf[PF_RDTSCP_INSTRUCTION_AVAILABLE]) features |= 0x400000000; /* rdtscp */
        if (pf[PF_RDWRFSGSBASE_AVAILABLE])       features |= 0x10000000;  /* fsgsbase */

        if (!strcmp( cpu_vendor, "AuthenticAMD" ))      features |= 0x00200000;  /* amd */
        else if (!strcmp( cpu_vendor, "GenuineIntel" )) features |= 0x01000000;  /* intel */
    }
    else
    {
        features = 0x00000275; /* vme | pge | pse | mtrr */
        if (pf[PF_RDTSC_INSTRUCTION_AVAILABLE])   features |= 0x00000002;  /* tsc */
        if (pf[PF_COMPARE_EXCHANGE_DOUBLE])       features |= 0x00000080;  /* cx8 */
        if (pf[PF_MMX_INSTRUCTIONS_AVAILABLE])    features |= 0x00000100;  /* mmx */
        if (pf[PF_XMMI_INSTRUCTIONS_AVAILABLE])   features |= 0x00042800;  /* sse | fxsr | clfsh */
        if (pf[PF_XMMI64_INSTRUCTIONS_AVAILABLE]) features |= 0x00010000;  /* sse2 */
        if (pf[PF_SSE3_INSTRUCTIONS_AVAILABLE])   features |= 0x00080000;  /* sse3 */
        if (pf[PF_RDRAND_INSTRUCTION_AVAILABLE])  features |= 0x02000000;  /* rdrand */
        if (pf[PF_NX_ENABLED])                    features |= 0x20000000;  /* nx */
        if (pf[PF_RDTSCP_INSTRUCTION_AVAILABLE])  features |= 0x100000000; /* rdtscp */
        if (pf[PF_3DNOW_INSTRUCTIONS_AVAILABLE])  features |= 0x00004000;  /* 3dnow */
        if (pf[PF_VIRT_FIRMWARE_ENABLED])         features |= 0x0c000000;  /* vmx */

        if (!strcmp( cpu_vendor, "GenuineIntel" ))      features |= 0x008000000; /* intel */
        else if (!strcmp( cpu_vendor, "AuthenticAMD" )) features |= 0x001000000; /* amd */
    }
    return features;
}

static void init_xstate_features( XSTATE_CONFIGURATION *xstate )
{
    static const ULONG64 supported_features = (1 << XSTATE_AVX) | (1 << XSTATE_MPX_BNDREGS) |
                                              (1 << XSTATE_MPX_BNDCSR) | (1 << XSTATE_AVX512_KMASK) |
                                              (1 << XSTATE_AVX512_ZMM_H) | (1 << XSTATE_AVX512_ZMM);
    ULONG64 supported_mask;
    unsigned int i, off, regs[4];

    do_cpuid( 0x0000000d, 0, regs );
    TRACE( "XSAVE details %#x, %#x, %#x, %#x.\n", regs[0], regs[1], regs[2], regs[3] );
    supported_mask = ((ULONG64)regs[3] << 32) | regs[0];
    supported_mask &= do_xgetbv(0) & supported_features;

    xstate->EnabledFeatures = (1 << XSTATE_LEGACY_FLOATING_POINT) | (1 << XSTATE_LEGACY_SSE) | supported_mask;
    xstate->EnabledVolatileFeatures = xstate->EnabledFeatures;
    xstate->AllFeatureSize = regs[1];

    do_cpuid( 0x0000000d, 1, regs );
    xstate->OptimizedSave          = !!(regs[0] & (1 << 0));
    xstate->CompactionEnabled      = !!(regs[0] & (1 << 1));
    xstate->ExtendedFeatureDisable = !!(regs[0] & (1 << 4));

    xstate->Features[0].Size = xstate->AllFeatures[0] = offsetof( XSAVE_FORMAT, XmmRegisters );
    xstate->Features[1].Size = xstate->AllFeatures[1] = sizeof(M128A) * 16;
    xstate->Features[1].Offset = xstate->Features[0].Size;
    off = sizeof(XSAVE_FORMAT) + sizeof(XSAVE_AREA_HEADER);
    supported_mask >>= 2;

    for (i = 2; supported_mask; ++i, supported_mask >>= 1)
    {
        if (!(supported_mask & 1)) continue;
        do_cpuid( 0x0000000d, i, regs );
        xstate->Features[i].Offset = regs[1];
        xstate->Features[i].Size = xstate->AllFeatures[i] = regs[0];
        if (regs[2] & 2)
        {
            xstate->AlignedFeatures |= (ULONG64)1 << i;
            off = (off + 63) & ~63;
        }
        off += xstate->Features[i].Size;
        TRACE( "xstate[%d] offset %x, size %x, aligned %d.\n", i,
               xstate->Features[i].Offset, xstate->Features[i].Size, !!(regs[2] & 2) );
    }

    xstate->Size = xstate->CompactionEnabled ? off :
           offsetof( XSAVE_FORMAT, XmmRegisters ) + xstate->Features[i - 1].Offset + xstate->Features[i - 1].Size;
    /* The size of every feature described here, which is the size just computed
     * plus the supervisor features, and there are none of those. It was taken
     * straight from CPUID above, so it covered whatever the host has enabled in
     * XCR0 rather than the features actually listed here -- 0x988 against the
     * 0x340 they add up to, and against 0x350 on Windows, whose extra 0x10 is
     * the one supervisor feature it enables. This field sits in the shared page
     * at a fixed address and is readable with no system call, so a value that
     * contradicts the features beside it is visible to anything that looks. */
    xstate->AllFeatureSize = xstate->Size;
    TRACE( "xstate size %x, compacted %d, optimized %d.\n",
           xstate->Size, xstate->CompactionEnabled, xstate->OptimizedSave );
}

void init_shared_data_cpuinfo( KUSER_SHARED_DATA *data )
{
    BOOLEAN *features = data->ProcessorFeatures;
    unsigned int regs[4];

    features[PF_FASTFAIL_AVAILABLE]      = TRUE;
    features[PF_COMPARE_EXCHANGE_DOUBLE] = TRUE;

    do_cpuid( 0x00000001, 0, regs ); /* get cpu features */
    features[PF_RDTSC_INSTRUCTION_AVAILABLE]   = !!(regs[3] & (1 << 4));
    features[PF_PAE_ENABLED]                   = !!(regs[3] & (1 << 6));
    features[PF_MMX_INSTRUCTIONS_AVAILABLE]    = !!(regs[3] & (1 << 23));
    features[PF_XMMI_INSTRUCTIONS_AVAILABLE]   = (regs[3] & (1 << 24)) && (regs[3] & (1 << 25));
    features[PF_XMMI64_INSTRUCTIONS_AVAILABLE] = !!(regs[3] & (1 << 26));
    features[PF_SSE3_INSTRUCTIONS_AVAILABLE]   = !!(regs[2] & (1 << 0));
    features[PF_VIRT_FIRMWARE_ENABLED]         = !!(regs[2] & (1 << 5));
    features[PF_SSSE3_INSTRUCTIONS_AVAILABLE]  = !!(regs[2] & (1 << 9));
    features[PF_COMPARE_EXCHANGE128]           = !!(regs[2] & (1 << 13));
    features[PF_SSE4_1_INSTRUCTIONS_AVAILABLE] = !!(regs[2] & (1 << 19));
    features[PF_SSE4_2_INSTRUCTIONS_AVAILABLE] = !!(regs[2] & (1 << 20));
    features[PF_XSAVE_ENABLED]                 = !!(regs[2] & (1 << 27));
    features[PF_AVX_INSTRUCTIONS_AVAILABLE]    = !!(regs[2] & (1 << 28));
    features[PF_RDRAND_INSTRUCTION_AVAILABLE]  = !!(regs[2] & (1 << 30));
    /* Windows reports this as absent on hardware that plainly supports it, so
     * detecting it correctly is itself the difference. Measured zero on
     * Windows 11 25H2 with workspace/tests/infoprobe.exe. */
    features[PF_SSE_DAZ_MODE_AVAILABLE] = FALSE;

    do_cpuid( 0x00000000, 0, regs );
    if (regs[0] >= 0x00000007)
    {
        do_cpuid( 0x00000007, 0, regs ); /* get extended features */
        features[PF_RDWRFSGSBASE_AVAILABLE]         = !!(regs[1] & (1 << 0));
        features[PF_AVX2_INSTRUCTIONS_AVAILABLE]    = !!(regs[1] & (1 << 5));
        features[PF_BMI2_INSTRUCTIONS_AVAILABLE]    = !!(regs[1] & (1 << 8));
        features[PF_ERMS_AVAILABLE]                 = !!(regs[1] & (1 << 9));
        features[PF_AVX512F_INSTRUCTIONS_AVAILABLE] = !!(regs[1] & (1 << 16));
        features[PF_RDPID_INSTRUCTION_AVAILABLE]    = !!(regs[2] & (1 << 22));
        features[PF_MOVDIR64B_INSTRUCTION_AVAILABLE]= !!(regs[2] & (1 << 28));
#if defined(__linux__) && defined(AT_HWCAP2)
        features[PF_RDWRFSGSBASE_AVAILABLE] &= !!(getauxval( AT_HWCAP2 ) & 2);
#endif
    }

    do_cpuid( 0x80000000, 0, regs );  /* get vendor cpuid level */
    if (regs[0] >= 0x80000001)
    {
        do_cpuid( 0x80000001, 0, regs );  /* get vendor features */
        features[PF_MONITORX_INSTRUCTION_AVAILABLE] = !!(regs[2] & (1 << 29));
        features[PF_NX_ENABLED]                     = !!(regs[3] & (1 << 20));
        features[PF_RDTSCP_INSTRUCTION_AVAILABLE]   = !!(regs[3] & (1 << 27));
        features[PF_VIRT_FIRMWARE_ENABLED]         |= !!(regs[2] & (1 << 2));
        features[PF_3DNOW_INSTRUCTIONS_AVAILABLE]   = !!(regs[3] & (1u << 31));
    }

    if (features[PF_XSAVE_ENABLED])
        init_xstate_features( &data->XState );
}

#elif defined(__arm__) || defined(__aarch64__)

static int has_feature( const char *line, const char *feat )
{
    size_t len = strlen(feat);

    while (*line)
    {
        while (*line == ' ' || *line == '\t') line++;
        if (!strncmp( line, feat, len ) && (!line[len] || isspace(line[len]))) return 1;
        while (*line && *line != ' ' && *line != '\t') line++;
    }
    return 0;
}

static void init_cpu_model(void)
{
    unsigned int implementer = 0x41, part = 0, variant = 0, revision = 0;
#ifdef linux
    char line[512];
    char *s, *value;
    FILE *f = fopen("/proc/cpuinfo", "r");
    if (f)
    {
        while (fgets( line, sizeof(line), f ))
        {
            /* NOTE: the ':' is the only character we can rely on */
            if (!(value = strchr(line,':'))) continue;
            /* terminate the valuename */
            s = value - 1;
            while ((s >= line) && (*s == ' ' || *s == '\t')) s--;
            s[1] = 0;
            /* and strip leading spaces from value */
            value += 1;
            while (*value == ' ' || *value == '\t') value++;
            if ((s = strchr( value,'\n' ))) *s = 0;
            if (!strcmp( line, "CPU implementer" )) implementer = strtoul( value, NULL, 0);
            else if (!strcmp( line, "CPU part" )) part = strtoul( value, NULL, 0);
            else if (!strcmp( line, "CPU variant" )) variant = strtoul( value, NULL, 0);
            else if (!strcmp( line, "CPU revision" )) revision = strtoul( value, NULL, 0);
            else if (!strcmp( line, "Features" ))
            {
                static const struct { ULONG flag; const char *name; } features[] =
                {
                    { PF_ARM_SHA3_INSTRUCTIONS_AVAILABLE, "sha3" },
                    { PF_ARM_SHA512_INSTRUCTIONS_AVAILABLE, "sha512" },
                    { PF_ARM_V82_I8MM_INSTRUCTIONS_AVAILABLE, "i8mm" },
                    { PF_ARM_V82_FP16_INSTRUCTIONS_AVAILABLE, "fphp" },
                    { PF_ARM_V86_BF16_INSTRUCTIONS_AVAILABLE, "bf16" },
                    { PF_ARM_V86_EBF16_INSTRUCTIONS_AVAILABLE, "ebf16" },
                    { PF_ARM_SME_INSTRUCTIONS_AVAILABLE, "sme" },
                    { PF_ARM_SME2_INSTRUCTIONS_AVAILABLE, "sme2" },
                    { PF_ARM_SME2_1_INSTRUCTIONS_AVAILABLE, "sme2p1" },
                    { PF_ARM_SME2_2_INSTRUCTIONS_AVAILABLE, "sme2p2" },
                    { PF_ARM_SME_AES_INSTRUCTIONS_AVAILABLE, "smeaes" },
                    { PF_ARM_SME_SBITPERM_INSTRUCTIONS_AVAILABLE, "smesbitperm" },
                    /* The PF_ARM_SME_SF8MM4_INSTRUCTIONS_AVAILABLE and
                     * PF_ARM_SME_SF8MM8_INSTRUCTIONS_AVAILABLE flags aren't exposed by
                     * the Linux kernel, see
                     * https://lists.infradead.org/pipermail/linux-arm-kernel/2025-January/991187.html */
                    { PF_ARM_SME_SF8DP2_INSTRUCTIONS_AVAILABLE, "smesf8dp2" },
                    { PF_ARM_SME_SF8DP4_INSTRUCTIONS_AVAILABLE, "smesf8dp4" },
                    { PF_ARM_SME_SF8FMA_INSTRUCTIONS_AVAILABLE, "smesf8fma" },
                    { PF_ARM_SME_F8F32_INSTRUCTIONS_AVAILABLE, "smef8f32" },
                    { PF_ARM_SME_F8F16_INSTRUCTIONS_AVAILABLE, "smef8f16" },
                    { PF_ARM_SME_F16F16_INSTRUCTIONS_AVAILABLE, "smef16f16" },
                    { PF_ARM_SME_B16B16_INSTRUCTIONS_AVAILABLE, "smeb16b16" },
                    { PF_ARM_SME_F64F64_INSTRUCTIONS_AVAILABLE, "smef64f64" },
                    { PF_ARM_SME_I16I64_INSTRUCTIONS_AVAILABLE, "smei16i64" },
                    { PF_ARM_SME_LUTv2_INSTRUCTIONS_AVAILABLE, "smelutv2" },
                    { PF_ARM_SME_FA64_INSTRUCTIONS_AVAILABLE, "smefa64" },
                };

                for (unsigned int i = 0; i < ARRAY_SIZE(features); i++)
                {
                    ULONG flag = features[i].flag - PROCESSOR_FEATURE_MAX;
                    if (!has_feature( value, features[i].name )) continue;
                    cpu_features_bitmap[flag / 64] |= 1ull << (flag % 64);
                }
            }
        }
        fclose( f );
    }
#endif
    cpu_level = part;
    cpu_revision = (variant << 8) | revision;
    cpu_id = (implementer << 24) | (variant << 20) | (0x0f << 16) | (part << 4) | revision;
    switch (implementer)
    {
    case 0x41: strcpy( cpu_vendor, "ARM" ); break;
    case 0x42: strcpy( cpu_vendor, "Broadcom" ); break;
    case 0x43: strcpy( cpu_vendor, "Cavium" ); break;
    case 0x44: strcpy( cpu_vendor, "DEC" ); break;
    case 0x4e: strcpy( cpu_vendor, "Nvidia" ); break;
    case 0x50: strcpy( cpu_vendor, "APM" ); break;
    case 0x51: strcpy( cpu_vendor, "Qualcomm" ); break;
    case 0x53: strcpy( cpu_vendor, "Samsung" ); break;
    case 0x56: strcpy( cpu_vendor, "Marvell" ); break;
    case 0x66: strcpy( cpu_vendor, "Faraday" ); break;
    case 0x69: strcpy( cpu_vendor, "Intel" ); break;
    }
}

static ULONGLONG get_cpu_features(void)
{
    return 0;  /* FIXME */
}

static void init_xstate_features( XSTATE_CONFIGURATION *xstate )
{
    xstate->EnabledFeatures = (1 << XSTATE_LEGACY_FLOATING_POINT) | (1 << XSTATE_LEGACY_SSE) | (1 << XSTATE_AVX);
    xstate->EnabledVolatileFeatures = xstate->EnabledFeatures;
    xstate->AllFeatureSize = 0x340;

    xstate->OptimizedSave = 0;
    xstate->CompactionEnabled = 0;

    xstate->Features[0].Size = xstate->AllFeatures[0] = offsetof(XSAVE_FORMAT, XmmRegisters);
    xstate->Features[1].Size = xstate->AllFeatures[1] = sizeof(M128A) * 16;
    xstate->Features[1].Offset = xstate->Features[0].Size;
    xstate->Features[2].Offset = 0x240;
    xstate->Features[2].Size = 0x100;
    xstate->Size = 0x340;
}

void init_shared_data_cpuinfo( KUSER_SHARED_DATA *data )
{
    BOOLEAN *features = data->ProcessorFeatures;

#ifdef linux
    FILE *f = fopen("/proc/cpuinfo", "r");
    if (f)
    {
        char *s, *value, line[512];
        while (fgets( line, sizeof(line), f ))
        {
            /* NOTE: the ':' is the only character we can rely on */
            if (!(value = strchr(line,':'))) continue;
            /* terminate the valuename */
            s = value - 1;
            while ((s >= line) && (*s == ' ' || *s == '\t')) s--;
            s[1] = 0;
            value++;
            if ((s = strchr( value, '\n' ))) *s = 0;
            if (strcmp( line, "Features" )) continue;
            features[PF_ARM_VFP_32_REGISTERS_AVAILABLE]          = has_feature( value, "vfpv3" );
            features[PF_ARM_NEON_INSTRUCTIONS_AVAILABLE]         = has_feature( value, "neon" );
            features[PF_ARM_DIVIDE_INSTRUCTION_AVAILABLE]        = has_feature( value, "idivt" );
            if (native_machine == IMAGE_FILE_MACHINE_ARMNT) break;
            features[PF_ARM_V8_CRC32_INSTRUCTIONS_AVAILABLE]     = has_feature( value, "crc32" );
            features[PF_ARM_V8_CRYPTO_INSTRUCTIONS_AVAILABLE]    = has_feature( value, "aes" );
            features[PF_ARM_V81_ATOMIC_INSTRUCTIONS_AVAILABLE]   = has_feature( value, "atomics" );
            features[PF_ARM_V82_DP_INSTRUCTIONS_AVAILABLE]       = has_feature( value, "asimddp" );
            features[PF_ARM_V83_JSCVT_INSTRUCTIONS_AVAILABLE]    = has_feature( value, "jscvt" );
            features[PF_ARM_V83_LRCPC_INSTRUCTIONS_AVAILABLE]    = has_feature( value, "lrcpc" );
            features[PF_ARM_SVE_INSTRUCTIONS_AVAILABLE]          = has_feature( value, "sve" );
            features[PF_ARM_SVE2_INSTRUCTIONS_AVAILABLE]         = has_feature( value, "sve2" );
            features[PF_ARM_SVE2_1_INSTRUCTIONS_AVAILABLE]       = has_feature( value, "sve2p1" );
            features[PF_ARM_SVE_AES_INSTRUCTIONS_AVAILABLE]      = has_feature( value, "sveaes" );
            features[PF_ARM_SVE_PMULL128_INSTRUCTIONS_AVAILABLE] = has_feature( value, "svepmull" );
            features[PF_ARM_SVE_BITPERM_INSTRUCTIONS_AVAILABLE]  = has_feature( value, "svebitperm" );
            features[PF_ARM_SVE_BF16_INSTRUCTIONS_AVAILABLE]     = has_feature( value, "svebf16" );
            features[PF_ARM_SVE_EBF16_INSTRUCTIONS_AVAILABLE]    = has_feature( value, "sveebf16" );
            features[PF_ARM_SVE_B16B16_INSTRUCTIONS_AVAILABLE]   = has_feature( value, "sveb16b16" );
            features[PF_ARM_SVE_SHA3_INSTRUCTIONS_AVAILABLE]     = has_feature( value, "svesha3" );
            features[PF_ARM_SVE_SM4_INSTRUCTIONS_AVAILABLE]      = has_feature( value, "svesm4" );
            features[PF_ARM_SVE_I8MM_INSTRUCTIONS_AVAILABLE]     = has_feature( value, "svei8mm" );
            features[PF_ARM_SVE_F32MM_INSTRUCTIONS_AVAILABLE]    = has_feature( value, "svef32mm" );
            features[PF_ARM_SVE_F64MM_INSTRUCTIONS_AVAILABLE]    = has_feature( value, "svef64mm" );
            features[PF_ARM_LSE2_AVAILABLE]                      = has_feature( value, "uscat" );
            break;
        }
        fclose( f );
    }
#endif

    features[PF_FASTFAIL_AVAILABLE]      = TRUE;
    features[PF_COMPARE_EXCHANGE_DOUBLE] = TRUE;

    if (native_machine == IMAGE_FILE_MACHINE_ARMNT) return;

    features[PF_ARM_V8_INSTRUCTIONS_AVAILABLE] = TRUE;
    features[PF_NX_ENABLED]                    = TRUE;

    /* add features for other architectures supported by wow64 */
    for (unsigned int i = 0; i < supported_machines_count; i++)
    {
        switch (supported_machines[i])
        {
        case IMAGE_FILE_MACHINE_ARMNT:
            features[PF_ARM_VFP_32_REGISTERS_AVAILABLE]   = TRUE;
            features[PF_ARM_NEON_INSTRUCTIONS_AVAILABLE]  = TRUE;
            features[PF_ARM_DIVIDE_INSTRUCTION_AVAILABLE] = TRUE;
            break;
        case IMAGE_FILE_MACHINE_I386:
            features[PF_MMX_INSTRUCTIONS_AVAILABLE]    = TRUE;
            features[PF_XMMI_INSTRUCTIONS_AVAILABLE]   = TRUE;
            features[PF_RDTSC_INSTRUCTION_AVAILABLE]   = TRUE;
            features[PF_XMMI64_INSTRUCTIONS_AVAILABLE] = TRUE;
            features[PF_SSE3_INSTRUCTIONS_AVAILABLE]   = TRUE;
            features[PF_COMPARE_EXCHANGE128]           = TRUE;
            features[PF_RDTSCP_INSTRUCTION_AVAILABLE]  = TRUE;
            features[PF_SSSE3_INSTRUCTIONS_AVAILABLE]  = TRUE;
            features[PF_SSE4_1_INSTRUCTIONS_AVAILABLE] = TRUE;
            features[PF_SSE4_2_INSTRUCTIONS_AVAILABLE] = TRUE;
            break;
        }
    }

    init_xstate_features( &data->XState );
}

#endif /* End architecture specific feature detection for CPUs */

static void fill_performance_core_info(void);
static BOOL sysfs_parse_bitmap(const char *filename, ULONG_PTR *mask);

void fill_cpu_override(void)
{
    const char *env_override = getenv("WINE_CPU_TOPOLOGY");
    unsigned int host_cpu_count;
    BOOL smt = FALSE;
    unsigned int i;
    char *s;

    if (!env_override)
        return;

#ifdef _SC_NPROCESSORS_ONLN
    host_cpu_count = sysconf(_SC_NPROCESSORS_ONLN);
    if (host_cpu_count < 1)
    {
        ERR("Failed to detect the number of processors.\n");
        return;
    }
#elif defined(CTL_HW) && defined(HW_NCPU)
    int mib[2];
    size_t len = sizeof(host_cpu_count);
    mib[0] = CTL_HW;
    mib[1] = HW_NCPU;
    if (sysctl(mib, 2, &num, &len, NULL, 0) != 0)
    {
        ERR("Failed to detect the number of processors.\n");
        return;
    }
#else
    FIXME("Detecting the number of processors is not supported.\n");
    return;
#endif

    if (host_cpu_count > MAXIMUM_PROCESSORS)
    {
        FIXME( "%d CPUs reported, clamping to supported count %d.\n", host_cpu_count, MAXIMUM_PROCESSORS );
        host_cpu_count = MAXIMUM_PROCESSORS;
    }

    cpu_override.mapping.cpu_count = strtol(env_override, &s, 10);
    if (s == env_override)
        goto error;

    if (!cpu_override.mapping.cpu_count || cpu_override.mapping.cpu_count > MAXIMUM_PROCESSORS)
    {
        ERR("Invalid logical CPU count %u, limit %u.\n", cpu_override.mapping.cpu_count, MAXIMUM_PROCESSORS);
        goto error;
    }

    if (!*s)
    {
        /* Auto assign given number of logical CPUs. */
        static const char core_info[] = "/sys/devices/system/cpu/cpu%u/topology/%s";
        char name[MAX_PATH];
        unsigned int attempt, count, j;
        ULONG_PTR masks[MAXIMUM_PROCESSORS];

        if (cpu_override.mapping.cpu_count >= host_cpu_count)
        {
            TRACE( "Override cpu count %u >= host cpu count %u.\n", cpu_override.mapping.cpu_count, host_cpu_count );
            cpu_override.mapping.cpu_count = 0;
            return;
        }

        fill_performance_core_info();

        for (i = 0; i < host_cpu_count; ++i)
        {
            snprintf(name, sizeof(name), core_info, i, "thread_siblings");
            masks[i] = 0;
            sysfs_parse_bitmap(name, &masks[i]);
        }
        for (attempt = 0; attempt < 3; ++attempt)
        {
            count = 0;
            for (i = 0; i < host_cpu_count && count < cpu_override.mapping.cpu_count; ++i)
            {
                if (attempt < 2 && performance_cores_capacity)
                {
                    if (i / 32 >= performance_cores_capacity) break;
                    if (!(performance_cores[i / 32] & (1 << (i % 32)))) goto skip_cpu;
                }
                cpu_override.mapping.host_cpu_id[count] = i;
                cpu_override.siblings_mask[count] = (ULONG_PTR)1 << count;
                for (j = 0; j < count; ++j)
                {
                    if (!(masks[cpu_override.mapping.host_cpu_id[j]] & masks[i])) continue;
                    if (attempt < 1) goto skip_cpu;
                    cpu_override.siblings_mask[j] |= (ULONG_PTR)1 << count;
                    cpu_override.siblings_mask[count] |= (ULONG_PTR)1 << j;
                }
                ++count;
skip_cpu:
                ;
            }
            if (count == cpu_override.mapping.cpu_count) break;
        }
        assert( count == cpu_override.mapping.cpu_count );
        goto done;
    }

    if (tolower(*s) == 's')
    {
        cpu_override.mapping.cpu_count *= 2;
        if (cpu_override.mapping.cpu_count > MAXIMUM_PROCESSORS)
        {
            ERR("Logical CPU count exceeds limit %u.\n", MAXIMUM_PROCESSORS);
            goto error;
        }
        smt = TRUE;
        ++s;
    }
    if (*s != ':')
        goto error;
    ++s;
    for (i = 0; i < cpu_override.mapping.cpu_count; ++i)
    {
        char *next;

        if (i)
        {
            if (*s != ',')
            {
                if (!*s)
                    ERR("Incomplete host CPU mapping string, %u CPUs mapping required.\n",
                            cpu_override.mapping.cpu_count);
                goto error;
            }
            ++s;
        }

        cpu_override.mapping.host_cpu_id[i] = strtol(s, &next, 10);
        if (smt) cpu_override.siblings_mask[i] = (ULONG_PTR)3 << (i & ~1);
        else     cpu_override.siblings_mask[i] = (ULONG_PTR)1 << i;
        if (next == s)
            goto error;
        if (cpu_override.mapping.host_cpu_id[i] >= host_cpu_count)
        {
            ERR("Invalid host CPU index %u (host_cpu_count %u).\n",
                    cpu_override.mapping.host_cpu_id[i], host_cpu_count);
            goto error;
        }
        s = next;
    }
    if (*s)
        goto error;

done:
    if (ERR_ON(ntdll))
    {
        MESSAGE("wine: overriding CPU configuration, %u logical CPUs, host CPUs ", cpu_override.mapping.cpu_count);
        for (i = 0; i < cpu_override.mapping.cpu_count; ++i)
        {
            if (i)
                MESSAGE(",");
            MESSAGE("%u", cpu_override.mapping.host_cpu_id[i]);
        }
        MESSAGE(".\n");
    }
    return;
error:
    cpu_override.mapping.cpu_count = 0;
    ERR("Invalid WINE_CPU_TOPOLOGY string %s (%s).\n", debugstr_a(env_override), debugstr_a(s));
}

struct cpu_topology_override *get_cpu_topology_override(void)
{
    return cpu_override.mapping.cpu_count ? &cpu_override.mapping : NULL;
}

static BOOL grow_logical_proc_buf(void)
{
    SYSTEM_LOGICAL_PROCESSOR_INFORMATION *new_data;
    unsigned int new_len;

    if (logical_proc_info_len < logical_proc_info_alloc_len) return TRUE;

    new_len = max( logical_proc_info_alloc_len * 2, logical_proc_info_len + 1 );
    if (!(new_data = realloc( logical_proc_info, new_len * sizeof(*new_data) ))) return FALSE;
    memset( new_data + logical_proc_info_alloc_len, 0,
            (new_len - logical_proc_info_alloc_len) * sizeof(*new_data) );
    logical_proc_info = new_data;
    logical_proc_info_alloc_len = new_len;
    return TRUE;
}

static BOOL grow_logical_proc_ex_buf( unsigned int add_size )
{
    SYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX *new_dataex;
    DWORD new_len;

    if ( logical_proc_info_ex_size + add_size <= logical_proc_info_ex_alloc_size ) return TRUE;

    new_len  = max( logical_proc_info_ex_alloc_size * 2, logical_proc_info_ex_alloc_size + add_size );
    if (!(new_dataex = realloc( logical_proc_info_ex, new_len ))) return FALSE;
    memset( (char *)new_dataex + logical_proc_info_ex_alloc_size, 0, new_len - logical_proc_info_ex_alloc_size );
    logical_proc_info_ex = new_dataex;
    logical_proc_info_ex_alloc_size = new_len;
    return TRUE;
}

static DWORD log_proc_ex_size_plus( DWORD size )
{
    /* add SYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX.Relationship and .Size */
    return sizeof(LOGICAL_PROCESSOR_RELATIONSHIP) + sizeof(DWORD) + size;
}

static DWORD count_bits( ULONG_PTR mask )
{
    DWORD count = 0;
    while (mask > 0)
    {
        if (mask & 1) ++count;
        mask >>= 1;
    }
    return count;
}

static BOOL logical_proc_info_ex_add_by_id( LOGICAL_PROCESSOR_RELATIONSHIP rel, DWORD id, ULONG_PTR mask )
{
    SYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX *dataex;
    unsigned int phys_cpu_id;
    unsigned int ofs = 0;

    while (ofs < logical_proc_info_ex_size)
    {
        dataex = (SYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX *)((char *)logical_proc_info_ex + ofs);
        if (rel == RelationProcessorPackage && dataex->Relationship == rel && dataex->Processor.Reserved[1] == id)
        {
            dataex->Processor.GroupMask[0].Mask |= mask;
            return TRUE;
        }
        else if (rel == RelationProcessorCore && dataex->Relationship == rel && dataex->Processor.Reserved[1] == id)
        {
            return TRUE;
        }
        ofs += dataex->Size;
    }

    /* TODO: For now, just one group. If more than 64 processors, then we
     * need another group. */
    if (!grow_logical_proc_ex_buf( log_proc_ex_size_plus( sizeof(PROCESSOR_RELATIONSHIP) ))) return FALSE;

    dataex = (SYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX *)((char *)logical_proc_info_ex + ofs);

    dataex->Relationship = rel;
    dataex->Size = log_proc_ex_size_plus( sizeof(PROCESSOR_RELATIONSHIP) );
    if (rel == RelationProcessorCore)
        dataex->Processor.Flags = count_bits( mask ) > 1 ? LTP_PC_SMT : 0;
    else
        dataex->Processor.Flags = 0;

    phys_cpu_id = cpu_override.mapping.cpu_count ? cpu_override.mapping.host_cpu_id[id] : id;
    if (rel == RelationProcessorCore && phys_cpu_id / 32 < performance_cores_capacity)
        dataex->Processor.EfficiencyClass = (performance_cores[phys_cpu_id / 32] >> (phys_cpu_id % 32)) & 1;
    else
        dataex->Processor.EfficiencyClass = 0;
    dataex->Processor.GroupCount = 1;
    dataex->Processor.GroupMask[0].Mask = mask;
    dataex->Processor.GroupMask[0].Group = 0;
    /* mark for future lookup */
    dataex->Processor.Reserved[0] = 0;
    dataex->Processor.Reserved[1] = id;

    logical_proc_info_ex_size += dataex->Size;
    return TRUE;
}

/* Store package and core information for a logical processor. Parsing of processor
 * data may happen in multiple passes; the 'id' parameter is then used to locate
 * previously stored data. The type of data stored in 'id' depends on 'rel':
 * - RelationProcessorPackage: package id ('CPU socket').
 * - RelationProcessorCore: physical core number.
 */
static BOOL logical_proc_info_add_by_id( LOGICAL_PROCESSOR_RELATIONSHIP rel, DWORD id, ULONG_PTR mask )
{
    unsigned int i;

    for (i = 0; i < logical_proc_info_len; i++)
    {
        if (rel == RelationProcessorPackage && logical_proc_info[i].Relationship == rel
            && logical_proc_info[i].Reserved[1] == id)
        {
            logical_proc_info[i].ProcessorMask |= mask;
            return logical_proc_info_ex_add_by_id( rel, id, mask );
        }
        else if (rel == RelationProcessorCore && logical_proc_info[i].Relationship == rel
                 && logical_proc_info[i].Reserved[1] == id)
            return logical_proc_info_ex_add_by_id( rel, id, mask );
    }

    if (!grow_logical_proc_buf()) return FALSE;

    logical_proc_info[i].Relationship = rel;
    logical_proc_info[i].ProcessorMask = mask;
    if (rel == RelationProcessorCore)
        logical_proc_info[i].ProcessorCore.Flags = count_bits( mask ) > 1 ? LTP_PC_SMT : 0;
    logical_proc_info[i].Reserved[0] = 0;
    logical_proc_info[i].Reserved[1] = id;
    logical_proc_info_len = i + 1;

    return logical_proc_info_ex_add_by_id( rel, id, mask );
}

static BOOL logical_proc_info_add_cache( ULONG_PTR mask, CACHE_DESCRIPTOR *cache )
{
    SYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX *dataex;
    unsigned int ofs = 0, i;

    for (i = 0; i < logical_proc_info_len; i++)
    {
        if (logical_proc_info[i].Relationship==RelationCache && logical_proc_info[i].ProcessorMask==mask
            && logical_proc_info[i].Cache.Level==cache->Level && logical_proc_info[i].Cache.Type==cache->Type)
            return TRUE;
    }

    if (!grow_logical_proc_buf()) return FALSE;

    logical_proc_info[i].Relationship = RelationCache;
    logical_proc_info[i].ProcessorMask = mask;
    logical_proc_info[i].Cache = *cache;
    logical_proc_info_len = i + 1;

    for (ofs = 0; ofs < logical_proc_info_ex_size; )
    {
        dataex = (SYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX *)((char *)logical_proc_info_ex + ofs);
        if (dataex->Relationship == RelationCache && dataex->Cache.GroupMask.Mask == mask
            && dataex->Cache.Level == cache->Level && dataex->Cache.Type == cache->Type)
            return TRUE;
        ofs += dataex->Size;
    }

    if (!grow_logical_proc_ex_buf( log_proc_ex_size_plus( sizeof(CACHE_RELATIONSHIP) ))) return FALSE;

    dataex = (SYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX *)((char *)logical_proc_info_ex + ofs);

    dataex->Relationship = RelationCache;
    dataex->Size = log_proc_ex_size_plus( sizeof(CACHE_RELATIONSHIP) );
    dataex->Cache.Level = cache->Level;
    dataex->Cache.Associativity = cache->Associativity;
    dataex->Cache.LineSize = cache->LineSize;
    dataex->Cache.CacheSize = cache->Size;
    dataex->Cache.Type = cache->Type;
    dataex->Cache.GroupMask.Mask = mask;
    dataex->Cache.GroupMask.Group = 0;

    logical_proc_info_ex_size += dataex->Size;

    return TRUE;
}

static BOOL logical_proc_info_add_numa_node( ULONG_PTR mask, DWORD node_id )
{
    SYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX *dataex;

    if (!grow_logical_proc_buf()) return FALSE;

    logical_proc_info[logical_proc_info_len].Relationship = RelationNumaNode;
    logical_proc_info[logical_proc_info_len].ProcessorMask = mask;
    logical_proc_info[logical_proc_info_len].NumaNode.NodeNumber = node_id;
    ++logical_proc_info_len;

    if (!grow_logical_proc_ex_buf( log_proc_ex_size_plus( sizeof(NUMA_NODE_RELATIONSHIP) ))) return FALSE;

    dataex = (SYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX *)((char *)logical_proc_info_ex + logical_proc_info_ex_size);

    dataex->Relationship = RelationNumaNode;
    dataex->Size = log_proc_ex_size_plus( sizeof(NUMA_NODE_RELATIONSHIP) );
    dataex->NumaNode.NodeNumber = node_id;
    dataex->NumaNode.GroupMask.Mask = mask;
    dataex->NumaNode.GroupMask.Group = 0;

    logical_proc_info_ex_size += dataex->Size;

    return TRUE;
}

static BOOL logical_proc_info_add_group( DWORD num_cpus, ULONG_PTR mask )
{
    SYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX *dataex;

    if (!grow_logical_proc_ex_buf( log_proc_ex_size_plus( sizeof(GROUP_RELATIONSHIP) ))) return FALSE;

    dataex = (SYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX *)(((char *)logical_proc_info_ex) + logical_proc_info_ex_size);

    dataex->Relationship = RelationGroup;
    dataex->Size = log_proc_ex_size_plus( sizeof(GROUP_RELATIONSHIP) );
    dataex->Group.MaximumGroupCount = 1;
    dataex->Group.ActiveGroupCount = 1;
    dataex->Group.GroupInfo[0].MaximumProcessorCount = num_cpus;
    dataex->Group.GroupInfo[0].ActiveProcessorCount = num_cpus;
    dataex->Group.GroupInfo[0].ActiveProcessorMask = mask;

    logical_proc_info_ex_size += dataex->Size;
    system_cpu_mask |= mask;
    return TRUE;
}

#ifdef linux

/* Helper function for counting bitmap values as commonly used by the Linux kernel
 * for storing CPU masks in sysfs. The format is comma separated lists of hex values
 * each max 32-bit e.g. "00ff" or even "00,00000000,0000ffff".
 *
 * Example files include:
 * - /sys/devices/system/cpu/cpu0/cache/index0/shared_cpu_map
 * - /sys/devices/system/cpu/cpu0/topology/thread_siblings
 */
static BOOL sysfs_parse_bitmap(const char *filename, ULONG_PTR *mask)
{
    FILE *f;
    unsigned int r;

    f = fopen(filename, "r");
    if (!f) return FALSE;

    while (!feof(f))
    {
        char op;
        if (!fscanf(f, "%x%c ", &r, &op)) break;
        *mask = (sizeof(ULONG_PTR)>sizeof(int) ? *mask << (8 * sizeof(DWORD)) : 0) + r;
    }
    fclose( f );
    return TRUE;
}

/* Helper function for counting number of elements in interval lists as used by
 * the Linux kernel. The format is comma separated list of intervals of which
 * each interval has the format of "begin-end" where begin and end are decimal
 * numbers. E.g. "0-7", "0-7,16-23"
 *
 * Example files include:
 * - /sys/devices/system/cpu/online
 * - /sys/devices/system/cpu/cpu0/cache/index0/shared_cpu_list
 * - /sys/devices/system/cpu/cpu0/topology/thread_siblings_list.
 */
static BOOL sysfs_count_list_elements(const char *filename, unsigned int *result)
{
    FILE *f;

    f = fopen(filename, "r");
    if (!f) return FALSE;

    while (!feof(f))
    {
        char op;
        unsigned int beg, end;

        if (!fscanf(f, "%u%c ", &beg, &op)) break;
        if(op == '-')
            fscanf(f, "%u%c ", &end, &op);
        else
            end = beg;

        *result += end - beg + 1;
    }
    fclose( f );
    return TRUE;
}

static void fill_performance_core_info(void)
{
    FILE *fpcore_list;
    unsigned int beg, end, i;
    char op = ',';
    ULONG *p;

    if (performance_cores_capacity) return;

    fpcore_list = fopen("/sys/devices/cpu_core/cpus", "r");
    if (!fpcore_list) return;

    performance_cores = calloc(16, sizeof(ULONG));
    if (!performance_cores) goto done;
    performance_cores_capacity = 16;

    while (!feof(fpcore_list) && op == ',')
    {
        if (!fscanf(fpcore_list, "%u %c ", &beg, &op)) break;
        if (op == '-') fscanf(fpcore_list, "%u %c ", &end, &op);
        else end = beg;

        for(i = beg; i <= end; i++)
        {
            if (i / 32 >= performance_cores_capacity)
            {
                p = realloc(performance_cores, performance_cores_capacity * 2 * sizeof(ULONG));
                if (!p) goto done;
                memset(p + performance_cores_capacity, 0, performance_cores_capacity * sizeof(ULONG));
                performance_cores = p;
                performance_cores_capacity *= 2;
            }
            performance_cores[i / 32] |= 1 << (i % 32);
        }
    }
done:
    fclose(fpcore_list);
}

/* for 'data', max_len is the array count. for 'dataex', max_len is in bytes */
static NTSTATUS create_logical_proc_info(void)
{
    static const char core_info[] = "/sys/devices/system/cpu/cpu%u/topology/%s";
    static const char cache_info[] = "/sys/devices/system/cpu/cpu%u/cache/index%u/%s";
    static const char numa_info[] = "/sys/devices/system/node/node%u/cpumap";
    const char *env_fake_logical_cores = getenv("WINE_LOGICAL_CPUS_AS_CORES");
    BOOL fake_logical_cpus_as_cores = env_fake_logical_cores && atoi(env_fake_logical_cores);
    FILE *fcpu_list, *fnuma_list, *f;
    unsigned int beg, end, i, j, r, num_cpus = 0, max_cpus = 0;
    char op, name[MAX_PATH];
    ULONG_PTR all_cpus_mask = 0;
    unsigned int cpu_id;

    /* On systems with a large number of CPU cores (32 or 64 depending on 32-bit or 64-bit),
     * we have issues parsing processor information:
     * - ULONG_PTR masks as used in data structures can't hold all cores. Requires splitting
     *   data appropriately into "processor groups". We are hard coding 1.
     * - Thread affinity code in wineserver and our CPU parsing code here work independently.
     *   So far the Windows mask applied directly to Linux, but process groups break that.
     *   (NUMA systems you may have multiple non-full groups.)
     */
    if(sysfs_count_list_elements("/sys/devices/system/cpu/present", &max_cpus) && max_cpus > MAXIMUM_PROCESSORS)
    {
        FIXME("Improve CPU info reporting: system supports %u logical cores, but only %u supported!\n",
                max_cpus, MAXIMUM_PROCESSORS);
    }

    fill_performance_core_info();

    fcpu_list = fopen("/sys/devices/system/cpu/online", "r");
    if (!fcpu_list) return STATUS_NOT_IMPLEMENTED;

    while (!feof(fcpu_list))
    {
        if (!fscanf(fcpu_list, "%u%c ", &beg, &op)) break;
        if (op == '-') fscanf(fcpu_list, "%u%c ", &end, &op);
        else end = beg;

        if (cpu_override.mapping.cpu_count)
        {
            beg = 0;
            end = cpu_override.mapping.cpu_count - 1;
        }

        for(i = beg; i <= end; i++)
        {
            unsigned int phys_core = 0;
            ULONG_PTR thread_mask = 0;

            if (i > 8 * sizeof(ULONG_PTR)) break;

            snprintf(name, sizeof(name), core_info, cpu_override.mapping.cpu_count ? cpu_override.mapping.host_cpu_id[i] : i, "physical_package_id");
            f = fopen(name, "r");
            if (f)
            {
                fscanf(f, "%u", &r);
                fclose(f);
            }
            else r = 0;
            if (!logical_proc_info_add_by_id( RelationProcessorPackage, r, (ULONG_PTR)1 << i ))
            {
                fclose(fcpu_list);
                return STATUS_NO_MEMORY;
            }

            /* Sysfs enumerates logical cores (and not physical cores), but Windows enumerates
             * by physical core. Upon enumerating a logical core in sysfs, we register a physical
             * core and all its logical cores. In order to not report physical cores multiple
             * times, we pass a unique physical core ID to logical_proc_info_add_by_id and let
             * that call figure out any duplication.
             * Obtain a unique physical core ID from the first element of thread_siblings_list.
             * This list provides logical cores sharing the same physical core. The IDs are based
             * on kernel cpu core numbering as opposed to a hardware core ID like provided through
             * 'core_id', so are suitable as a unique ID.
             */

            /* Mask of logical threads sharing same physical core in kernel core numbering. */
            snprintf(name, sizeof(name), core_info, i, "thread_siblings");
            if (cpu_override.mapping.cpu_count)
            {
                thread_mask = cpu_override.siblings_mask[i];
            }
            else
            {
                if(fake_logical_cpus_as_cores || !sysfs_parse_bitmap(name, &thread_mask)) thread_mask = (ULONG_PTR)1<<i;
            }
            /* Needed later for NumaNode and Group. */
            all_cpus_mask |= thread_mask;

            if (cpu_override.mapping.cpu_count)
            {
                assert( thread_mask );
                for (phys_core = 0; ; ++phys_core)
                    if (thread_mask & ((ULONG_PTR)1 << phys_core)) break;
            }
            else
            {
                snprintf(name, sizeof(name), core_info, i, "thread_siblings_list");
                f = fake_logical_cpus_as_cores ? NULL : fopen(name, "r");
                if (f)
                {
                    fscanf(f, "%d%c", &phys_core, &op);
                    fclose(f);
                }
                else phys_core = i;
            }

            if (!logical_proc_info_add_by_id( RelationProcessorCore, phys_core, thread_mask ))
            {
                fclose(fcpu_list);
                return STATUS_NO_MEMORY;
            }

            cpu_id = cpu_override.mapping.cpu_count ? cpu_override.mapping.host_cpu_id[i] : i;

            for(j = 0; j < 4; j++)
            {
                CACHE_DESCRIPTOR cache = { .Associativity = 8, .LineSize = 64, .Type = CacheUnified, .Size = 64 * 1024 };
                ULONG_PTR mask = 0;

                snprintf(name, sizeof(name), cache_info, cpu_id, j, "shared_cpu_map");
                if(!sysfs_parse_bitmap(name, &mask)) continue;

                snprintf(name, sizeof(name), cache_info, cpu_id, j, "level");
                f = fopen(name, "r");
                if(!f) continue;
                fscanf(f, "%u", &r);
                fclose(f);
                cache.Level = r;

                snprintf(name, sizeof(name), cache_info, cpu_id, j, "ways_of_associativity");
                if ((f = fopen(name, "r")))
                {
                    fscanf(f, "%u", &r);
                    fclose(f);
                    cache.Associativity = r;
                }

                snprintf(name, sizeof(name), cache_info, cpu_id, j, "coherency_line_size");
                if ((f = fopen(name, "r")))
                {
                    fscanf(f, "%u", &r);
                    fclose(f);
                    cache.LineSize = r;
                }

                snprintf(name, sizeof(name), cache_info, cpu_id, j, "size");
                if ((f = fopen(name, "r")))
                {
                    fscanf(f, "%u%c", &r, &op);
                    fclose(f);
                    if(op != 'K')
                        WARN("unknown cache size %u%c\n", r, op);
                    cache.Size = (op=='K' ? r*1024 : r);
                }

                snprintf(name, sizeof(name), cache_info, cpu_id, j, "type");
                if ((f = fopen(name, "r")))
                {
                    fscanf(f, "%s", name);
                    fclose(f);
                    if (!memcmp(name, "Data", 5))
                        cache.Type = CacheData;
                    else if(!memcmp(name, "Instruction", 11))
                        cache.Type = CacheInstruction;
                    else
                        cache.Type = CacheUnified;
                }

                if (cpu_override.mapping.cpu_count)
                {
                    ULONG_PTR host_mask = mask;
                    unsigned int id;

                    mask = 0;
                    for (id = 0; id < cpu_override.mapping.cpu_count; ++id)
                        if (host_mask & ((ULONG_PTR)1 << cpu_override.mapping.host_cpu_id[id]))
                            mask |= (ULONG_PTR)1 << id;

                    assert(mask);
                }

                if (!logical_proc_info_add_cache( mask, &cache ))
                {
                    fclose(fcpu_list);
                    return STATUS_NO_MEMORY;
                }
            }
        }

        if (cpu_override.mapping.cpu_count)
            break;
    }
    fclose(fcpu_list);

    num_cpus = count_bits(all_cpus_mask);

    fnuma_list = fopen("/sys/devices/system/node/online", "r");
    if (!fnuma_list)
    {
        if (!logical_proc_info_add_numa_node( all_cpus_mask, 0 ))
            return STATUS_NO_MEMORY;
    }
    else
    {
        while (!feof(fnuma_list))
        {
            if (!fscanf(fnuma_list, "%u%c ", &beg, &op))
                break;
            if (op == '-') fscanf(fnuma_list, "%u%c ", &end, &op);
            else end = beg;

            for (i = beg; i <= end; i++)
            {
                ULONG_PTR mask = 0;

                snprintf(name, sizeof(name), numa_info, i);
                if (!sysfs_parse_bitmap( name, &mask )) continue;

                if (!logical_proc_info_add_numa_node( mask, i ))
                {
                    fclose(fnuma_list);
                    return STATUS_NO_MEMORY;
                }
            }
        }
        fclose(fnuma_list);
    }

    logical_proc_info_add_group( num_cpus, all_cpus_mask );

    performance_cores_capacity = 0;
    free(performance_cores);
    performance_cores = NULL;

    return STATUS_SUCCESS;
}

#elif defined(__APPLE__)

/* for 'data', max_len is the array count. for 'dataex', max_len is in bytes */
static NTSTATUS create_logical_proc_info(void)
{
    unsigned int pkgs_no, cores_no, lcpu_no, lcpu_per_core, cores_per_package, assoc;
    unsigned int cache_ctrs[10] = {0};
    ULONG_PTR all_cpus_mask = 0;
    CACHE_DESCRIPTOR cache[10];
    LONGLONG cache_size, cache_line_size, cache_sharing[10];
    size_t size;
    unsigned int p, i, j, k;

    lcpu_no = peb->NumberOfProcessors;

    size = sizeof(pkgs_no);
    if (sysctlbyname("hw.packages", &pkgs_no, &size, NULL, 0))
        pkgs_no = 1;

    size = sizeof(cores_no);
    if (sysctlbyname("hw.physicalcpu", &cores_no, &size, NULL, 0))
        cores_no = lcpu_no;

    TRACE("%u logical CPUs from %u physical cores across %u packages\n",
            lcpu_no, cores_no, pkgs_no);

    lcpu_per_core = lcpu_no / cores_no;
    cores_per_package = cores_no / pkgs_no;

    memset(cache, 0, sizeof(cache));
    cache[1].Level = 1;
    cache[1].Type = CacheInstruction;
    cache[1].Associativity = 8; /* reasonable default */
    cache[1].LineSize = 0x40; /* reasonable default */
    cache[2].Level = 1;
    cache[2].Type = CacheData;
    cache[2].Associativity = 8;
    cache[2].LineSize = 0x40;
    cache[3].Level = 2;
    cache[3].Type = CacheUnified;
    cache[3].Associativity = 8;
    cache[3].LineSize = 0x40;
    cache[4].Level = 3;
    cache[4].Type = CacheUnified;
    cache[4].Associativity = 12;
    cache[4].LineSize = 0x40;

    size = sizeof(cache_line_size);
    if (!sysctlbyname("hw.cachelinesize", &cache_line_size, &size, NULL, 0))
    {
        for (i = 1; i < 5; i++) cache[i].LineSize = cache_line_size;
    }

    /* TODO: set actual associativity for all caches */
    size = sizeof(assoc);
    if (!sysctlbyname("machdep.cpu.cache.L2_associativity", &assoc, &size, NULL, 0))
        cache[3].Associativity = assoc;

    size = sizeof(cache_size);
    if (!sysctlbyname("hw.l1icachesize", &cache_size, &size, NULL, 0))
        cache[1].Size = cache_size;
    size = sizeof(cache_size);
    if (!sysctlbyname("hw.l1dcachesize", &cache_size, &size, NULL, 0))
        cache[2].Size = cache_size;
    size = sizeof(cache_size);
    if (!sysctlbyname("hw.l2cachesize", &cache_size, &size, NULL, 0))
        cache[3].Size = cache_size;
    size = sizeof(cache_size);
    if (!sysctlbyname("hw.l3cachesize", &cache_size, &size, NULL, 0))
        cache[4].Size = cache_size;

    size = sizeof(cache_sharing);
    if (sysctlbyname("hw.cacheconfig", cache_sharing, &size, NULL, 0) < 0)
    {
        cache_sharing[1] = lcpu_per_core;
        cache_sharing[2] = lcpu_per_core;
        cache_sharing[3] = lcpu_per_core;
        cache_sharing[4] = lcpu_no;
    }
    else
    {
        /* in cache[], indexes 1 and 2 are l1 caches */
        cache_sharing[4] = cache_sharing[3];
        cache_sharing[3] = cache_sharing[2];
        cache_sharing[2] = cache_sharing[1];
    }

    for(p = 0; p < pkgs_no; ++p)
    {
        for(j = 0; j < cores_per_package && p * cores_per_package + j < cores_no; ++j)
        {
            ULONG_PTR mask = 0;
            DWORD phys_core;

            for(k = 0; k < lcpu_per_core; ++k) mask |= (ULONG_PTR)1 << (j * lcpu_per_core + k);

            all_cpus_mask |= mask;

            /* add to package */
            if(!logical_proc_info_add_by_id( RelationProcessorPackage, p, mask ))
                return STATUS_NO_MEMORY;

            /* add new core */
            phys_core = p * cores_per_package + j;
            if(!logical_proc_info_add_by_id( RelationProcessorCore, phys_core, mask ))
                return STATUS_NO_MEMORY;

            for(i = 1; i < 5; ++i)
            {
                if(cache_ctrs[i] == 0 && cache[i].Size > 0)
                {
                    mask = 0;
                    for(k = 0; k < cache_sharing[i]; ++k)
                        mask |= (ULONG_PTR)1 << (j * lcpu_per_core + k);

                    if (!logical_proc_info_add_cache( mask, &cache[i] ))
                        return STATUS_NO_MEMORY;
                }

                cache_ctrs[i] += lcpu_per_core;
                if(cache_ctrs[i] == cache_sharing[i]) cache_ctrs[i] = 0;
            }
        }
    }

    /* OSX doesn't support NUMA, so just make one NUMA node for all CPUs */
    if(!logical_proc_info_add_numa_node( all_cpus_mask, 0 ))
        return STATUS_NO_MEMORY;

    logical_proc_info_add_group( lcpu_no, all_cpus_mask );

    return STATUS_SUCCESS;
}

#elif defined(HAVE_LIBHWLOC)

static NTSTATUS add_hwloc_cache(hwloc_obj_t obj, int level)
{
    CACHE_DESCRIPTOR cache;

    memset(&cache, 0, sizeof(cache));
    cache.Level = level;
    if (obj->attr)
    {
        cache.Associativity = obj->attr->cache.associativity;
        cache.LineSize = obj->attr->cache.linesize;
        cache.Size = obj->attr->cache.size;
        switch (obj->attr->cache.type)
        {
        case HWLOC_OBJ_CACHE_UNIFIED:
            cache.Type = CacheUnified;
            break;
        case HWLOC_OBJ_CACHE_DATA:
            cache.Type = CacheData;
            break;
        case HWLOC_OBJ_CACHE_INSTRUCTION:
            cache.Type = CacheInstruction;
            break;
        default:
            break;
        }
    }
    if (!logical_proc_info_add_cache(hwloc_bitmap_to_ulong(obj->cpuset), &cache))
        return STATUS_NO_MEMORY;
    return STATUS_SUCCESS;
}

static NTSTATUS add_hwloc_numa_nodes(hwloc_topology_t topology)
{
    hwloc_obj_t obj;

    for (obj = hwloc_get_obj_by_type(topology, HWLOC_OBJ_NUMANODE, 0); obj != NULL; obj = obj->next_cousin)
    {
        if (!logical_proc_info_add_numa_node(obj->logical_index, hwloc_bitmap_to_ulong(obj->cpuset)))
            return STATUS_NO_MEMORY;
    }
    return STATUS_SUCCESS;
}

static NTSTATUS traverse_hwloc_topology(hwloc_obj_t obj)
{
    int i;
    NTSTATUS nt_status = STATUS_SUCCESS;

    switch (obj->type)
    {
    case HWLOC_OBJ_PACKAGE:
        if (!logical_proc_info_add_by_id(RelationProcessorPackage, obj->logical_index, hwloc_bitmap_to_ulong(obj->cpuset)))
            return STATUS_NO_MEMORY;
        break;
    case HWLOC_OBJ_CORE:
        if (!logical_proc_info_add_by_id(RelationProcessorCore, obj->logical_index, hwloc_bitmap_to_ulong(obj->cpuset)))
            return STATUS_NO_MEMORY;
        break;
    case HWLOC_OBJ_L1CACHE:
    case HWLOC_OBJ_L1ICACHE:
        nt_status = add_hwloc_cache(obj, 1);
        break;
    case HWLOC_OBJ_L2CACHE:
    case HWLOC_OBJ_L2ICACHE:
        nt_status = add_hwloc_cache(obj, 2);
        break;
    case HWLOC_OBJ_L3CACHE:
    case HWLOC_OBJ_L3ICACHE:
        nt_status = add_hwloc_cache(obj, 3);
        break;
    case HWLOC_OBJ_L4CACHE:
        nt_status = add_hwloc_cache(obj, 4);
        break;
    case HWLOC_OBJ_L5CACHE:
        nt_status = add_hwloc_cache(obj, 5);
        break;
    default:
        break;
    }

    for (i = 0; i < obj->arity && nt_status == STATUS_SUCCESS; i++)
        nt_status = traverse_hwloc_topology(obj->children[i]);
    return nt_status;
}

static NTSTATUS create_logical_proc_info(void)
{
    NTSTATUS nt_status = STATUS_SUCCESS;
    int ret;
    hwloc_topology_t topology;
    hwloc_obj_t root_obj;

    ret = hwloc_topology_init(&topology);
    if (ret != 0)
        return STATUS_NO_MEMORY;

    hwloc_topology_set_icache_types_filter(topology, HWLOC_TYPE_FILTER_KEEP_ALL);
    ret = hwloc_topology_load(topology);
    if (ret != 0)
    {
        nt_status = STATUS_NO_MEMORY;
        goto end;
    }

    root_obj = hwloc_get_root_obj(topology);
    if (root_obj == NULL)
    {
        nt_status = STATUS_NO_MEMORY;
        goto end;
    }

    nt_status = traverse_hwloc_topology(root_obj);
    if (nt_status != STATUS_SUCCESS)
        goto end;

    nt_status = add_hwloc_numa_nodes(topology);
    if (nt_status != STATUS_SUCCESS)
        goto end;

    if (!logical_proc_info_add_group(hwloc_get_nbobjs_by_type(topology, HWLOC_OBJ_PU), hwloc_bitmap_to_ulong(root_obj->cpuset)))
    {
        nt_status = STATUS_NO_MEMORY;
        goto end;
    }

end:
    hwloc_topology_destroy(topology);
    return nt_status;
}

#else

static NTSTATUS create_logical_proc_info(void)
{
    FIXME("stub\n");
    return STATUS_NOT_IMPLEMENTED;
}
#endif

#ifdef linux

static double tsc_from_jiffies[MAXIMUM_PROCESSORS];

static void init_tsc_frequency(void)
{
    unsigned long clk_tck = sysconf( _SC_CLK_TCK );
    char filename[128];
    unsigned long val;
    unsigned int i;
    FILE *f;

    for (i = 0; i < MAXIMUM_PROCESSORS; ++i)
    {
        if (system_cpu_mask && !(system_cpu_mask & ((ULONG_PTR)1 << i))) continue;
        snprintf( filename, sizeof(filename), "/sys/devices/system/cpu/cpu%d/cpufreq/base_frequency", i );
        if (!(f = fopen( filename, "r" ))) break;
        if (fscanf( f, "%lu", &val ) == 1) tsc_from_jiffies[i] = 1000.0 * val / clk_tck;
        fclose( f );
    }
}
#else

static void init_tsc_frequency(void)
{
}

#endif

static pthread_once_t logical_proc_init_once = PTHREAD_ONCE_INIT;

static void init_logical_proc_info(void)
{
    NTSTATUS status;

    if ((status = create_logical_proc_info()))
    {
        FIXME( "Failed to get logical processor information, status %#x.\n", status );
        free( logical_proc_info );
        logical_proc_info = NULL;
        logical_proc_info_len = 0;

        free( logical_proc_info_ex );
        logical_proc_info_ex = NULL;
        logical_proc_info_ex_size = 0;
    }
    else
    {
        logical_proc_info = realloc( logical_proc_info, logical_proc_info_len * sizeof(*logical_proc_info) );
        logical_proc_info_alloc_len = logical_proc_info_len;
        logical_proc_info_ex = realloc( logical_proc_info_ex, logical_proc_info_ex_size );
        logical_proc_info_ex_alloc_size = logical_proc_info_ex_size;
    }
    init_tsc_frequency();
}

static void read_dev_urandom( void *buf, ULONG len )
{
    int fd = open( "/dev/urandom", O_RDONLY );
    if (fd != -1)
    {
        int ret;
        do
        {
            ret = read( fd, buf, len );
        }
        while (ret == -1 && errno == EINTR);
        close( fd );
    }
    else WARN( "can't open /dev/urandom\n" );
}

void get_random( void *buf, ULONG len )
{
#ifdef HAVE_GETRANDOM
    int ret;
    do
    {
        ret = getrandom( buf, len, 0 );
    }
    while (ret == -1 && errno == EINTR);

    if (ret == -1 && errno == ENOSYS) read_dev_urandom( buf, len );
#else
    read_dev_urandom( buf, len );
#endif
}

/******************************************************************
 *		init_cpu_info
 *
 * Init a couple of places with CPU related information.
 */
void init_cpu_info(void)
{
    long num;

#ifdef _SC_NPROCESSORS_ONLN
    num = sysconf(_SC_NPROCESSORS_ONLN);
    if (num < 1)
    {
        num = 1;
        WARN("Failed to detect the number of processors.\n");
    }
#elif defined(CTL_HW) && defined(HW_NCPU)
    int mib[2];
    size_t len = sizeof(num);
    mib[0] = CTL_HW;
    mib[1] = HW_NCPU;
    if (sysctl(mib, 2, &num, &len, NULL, 0) != 0)
    {
        num = 1;
        WARN("Failed to detect the number of processors.\n");
    }
#else
    num = 1;
    FIXME("Detecting the number of processors is not supported.\n");
#endif

    peb->NumberOfProcessors = cpu_override.mapping.cpu_count
            ? cpu_override.mapping.cpu_count : num;
    init_cpu_model();
    get_random( &process_cookie, sizeof(process_cookie) );
}

static SYSTEM_CPU_INFORMATION get_cpuinfo(void)
{
    SYSTEM_CPU_INFORMATION info =
    {
        .ProcessorLevel        = cpu_level,
        .ProcessorRevision     = cpu_revision,
        .MaximumProcessors     = peb->NumberOfProcessors,
        .ProcessorFeatureBits  = get_cpu_features(),
#ifdef __arm__
        .ProcessorArchitecture = PROCESSOR_ARCHITECTURE_ARM,
#elif defined __aarch64__
        .ProcessorArchitecture = PROCESSOR_ARCHITECTURE_ARM64,
#elif defined(__i386__)
        .ProcessorArchitecture = PROCESSOR_ARCHITECTURE_INTEL,
#elif defined(__x86_64__)
        .ProcessorArchitecture = PROCESSOR_ARCHITECTURE_AMD64,
#endif
    };

    TRACE( "CPU arch %d, level %d, rev %d, features 0x%x\n",
           info.ProcessorArchitecture, info.ProcessorLevel,
           info.ProcessorRevision, info.ProcessorFeatureBits );
    return info;
}

static NTSTATUS create_cpuset_info(SYSTEM_CPU_SET_INFORMATION *info)
{
    const SYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX *proc_info;
    const DWORD cpu_info_size = logical_proc_info_ex_size;
    BYTE core_index, cache_index, max_cache_level;
    unsigned int i, j, count;
    ULONG64 cpu_mask;

    if (!logical_proc_info_ex) return STATUS_NOT_IMPLEMENTED;

    count = peb->NumberOfProcessors;

    max_cache_level = 0;
    proc_info = logical_proc_info_ex;
    for (i = 0; (char *)proc_info != (char *)logical_proc_info_ex + cpu_info_size; ++i)
    {
        if (proc_info->Relationship == RelationCache)
        {
            if (max_cache_level < proc_info->Cache.Level)
                max_cache_level = proc_info->Cache.Level;
        }
        proc_info = (SYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX *)((BYTE *)proc_info + proc_info->Size);
    }

    memset(info, 0, count * sizeof(*info));

    core_index = 0;
    cache_index = 0;
    proc_info = (SYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX *)logical_proc_info_ex;
    for (i = 0; i < count; ++i)
    {
        info[i].Size = sizeof(*info);
        info[i].Type = CpuSetInformation;
        info[i].CpuSet.Id = 0x100 + i;
        info[i].CpuSet.LogicalProcessorIndex = i;
    }

    for (i = 0; (char *)proc_info != (char *)logical_proc_info_ex + cpu_info_size; ++i)
    {
        if (proc_info->Relationship == RelationProcessorCore)
        {
            if (proc_info->Processor.GroupCount != 1)
            {
                FIXME("Unsupported group count %u.\n", proc_info->Processor.GroupCount);
                continue;
            }
            cpu_mask = proc_info->Processor.GroupMask[0].Mask;
            for (j = 0; j < count; ++j)
                if (((ULONG64)1 << j) & cpu_mask)
                {
                    info[j].CpuSet.CoreIndex = core_index;
                    info[j].CpuSet.EfficiencyClass = proc_info->Processor.EfficiencyClass;
                }
            ++core_index;
        }
        else if (proc_info->Relationship == RelationCache)
        {
            if (proc_info->Cache.Level == max_cache_level)
            {
                cpu_mask = proc_info->Cache.GroupMask.Mask;
                for (j = 0; j < count; ++j)
                    if (((ULONG64)1 << j) & cpu_mask)
                        info[j].CpuSet.LastLevelCacheIndex = cache_index;
            }
            ++cache_index;
        }
        else if (proc_info->Relationship == RelationNumaNode)
        {
            cpu_mask = proc_info->NumaNode.GroupMask.Mask;
            for (j = 0; j < count; ++j)
                if (((ULONG64)1 << j) & cpu_mask)
                    info[j].CpuSet.NumaNodeIndex = proc_info->NumaNode.NodeNumber;
        }
        proc_info = (SYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX *)((char *)proc_info + proc_info->Size);
    }

    return STATUS_SUCCESS;
}

struct smbios_buffer
{
    struct smbios_prologue *prologue;  /* prologue followed by data */
    unsigned int            size;      /* allocated size past prologue */
    WORD                    handle;    /* handle count */
};

static WORD append_smbios( struct smbios_buffer *buf, struct smbios_header *hdr,
                           const char *strings[], unsigned int strings_count )
{
    struct smbios_prologue *prologue = buf->prologue;
    unsigned int i, len = hdr->length;
    char *pos;

    for (i = 0; i < strings_count; i++) len += strlen( strings[i] ) + 1;
    len += 1 + !strings_count;

    if (!prologue)
    {
        unsigned int size = max( 1024, len );

        if (!(prologue = malloc( sizeof(*prologue) + size ))) return 0;
        prologue->calling_method = 0;
        prologue->major_version  = SMBIOS_MAJOR_VERSION;
        prologue->minor_version  = SMBIOS_MINOR_VERSION;
        prologue->revision       = 0;
        prologue->length         = 0;

        buf->prologue = prologue;
        buf->size     = size;
        buf->handle   = 0;
    }
    else if (prologue->length + len > buf->size)
    {
        unsigned int size = max( prologue->length + len, buf->size * 2 );
        if (!(prologue = realloc( buf->prologue, sizeof(*prologue) + size ))) return 0;
        buf->prologue = prologue;
        buf->size     = size;
    }

    pos = (char *)(prologue + 1) + prologue->length;
    hdr->handle = buf->handle++;
    memcpy( pos, hdr, hdr->length );
    pos += hdr->length;
    for (i = 0; i < strings_count; i++)
    {
        strcpy( pos, strings[i] );
        pos += strlen( strings[i] ) + 1;
    }
    if (!strings_count) *pos++ = 0;
    *pos = 0;
    prologue->length += len;
    return hdr->handle;
}

#define ADD_STR(str) (*(str) ? (strings[string_count] = (str), ++string_count) : 0)

static WORD append_smbios_bios( struct smbios_buffer *buf, const char *vendor, const char *version,
                                const char *date )
{
    const char *strings[3];
    unsigned int string_count = 0;
    struct smbios_bios bios = { .hdr.type = SMBIOS_TYPE_BIOS, .hdr.length = sizeof(bios) };

    bios.vendor                    = ADD_STR( vendor );
    bios.version                   = ADD_STR( version );
    bios.start                     = 0xe000;
    bios.date                      = ADD_STR( date );
    bios.characteristics           = 0x8;  /* not supported */
    bios.system_bios_major_release = 0xFF; /* not supported */
    bios.system_bios_minor_release = 0xFF; /* not supported */
    bios.ec_firmware_major_release = 0xFF; /* not supported */
    bios.ec_firmware_minor_release = 0xFF; /* not supported */
    return append_smbios( buf, &bios.hdr, strings, string_count );
}

static WORD append_smbios_system( struct smbios_buffer *buf, const char *vendor, const char *product,
                                  const char *version, const char *serial, const char *sku,
                                  const char *family, const GUID *uuid )
{
    const char *strings[6];
    unsigned int string_count = 0;
    struct smbios_system system = { .hdr.type = SMBIOS_TYPE_SYSTEM, .hdr.length = sizeof(system) };

    system.vendor       = ADD_STR( vendor );
    system.product      = ADD_STR( product );
    system.version      = ADD_STR( version );
    system.serial       = ADD_STR( serial );
    memcpy( &system.uuid, uuid, sizeof(*uuid) );
    system.wake_up_type = 0x06; /* power switch */
    system.sku_number   = ADD_STR( sku );
    system.family       = ADD_STR( family );
    return append_smbios( buf, &system.hdr, strings, string_count );
}

static WORD append_smbios_chassis( struct smbios_buffer *buf, BYTE type, const char *vendor,
                                   const char *version, const char *serial, const char *asset_tag )
{
    const char *strings[4];
    unsigned int string_count = 0;
    struct smbios_chassis chassis = { .hdr.type = SMBIOS_TYPE_CHASSIS, .hdr.length = sizeof(chassis) };

    chassis.vendor                       = ADD_STR( vendor );
    chassis.type                         = type ? type : 2; /* unknown */
    chassis.version                      = ADD_STR( version );
    chassis.serial                       = ADD_STR( serial );
    chassis.asset_tag                    = ADD_STR( asset_tag );
    chassis.boot_state                   = 0x02; /* unknown */
    chassis.power_supply_state           = 0x02; /* unknown */
    chassis.thermal_state                = 0x02; /* unknown */
    chassis.security_status              = 0x02; /* unknown */
    chassis.oem_defined                  = 0;
    chassis.height                       = 0; /* undefined */
    chassis.num_power_cords              = 0; /* unspecified */
    chassis.num_contained_elements       = 0;
    chassis.contained_element_rec_length = 3;
    return append_smbios( buf, &chassis.hdr, strings, string_count );
}

static WORD append_smbios_board( struct smbios_buffer *buf, WORD chassis_handle, const char *vendor,
                                 const char *product, const char *version, const char *serial,
                                 const char *asset_tag )
{
    const char *strings[5];
    unsigned int string_count = 0;
    struct smbios_board board = { .hdr.type = SMBIOS_TYPE_BASEBOARD, .hdr.length = sizeof(board) };

    board.vendor                = ADD_STR( vendor );
    board.product               = ADD_STR( product );
    board.version               = ADD_STR( version );
    board.serial                = ADD_STR( serial );
    board.asset_tag             = ADD_STR( asset_tag );
    board.feature_flags         = 0x5; /* hosting board, removable */
    board.location              = 0;
    board.chassis_handle        = chassis_handle;
    board.board_type            = 0xa; /* motherboard */
    board.num_contained_handles = 0;
    return append_smbios( buf, &board.hdr, strings, string_count );
}

static WORD append_smbios_processor( struct smbios_buffer *buf, WORD core_count, WORD thread_count,
                                     WORD family, const char *socket, const char *vendor,
                                     const char *version, const char *serial, const char *asset_tag )
{
    const char *strings[5];
    unsigned int string_count = 0;
    struct smbios_processor proc = { .hdr.type = SMBIOS_TYPE_PROCESSOR, .hdr.length = sizeof(proc) };

    proc.socket         = ADD_STR( socket );
    proc.type           = 3;  /* central processor */
    proc.family         = family ? min( family, 0xfe ) : 2; /* unknown */
    proc.vendor         = ADD_STR( vendor );
    proc.id             = cpu_id;
    proc.version        = ADD_STR( version );
    proc.status         = 0x41; /* cpu enabled */
    proc.upgrade        = 2;  /* unknown */
    proc.l1cache        = 0xffff;  /* unknown */
    proc.l2cache        = 0xffff;  /* unknown */
    proc.l3cache        = 0xffff;  /* unknown */
    proc.serial         = ADD_STR( serial );
    proc.asset_tag      = ADD_STR( asset_tag );
    proc.core_count     = min( core_count, 0xff );
    proc.core_enabled   = min( core_count, 0xff );
    proc.thread_count   = min( thread_count, 0xff );
    proc.family2        = family ? family : 2;
    proc.core_count2    = core_count;
    proc.core_enabled2  = core_count;
    proc.thread_count2  = thread_count;
    if (sizeof(void *) > sizeof(int)) proc.characteristics |= 1 << 2;  /* 64-bit */
    if (core_count > 1) proc.characteristics |= 1 << 3;  /* multi-core */
    if (thread_count > core_count) proc.characteristics |= 1 << 4;  /* multi-thread */
    return append_smbios( buf, &proc.hdr, strings, string_count );
}

static WORD append_smbios_boot_info( struct smbios_buffer *buf )
{
    struct smbios_boot_info boot = { .hdr.type = SMBIOS_TYPE_BOOTINFO, .hdr.length = sizeof(boot) };

    return append_smbios( buf, &boot.hdr, NULL, 0 );
}

#ifdef __aarch64__
#ifdef linux

#include <asm/hwcap.h>

static DWORD get_core_id_regs_arm64( struct smbios_wine_id_reg_value_arm64 *regs,
                                     WORD logical_thread_id )
{
    static const char midr_el1_path[] = "/sys/devices/system/cpu/cpu%u/regs/identification/midr_el1";
    char path_buf[0x100];
    unsigned long value;
    DWORD regidx = 0;
    FILE *fp;

    /* MIDR_EL1 can vary across cores, so read it from sysfs. */
    snprintf( path_buf, sizeof(path_buf), midr_el1_path, logical_thread_id );
    if ((fp = fopen( path_buf, "r" )))
    {
        fscanf( fp, "%lx", &value );
        fclose( fp );
        regs[regidx++] = (struct smbios_wine_id_reg_value_arm64){ 0x4000, value };
    }

    if (!(getauxval(AT_HWCAP) & HWCAP_CPUID))
    {
        WARN( "Skipping ID register population as kernel is missing emulation support.\n" );
        return regidx;
    }

#define STR(a) #a
#define READ_ID_REG(reg_id) \
    /* mrs x0, #reg_id */ \
    __asm__ __volatile__( ".inst " STR(0xd5300000 | reg_id << 5) "\n\t" \
                          "mov %0, x0" : "=r"(value) :: "x0" ); \
    regs[regidx++] = (struct smbios_wine_id_reg_value_arm64){ reg_id, value };

    /* Linux traps reads to these ID registers and emulates them. They do not vary across cores,
     * if the kernel doesn't support a specific ID register it will read as zero. */
    READ_ID_REG( 0x4020 ); /* ID_AA64PFR0_EL1 */
    READ_ID_REG( 0x4021 ); /* ID_AA64PFR1_EL1 */
    READ_ID_REG( 0x4024 ); /* ID_AA64ZFR0_EL1 */
    READ_ID_REG( 0x4025 ); /* ID_AA64SMFR0_EL1 */
    READ_ID_REG( 0x4028 ); /* ID_AA64DFR0_EL1 */
    READ_ID_REG( 0x4029 ); /* ID_AA64DFR1_EL1 */
    READ_ID_REG( 0x402c ); /* ID_AA64AFR0_EL1 */
    READ_ID_REG( 0x402d ); /* ID_AA64AFR1_EL1 */
    READ_ID_REG( 0x4030 ); /* ID_AA64ISAR0_EL1 */
    READ_ID_REG( 0x4031 ); /* ID_AA64ISAR1_EL1 */
    READ_ID_REG( 0x4032 ); /* ID_AA64ISAR2_EL1 */
    READ_ID_REG( 0x4038 ); /* ID_AA64MMFR0_EL1 */
    READ_ID_REG( 0x4039 ); /* ID_AA64MMFR1_EL1 */
    READ_ID_REG( 0x403a ); /* ID_AA64MMFR2_EL1 */
    READ_ID_REG( 0x5801 ); /* CTR_EL0 */
    /* Windows exposes SCTLR_EL1, ACTLR_EL1, TTBR0_EL1 and MAIR_EL1, but these are inaccessible under
     * linux so leave them unpopulated. */

#undef READ_ID_REG
#undef STR
    return regidx;
}

#else

static DWORD get_core_id_regs_arm64( struct smbios_wine_id_reg_value_arm64 *regs,
                                     WORD logical_thread_id )
{
    FIXME("stub\n");
    return 0;
}

#endif

static WORD append_smbios_wine_core_id_regs_arm64( struct smbios_buffer *buf, WORD ref_handle,
                                                   WORD logical_thread_id )
{

    WORD length;
    BYTE info_buf[0xff];
    struct smbios_processor_additional_info *proc_additional_info =
        (struct smbios_processor_additional_info *)info_buf;
    struct smbios_wine_core_id_regs_arm64 *core_id_regs =
        (struct smbios_wine_core_id_regs_arm64 *)proc_additional_info->info_block.data;

    proc_additional_info->hdr.type = SMBIOS_TYPE_PROCESSOR_ADDITIONAL_INFO;
    proc_additional_info->ref_handle = ref_handle;
    proc_additional_info->info_block.processor_type = 5; /* 64 bit ARM */

    core_id_regs->num_regs = get_core_id_regs_arm64( core_id_regs->regs, logical_thread_id );

    length = sizeof(struct smbios_processor_additional_info) +
             sizeof(struct smbios_wine_core_id_regs_arm64) +
             core_id_regs->num_regs * sizeof(struct smbios_wine_id_reg_value_arm64);

    proc_additional_info->hdr.length = length;
    proc_additional_info->info_block.length = length - 6;

    return append_smbios( buf, &proc_additional_info->hdr, NULL, 0 );
}

#endif

static void append_smbios_end( struct smbios_buffer *buf )
{
    struct smbios_header end = { .type = SMBIOS_TYPE_END, .length = sizeof(end) };

    append_smbios( buf, &end, NULL, 0 );
}

static void create_smbios_processors( struct smbios_buffer *buf )
{
    char socket[20], name[49];
    SYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX *p;
    UINT i, family = 0, core_count = 0, thread_count = 0, pkg_count = 0;
#ifdef __aarch64__
    UINT logical_thread_id = 0;
    WORD proc_handle;
#endif

    pthread_once( &logical_proc_init_once, init_logical_proc_info );
    strcpy( name, cpu_name );
    for (i = strlen(name); i > 0 && name[i - 1] == ' '; i--) name[i - 1] = 0;

    for (p = logical_proc_info_ex;
         (char *)p != (char *)logical_proc_info_ex + logical_proc_info_ex_size;
         p = (SYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX *)((char *)p + p->Size) )
    {
        switch (p->Relationship)
        {
        case RelationProcessorPackage:
            if (!pkg_count++) break;
            snprintf( socket, sizeof(socket), "Socket #%u", pkg_count - 1 );
#ifdef __aarch64__
            proc_handle = append_smbios_processor( buf, core_count, thread_count, family,
                                                   socket, cpu_vendor, name, "", "" );
            for (i = 0; i < thread_count; logical_thread_id++, i++)
                append_smbios_wine_core_id_regs_arm64( buf, proc_handle, logical_thread_id );
#else
            append_smbios_processor( buf, core_count, thread_count, family,
                                     socket, cpu_vendor, name, "", "" );
#endif
            core_count = thread_count = 0;
            break;
        case RelationProcessorCore:
            core_count++;
            thread_count++;
            if (p->Processor.Flags & LTP_PC_SMT) thread_count++;
            break;
        default:
            break;
        }
    }
    snprintf( socket, sizeof(socket), "Socket #%u", pkg_count - 1 );
#ifdef __aarch64__
    proc_handle = append_smbios_processor( buf, core_count, thread_count, family,
                                           socket, cpu_vendor, name, "", "" );
    /* Create these in order so they can be looked up by indexing all additional processor
     * info structures by the logical thread id. */
    for (i = 0; i < thread_count; logical_thread_id++, i++)
        append_smbios_wine_core_id_regs_arm64( buf, proc_handle, logical_thread_id );
#else
    append_smbios_processor( buf, core_count, thread_count, family, socket, cpu_vendor, name, "", "" );
#endif
}

#undef ADD_STR

#ifdef linux

static const char *get_smbios_string( const char *path, char *str, size_t size )
{
    FILE *file;
    size_t len;

    str[0] = 0;
    if (!(file = fopen(path, "r"))) return str;

    len = fread( str, 1, size - 1, file );
    fclose( file );

    if (len >= 1 && str[len - 1] == '\n') len--;
    str[len] = 0;
    return str;
}

/* --- The machine's identity, when the firmware's own is out of reach -------
 *
 * The real serial numbers and UUID live in the SMBIOS table. Linux keeps every
 * copy of them root-only on purpose -- /sys/firmware/dmi/tables, the
 * per-field serial files under /sys/class/dmi/id, product_uuid and /dev/mem
 * alike -- because
 * they identify the machine. TuxBlox never asks for root, so on a system that
 * has not been told to relax those permissions there is nothing true to report.
 *
 * What follows builds an identity from the hardware that IS readable, so a
 * machine looks like itself instead of looking like nothing. It is derived, not
 * the firmware's own value, and it is not presented as such: get_smbios_from_
 * sysfs() still wins whenever the real table can be read.
 *
 * THE COMPUTATION BELOW MUST NOT CHANGE. Every byte of it -- the inputs, their
 * order, the separator, the hash -- decides what a machine reports. Change any
 * of it and every existing install looks like a brand new computer, which is
 * indistinguishable from someone evading a ban. Fix it only for a real defect,
 * and only with the repo owner's explicit agreement.
 *
 * The inputs are the ones that identify the machine and do not churn: the
 * board, the processor, and the first permanent MAC. A graphics card or a stick
 * of RAM is deliberately not included -- a firmware UUID does not change when
 * you upgrade one, and neither should this. Uniqueness comes from the MAC,
 * which also fills the node field exactly as real firmware does, so two
 * identical builds still differ.
 *
 * Nothing here reads an environment variable or a registry key. It is not
 * meant to be adjustable: an adjustable machine identity is a ban-evasion
 * tool, which is the one thing this must never become.
 */

/* SHA-256, FIPS 180-4. Self-contained: ntdll's unix half links no crypto. */
struct sha256_ctx
{
    unsigned int h[8];
    unsigned long long len;
    unsigned char buf[64];
    unsigned int buf_len;
};

#define SHA256_ROR(x,n) (((x) >> (n)) | ((x) << (32 - (n))))

static const unsigned int sha256_k[64] =
{
    0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5,
    0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3, 0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174,
    0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc, 0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
    0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7, 0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967,
    0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13, 0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
    0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3, 0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
    0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
    0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208, 0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2
};

static void sha256_block( struct sha256_ctx *ctx, const unsigned char *p )
{
    unsigned int w[64], a, b, c, d, e, f, g, h, s0, s1, ch, maj, t1, t2;
    int i;

    for (i = 0; i < 16; i++)
        w[i] = (unsigned int)p[i * 4] << 24 | (unsigned int)p[i * 4 + 1] << 16 |
               (unsigned int)p[i * 4 + 2] << 8 | (unsigned int)p[i * 4 + 3];
    for (i = 16; i < 64; i++)
    {
        s0 = SHA256_ROR(w[i - 15], 7) ^ SHA256_ROR(w[i - 15], 18) ^ (w[i - 15] >> 3);
        s1 = SHA256_ROR(w[i - 2], 17) ^ SHA256_ROR(w[i - 2], 19) ^ (w[i - 2] >> 10);
        w[i] = w[i - 16] + s0 + w[i - 7] + s1;
    }

    a = ctx->h[0]; b = ctx->h[1]; c = ctx->h[2]; d = ctx->h[3];
    e = ctx->h[4]; f = ctx->h[5]; g = ctx->h[6]; h = ctx->h[7];

    for (i = 0; i < 64; i++)
    {
        s1 = SHA256_ROR(e, 6) ^ SHA256_ROR(e, 11) ^ SHA256_ROR(e, 25);
        ch = (e & f) ^ (~e & g);
        t1 = h + s1 + ch + sha256_k[i] + w[i];
        s0 = SHA256_ROR(a, 2) ^ SHA256_ROR(a, 13) ^ SHA256_ROR(a, 22);
        maj = (a & b) ^ (a & c) ^ (b & c);
        t2 = s0 + maj;
        h = g; g = f; f = e; e = d + t1;
        d = c; c = b; b = a; a = t1 + t2;
    }

    ctx->h[0] += a; ctx->h[1] += b; ctx->h[2] += c; ctx->h[3] += d;
    ctx->h[4] += e; ctx->h[5] += f; ctx->h[6] += g; ctx->h[7] += h;
}

static void sha256_init( struct sha256_ctx *ctx )
{
    ctx->h[0] = 0x6a09e667; ctx->h[1] = 0xbb67ae85; ctx->h[2] = 0x3c6ef372; ctx->h[3] = 0xa54ff53a;
    ctx->h[4] = 0x510e527f; ctx->h[5] = 0x9b05688c; ctx->h[6] = 0x1f83d9ab; ctx->h[7] = 0x5be0cd19;
    ctx->len = 0;
    ctx->buf_len = 0;
}

static void sha256_update( struct sha256_ctx *ctx, const void *data, size_t size )
{
    const unsigned char *p = data;
    size_t take;

    ctx->len += size;
    while (size)
    {
        take = 64 - ctx->buf_len;
        if (take > size) take = size;
        memcpy( ctx->buf + ctx->buf_len, p, take );
        ctx->buf_len += take;
        p += take;
        size -= take;
        if (ctx->buf_len == 64)
        {
            sha256_block( ctx, ctx->buf );
            ctx->buf_len = 0;
        }
    }
}

static void sha256_final( struct sha256_ctx *ctx, unsigned char out[32] )
{
    unsigned long long bits = ctx->len * 8;
    unsigned char pad[8];
    static const unsigned char one = 0x80;
    static const unsigned char zero = 0x00;
    int i;

    sha256_update( ctx, &one, 1 );
    while (ctx->buf_len != 56) sha256_update( ctx, &zero, 1 );
    for (i = 0; i < 8; i++) pad[i] = (unsigned char)(bits >> (56 - i * 8));
    sha256_update( ctx, pad, 8 );

    for (i = 0; i < 8; i++)
    {
        out[i * 4]     = (unsigned char)(ctx->h[i] >> 24);
        out[i * 4 + 1] = (unsigned char)(ctx->h[i] >> 16);
        out[i * 4 + 2] = (unsigned char)(ctx->h[i] >> 8);
        out[i * 4 + 3] = (unsigned char)ctx->h[i];
    }
}

/* Adds one input, length-prefixed, so that two different sets of values can
 * never produce the same byte stream. */
static void identity_add( struct sha256_ctx *ctx, const char *value )
{
    unsigned char len[2];
    size_t size = value ? strlen( value ) : 0;

    if (size > 0xffff) size = 0xffff;
    len[0] = (unsigned char)(size >> 8);
    len[1] = (unsigned char)size;
    sha256_update( ctx, len, 2 );
    if (size) sha256_update( ctx, value, size );
}

/* First key from /proc/cpuinfo, which is the first processor's -- the same on
 * every core of the machines this runs on. */
static void identity_add_cpuinfo( struct sha256_ctx *ctx, const char *key )
{
    char line[256], *colon;
    size_t key_len = strlen( key );
    FILE *f = fopen( "/proc/cpuinfo", "r" );

    if (!f)
    {
        identity_add( ctx, NULL );
        return;
    }
    while (fgets( line, sizeof(line), f ))
    {
        if (strncmp( line, key, key_len )) continue;
        if (!(colon = strchr( line, ':' ))) continue;
        colon++;
        while (*colon == ' ' || *colon == '\t') colon++;
        colon[strcspn( colon, "\n" )] = 0;
        identity_add( ctx, colon );
        fclose( f );
        return;
    }
    fclose( f );
    identity_add( ctx, NULL );
}

/* The machine's first permanent MAC, in interface-name order so the answer does
 * not depend on the order the kernel happened to bring the devices up.
 * addr_assign_type 0 means the address is the hardware's own, which skips
 * randomised and software-assigned addresses (bridges, tunnels, containers).
 * Zeroed if the machine has no such interface at all. */
static void get_permanent_mac( unsigned char mac[6] )
{
    char best[32] = { 0 }, path[256], buf[64];
    struct dirent *de;
    unsigned int v[6];
    DIR *dir;
    FILE *f;
    int i;

    memset( mac, 0, 6 );
    if (!(dir = opendir( "/sys/class/net" ))) return;
    while ((de = readdir( dir )))
    {
        if (de->d_name[0] == '.' || !strcmp( de->d_name, "lo" )) continue;
        /* No real interface name reaches this length; skipping rather than
         * truncating keeps two of them from collapsing into one. */
        if (strlen( de->d_name ) >= sizeof(best)) continue;
        if (best[0] && strcmp( de->d_name, best ) >= 0) continue;

        snprintf( path, sizeof(path), "/sys/class/net/%.31s/addr_assign_type", de->d_name );
        if (!(f = fopen( path, "r" ))) continue;
        buf[0] = 0;
        if (!fgets( buf, sizeof(buf), f ) || atoi( buf ) != 0)
        {
            fclose( f );
            continue;
        }
        fclose( f );

        snprintf( path, sizeof(path), "/sys/class/net/%.31s/address", de->d_name );
        if (!(f = fopen( path, "r" ))) continue;
        buf[0] = 0;
        if (fgets( buf, sizeof(buf), f ) &&
            sscanf( buf, "%x:%x:%x:%x:%x:%x", &v[0], &v[1], &v[2], &v[3], &v[4], &v[5] ) == 6)
        {
            snprintf( best, sizeof(best), "%.31s", de->d_name );
            for (i = 0; i < 6; i++) mac[i] = (unsigned char)v[i];
        }
        fclose( f );
    }
    closedir( dir );
}

/* Hashes the machine's identity. See the block comment above before touching
 * the inputs or their order. */
static void get_machine_identity( unsigned char out[32], unsigned char mac[6] )
{
    static const char domain[] = "TuxBlox machine identity v1";
    char value[256];
    struct sha256_ctx ctx;

    sha256_init( &ctx );
    identity_add( &ctx, domain );
    identity_add( &ctx, get_smbios_string( "/sys/class/dmi/id/board_vendor", value, sizeof(value) ) );
    identity_add( &ctx, get_smbios_string( "/sys/class/dmi/id/board_name", value, sizeof(value) ) );
    identity_add( &ctx, get_smbios_string( "/sys/class/dmi/id/product_name", value, sizeof(value) ) );
    identity_add_cpuinfo( &ctx, "vendor_id" );
    identity_add_cpuinfo( &ctx, "model name" );

    get_permanent_mac( mac );
    sha256_update( &ctx, mac, 6 );
    sha256_final( &ctx, out );
}

/* One field's worth of identity. Each field is its own hash of the machine
 * identity and the field's name, never a slice of a shared one: real hardware
 * does not carry a serial that is part of its UUID, and anything that reads two
 * of these fields would otherwise see the scheme immediately. Knowing one field
 * says nothing about any other. */
static void derive_field( const char *purpose, unsigned char out[32] )
{
    unsigned char id[32], mac[6];
    struct sha256_ctx ctx;

    get_machine_identity( id, mac );
    sha256_init( &ctx );
    sha256_update( &ctx, id, 32 );
    identity_add( &ctx, purpose );
    sha256_final( &ctx, out );
}

/* Fills str with up to size-1 hex digits of this field's own value. */
static const char *machine_derived_serial( char *str, size_t size, const char *purpose )
{
    static const char hex[] = "0123456789ABCDEF";
    unsigned char field[32];
    size_t i;

    derive_field( purpose, field );
    for (i = 0; i + 1 < size && i < 2 * sizeof(field); i++)
        str[i] = hex[(field[i / 2] >> (i & 1 ? 0 : 4)) & 0xf];
    str[i] = 0;
    return str;
}

static GUID *get_system_uuid( GUID *uuid )
{
    unsigned char id[32], mac[6], field[32];

    get_machine_identity( id, mac );
    derive_field( "system uuid", field );

    /* The first ten bytes carry the hash; the last six are the MAC, which is
     * where real firmware puts it -- so that part of the value is the
     * machine's own rather than derived. */
    uuid->Data1 = (unsigned int)field[0] << 24 | (unsigned int)field[1] << 16 |
                  (unsigned int)field[2] << 8 | field[3];
    uuid->Data2 = (unsigned short)(field[4] << 8 | field[5]);
    uuid->Data3 = (unsigned short)(field[6] << 8 | field[7]);

    /* Top two bits of this byte are the variant field. Real firmware sets them
     * to 10, and leaving the hash's own bits there would leave a UUID that no
     * machine reports -- a tell in itself. The version nibble is deliberately
     * left as the hash left it: firmware does not keep that one either (this
     * board reports 13, which is not a version at all). */
    uuid->Data4[0] = (unsigned char)((field[8] & 0x3f) | 0x80);
    uuid->Data4[1] = field[9];
    memcpy( uuid->Data4 + 2, mac, 6 );
    return uuid;
}

/* Case-insensitive search; needle must already be lower case. */
static int contains_ci( const char *haystack, const char *needle )
{
    size_t i, j;

    for (i = 0; haystack[i]; i++)
    {
        for (j = 0; needle[j]; j++)
        {
            char c = haystack[i + j];
            if (c >= 'A' && c <= 'Z') c = (char)(c + 32);
            if (c != needle[j]) break;
        }
        if (!needle[j]) return 1;
    }
    return 0;
}

/* Whether a field still holds whatever the firmware shipped, rather than
 * something a manufacturer wrote into it. */
static int is_oem_placeholder( const char *s )
{
    static const char *const markers[] =
    {
        "to be filled", "default string", "not specified", "not applicable",
        "serial number", "asset tag", "o.e.m.", "unknown", "xxxxx"
    };
    static const char *const exact[] = { "none", "n/a", "na", "0", "123456789" };
    unsigned int i;

    if (!s || !s[0]) return 0;
    for (i = 0; i < sizeof(markers) / sizeof(markers[0]); i++)
        if (contains_ci( s, markers[i] )) return 1;
    for (i = 0; i < sizeof(exact) / sizeof(exact[0]); i++)
        if (contains_ci( s, exact[i] ) && strlen( s ) == strlen( exact[i] )) return 1;
    return 0;
}

/* The wording this machine's own firmware uses for a field nobody filled in.
 *
 * A serial is written by whoever assembles the finished computer. On a machine
 * somebody built themselves that step never happened, so the firmware's own
 * placeholder survives -- and that is the truthful answer, not a missing one.
 * Whether it happened here is readable even when the serials are not: the SKU,
 * the family and the asset tags are set at the same point on the line and share
 * the same fate. If they still hold a placeholder, so do the serials.
 *
 * The string is copied from whichever of those fields has it, so the exact
 * wording is the firmware's own -- "To be filled by O.E.M.", "Default string"
 * and several others are all real, and inventing the wrong one would be its own
 * mismatch. Returns NULL when this machine was properly programmed, which is
 * what a built-and-sold computer looks like; there a placeholder would be
 * wrong and a serial is derived instead. */
static const char *get_oem_placeholder( char *str, size_t size )
{
    static const char *const probes[] =
    {
        "/sys/class/dmi/id/product_sku",
        "/sys/class/dmi/id/product_family",
        "/sys/class/dmi/id/board_asset_tag",
        "/sys/class/dmi/id/chassis_asset_tag",
        "/sys/class/dmi/id/product_version",
    };
    unsigned int i;

    for (i = 0; i < sizeof(probes) / sizeof(probes[0]); i++)
    {
        get_smbios_string( probes[i], str, size );
        if (is_oem_placeholder( str )) return str;
    }
    str[0] = 0;
    return NULL;
}

/* These two, and get_board_serial below, report the real serial wherever the
 * system allows it to be read. Failing that, a machine nobody serialised gets
 * its firmware's own placeholder, and one that was serialised gets a derived
 * value -- see the block comment on the machine identity above. */
static const char *get_system_serial( char *str, size_t size )
{
    get_smbios_string( "/sys/class/dmi/id/product_serial", str, size );
    if (!str[0] && !get_oem_placeholder( str, size ))
        machine_derived_serial( str, size < 17 ? size : 17, "system serial" );
    return str;
}

static const char *get_chassis_serial( char *str, size_t size )
{
    get_smbios_string( "/sys/class/dmi/id/chassis_serial", str, size );
    if (!str[0] && !get_oem_placeholder( str, size ))
        machine_derived_serial( str, size < 17 ? size : 17, "chassis serial" );
    return str;
}

static const char *get_board_serial( char *str, size_t size )
{
    get_smbios_string( "/sys/class/dmi/id/board_serial", str, size );
    /* Always derived, never the placeholder: the board is the one part that was
     * manufactured, and its maker programmes a serial even when the machine
     * around it was never serialised. This used to print the UUID's own sixteen
     * bytes, which made the board serial a transcription of the UUID -- and of
     * the MAC inside it. */
    if (!str[0]) machine_derived_serial( str, size < 17 ? size : 17, "board serial" );
    return str;
}

#define DMI_TABLE_DIR "/sys/firmware/dmi/tables"

/* Read a whole sysfs file, which hands back at most a page per read. */
static void *read_sysfs_file( const char *path, size_t *ret_size, size_t head_room )
{
    off_t done = 0, got;
    struct stat st;
    char *buf;
    int fd;

    if ((fd = open( path, O_RDONLY )) == -1) return NULL;
    if (fstat( fd, &st ) == -1 || st.st_size <= 0 || !(buf = malloc( head_room + st.st_size )))
    {
        close( fd );
        return NULL;
    }
    for (; done < st.st_size; done += got)
    {
        got = read( fd, buf + head_room + done, st.st_size - done );
        if (got <= 0) break;
    }
    close( fd );
    if (done != st.st_size)
    {
        free( buf );
        return NULL;
    }
    *ret_size = st.st_size;
    return buf;
}

/* The firmware's own SMBIOS table.
 *
 * Otherwise this is built from the handful of fields sysfs decodes into
 * /sys/class/dmi/id, which is a small part of what the firmware provides --
 * 591 bytes against this machine's real 2655 -- and describes a machine
 * whose SMBIOS version does not match its own entry point either. The real
 * table is a file, under the same access as the ACPI tables, so prefer it. */
static struct smbios_prologue *get_smbios_from_sysfs(void)
{
    struct smbios_prologue *prologue;
    size_t table_size, eps_size;
    BYTE *eps;

    if (!(prologue = read_sysfs_file( DMI_TABLE_DIR "/DMI", &table_size, sizeof(*prologue) )))
        return NULL;

    prologue->calling_method = 0;
    prologue->major_version  = SMBIOS_MAJOR_VERSION;
    prologue->minor_version  = SMBIOS_MINOR_VERSION;
    prologue->revision       = 0;
    prologue->length         = table_size;

    /* the version belongs to the entry point, not the table */
    if ((eps = read_sysfs_file( DMI_TABLE_DIR "/smbios_entry_point", &eps_size, 0 )))
    {
        if (eps_size >= 9 && !memcmp( eps, "_SM3_", 5 ))
        {
            prologue->major_version = eps[7];
            prologue->minor_version = eps[8];
            prologue->revision      = eps[9 - 1];
        }
        else if (eps_size >= 8 && !memcmp( eps, "_SM_", 4 ))
        {
            prologue->major_version = eps[6];
            prologue->minor_version = eps[7];
        }
        free( eps );
    }
    return prologue;
}

static struct smbios_prologue *create_smbios_data(void)
{
    char vendor[128], version[128], date[128], product[128], serial[128];
    char sku[128], family[128], asset_tag[128], type[11];
    struct smbios_prologue *real;
    GUID uuid;
    BYTE chassis;
    struct smbios_buffer buf = { 0 };

    if ((real = get_smbios_from_sysfs())) return real;

#define S(s) s, sizeof(s)
    append_smbios_bios( &buf,
                        get_smbios_string( "/sys/class/dmi/id/bios_vendor", S(vendor) ),
                        get_smbios_string( "/sys/class/dmi/id/bios_version", S(version) ),
                        get_smbios_string( "/sys/class/dmi/id/bios_date", S(date) ));

    append_smbios_system( &buf,
                          get_smbios_string( "/sys/class/dmi/id/sys_vendor", S(vendor) ),
                          get_smbios_string( "/sys/class/dmi/id/product_name", S(product) ),
                          get_smbios_string( "/sys/class/dmi/id/product_version", S(version) ),
                          get_system_serial( S(serial) ),
                          get_smbios_string( "/sys/class/dmi/id/product_sku", S(sku) ),
                          get_smbios_string( "/sys/class/dmi/id/product_family", S(family) ),
                          get_system_uuid( &uuid ));

    get_smbios_string( "/sys/class/dmi/id/chassis_type", S(type) );
    chassis = append_smbios_chassis( &buf, atoi(type),
                                     get_smbios_string( "/sys/class/dmi/id/chassis_vendor", S(vendor) ),
                                     get_smbios_string( "/sys/class/dmi/id/chassis_version", S(version) ),
                                     get_chassis_serial( S(serial) ),
                                     get_smbios_string( "/sys/class/dmi/id/chassis_tag", S(asset_tag) ));

    append_smbios_board( &buf, chassis,
                         get_smbios_string( "/sys/class/dmi/id/board_vendor", S(vendor) ),
                         get_smbios_string( "/sys/class/dmi/id/board_name", S(product) ),
                         get_smbios_string( "/sys/class/dmi/id/board_version", S(version) ),
                         get_board_serial( S(serial) ),
                         get_smbios_string( "/sys/class/dmi/id/board_asset_tag", S(asset_tag) ));
#undef S

    create_smbios_processors( &buf );
    append_smbios_boot_info( &buf );
    append_smbios_end( &buf );
    return buf.prologue;
}

#elif defined(__APPLE__)

static struct smbios_prologue *get_smbios_from_iokit(void)
{
    io_service_t service;
    CFDataRef data;
    const UInt8 *ptr;
    CFIndex len;
    struct smbios_prologue *prologue;
    BYTE major_version = SMBIOS_MAJOR_VERSION, minor_version = SMBIOS_MINOR_VERSION;

    if (!(service = IOServiceGetMatchingService(0, IOServiceMatching("AppleSMBIOS"))))
    {
        WARN("can't find AppleSMBIOS service\n");
        return NULL;
    }

    if (!(data = IORegistryEntryCreateCFProperty(service, CFSTR("SMBIOS-EPS"), kCFAllocatorDefault, 0)))
    {
        WARN("can't find SMBIOS entry point\n");
        IOObjectRelease(service);
        return NULL;
    }

    len = CFDataGetLength(data);
    ptr = CFDataGetBytePtr(data);
    if (len >= 8 && !memcmp(ptr, "_SM_", 4))
    {
        major_version = ptr[6];
        minor_version = ptr[7];
    }
    CFRelease(data);

    if (!(data = IORegistryEntryCreateCFProperty(service, CFSTR("SMBIOS"), kCFAllocatorDefault, 0)))
    {
        WARN("can't find SMBIOS table\n");
        IOObjectRelease(service);
        return NULL;
    }

    len = CFDataGetLength(data);
    ptr = CFDataGetBytePtr(data);
    if ((prologue = malloc( sizeof(*prologue) + len )))
    {
        prologue->calling_method = 0;
        prologue->major_version = major_version;
        prologue->minor_version = minor_version;
        prologue->revision = 0;
        prologue->length = len;
        memcpy( prologue + 1, ptr, len );
    }
    CFRelease(data);
    IOObjectRelease(service);
    return prologue;
}

static void cf_to_string( CFTypeRef type_ref, char *buffer, size_t buffer_size )
{
    buffer[0] = 0;
    if (!type_ref)
        return;

    if (CFGetTypeID(type_ref) == CFDataGetTypeID())
    {
        size_t length = MIN(CFDataGetLength(type_ref), buffer_size);
        CFDataGetBytes(type_ref, CFRangeMake(0, length), (UInt8*)buffer);
        buffer[length] = 0;
    }
    else if (CFGetTypeID(type_ref) == CFStringGetTypeID())
    {
        CFStringGetCString(type_ref, buffer, buffer_size, kCFStringEncodingASCII);
    }

    CFRelease(type_ref);
}

static struct smbios_prologue *create_smbios_data(void)
{
    io_service_t platform_expert;
    CFDataRef cf_manufacturer, cf_model;
    CFStringRef cf_serial_number, cf_uuid_string;
    char manufacturer[128], model[128], serial_number[128];
    GUID system_uuid = {0};
    BYTE chassis;
    struct smbios_buffer buf = { 0 };
    struct smbios_prologue *ret;

    ret = get_smbios_from_iokit();
    if (ret)
    {
        /* wineboot requires SMBIOS 2.5 or higher tables. */
        if ((ret->major_version >= 3) ||
            (ret->major_version == 2 && ret->minor_version >= 5))
            return ret;
        else
            free(ret);
    }

    /* Apple Silicon Macs don't have SMBIOS, we need to generate it.
     * Use strings and data from IOKit when available.
     */

    platform_expert = IOServiceGetMatchingService(0, IOServiceMatching("IOPlatformExpertDevice"));
    if (!platform_expert)
        return NULL;

    cf_manufacturer = IORegistryEntryCreateCFProperty(platform_expert, CFSTR("manufacturer"), kCFAllocatorDefault, 0);
    cf_model = IORegistryEntryCreateCFProperty(platform_expert, CFSTR("model"), kCFAllocatorDefault, 0);
    cf_serial_number = IORegistryEntryCreateCFProperty(platform_expert, CFSTR(kIOPlatformSerialNumberKey), kCFAllocatorDefault, 0);
    cf_uuid_string = IORegistryEntryCreateCFProperty(platform_expert, CFSTR(kIOPlatformUUIDKey), kCFAllocatorDefault, 0);

    cf_to_string(cf_manufacturer, manufacturer, sizeof(manufacturer));
    cf_to_string(cf_model, model, sizeof(model));
    cf_to_string(cf_serial_number, serial_number, sizeof(serial_number));

    if (cf_uuid_string)
    {
        CFUUIDRef cf_uuid;
        CFUUIDBytes bytes;

        cf_uuid = CFUUIDCreateFromString(kCFAllocatorDefault, cf_uuid_string);
        bytes = CFUUIDGetUUIDBytes(cf_uuid);

        system_uuid.Data1 = (bytes.byte0 << 24) | (bytes.byte1 << 16) | (bytes.byte2 << 8) | bytes.byte3;
        system_uuid.Data2 = (bytes.byte4 << 8) | bytes.byte5;
        system_uuid.Data3 = (bytes.byte6 << 8) | bytes.byte7;
        memcpy(&system_uuid.Data4, &bytes.byte8, sizeof(system_uuid.Data4));

        CFRelease(cf_uuid);
        CFRelease(cf_uuid_string);
    }

    IOObjectRelease(platform_expert);

    append_smbios_bios( &buf, manufacturer, "1.0", "01/01/2021" );
    append_smbios_system( &buf, manufacturer, model, "1.0", serial_number, "", model, &system_uuid );
    chassis = append_smbios_chassis( &buf, 0, manufacturer, "", serial_number, "" );
    append_smbios_board( &buf, chassis, manufacturer, model, model, serial_number, "" );
    create_smbios_processors( &buf );
    append_smbios_boot_info( &buf );
    append_smbios_end( &buf );
    return buf.prologue;
}

#else

static struct smbios_prologue *create_smbios_data(void)
{
    static const char *vendor  = "The Wine project";
    static const char *product = "Wine";
    static const char *version = PACKAGE_VERSION;
    static const char *serial  = "0";
    GUID uuid = { 0 };
    BYTE chassis;
    struct smbios_buffer buf = { 0 };

    append_smbios_bios( &buf, vendor, version, "01/01/2021" );
    append_smbios_system( &buf, vendor, product, version, serial, "", "", &uuid );
    chassis = append_smbios_chassis( &buf, 0, vendor, version, serial, "" );
    append_smbios_board( &buf, chassis, vendor, product, version, serial, "" );
    create_smbios_processors( &buf );
    append_smbios_boot_info( &buf );
    append_smbios_end( &buf );
    return buf.prologue;
}

#endif


/* Windows reports success for these and writes nothing at all, so the caller's
 * buffer is left exactly as it was. */
static const unsigned char writes_nothing[1];

static const unsigned char sys_7_data[] =
{
    0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
};

static const unsigned char sys_9_data[] =
{
    0x00, 0x00, 0x00, 0x00,
};

static const unsigned char sys_55_data[] =
{
    0x00, 0x00, 0x00, 0x00,
};

static const unsigned char sys_59_data[] =
{
    0x01, 0x00, 0x00, 0x00,
};

static const unsigned char sys_60_data[] =
{
    0x00, 0x00, 0x00, 0x00,
};

static const unsigned char sys_65_data[] =
{
    0x00, 0x00, 0x00, 0x00,
};

static const unsigned char sys_70_data[] =
{
    0x01, 0x00, 0x00, 0x00,
};

static const unsigned char sys_86_data[] =
{
    0x00,
};

static const unsigned char sys_87_data[] =
{
    0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00,
};

static const unsigned char sys_92_data[] =
{
    0x04, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00,
};

static const unsigned char sys_145_data[] =
{
    0x00, 0x01,
};

static const unsigned char sys_147_data[] =
{
    0x00,
};

static const unsigned char sys_151_data[] =
{
    0x00, 0x00, 0x00, 0x00,
};

static const unsigned char sys_157_data[] =
{
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
};

static const unsigned char sys_158_data[] =
{
    0x01,
};

static const unsigned char sys_166_data[] =
{
    0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
};

static const unsigned char sys_174_data[] =
{
    0x05, 0x00, 0x00, 0x00, 0x18, 0x00, 0x00, 0x00, 0x30, 0x00, 0x00, 0x00,
    0x4c, 0x00, 0x00, 0x00, 0x60, 0x00, 0x00, 0x00, 0xdc, 0x00, 0x00, 0x00,
};

static const unsigned char sys_192_data[] =
{
    0x07, 0x00, 0x00, 0x00, 0x40, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
};

static const unsigned char sys_195_data[] =
{
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
};

static const unsigned char sys_196_data[] =
{
    0x20, 0x20, 0x00, 0x00,
};

static const unsigned char sys_202_data[] =
{
    0x00,
};

static const unsigned char sys_207_data[] =
{
    0x00, 0x00, 0x00, 0x00,
};

static const unsigned char sys_221_data[] =
{
    0x03, 0x00, 0x00, 0x00,
};

static const unsigned char sys_227_data[] =
{
    0x01,
};

static const unsigned char sys_243_data[] =
{
    0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00,
};

/* Twenty classes this build sized correctly and then refused as not
 * implemented. Windows answers every one of them, and refusing a class the
 * system has is the kind of difference that cannot be innocent. The bytes are
 * what the reference machine returned, from workspace/tests/infoprobe. */
/* offsets 0x0,0x1,0x2,0x3 the machine left untouched; zero here. */
static const unsigned char sys_24_data[] =
{
    0x00, 0x00, 0x00, 0x00, 0x04, 0x00, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00,
    0x14, 0x00, 0x00, 0x00, 0x14, 0x00, 0x00, 0x00,
};
/* offsets 0x4 moved between reads; zero here. */
static const unsigned char sys_33_data[] =
{
    0x00, 0x00, 0x00, 0x00, 0x00, 0x1a, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00,
};
/* offsets 0x0,0x1 moved between reads; zero here. */
static const unsigned char sys_36_data[] =
{
    0x00, 0x00, 0x1b, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
};
/* offsets 0x12 moved between reads; offsets 0x4,0x5,0x6,0x7,0xa,0xb,0xc,0xd,0xe,0xf the machine left untouched; zero here. */
static const unsigned char sys_43_data[] =
{
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x18, 0x00, 0x00, 0x62, 0x9d, 0x01, 0x00, 0x00,
};
static const unsigned char sys_50_data[] =
{
    0x00, 0x00, 0x00, 0x00, 0x00, 0x80, 0xff, 0xff,
};
/* offsets 0x1,0x10,0x28 moved between reads; offsets 0x14,0x15,0x16,0x17 the machine left untouched; zero here. */
static const unsigned char sys_81_data[] =
{
    0x00, 0x00, 0x17, 0x07, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x0c, 0x09,
    0x00, 0x00, 0x00, 0x00, 0x00, 0xe9, 0x05, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x01, 0x00, 0x00, 0x00, 0x00, 0xd0, 0x0a, 0x00, 0x00, 0x00, 0x00, 0x00,
    0xe9, 0xd0, 0x0a, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00,
};
static const unsigned char sys_115_data[] =
{
    0x98, 0x3a, 0x00, 0x00, 0x10, 0x27, 0x00, 0x00,
};
/* offsets 0x1,0x2,0x10,0x28,0x30,0x31 moved between reads; offsets 0x14,0x15,0x16,0x17 the machine left untouched; zero here. */
static const unsigned char sys_119_data[] =
{
    0x00, 0x00, 0x00, 0x0c, 0x00, 0x00, 0x00, 0x00, 0x00, 0xb0, 0x6a, 0x0f,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x67, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x01, 0x00, 0x00, 0x00, 0x00, 0x37, 0x0b, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x0b, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00,
};
/* offsets 0x14,0x15,0x16,0x17 the machine left untouched; zero here. */
static const unsigned char sys_120_data[] =
{
    0x00, 0xe0, 0xb1, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0xe0, 0xb1, 0x01,
    0x00, 0x00, 0x00, 0x00, 0xa2, 0x50, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x67, 0x6c, 0x0a, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x67, 0x6c, 0x0a, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00,
};
/* offsets 0x0,0x1,0x8,0x9,0x18,0x19 moved between reads; zero here. */
static const unsigned char sys_123_data[] =
{
    0x00, 0x00, 0x6a, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x1a, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x0a, 0xb6, 0x87, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x1a, 0x00, 0x00, 0x00, 0x00, 0x00,
};
static const unsigned char sys_126_data[] =
{
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x0a, 0x00, 0x00,
    0xa0, 0x05, 0x00, 0x00, 0x00, 0x10, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x04, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
};
static const unsigned char sys_135_data[] =
{
    0x0c, 0x00, 0x00, 0x00, 0x06, 0x00, 0x00, 0x00,
};
/* offsets 0xc,0xd,0xe,0xf,0x1c,0x1d,0x1e,0x1f the machine left untouched; zero here. */
static const unsigned char sys_153_data[] =
{
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
};
static const unsigned char sys_156_data[] =
{
    0x00, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x00, 0x5c, 0x2d, 0x00, 0x27,
    0x01, 0x00, 0x00, 0x00, 0x1b, 0x1e, 0x01, 0x03, 0x80, 0x3c, 0x21, 0x78,
    0x3c, 0x2d, 0x1f, 0xa6, 0x56, 0x4f, 0xa0, 0x26, 0x0d, 0x4f, 0x53, 0xa5,
    0x6b, 0x80, 0xb3, 0x00, 0xa9, 0xc0, 0x95, 0x00, 0x81, 0x00, 0x81, 0x40,
    0x81, 0x80, 0x81, 0xc0, 0x71, 0x40, 0x56, 0x5e, 0x00, 0xa0, 0xa0, 0xa0,
    0x29, 0x50, 0x30, 0x20, 0x35, 0x00, 0x55, 0x50, 0x21, 0x00, 0x00, 0x1a,
    0x02, 0x3a, 0x80, 0x18, 0x71, 0x38, 0x2d, 0x40, 0x58, 0x2c, 0x45, 0x00,
    0x55, 0x50, 0x21, 0x00, 0x00, 0x1e, 0x00, 0x00, 0x00, 0xfd, 0x00, 0x30,
    0x90, 0x1e, 0xfa, 0x3c, 0x00, 0x0a, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20,
    0x00, 0x00, 0x00, 0xfc, 0x00, 0x45, 0x32, 0x37, 0x51, 0x48, 0x44, 0x2d,
    0x47, 0x0a, 0x20, 0x20, 0x20, 0x20, 0x01, 0x9b,
};
/* offsets 0x9,0xa,0x19,0x1a moved between reads; zero here. */
static const unsigned char sys_182_data[] =
{
    0x00, 0xa0, 0x60, 0xfb, 0x07, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x9e,
    0x06, 0x00, 0x00, 0x00, 0x00, 0xf0, 0x3b, 0x98, 0x07, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0xb3, 0x01, 0x00, 0x00, 0x00, 0x00, 0x30, 0x06, 0x36,
    0x00, 0x00, 0x00, 0x00, 0x00, 0xa0, 0x60, 0x7b, 0x08, 0x00, 0x00, 0x00,
    0x00, 0xc0, 0x43, 0xb6, 0x01, 0x00, 0x00, 0x00,
};
static const unsigned char sys_184_data[] =
{
    0x00, 0xa0, 0x60, 0xfb, 0x07, 0x00, 0x00, 0x00, 0x00, 0x10, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0xff, 0xff, 0x37, 0x1f, 0x08, 0x00, 0x00, 0x00,
};
static const unsigned char sys_198_data[] =
{
    0x46, 0x42, 0x50, 0x54, 0x38, 0x00, 0x00, 0x00, 0x02, 0x00, 0x30, 0x02,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0xb6, 0xe7, 0x01, 0xa9, 0x02, 0x00, 0x00, 0x00, 0x1a, 0x88, 0x3d, 0xaa,
    0x02, 0x00, 0x00, 0x00, 0xf8, 0x2b, 0x49, 0x30, 0x03, 0x00, 0x00, 0x00,
    0x20, 0x91, 0x89, 0x30, 0x03, 0x00, 0x00, 0x00,
};
static const unsigned char sys_201_data[] =
{
    0xf9, 0xd3, 0x86, 0x7c, 0x10, 0xae, 0x76, 0x07,
};
static const unsigned char sys_213_data[] =
{
    0xf1, 0xde, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
};

/* TUXBLOX_DIAG_SYSCLASS: force the answer for a system information class,
 * written "183:0" or "183:0@10" -- class:status[@size], comma-separated. With
 * a size, the override applies only to a query at least that big and reports
 * that length, which is how a class that takes an input structure is asked.
 *
 * The table below was measured by asking each class with a zero-length buffer,
 * which is the only length that can be asked blind. A class whose refusal at
 * length 0 is right can still be wrong at the length its caller really uses,
 * and nothing outside the program says which. This A/Bs a candidate answer
 * without a rebuild per candidate. Diagnostic only, off unless set. */
#define DIAG_SYSCLASS_MAX 8

static struct
{
    unsigned int class, status, len;
    int has_len;
} diag_sysclass[DIAG_SYSCLASS_MAX];
static unsigned int diag_sysclass_count;
static int diag_sysclass_parsed;

static void diag_sysclass_parse(void)
{
    const char *v;

    if (diag_sysclass_parsed) return;
    diag_sysclass_parsed = 1;
    if (!(v = getenv( "TUXBLOX_DIAG_SYSCLASS" ))) return;
    while (*v && diag_sysclass_count < DIAG_SYSCLASS_MAX)
    {
        char *end;
        unsigned int c = strtoul( v, &end, 0 );

        if (end == v || *end != ':') break;
        v = end + 1;
        diag_sysclass[diag_sysclass_count].class = c;
        diag_sysclass[diag_sysclass_count].status = strtoul( v, &end, 16 );
        if (end == v) break;
        v = end;
        if (*v == '@')
        {
            diag_sysclass[diag_sysclass_count].len = strtoul( v + 1, &end, 16 );
            diag_sysclass[diag_sysclass_count].has_len = 1;
            v = end;
        }
        ERR( "DIAG sysclass %u forced to status %08x len %x\n", c,
             diag_sysclass[diag_sysclass_count].status,
             diag_sysclass[diag_sysclass_count].len );
        diag_sysclass_count++;
        if (*v == ',') v++;
    }
}

/* Whether this class is forced, and to what. Logs the caller's input buffer,
 * which for the classes that take one is the question being asked. */
static BOOL diag_sysclass_forced( unsigned int class, const void *info, unsigned int size,
                                  unsigned int *status, unsigned int *len )
{
    unsigned int i;

    diag_sysclass_parse();
    for (i = 0; i < diag_sysclass_count; i++)
    {
        if (diag_sysclass[i].class != class) continue;
        if (diag_sysclass[i].has_len && size < diag_sysclass[i].len) continue;
        *status = diag_sysclass[i].status;
        *len = diag_sysclass[i].has_len ? diag_sysclass[i].len : size;
        if (info && size >= 8)
            ERR( "DIAG sysclass %u size=%u in=%016llx%s -> %08x\n", class, size,
                 (unsigned long long)*(const ULONG64 *)info,
                 size >= 16 ? "..." : "", *status );
        return TRUE;
    }
    return FALSE;
}

static const struct known_class known_system_classes[] =
{
    /* Classes this build used to deny outright. A real Windows 11 25H2 has all
     * of them, and answering "no such class" to a class the system has is a
     * difference in the one direction that cannot be innocent: measured
     * against the reference machine there was not a single class we had and it
     * did not. Statuses from workspace/tests/infoprobe.
     *
     * The three that report success for a zero-length query are recorded as
     * exactly that. What Windows writes into a larger buffer for them was not
     * measured, so nothing is written here. */
    {  45, 0x00000000,      0 },
    { 128, 0x00000000,      0 },
    { 241, 0xc0000004, 283380 },
    { 244, 0x00000000,      0 },
    { 247, 0xc0000061,      0 },

    {   4, 0xc0000002, NO_LENGTH },
    {   6, 0xc00000bb, NO_LENGTH },
{   7, 0xc0000004,     24, sys_7_data, 24 },
    {   9, 0xc0000004,      4, sys_9_data, 4 },
    {  10, 0xc0000002, NO_LENGTH },
    {  12, 0xc0000004, 56 },
    {  13, 0xc0000002, 0 },
    {  14, 0xc0000002, 0 },
    {  15, 0xc0000002, 0 },
    /* Ten classes that report the size they want and then refuse the read.
     * This build described the size and then failed the read as not
     * implemented, which is a status the reference machine never returns for
     * any class; what it returns is its own refusal, and the refusal differs
     * per class. Roblox Player sweeps every system class in turn and reads
     * exactly this pair, so the second half has to be the measured one.
     * Recorded here rather than implemented: these all take an input structure
     * in the same buffer, and the reference machine is refusing the input it
     * was given, not the class. */
    {  17, 0xc0000004, 64, NULL, 0, 0xc0000001 },
    {  19, 0xc0000002, 0 },
    {  20, 0xc0000003, NO_LENGTH },
    {  24, 0xc0000004,   20, sys_24_data, 20 },
    {  25, 0xc0000002, NO_LENGTH },
    {  26, 0xc0000003, NO_LENGTH },
    {  27, 0xc0000003, NO_LENGTH },
    {  29, 0xc0000002, NO_LENGTH },
    {  30, 0xc0000003, NO_LENGTH },
    {  31, 0xc000000d, 0 },
    {  32, 0xc0000003, NO_LENGTH },
    {  33, 0xc0000004,   16, sys_33_data, 16 },
    {  34, 0xc0000003, NO_LENGTH },
    {  36, 0xc0000004,   48, sys_36_data, 48 },
    {  38, 0xc0000003, NO_LENGTH },
    {  39, 0xc0000003, NO_LENGTH },
    {  40, 0xc0000003, NO_LENGTH },
    {  41, 0xc0000003, NO_LENGTH },
    {  42, 0xc0000004, 576 },
    {  43, 0xc0000004,   24, sys_43_data, 24 },
    {  46, 0xc0000003, NO_LENGTH },
    {  47, 0xc0000003, NO_LENGTH },
    {  48, 0xc0000003, NO_LENGTH },
    {  49, 0xc0000003, NO_LENGTH },
    {  50, 0xc0000004,    8, sys_50_data, 8 },
    {  51, 0xc0000004,    144, writes_nothing, 0 },
    {  52, 0xc0000003, NO_LENGTH },
    {  53, 0xc0000004, 16, NULL, 0, 0xc0000005 },
    {  54, 0xc0000003, NO_LENGTH },
    {  55, 0xc0000004,      4, sys_55_data, 4 },
    {  56, 0xc0000022, 0 },
    {  59, 0xc0000004,      4, sys_59_data, 4 },
    {  60, 0xc0000004,      4, sys_60_data, 4 },
    {  61, 0xc0000004, 960 },
    {  65, 0xc0000004,      4, sys_65_data, 4 },
    {  66, 0xc0000004, 32 },
    {  67, 0xc0000003, NO_LENGTH },
    {  68, 0xc0000003, NO_LENGTH },
    {  69, 0xc00000bb, 0 },
    {  70, 0xc0000004,      4, sys_70_data, 4 },
    {  71, 0xc0000003, NO_LENGTH },
    {  72, 0xc000000d, NO_LENGTH },
    {  74, 0xc0000003, NO_LENGTH },
    {  75, 0xc0000003, NO_LENGTH },
    {  78, 0xc0000003, NO_LENGTH },
    {  79, 0xc0000004, 0 },
    {  80, 0xc0000004, 176 },
    {  81, 0xc0000004,   64, sys_81_data, 64 },
    {  82, 0xc0000003, NO_LENGTH },
    {  84, 0xc0000003, NO_LENGTH },
    {  85, 0xc0000003, NO_LENGTH },
    {  86, 0xc0000004,     40, sys_86_data, 1 },
    {  87, 0xc0000004,      8, sys_87_data, 8 },
    {  89, 0xc0000003, NO_LENGTH },
    {  91, 0xc00000f0, 0 },
    {  92, 0xc0000004,     40, sys_92_data, 40 },
    {  93, 0xc0000003, NO_LENGTH },
    {  94, 0xc0000003, NO_LENGTH },
    {  95, 0xc00000bb, NO_LENGTH },
    {  96, 0xc0000002, NO_LENGTH },
    {  97, 0xc0000003, NO_LENGTH },
    {  98, 0xc0000023, 64 },
    {  99, 0xc0000023, 60 },
    { 100, 0xc0000004, 536 },
    { 101, 0xc0000004, 8, NULL, 0, 0xc0000225 },
    { 104, 0xc0000003, NO_LENGTH },
    { 107, 0xc0000003, NO_LENGTH },
    { 106, 0xc0000003,      0 },  /* refused as an invalid class, but with a zero length reported */
    { 108, 0xc0000023, 96 },
    { 109, 0xc0000206, 0 },
    { 110, 0xc0000003, NO_LENGTH },
    { 111, 0xc0000003, NO_LENGTH },
    { 112, 0xc0000023, 16 },
    { 113, 0xc00001a9, 0 },
    { 115, 0xc0000004,    8, sys_115_data, 8 },
    { 116, 0xc0000023, 40 },
    /* SystemTpmBootEntropyInformation. The kernel hands the boot entropy to
     * its first caller and denies everyone else, so an ordinary process is
     * told the size it would need and then refused. */
    { 117, 0xc0000004, 1096, NULL, 0, 0xc0000022 },
    { 118, 0xc0000004,    272, writes_nothing, 0 },
    { 119, 0xc0000004,   64, sys_119_data, 64 },
    { 120, 0xc0000004,   64, sys_120_data, 64 },
    { 121, 0xc0000003, NO_LENGTH },
    { 122, 0xc0000004, 8, NULL, 0, 0xc00000bb },
    { 123, 0xc0000004,   32, sys_123_data, 32 },
    { 124, 0xc0000004, 12, NULL, 0, 0xc00000bb },
    { 125, 0xc0000003, NO_LENGTH },
    { 126, 0xc0000004,   32, sys_126_data, 32 },
    { 127, 0xc0000003, NO_LENGTH },
    { 129, 0xc0000003, NO_LENGTH },
    { 130, 0xc0000003, NO_LENGTH },
    { 131, 0xc0000003, NO_LENGTH },
    { 132, 0xc0000003, NO_LENGTH },
    { 133, 0xc0000061, NO_LENGTH },
    { 134, 0xc0000004, 32, NULL, 0, 0xc0000005 },
    { 135, 0xc0000004,    8, sys_135_data, 8 },
    { 136, 0xc0000004, 48, NULL, 0, 0xc0000005 },
    { 137, 0xc0000004, 48, NULL, 0, 0xc0000005 },
    { 138, 0xc0000023, 240 },
    { 139, 0xc0000206, 0 },
    { 140, 0xc0000023, 377672 },
    { 141, 0xc0000004, 864 },
    { 142, 0xc0000003, NO_LENGTH },
    { 143, 0x80430006, 0 },
    { 145, 0xc0000004,      2, sys_145_data, 2 },
    { 146, 0xc0000003, NO_LENGTH },
    { 147, 0xc0000004,      1, sys_147_data, 1 },
    { 148, 0xc0000022, 0 },
    { 150, 0xc0000061, NO_LENGTH },
    { 151, 0xc0000004,      4, sys_151_data, 4 },
    { 152, 0xc0000003, NO_LENGTH },
    { 153, 0xc0000004,   32, sys_153_data, 32 },
    { 155, 0xc0000003, NO_LENGTH },
    { 156, 0xc0000004,  128, sys_156_data, 128 },
    { 157, 0xc0000004,     24, sys_157_data, 24 },
    { 158, 0xc0000004,      1, sys_158_data, 1 },
    { 159, 0xc00000f0, 0 },
    { 160, 0xc000000d, NO_LENGTH },
    { 161, 0xc0000003, NO_LENGTH },
    { 166, 0xc0000004,      8, sys_166_data, 8 },
    { 167, 0xc0000022, 0 },
    { 168, 0xc0000003, NO_LENGTH },
    { 169, 0xc00000f0, 0 },
    { 170, 0xc0000003, NO_LENGTH },
    { 171, 0x80430006, 0 },
    { 172, 0xc0000004, 7312 },
    { 173, 0xc0000022, NO_LENGTH },
    { 176, 0xc0000003, NO_LENGTH },
    { 177, 0xc0000003, NO_LENGTH },
    { 178, 0xc000000d, NO_LENGTH },
    { 179, 0xc0eb0006, 0 },
    { 180, 0xc0000003, NO_LENGTH },
    { 181, 0xc000000d, NO_LENGTH },
    { 182, 0xc0000004,   56, sys_182_data, 56 },
    { 183, 0xc0000004, NO_LENGTH },
    { 184, 0xc0000004,   24, sys_184_data, 24 },
    { 185, 0xc000000d, 0 },
    { 186, 0x80430006, NO_LENGTH },
    { 187, 0xc0000003, NO_LENGTH },
    { 188, 0xc0000004, NO_LENGTH },
    { 189, 0xc0000004, 273216 },
    { 190, 0xc0000004, 0 },
    { 191, 0xc0000003, NO_LENGTH },
    { 192, 0xc0000004,     32, sys_192_data, 32 },
    { 193, 0xc0000023, 8 },
    { 194, 0xc0000061, 0 },
    { 195, 0xc0000004,      8, sys_195_data, 8 },
    { 196, 0xc0000004,      4, sys_196_data, 4 },
    { 198, 0xc0000004,   56, sys_198_data, 56 },
    { 199, 0xc0000004, 24, NULL, 0, 0xc0000005 },
    { 200, 0xc0000023, 64 },
    { 201, 0xc0000004,    8, sys_201_data, 8 },
    { 202, 0xc0000004,      1, sys_202_data, 1 },
    { 203, 0xc0000003, NO_LENGTH },
    { 204, 0xc0000003, NO_LENGTH },
    { 205, 0xc0000003, NO_LENGTH },
    { 207, 0xc0000004,      4, sys_207_data, 4 },
    { 208, 0xc0000004, 0 },
    { 209, 0xc0000022, 0 },
    { 210, 0xc0000003, NO_LENGTH },
    { 211, 0xc0000003, NO_LENGTH },
    { 212, 0xc0000003, NO_LENGTH },
    { 213, 0xc0000004,    8, sys_213_data, 8 },
    { 214, 0xc0000061, NO_LENGTH },
    { 215, 0xc0000061, 0 },
    { 216, 0xc0000004, 32, NULL, 0, 0xc0000001 },
    { 217, 0xc0000003, NO_LENGTH },
    { 218, 0xc0000003, NO_LENGTH },
    { 219, 0xc0000003, NO_LENGTH },
    { 220, 0xc0000003, NO_LENGTH },
    { 221, 0xc0000004,      4, sys_221_data, 4 },
    { 222, 0xc0000003, NO_LENGTH },
    { 223, 0xc0000003, NO_LENGTH },
    { 224, 0xc0000003, NO_LENGTH },
    { 225, 0xc0000003, NO_LENGTH },
    { 226, 0xc0000003, NO_LENGTH },
    { 227, 0xc0000004,      1, sys_227_data, 1 },
    { 228, 0xc0000004, 0 },
    { 229, 0xc0000004, 0 },
    { 230, 0xc000000d, NO_LENGTH },
    { 231, 0xc0000003, NO_LENGTH },
    { 232, 0xc00000bb, 0 },
    { 233, 0xc0000003, NO_LENGTH },
    { 234, 0xc0000004, 17048 },
    { 235, 0xc0000022, 0 },
    { 236, 0xc00000bb, 0 },
    { 237, 0xc00000bb, 0 },
    { 238, 0xc0000003, NO_LENGTH },
    { 239, 0xc0000003, NO_LENGTH },
    { 240, 0xc0000003, NO_LENGTH },
    { 243, 0xc0000004,     16, sys_243_data, 16 },
};


#define ACPI_TABLE_DIR "/sys/firmware/acpi/tables"
#define ACPI_SIG(a,b,c,d) ((ULONG)(a) | ((ULONG)(b) << 8) | ((ULONG)(c) << 16) | ((ULONG)(d) << 24))
#define SIG_DSDT ACPI_SIG('D','S','D','T')
#define SIG_FACS ACPI_SIG('F','A','C','S')

/* The ACPI tables themselves are readable only by root, but the directory
 * holding them is not: listing it names every table the firmware provides.
 * That listing is the whole of what an enumerate asks for, so it needs no
 * privilege and invents nothing -- these are this machine's own tables, and
 * the same firmware would hand Windows the same set. Sysfs numbers repeated
 * signatures (SSDT1, SSDT2, ...); the signature is the first four characters.
 *
 * A table ID is the signature as it sits in memory, so a little-endian DWORD,
 * which is the opposite order from the provider signature next to it. */
static ULONG acpi_table_signature( const struct dirent *de )
{
    ULONG sig = 0;
    int i;

    if (de->d_type != DT_REG) return 0;  /* "data" and "dynamic" are directories */
    for (i = 0; i < 4; i++)
    {
        unsigned char c = de->d_name[i];
        if (c < 0x20 || c > 0x7e) return 0;
        sig |= (ULONG)c << (i * 8);
    }
    if (de->d_name[4] && (de->d_name[4] < '0' || de->d_name[4] > '9')) return 0;
    return sig;
}

/* DSDT and FACS are not entries in the root table -- they are reached through
 * pointers inside the FADT -- so an enumerate does not list them, though a get
 * still returns them. sysfs makes no such distinction and files them with the
 * rest. Measured: Windows lists 21 tables on this machine where the directory
 * holds 23 files. */
static BOOL acpi_table_is_enumerated( ULONG sig )
{
    return sig != SIG_DSDT && sig != SIG_FACS;
}

static NTSTATUS enum_acpi_tables( SYSTEM_FIRMWARE_TABLE_INFORMATION *sfti, ULONG available_len,
                                  ULONG *required_len )
{
    ULONG *ids = NULL, count = 0, capacity = 0, len;
    struct dirent *de;
    DIR *dir;

    if (!(dir = opendir( ACPI_TABLE_DIR )))
    {
        WARN( "cannot list %s\n", ACPI_TABLE_DIR );
        return STATUS_NOT_FOUND;
    }
    while ((de = readdir( dir )))
    {
        ULONG sig = acpi_table_signature( de );

        if (!sig || !acpi_table_is_enumerated( sig )) continue;
        if (count == capacity)
        {
            ULONG new_capacity = capacity ? capacity * 2 : 32;
            ULONG *new_ids = realloc( ids, new_capacity * sizeof(*ids) );

            if (!new_ids)
            {
                free( ids );
                closedir( dir );
                return STATUS_NO_MEMORY;
            }
            ids = new_ids;
            capacity = new_capacity;
        }
        ids[count++] = sig;
    }
    closedir( dir );

    if (!count)
    {
        free( ids );
        return STATUS_NOT_FOUND;
    }

    sfti->TableBufferLength = len = count * sizeof(*ids);
    *required_len = offsetof( SYSTEM_FIRMWARE_TABLE_INFORMATION, TableBuffer[len] );
    if (available_len < *required_len)
    {
        free( ids );
        return STATUS_BUFFER_TOO_SMALL;
    }
    memcpy( sfti->TableBuffer, ids, len );
    free( ids );
    return STATUS_SUCCESS;
}

/* Serve the table itself where the file can be read -- root, or a machine
 * whose owner has granted access. Where it cannot, say so rather than
 * describing a table nobody has looked at: what these tables describe (the
 * IOMMU, among other things) is exactly the sort of thing invented content
 * would misrepresent. A signature the directory does not list is genuinely
 * absent, and saying so costs nothing. */
static NTSTATUS get_acpi_table( SYSTEM_FIRMWARE_TABLE_INFORMATION *sfti, ULONG available_len,
                                ULONG *required_len )
{
    char path[sizeof(ACPI_TABLE_DIR) + 1 + sizeof(((struct dirent *)0)->d_name)];
    NTSTATUS status = STATUS_NOT_FOUND;
    off_t done = 0, got;
    int best = -1;
    struct dirent *de;
    struct stat st;
    DIR *dir;
    int fd;

    if (!(dir = opendir( ACPI_TABLE_DIR ))) return STATUS_NOT_FOUND;
    while ((de = readdir( dir )))
    {
        int instance;

        if (acpi_table_signature( de ) != sfti->TableID) continue;
        instance = de->d_name[4] ? atoi( de->d_name + 4 ) : 0;
        if (best != -1 && instance >= best) continue;
        best = instance;
        snprintf( path, sizeof(path), ACPI_TABLE_DIR "/%s", de->d_name );
        status = STATUS_ACCESS_DENIED;
    }
    closedir( dir );
    if (status == STATUS_NOT_FOUND)
    {
        /* measured: a signature the machine does not have still reports a
         * length back, it is only the table that is missing */
        *required_len = offsetof( SYSTEM_FIRMWARE_TABLE_INFORMATION, TableBuffer[sizeof(ULONG)] );
        return status;
    }

    if ((fd = open( path, O_RDONLY )) == -1)
    {
        static int once;

        if (!once++)
            FIXME( "cannot read %s (%s); firmware tables are readable by root only\n",
                   path, strerror( errno ) );
        return status;
    }
    if (fstat( fd, &st ) == -1 || st.st_size <= 0)
    {
        close( fd );
        return STATUS_UNSUCCESSFUL;
    }

    sfti->TableBufferLength = st.st_size;
    *required_len = offsetof( SYSTEM_FIRMWARE_TABLE_INFORMATION, TableBuffer[st.st_size] );
    if (available_len < *required_len)
    {
        close( fd );
        return STATUS_BUFFER_TOO_SMALL;
    }
    /* sysfs hands back at most a page at a time, so anything past the first
     * 4 KB -- which is most of DSDT and the SSDTs -- needs more than one read */
    for (done = 0; done < st.st_size; done += got)
    {
        got = read( fd, sfti->TableBuffer + done, st.st_size - done );
        if (got <= 0) break;
    }
    close( fd );
    return done == st.st_size ? STATUS_SUCCESS : STATUS_UNSUCCESSFUL;
}


static NTSTATUS enum_firmware_info( SYSTEM_FIRMWARE_TABLE_INFORMATION *sfti, ULONG available_len,
                                    ULONG *required_len )
{
    ULONG len;

    switch (sfti->ProviderSignature)
    {
    case RSMB:
        sfti->TableBufferLength = len = sizeof(UINT);
        *required_len = offsetof( SYSTEM_FIRMWARE_TABLE_INFORMATION, TableBuffer[len] );
        if (available_len < *required_len) return STATUS_BUFFER_TOO_SMALL;
        *(UINT *)sfti->TableBuffer = 0;
        return STATUS_SUCCESS;

    case ACPI:
        return enum_acpi_tables( sfti, available_len, required_len );

    default:
        /* Windows refuses FIRM and provider 0 the same way -- measured, so this
         * is the right answer rather than a gap. TRACE, not FIXME. */
        TRACE("info_class SYSTEM_FIRMWARE_TABLE_INFORMATION provider %08x\n", (unsigned int)sfti->ProviderSignature);
        return STATUS_NOT_IMPLEMENTED;
    }
}

/* The drivers a Windows kernel has loaded. Wine has none, and there is no true
 * answer to give here. What there is, is a real one: this list is what a real
 * Windows 11 machine reports, read off it with workspace/tests/modprobe.exe --
 * names, sizes and load counts, in the order it gave them. The invented list
 * that used to be here had every size a round multiple of 64K, every load count
 * 1, and ntoskrnl.exe at 8 MB against the real 21 MB, none of which survives
 * comparison with a reference.
 *
 * Every base address is zero. That is not a gap: Windows does not disclose
 * kernel addresses to a caller without the privilege for them, and returns
 * zeroes to an ordinary process, which is how Roblox runs.
 *
 * Twenty-five of the 194 entries are deliberately left out, because keeping
 * them would say something untrue about THIS machine rather than merely
 * incomplete:
 *
 *   - the ones belonging to that machine's own hardware (its GPU, its chipset,
 *     its network adapter). Shipping them would have every install claim the
 *     same hardware whatever it really has. Nothing is invented to replace
 *     them, so there is no display miniport in this list -- an absence, which
 *     is the smaller of the two errors available.
 *   - the Hyper-V drivers. That machine runs a hypervisor; TuxBlox reports that
 *     none is running, and a list that says otherwise contradicts its own
 *     answer two questions earlier.
 *   - drivers installed by third-party software rather than by Windows.
 */
static const struct { const char *name; ULONG size; USHORT load_count; } kernel_modules[] =
{
    { "\\SystemRoot\\system32\\ntoskrnl.exe", 0x1450000, 119 },
    { "\\SystemRoot\\system32\\hal.dll", 0x6000, 39 },
    { "\\SystemRoot\\system32\\kd.dll", 0xb000, 2 },
    { "\\SystemRoot\\system32\\symcryptk.dll", 0xd000, 2 },
    { "\\SystemRoot\\System32\\drivers\\cng.sys", 0xe9000, 19 },
    { "\\SystemRoot\\System32\\drivers\\CLFS.SYS", 0x8c000, 4 },
    { "\\SystemRoot\\System32\\drivers\\tm.sys", 0x2a000, 1 },
    { "\\SystemRoot\\System32\\drivers\\winaccel.sys", 0x15000, 1 },
    { "\\SystemRoot\\system32\\PSHED.dll", 0x1d000, 2 },
    { "\\SystemRoot\\system32\\BOOTVID.dll", 0xd000, 1 },
    { "\\SystemRoot\\System32\\drivers\\FLTMGR.SYS", 0x95000, 18 },
    { "\\SystemRoot\\System32\\drivers\\msrpc.sys", 0x65000, 19 },
    { "\\SystemRoot\\System32\\drivers\\ksecdd.sys", 0x36000, 28 },
    { "\\SystemRoot\\System32\\drivers\\clipsp.sys", 0x117000, 3 },
    { "\\SystemRoot\\System32\\drivers\\cmimcext.sys", 0x12000, 1 },
    { "\\SystemRoot\\System32\\drivers\\werkernel.sys", 0x17000, 5 },
    { "\\SystemRoot\\System32\\drivers\\ntosext.sys", 0xd000, 1 },
    { "\\SystemRoot\\System32\\drivers\\watchdog.sys", 0x20000, 8 },
    { "\\SystemRoot\\System32\\drivers\\WMILIB.SYS", 0xd000, 17 },
    { "\\SystemRoot\\System32\\drivers\\dxgkrnl.sys", 0x4f8000, 6 },
    { "\\SystemRoot\\System32\\win32k.sys", 0xc7000, 5 },
    { "\\SystemRoot\\system32\\CI.dll", 0x11a000, 3 },
    { "\\SystemRoot\\System32\\drivers\\globmerger.sys", 0x20000, 2 },
    { "\\SystemRoot\\system32\\drivers\\Wdf01000.sys", 0xf0000, 1 },
    { "\\SystemRoot\\system32\\drivers\\WppRecorder.sys", 0x13000, 40 },
    { "\\SystemRoot\\system32\\drivers\\WDFLDR.SYS", 0x17000, 40 },
    { "\\SystemRoot\\System32\\DriverStore\\FileRepository\\prm.inf_amd64_7c38475757a1f016\\PRM.sys", 0xf000, 1 },
    { "\\SystemRoot\\System32\\Drivers\\acpiex.sys", 0x2a000, 1 },
    { "\\SystemRoot\\system32\\drivers\\msseccore.sys", 0xf000, 1 },
    { "\\SystemRoot\\System32\\drivers\\ACPI.sys", 0xd6000, 1 },
    { "\\SystemRoot\\System32\\drivers\\msisadrv.sys", 0xc000, 1 },
    { "\\SystemRoot\\System32\\drivers\\pci.sys", 0x92000, 1 },
    { "\\SystemRoot\\System32\\drivers\\tpm.sys", 0x5a000, 1 },
    { "\\SystemRoot\\system32\\drivers\\WindowsTrustedRT.sys", 0x18000, 1 },
    { "\\SystemRoot\\System32\\drivers\\WindowsTrustedRTProxy.sys", 0xc000, 1 },
    { "\\SystemRoot\\System32\\drivers\\pcw.sys", 0x17000, 1 },
    { "\\SystemRoot\\System32\\drivers\\vdrvroot.sys", 0x1c000, 1 },
    { "\\SystemRoot\\system32\\drivers\\pdc.sys", 0x35000, 1 },
    { "\\SystemRoot\\system32\\drivers\\CEA.sys", 0x18000, 3 },
    { "\\SystemRoot\\System32\\drivers\\partmgr.sys", 0x35000, 1 },
    { "\\SystemRoot\\System32\\drivers\\spaceport.sys", 0xf7000, 1 },
    { "\\SystemRoot\\System32\\drivers\\volmgr.sys", 0x1d000, 1 },
    { "\\SystemRoot\\System32\\drivers\\volmgrx.sys", 0x66000, 1 },
    { "\\SystemRoot\\System32\\drivers\\mountmgr.sys", 0x1f000, 1 },
    { "\\SystemRoot\\System32\\drivers\\storahci.sys", 0x37000, 1 },
    { "\\SystemRoot\\System32\\drivers\\storport.sys", 0x27a000, 2 },
    { "\\SystemRoot\\System32\\drivers\\stornvme.sys", 0x4e000, 1 },
    { "\\SystemRoot\\System32\\drivers\\EhStorClass.sys", 0x28000, 1 },
    { "\\SystemRoot\\System32\\drivers\\fileinfo.sys", 0x1d000, 1 },
    { "\\SystemRoot\\System32\\Drivers\\Wof.sys", 0x45000, 1 },
    { "\\SystemRoot\\system32\\drivers\\wd\\WdFilter.sys", 0x9b000, 1 },
    { "\\SystemRoot\\System32\\Drivers\\Ntfs.sys", 0x36a000, 1 },
    { "\\SystemRoot\\System32\\Drivers\\Fs_Rec.sys", 0xf000, 1 },
    { "\\SystemRoot\\system32\\drivers\\ndis.sys", 0x1be000, 29 },
    { "\\SystemRoot\\system32\\drivers\\NETIO.SYS", 0xb6000, 29 },
    { "\\SystemRoot\\system32\\drivers\\fse.sys", 0x36000, 1 },
    { "\\SystemRoot\\system32\\drivers\\fwpkclnt.sys", 0x8b000, 12 },
    { "\\SystemRoot\\System32\\Drivers\\ksecpkg.sys", 0x38000, 1 },
    { "\\SystemRoot\\System32\\Drivers\\Kerb3961Kernel.sys", 0x24000, 1 },
    { "\\SystemRoot\\System32\\drivers\\tcpip.sys", 0x358000, 1 },
    { "\\SystemRoot\\System32\\drivers\\wfplwfs.sys", 0x3c000, 1 },
    { "\\SystemRoot\\system32\\drivers\\VmsProxy.sys", 0xf000, 1 },
    { "\\SystemRoot\\system32\\drivers\\VmsProxyHNic.sys", 0x10000, 1 },
    { "\\SystemRoot\\System32\\DRIVERS\\fvevol.sys", 0xed000, 1 },
    { "\\SystemRoot\\System32\\drivers\\volume.sys", 0xb000, 1 },
    { "\\SystemRoot\\System32\\drivers\\volsnap.sys", 0x82000, 1 },
    { "\\SystemRoot\\System32\\drivers\\rdyboost.sys", 0x4e000, 1 },
    { "\\SystemRoot\\System32\\drivers\\nvmedisk.sys", 0x1d000, 1 },
    { "\\SystemRoot\\System32\\Drivers\\mup.sys", 0x2b000, 4 },
    { "\\SystemRoot\\system32\\drivers\\iorate.sys", 0x14000, 1 },
    { "\\SystemRoot\\System32\\drivers\\disk.sys", 0x20000, 1 },
    { "\\SystemRoot\\System32\\drivers\\CLASSPNP.SYS", 0x79000, 1 },
    { "\\SystemRoot\\System32\\Drivers\\crashdmp.sys", 0x28000, 1 },
    { "\\SystemRoot\\System32\\drivers\\cdrom.sys", 0x36000, 1 },
    { "\\SystemRoot\\system32\\drivers\\filecrypt.sys", 0x17000, 1 },
    { "\\SystemRoot\\system32\\drivers\\tbs.sys", 0xf000, 1 },
    { "\\SystemRoot\\system32\\drivers\\UCPD.sys", 0x30000, 1 },
    { "\\SystemRoot\\System32\\Drivers\\Null.SYS", 0xd000, 1 },
    { "\\SystemRoot\\System32\\Drivers\\Beep.SYS", 0xa000, 1 },
    { "\\SystemRoot\\System32\\DriverStore\\FileRepository\\uiomap.inf_amd64_e6e3e44178152d2b\\uiomap.sys", 0x11000, 1 },
    { "\\SystemRoot\\System32\\DriverStore\\FileRepository\\basicdisplay.inf_amd64_9f34636ebdd89def\\BasicDisplay.sys", 0x1a000, 1 },
    { "\\SystemRoot\\System32\\DriverStore\\FileRepository\\basicrender.inf_amd64_cf45ae7c2c82746d\\BasicRender.sys", 0x12000, 1 },
    { "\\SystemRoot\\System32\\Drivers\\Npfs.SYS", 0x1c000, 1 },
    { "\\SystemRoot\\System32\\Drivers\\Msfs.SYS", 0x13000, 1 },
    { "\\SystemRoot\\System32\\Drivers\\CimFS.SYS", 0x43000, 1 },
    { "\\SystemRoot\\system32\\DRIVERS\\tdx.sys", 0x2a000, 1 },
    { "\\SystemRoot\\system32\\DRIVERS\\TDI.SYS", 0x11000, 7 },
    { "\\SystemRoot\\System32\\DRIVERS\\netbt.sys", 0x59000, 1 },
    { "\\SystemRoot\\system32\\drivers\\afunix.sys", 0x15000, 1 },
    { "\\SystemRoot\\system32\\drivers\\afd.sys", 0xb6000, 1 },
    { "\\SystemRoot\\System32\\drivers\\vwififlt.sys", 0x1e000, 1 },
    { "\\SystemRoot\\system32\\drivers\\vfpext.sys", 0x1a1000, 1 },
    { "\\SystemRoot\\System32\\drivers\\pacer.sys", 0x31000, 1 },
    { "\\SystemRoot\\System32\\drivers\\ndiscap.sys", 0x15000, 1 },
    { "\\SystemRoot\\system32\\drivers\\netbios.sys", 0x16000, 1 },
    { "\\SystemRoot\\system32\\DRIVERS\\rdbss.sys", 0x8e000, 5 },
    { "\\SystemRoot\\system32\\drivers\\csc.sys", 0x99000, 1 },
    { "\\SystemRoot\\system32\\drivers\\nsiproxy.sys", 0x13000, 1 },
    { "\\SystemRoot\\System32\\drivers\\npsvctrig.sys", 0xf000, 1 },
    { "\\SystemRoot\\System32\\drivers\\mssmbios.sys", 0x11000, 1 },
    { "\\SystemRoot\\System32\\Drivers\\dfsc.sys", 0x30000, 1 },
    { "\\SystemRoot\\system32\\drivers\\bam.sys", 0x1c000, 1 },
    { "\\SystemRoot\\system32\\DRIVERS\\ahcache.sys", 0x5b000, 1 },
    { "\\SystemRoot\\System32\\DriverStore\\FileRepository\\compositebus.inf_amd64_8cf6fa9d3afdfa25\\CompositeBus.sys", 0x14000, 1 },
    { "\\SystemRoot\\System32\\drivers\\kdnic.sys", 0x11000, 1 },
    { "\\SystemRoot\\System32\\DriverStore\\FileRepository\\umbus.inf_amd64_914dd46b4b013b1b\\umbus.sys", 0x16000, 1 },
    { "\\SystemRoot\\System32\\drivers\\USBXHCI.SYS", 0xc0000, 1 },
    { "\\SystemRoot\\system32\\drivers\\ucx01000.sys", 0x48000, 1 },
    { "\\SystemRoot\\system32\\drivers\\netadaptercx.sys", 0x5f000, 1 },
    { "\\SystemRoot\\System32\\Drivers\\ExecutionContext.sys", 0x20000, 1 },
    { "\\SystemRoot\\System32\\drivers\\HDAudBus.sys", 0x32000, 1 },
    { "\\SystemRoot\\System32\\drivers\\portcls.sys", 0x75000, 4 },
    { "\\SystemRoot\\System32\\drivers\\drmk.sys", 0x1e000, 1 },
    { "\\SystemRoot\\System32\\drivers\\ks.sys", 0x86000, 5 },
    { "\\SystemRoot\\System32\\drivers\\serial.sys", 0x1e000, 1 },
    { "\\SystemRoot\\System32\\drivers\\serenum.sys", 0x10000, 1 },
    { "\\SystemRoot\\System32\\Drivers\\msgpioclx.sys", 0x35000, 1 },
    { "\\SystemRoot\\System32\\drivers\\wmiacpi.sys", 0xe000, 1 },
    { "\\SystemRoot\\system32\\drivers\\ksthunk.sys", 0x12000, 1 },
    { "\\SystemRoot\\System32\\drivers\\NdisVirtualBus.sys", 0xe000, 1 },
    { "\\SystemRoot\\System32\\DriverStore\\FileRepository\\swenum.inf_amd64_8b9dd76f362e3ec6\\swenum.sys", 0xc000, 1 },
    { "\\SystemRoot\\System32\\drivers\\rdpbus.sys", 0x10000, 1 },
    { "\\SystemRoot\\System32\\Drivers\\fastfat.SYS", 0x6a000, 1 },
    { "\\SystemRoot\\System32\\drivers\\UsbHub3.sys", 0xb3000, 1 },
    { "\\SystemRoot\\System32\\drivers\\USBD.SYS", 0x10000, 4 },
    { "\\SystemRoot\\System32\\drivers\\HdAudio.sys", 0x85000, 1 },
    { "\\SystemRoot\\System32\\win32kbase.sys", 0x332000, 3 },
    { "\\SystemRoot\\System32\\drivers\\HIDPARSE.SYS", 0x19000, 5 },
    { "\\SystemRoot\\System32\\win32kfull.sys", 0x412000, 1 },
    { "\\SystemRoot\\System32\\win32kbase_rs.sys", 0x26000, 1 },
    { "\\SystemRoot\\System32\\drivers\\usbccgp.sys", 0x34000, 1 },
    { "\\SystemRoot\\System32\\drivers\\hidusb.sys", 0x14000, 1 },
    { "\\SystemRoot\\System32\\drivers\\HIDCLASS.SYS", 0x50000, 1 },
    { "\\SystemRoot\\System32\\drivers\\mouhid.sys", 0x11000, 1 },
    { "\\SystemRoot\\System32\\drivers\\mouclass.sys", 0x15000, 1 },
    { "\\SystemRoot\\System32\\drivers\\kbdhid.sys", 0x14000, 1 },
    { "\\SystemRoot\\System32\\drivers\\kbdclass.sys", 0x15000, 1 },
    { "\\SystemRoot\\System32\\Drivers\\dump_dumpstorport.sys", 0x18000, 2 },
    { "\\SystemRoot\\System32\\drivers\\dump_stornvme.sys", 0x4e000, 1 },
    { "\\SystemRoot\\System32\\Drivers\\dump_dumpfve.sys", 0x24000, 1 },
    { "\\SystemRoot\\system32\\drivers\\usbaudio.sys", 0x48000, 1 },
    { "\\SystemRoot\\System32\\drivers\\dxgmms2.sys", 0x122000, 1 },
    { "\\SystemRoot\\System32\\drivers\\monitor.sys", 0x1f000, 1 },
    { "\\SystemRoot\\System32\\cdd.dll", 0x52000, 1 },
    { "\\SystemRoot\\system32\\drivers\\bfs.sys", 0x27000, 1 },
    { "\\SystemRoot\\system32\\drivers\\luafv.sys", 0x34000, 1 },
    { "\\SystemRoot\\system32\\drivers\\wcifs.sys", 0x3d000, 1 },
    { "\\SystemRoot\\system32\\drivers\\cldflt.sys", 0x94000, 1 },
    { "\\SystemRoot\\system32\\drivers\\UnionFS.sys", 0x77000, 1 },
    { "\\SystemRoot\\system32\\drivers\\storqosflt.sys", 0x1c000, 1 },
    { "\\SystemRoot\\system32\\drivers\\bindflt.sys", 0x2e000, 1 },
    { "\\SystemRoot\\system32\\drivers\\mslldp.sys", 0x1b000, 1 },
    { "\\SystemRoot\\system32\\drivers\\lltdio.sys", 0x19000, 1 },
    { "\\SystemRoot\\system32\\drivers\\rspndr.sys", 0x1c000, 1 },
    { "\\SystemRoot\\System32\\DRIVERS\\wanarp.sys", 0x20000, 1 },
    { "\\SystemRoot\\system32\\drivers\\HTTP.sys", 0x216000, 1 },
    { "\\SystemRoot\\system32\\DRIVERS\\bowser.sys", 0x28000, 1 },
    { "\\SystemRoot\\System32\\drivers\\mpsdrv.sys", 0x1c000, 1 },
    { "\\SystemRoot\\system32\\DRIVERS\\mrxsmb.sys", 0xe5000, 2 },
    { "\\SystemRoot\\system32\\DRIVERS\\mrxsmb20.sys", 0x5c000, 1 },
    { "\\SystemRoot\\System32\\DRIVERS\\srvnet.sys", 0x64000, 2 },
    { "\\SystemRoot\\system32\\drivers\\Ndu.sys", 0x2f000, 1 },
    { "\\SystemRoot\\system32\\drivers\\mmcss.sys", 0x16000, 1 },
    { "\\SystemRoot\\system32\\drivers\\peauth.sys", 0xd5000, 1 },
    { "\\SystemRoot\\System32\\drivers\\tcpipreg.sys", 0x16000, 1 },
    { "\\SystemRoot\\System32\\drivers\\wtd.sys", 0x1d000, 1 },
    { "\\SystemRoot\\System32\\DRIVERS\\srv2.sys", 0xf8000, 1 },
    { "\\SystemRoot\\System32\\drivers\\condrv.sys", 0x14000, 1 },
    { "\\SystemRoot\\system32\\drivers\\wd\\WdNisDrv.sys", 0x1d000, 1 },
};

static void fill_module_info( RTL_PROCESS_MODULE_INFORMATION *sm, ULONG i )
{
    sm->ImageBaseAddress = NULL;   /* not disclosed without the privilege for it */
    sm->ImageSize        = kernel_modules[i].size;
    sm->LoadOrderIndex   = i;
    sm->LoadCount        = kernel_modules[i].load_count;
    strcpy( (char *)sm->Name, kernel_modules[i].name );
    sm->NameOffset = strrchr( kernel_modules[i].name, '\\' ) - kernel_modules[i].name + 1;
}

static NTSTATUS get_firmware_info( SYSTEM_FIRMWARE_TABLE_INFORMATION *sfti, ULONG available_len,
                                   ULONG *required_len )
{
    static struct smbios_prologue *smbios_data;
    ULONG len;

    switch (sfti->ProviderSignature)
    {
    case RSMB:
        if (!smbios_data)
        {
            struct smbios_prologue *data = create_smbios_data();
            if (!data) return STATUS_NO_MEMORY;
            if (InterlockedCompareExchangePointer( (void **)&smbios_data, data, NULL )) free( data );
        }
        len = sizeof(*smbios_data) + smbios_data->length;
        sfti->TableBufferLength = len;
        *required_len = offsetof( SYSTEM_FIRMWARE_TABLE_INFORMATION, TableBuffer[len] );
        if (available_len < *required_len) return STATUS_BUFFER_TOO_SMALL;
        memcpy( sfti->TableBuffer, smbios_data, len );
        return STATUS_SUCCESS;

    case ACPI:
        return get_acpi_table( sfti, available_len, required_len );

    default:
        /* Windows refuses FIRM and provider 0 the same way -- measured, so this
         * is the right answer rather than a gap. TRACE, not FIXME. */
        TRACE("info_class SYSTEM_FIRMWARE_TABLE_INFORMATION provider %08x\n", (unsigned int)sfti->ProviderSignature);
        return STATUS_NOT_IMPLEMENTED;
    }
}

static void get_performance_info( SYSTEM_PERFORMANCE_INFORMATION *info )
{
    unsigned long long totalram = 0, freeram = 0, totalswap = 0, freeswap = 0;

    memset( info, 0, sizeof(*info) );

#if defined(linux)
    {
        FILE *fp;

        if ((fp = fopen("/proc/uptime", "r")))
        {
            double uptime, idle_time;

            fscanf(fp, "%lf %lf", &uptime, &idle_time);
            fclose(fp);
            info->IdleTime.QuadPart = 10000000 * idle_time;
        }
    }
#elif defined(__FreeBSD__) || defined(__FreeBSD_kernel__)
    {
        static int clockrate_name[] = { CTL_KERN, KERN_CLOCKRATE };
        size_t size = 0;
        struct clockinfo clockrate;
        long ptimes[CPUSTATES];

        size = sizeof(clockrate);
        if (!sysctl(clockrate_name, 2, &clockrate, &size, NULL, 0))
        {
            size = sizeof(ptimes);
            if (!sysctlbyname("kern.cp_time", ptimes, &size, NULL, 0))
                info->IdleTime.QuadPart = (ULONGLONG)ptimes[CP_IDLE] * 10000000 / clockrate.stathz;
        }
    }
#elif defined(__APPLE__)
    {
        host_name_port_t host = mach_host_self();
        struct host_cpu_load_info load_info;
        mach_msg_type_number_t count;

        count = HOST_CPU_LOAD_INFO_COUNT;
        if (host_statistics(host, HOST_CPU_LOAD_INFO, (host_info_t)&load_info, &count) == KERN_SUCCESS)
            info->IdleTime.QuadPart = (ULONGLONG)load_info.cpu_ticks[CPU_STATE_IDLE] * 100000;
        mach_port_deallocate(mach_task_self(), host);
    }
#else
    {
        static ULONGLONG idle;
        /* many programs expect IdleTime to change so fake change */
        info->IdleTime.QuadPart = ++idle;
    }
#endif

#ifdef linux
    {
        FILE *fp;

        if ((fp = fopen("/proc/meminfo", "r")))
        {
            unsigned long long value, mem_available = 0;
            char line[64];

            while (fgets(line, sizeof(line), fp))
            {
                if(sscanf(line, "MemTotal: %llu kB", &value) == 1)
                    totalram += value * 1024;
                else if(sscanf(line, "MemFree: %llu kB", &value) == 1)
                    freeram += value * 1024;
                else if(sscanf(line, "SwapTotal: %llu kB", &value) == 1)
                    totalswap += value * 1024;
                else if(sscanf(line, "SwapFree: %llu kB", &value) == 1)
                    freeswap += value * 1024;
                else if (sscanf(line, "Buffers: %llu", &value))
                    freeram += value * 1024;
                else if (sscanf(line, "Cached: %llu", &value))
                    freeram += value * 1024;
                else if (sscanf(line, "MemAvailable: %llu", &value))
                    mem_available = value * 1024;
            }
            fclose(fp);
            totalram -= min( totalram, ram_reporting_bias );
            if (mem_available) freeram = mem_available;
            if ((long long)freeram >= ram_reporting_bias) freeram -= ram_reporting_bias;
            else
            {
                long long bias = ram_reporting_bias - freeram;
                freeswap -= min( bias, freeswap );
                freeram = 0;
            }
        }
    }
#elif defined(__FreeBSD__) || defined(__FreeBSD_kernel__) || defined(__NetBSD__) || \
    defined(__OpenBSD__) || defined(__DragonFly__) || defined(__APPLE__)
    {
#ifdef __APPLE__
        unsigned int val;
#else
        unsigned long val;
#endif
        int mib[2];
        size_t size_sys;

        mib[0] = CTL_HW;
#ifdef HW_MEMSIZE
        {
            uint64_t val64;
            mib[1] = HW_MEMSIZE;
            size_sys = sizeof(val64);
            if (!sysctl(mib, 2, &val64, &size_sys, NULL, 0) && size_sys == sizeof(val64)) totalram = val64;
        }
#endif

#ifdef HAVE_MACH_MACH_H
        {
            host_name_port_t host = mach_host_self();
            mach_msg_type_number_t count;
#ifdef HOST_VM_INFO64_COUNT
            vm_statistics64_data_t vm_stat;
            vm_size_t mac_page_size;

            if (host_page_size(host, &mac_page_size) != KERN_SUCCESS)
            {
                mac_page_size = sysconf( _SC_PAGESIZE );
                WARN("Can't get host's page size, fallback to %lx.\n", mac_page_size);
            }

            count = HOST_VM_INFO64_COUNT;
            if (host_statistics64(host, HOST_VM_INFO64, (host_info64_t)&vm_stat, &count) == KERN_SUCCESS)
                freeram = (vm_stat.free_count + vm_stat.inactive_count) * (ULONGLONG)mac_page_size;
#endif
            if (!totalram)
            {
                host_basic_info_data_t info;
                count = HOST_BASIC_INFO_COUNT;
                if (host_info(host, HOST_BASIC_INFO, (host_info_t)&info, &count) == KERN_SUCCESS)
                    totalram = info.max_mem;
            }
            mach_port_deallocate(mach_task_self(), host);
        }
#endif

        if (!totalram)
        {
            mib[1] = HW_PHYSMEM;
            size_sys = sizeof(val);
            if (!sysctl(mib, 2, &val, &size_sys, NULL, 0) && size_sys == sizeof(val)) totalram = val;
        }
        if (!freeram)
        {
            mib[1] = HW_USERMEM;
            size_sys = sizeof(val);
            if (!sysctl(mib, 2, &val, &size_sys, NULL, 0) && size_sys == sizeof(val)) freeram = val;
        }
#ifdef VM_SWAPUSAGE
        {
            struct xsw_usage swap;
            mib[0] = CTL_VM;
            mib[1] = VM_SWAPUSAGE;
            size_sys = sizeof(swap);
            if (!sysctl(mib, 2, &swap, &size_sys, NULL, 0) && size_sys == sizeof(swap))
            {
                totalswap = swap.xsu_total;
                freeswap = swap.xsu_avail;
            }
        }
#endif
    }
#endif

    /* Titan Quest refuses to run if TotalPageFile <= TotalPhys */
    if (!totalswap) totalswap = page_size;

    info->AvailablePages      = freeram / page_size;
    info->TotalCommittedPages = (totalram + totalswap - freeram - freeswap) / page_size;
    info->TotalCommitLimit    = (totalram + totalswap) / page_size;
    /* The eight counters after this describe kernel bookkeeping that has no
     * counterpart here, and the class as a whole is the size a caller checks
     * rather than the numbers: measured on the reference machine it is 376
     * bytes and this build reported 312, which is what the struct grew for.
     * The one that can be answered honestly is answered. */
    info->ResidentAvailablePages = freeram / page_size;
}

#ifdef linux

static void get_cpu_idle_cycle_times( ULONG64 *times )
{
    unsigned int index, host_index, count;
    char line[256], name[32];
    unsigned long long idle;
    FILE *f;

    memset( times, 0, peb->NumberOfProcessors * sizeof(*times) );
    if (!(f = fopen( "/proc/stat", "r" ))) return;

    /* skip combined cpu statistics line. */
    fgets( line, sizeof(line), f );

    index = 0;
    while (fgets( line, sizeof(line), f ) && index < peb->NumberOfProcessors)
    {
        count = sscanf(line, "%s %*u %*u %*u %llu", name, &idle);

        if (count < 2 || strncmp( name, "cpu", 3 )) break;
        host_index = atoi( name + 3 );
        if (system_cpu_mask && !(system_cpu_mask & ((ULONG_PTR)1 << host_index))) continue;
        times[index] = idle * tsc_from_jiffies[host_index];
        ++index;
    }

    fclose( f );
}
#else

static void get_cpu_idle_cycle_times( ULONG64 *times )
{
    static int once;

    if (!once++) FIXME( "SystemProcessorIdleCycleTimeInformation stub.\n" );
    memset( times, 0, peb->NumberOfProcessors * sizeof(*times) );
}

#endif


/* calculate the mday of dst change date, so that for instance Sun 5 Oct 2007
 * (last Sunday in October of 2007) becomes Sun Oct 28 2007
 *
 * Note: year, day and month must be in unix format.
 */
static int weekday_to_mday(int year, int day, int mon, int day_of_week)
{
    struct tm date;
    time_t tmp;
    int wday, mday;

    /* find first day in the month matching week day of the date */
    memset(&date, 0, sizeof(date));
    date.tm_year = year;
    date.tm_mon = mon;
    date.tm_mday = -1;
    date.tm_wday = -1;
    do
    {
        date.tm_mday++;
        date.tm_isdst = -1;
        tmp = mktime(&date);
    } while (date.tm_wday != day_of_week || date.tm_mon != mon);

    mday = date.tm_mday;

    /* find number of week days in the month matching week day of the date */
    wday = 1; /* 1 - 1st, ...., 5 - last */
    while (wday < day)
    {
        struct tm *tm;

        date.tm_mday += 7;
        date.tm_isdst = -1;
        tmp = mktime(&date);
        tm = localtime(&tmp);
        if (tm->tm_mon != mon)
            break;
        mday = tm->tm_mday;
        wday++;
    }

    return mday;
}

static BOOL match_tz_date( const RTL_SYSTEM_TIME *st, const RTL_SYSTEM_TIME *reg_st )
{
    WORD wDay;

    if (st->wMonth != reg_st->wMonth) return FALSE;
    if (!st->wMonth) return TRUE; /* no transition dates */
    wDay = reg_st->wDay;
    if (!reg_st->wYear) /* date in a day-of-week format */
        wDay = weekday_to_mday(st->wYear - 1900, reg_st->wDay, reg_st->wMonth - 1, reg_st->wDayOfWeek);

    /* special case for 23:59:59.999, match with 0:00:00.000 on the following day */
    if (!reg_st->wYear && reg_st->wHour == 23 && reg_st->wMinute == 59 &&
        reg_st->wSecond == 59 && reg_st->wMilliseconds == 999)
        return (st->wDay == wDay + 1 && !st->wHour && !st->wMinute && !st->wSecond && !st->wMilliseconds);

    return (st->wDay == wDay &&
            st->wHour == reg_st->wHour &&
            (st->wMinute == reg_st->wMinute || (st->wMinute == 30 && !reg_st->wMinute)) &&
            st->wSecond == reg_st->wSecond &&
            st->wMilliseconds == reg_st->wMilliseconds);
}

static BOOL match_tz_info( const RTL_DYNAMIC_TIME_ZONE_INFORMATION *tzi,
                           const RTL_DYNAMIC_TIME_ZONE_INFORMATION *reg_tzi )
{
    return (tzi->Bias == reg_tzi->Bias &&
            match_tz_date(&tzi->StandardDate, &reg_tzi->StandardDate) &&
            match_tz_date(&tzi->DaylightDate, &reg_tzi->DaylightDate));
}

static BOOL reg_query_value( HKEY key, LPCWSTR name, DWORD type, void *data, DWORD count )
{
    char buf[256];
    UNICODE_STRING nameW;
    KEY_VALUE_PARTIAL_INFORMATION *info = (KEY_VALUE_PARTIAL_INFORMATION *)buf;

    if (count > sizeof(buf) - offsetof(KEY_VALUE_PARTIAL_INFORMATION, Data)) return FALSE;

    nameW.Buffer = (WCHAR *)name;
    nameW.Length = wcslen( name ) * sizeof(WCHAR);
    if (NtQueryValueKey( key, &nameW, KeyValuePartialInformation, buf, sizeof(buf), &count ))
        return FALSE;

    if (info->Type != type) return FALSE;
    memcpy( data, info->Data, info->DataLength );
    return TRUE;
}

static BOOL read_reg_tz_info( HANDLE key, UNICODE_STRING *zone_key_name, int year, RTL_DYNAMIC_TIME_ZONE_INFORMATION *tzi )
{
    static const WCHAR stdW[] = { 'S','t','d',0 };
    static const WCHAR dltW[] = { 'D','l','t',0 };
    static const WCHAR mui_stdW[] = { 'M','U','I','_','S','t','d',0 };
    static const WCHAR mui_dltW[] = { 'M','U','I','_','D','l','t',0 };
    static const WCHAR tziW[] = { 'T','Z','I',0 };
    static const WCHAR Dynamic_DstW[] = { 'D','y','n','a','m','i','c',' ','D','S','T',0 };
    HANDLE subkey, subkey_dyn;
    OBJECT_ATTRIBUTES attr;
    struct tz_reg_data
    {
        LONG bias;
        LONG std_bias;
        LONG dlt_bias;
        RTL_SYSTEM_TIME std_date;
        RTL_SYSTEM_TIME dlt_date;
    } tz_data;
    BOOL is_dynamic = FALSE;
    UNICODE_STRING name;
    BOOL ret = FALSE;
    char buffer[16];
    WCHAR yearW[16];

    InitializeObjectAttributes( &attr, zone_key_name, 0, key, NULL );
    if (NtOpenKey( &subkey, KEY_READ, &attr )) return FALSE;

    memset( tzi, 0, sizeof(*tzi) );
    memcpy(tzi->TimeZoneKeyName, zone_key_name->Buffer, zone_key_name->Length);
    tzi->TimeZoneKeyName[zone_key_name->Length / sizeof(WCHAR)] = 0;

    if (!reg_query_value(subkey, mui_stdW, REG_SZ, tzi->StandardName, sizeof(tzi->StandardName)) &&
        !reg_query_value(subkey, stdW, REG_SZ, tzi->StandardName, sizeof(tzi->StandardName)))
        goto done;

    if (!reg_query_value(subkey, mui_dltW, REG_SZ, tzi->DaylightName, sizeof(tzi->DaylightName)) &&
        !reg_query_value(subkey, dltW, REG_SZ, tzi->DaylightName, sizeof(tzi->DaylightName)))
        goto done;

    /* Check for Dynamic DST entry first */
    name.Buffer = (WCHAR *)Dynamic_DstW;
    name.Length = sizeof(Dynamic_DstW) - sizeof(WCHAR);
    attr.RootDirectory = subkey;
    attr.ObjectName = &name;
    if (!NtOpenKey( &subkey_dyn, KEY_READ, &attr ))
    {
        snprintf( buffer, sizeof(buffer), "%u", year );
        ascii_to_unicode( yearW, buffer, strlen(buffer) + 1 );
        is_dynamic = reg_query_value( subkey_dyn, yearW, REG_BINARY, &tz_data, sizeof(tz_data) );
        NtClose( subkey_dyn );
    }
    if (!is_dynamic && !reg_query_value( subkey, tziW, REG_BINARY, &tz_data, sizeof(tz_data) ))
        goto done;

    tzi->Bias = tz_data.bias;
    tzi->StandardBias = tz_data.std_bias;
    tzi->DaylightBias = tz_data.dlt_bias;
    tzi->StandardDate = tz_data.std_date;
    tzi->DaylightDate = tz_data.dlt_date;

    ret = TRUE;

done:
    NtClose( subkey );
    return ret;
}

static void find_reg_tz_info(RTL_DYNAMIC_TIME_ZONE_INFORMATION *tzi, int year)
{
    RTL_DYNAMIC_TIME_ZONE_INFORMATION reg_tzi;
    HANDLE key;
    ULONG idx, len;
    OBJECT_ATTRIBUTES attr;
    UNICODE_STRING nameW;
    char buffer[128];
    KEY_BASIC_INFORMATION *info = (KEY_BASIC_INFORMATION *)buffer;

    init_unicode_string( &nameW, Time_ZonesW );
    InitializeObjectAttributes( &attr, &nameW, 0, 0, NULL );
    if (NtOpenKey( &key, KEY_READ, &attr )) return;

    idx = 0;
    while (!NtEnumerateKey( key, idx++, KeyBasicInformation, buffer, sizeof(buffer), &len ))
    {
        nameW.Buffer = info->Name;
        nameW.Length = info->NameLength;
        if (!read_reg_tz_info( key, &nameW, year, &reg_tzi )) continue;

        TRACE("%s: bias %d\n", debugstr_us(&nameW), reg_tzi.Bias);
        TRACE("std (d/m/y): %u/%02u/%04u day of week %u %u:%02u:%02u.%03u bias %d\n",
              reg_tzi.StandardDate.wDay, reg_tzi.StandardDate.wMonth,
              reg_tzi.StandardDate.wYear, reg_tzi.StandardDate.wDayOfWeek,
              reg_tzi.StandardDate.wHour, reg_tzi.StandardDate.wMinute,
              reg_tzi.StandardDate.wSecond, reg_tzi.StandardDate.wMilliseconds,
              reg_tzi.StandardBias);
        TRACE("dst (d/m/y): %u/%02u/%04u day of week %u %u:%02u:%02u.%03u bias %d\n",
              reg_tzi.DaylightDate.wDay, reg_tzi.DaylightDate.wMonth,
              reg_tzi.DaylightDate.wYear, reg_tzi.DaylightDate.wDayOfWeek,
              reg_tzi.DaylightDate.wHour, reg_tzi.DaylightDate.wMinute,
              reg_tzi.DaylightDate.wSecond, reg_tzi.DaylightDate.wMilliseconds,
              reg_tzi.DaylightBias);

        if (match_tz_info( tzi, &reg_tzi ))
        {
            *tzi = reg_tzi;
            NtClose( key );
            return;
        }
    }
    NtClose( key );

    if (idx == 1) return;  /* registry info not initialized yet */

    FIXME("Can't find matching timezone information in the registry for "
          "bias %d, std (d/m/y): %u/%02u/%04u, dlt (d/m/y): %u/%02u/%04u\n",
          tzi->Bias,
          tzi->StandardDate.wDay, tzi->StandardDate.wMonth, tzi->StandardDate.wYear,
          tzi->DaylightDate.wDay, tzi->DaylightDate.wMonth, tzi->DaylightDate.wYear);
}

static time_t find_dst_change(time_t start, time_t end, int *is_dst)
{
    struct tm *tm;
    ULONGLONG min = (sizeof(time_t) == sizeof(int)) ? (ULONG)start : start;
    ULONGLONG max = (sizeof(time_t) == sizeof(int)) ? (ULONG)end : end;
    time_t pos;

    tm = localtime(&start);
    *is_dst = !tm->tm_isdst;
    TRACE("starting date isdst %d, %s", !*is_dst, ctime(&start));

    for (pos = min; pos <= max; pos += 30 * 24 * 3600)
    {
        tm = localtime(&pos);
        if (tm->tm_isdst == *is_dst)
        {
            max = pos;
            break;
        }
    }

    while (min <= max)
    {
        pos = (min + max) / 2;
        tm = localtime(&pos);

        if (tm->tm_isdst != *is_dst)
            min = pos + 1;
        else
            max = pos - 1;
    }
    return min;
}

static BOOL get_tz_info_from_zoneinfo_name( RTL_DYNAMIC_TIME_ZONE_INFORMATION *tzi, const char *name, int year )
{
    static const WCHAR wine_tz_map[] = { '\\','R','e','g','i','s','t','r','y','\\',
        'M','a','c','h','i','n','e','\\',
        'S','o','f','t','w','a','r','e','\\',
        'T','u','x','B','l','o','x','\\',
        'T','i','m','e',' ','Z','o','n','e','s','\\',
        'T','Z',' ','M','a','p','p','i','n','g',
        0 };

    const char *tzinfo_dir = getenv( "TZDIR" );
    UNICODE_STRING key_name, win_name;
    WCHAR nameW[64], win_nameW[64];
    OBJECT_ATTRIBUTES attr;
    char buf[MAX_PATH];
    HANDLE key;
    BOOL ret;
    FILE *f;

    TRACE( "name %s.\n", debugstr_a( name ));

    if (strlen(name) >= ARRAY_SIZE(nameW)) return FALSE;

    if (!tzinfo_dir) tzinfo_dir = default_tzinfo_dir;
    snprintf( buf, sizeof(buf), "%s/%s", tzinfo_dir, name );

    if (!(f = fopen( buf, "r" )))
    {
        WARN( "Could not open %s.\n", debugstr_a( buf ));
        return FALSE;
    }
    fclose( f );

    init_unicode_string( &key_name, wine_tz_map );
    InitializeObjectAttributes( &attr, &key_name, 0, NULL, NULL );
    if (NtOpenKey( &key, KEY_READ, &attr )) return FALSE;

    ascii_to_unicode( nameW, name, strlen( name ) + 1 );
    ret = reg_query_value( key, nameW, REG_SZ, win_nameW, sizeof(win_nameW) );
    NtClose( key );
    if (!ret) return FALSE;
    TRACE( "got %s for %s.\n", debugstr_w(win_nameW), debugstr_a( name ) );

    init_unicode_string( &key_name, Time_ZonesW );
    if (NtOpenKey( &key, KEY_READ, &attr )) return FALSE;
    init_unicode_string( &win_name, win_nameW );
    ret = read_reg_tz_info( key, &win_name, year, tzi );
    NtClose( key );
    return ret;
}

static BOOL get_system_config_tz_info( RTL_DYNAMIC_TIME_ZONE_INFORMATION *tzi, int year )
{
    char path[PATH_MAX];
    const char *str;
    int len;

    if ((str = getenv( "TZ" )))
    {
        if (*str == ':') ++str;
        return get_tz_info_from_zoneinfo_name( tzi, str, year );
    }

    if (!realpath( "/etc/localtime", path )) return FALSE;
    len = sizeof( default_tzinfo_dir ) - 1;
    if (strncmp( path, default_tzinfo_dir, len )) return FALSE;
    if (path[len] != '/') return FALSE;
    return get_tz_info_from_zoneinfo_name( tzi, path + len + 1, year );
}

static LONG64 get_current_tz_bias(void)
{
    ULONG high, low;

    do
    {
        high = user_shared_data->TimeZoneBias.High1Time;
        low = user_shared_data->TimeZoneBias.LowPart;
    }
    while (high != user_shared_data->TimeZoneBias.High2Time);

    return ((LONG64)high << 32) | low;
}

static void get_timezone_info( RTL_DYNAMIC_TIME_ZONE_INFORMATION *tzi )
{
    static RTL_DYNAMIC_TIME_ZONE_INFORMATION cached_tzi;
    static int current_year = -1, current_bias = 65535;
    RTL_DYNAMIC_TIME_ZONE_INFORMATION reg_tzi;
    struct tm *tm, tm1, tm2;
    time_t year_start, year_end, tmp, dlt = 0, std = 0;
    int is_dst, bias;
    BOOL inverted_dst;

    mutex_lock( &timezone_mutex );

    year_start = time(NULL);
    tm = gmtime(&year_start);
    bias = (LONG)(mktime(tm) - year_start) / 60;

    tm = localtime(&year_start);
    if (current_year == tm->tm_year && current_bias == bias)
    {
        *tzi = cached_tzi;
        mutex_unlock( &timezone_mutex );
        return;
    }

    current_year = tm->tm_year;
    current_bias = bias;
    tm1 = tm2 = *tm;
    tm1.tm_isdst = 0;
    tm2.tm_isdst = 1;
    inverted_dst = mktime(&tm1) < mktime(&tm2);
    if (inverted_dst) bias += 60;

    memset(tzi, 0, sizeof(*tzi));
    TRACE("tz data will be valid through year %d, bias %d, inverted_dst %d\n", tm->tm_year + 1900, bias, inverted_dst);

    tzi->Bias = bias;

    tm->tm_isdst = inverted_dst;
    tm->tm_mday = 1;
    tm->tm_mon = tm->tm_hour = tm->tm_min = tm->tm_sec = tm->tm_wday = tm->tm_yday = 0;
    year_start = mktime(tm);
    TRACE("year_start: %s", ctime(&year_start));

    tm->tm_isdst = inverted_dst;
    tm->tm_mday = tm->tm_wday = tm->tm_yday = 0;
    tm->tm_mon = 12;
    tm->tm_hour = 23;
    tm->tm_min = tm->tm_sec = 59;
    year_end = mktime(tm);
    TRACE("year_end: %s", ctime(&year_end));

    tmp = find_dst_change(year_start, year_end, &is_dst);
    if (inverted_dst) is_dst = !is_dst;
    if (is_dst)
        dlt = tmp;
    else
        std = tmp;

    tmp = find_dst_change(tmp, year_end, &is_dst);
    if (inverted_dst) is_dst = !is_dst;
    if (is_dst)
        dlt = tmp;
    else
        std = tmp;

    TRACE("std: %s", ctime(&std));
    TRACE("dlt: %s", ctime(&dlt));

    if (dlt == std || !dlt || !std)
        TRACE("there is no daylight saving rules in this time zone\n");
    else
    {
        tmp = dlt - tzi->Bias * 60;
        tm = gmtime(&tmp);
        TRACE("dlt gmtime: %s", asctime(tm));

        tzi->DaylightBias = -60;
        tzi->DaylightDate.wYear = tm->tm_year + 1900;
        tzi->DaylightDate.wMonth = tm->tm_mon + 1;
        tzi->DaylightDate.wDayOfWeek = tm->tm_wday;
        tzi->DaylightDate.wDay = tm->tm_mday;
        tzi->DaylightDate.wHour = tm->tm_hour;
        tzi->DaylightDate.wMinute = tm->tm_min;
        tzi->DaylightDate.wSecond = tm->tm_sec;
        tzi->DaylightDate.wMilliseconds = 0;

        TRACE("daylight (d/m/y): %u/%02u/%04u day of week %u %u:%02u:%02u.%03u bias %d\n",
            tzi->DaylightDate.wDay, tzi->DaylightDate.wMonth,
            tzi->DaylightDate.wYear, tzi->DaylightDate.wDayOfWeek,
            tzi->DaylightDate.wHour, tzi->DaylightDate.wMinute,
            tzi->DaylightDate.wSecond, tzi->DaylightDate.wMilliseconds,
            tzi->DaylightBias);

        tmp = std - tzi->Bias * 60 - tzi->DaylightBias * 60;
        tm = gmtime(&tmp);
        TRACE("std gmtime: %s", asctime(tm));

        tzi->StandardBias = 0;
        tzi->StandardDate.wYear = tm->tm_year + 1900;
        tzi->StandardDate.wMonth = tm->tm_mon + 1;
        tzi->StandardDate.wDayOfWeek = tm->tm_wday;
        tzi->StandardDate.wDay = tm->tm_mday;
        tzi->StandardDate.wHour = tm->tm_hour;
        tzi->StandardDate.wMinute = tm->tm_min;
        tzi->StandardDate.wSecond = tm->tm_sec;
        tzi->StandardDate.wMilliseconds = 0;

        TRACE("standard (d/m/y): %u/%02u/%04u day of week %u %u:%02u:%02u.%03u bias %d\n",
            tzi->StandardDate.wDay, tzi->StandardDate.wMonth,
            tzi->StandardDate.wYear, tzi->StandardDate.wDayOfWeek,
            tzi->StandardDate.wHour, tzi->StandardDate.wMinute,
            tzi->StandardDate.wSecond, tzi->StandardDate.wMilliseconds,
            tzi->StandardBias);
    }

    if (get_system_config_tz_info( &reg_tzi, current_year + 1900 ))
    {
        if (match_tz_info( tzi, &reg_tzi ))
        {
            cached_tzi = *tzi = reg_tzi;
            mutex_unlock( &timezone_mutex );
            return;
        }
        WARN( "System config TZ info didn't match guessed parameters, falling back to search.\n" );
    }
    find_reg_tz_info(tzi, current_year + 1900);
    cached_tzi = *tzi;
    mutex_unlock( &timezone_mutex );
}


/* Padding: a genuine Windows box always has 100+ background system processes
 * running regardless of what the user launched; the real list wineserver hands
 * back below is just whatever this specific prefix session actually started
 * (a handful of entries), which is itself a Wine/sandbox fingerprint tools can
 * key off of (see otherapps/winedetector.exe's "Process" test, which walks this
 * exact list via EnumProcesses/NtQuerySystemInformation and OpenProcess()s each
 * entry). Pad with plausible, clearly-synthetic entries representing common
 * Windows system processes, mirroring the SystemModuleInformationEx fake-driver-
 * list fix elsewhere in this file for the same reason. These are inert
 * placeholders only -- not backed by a real process, so OpenProcess against one
 * of these fake PIDs fails the same way it would against any PID that has
 * already exited on a real system, which is an ordinary, expected race any
 * well-behaved enumerator already has to tolerate. */
/* Processes that only exist because of how TuxBlox runs Windows programs.
 * Nothing on Windows is called any of these, so listing them says plainly what
 * the program is running under. They are left out of the list entirely rather
 * than renamed, since a renamed entry still has its real name on disk for
 * anything that opens it and asks. */
static BOOL is_our_own_process( const WCHAR *name, unsigned int len )
{
    static const WCHAR winedeviceW[] = {'w','i','n','e','d','e','v','i','c','e','.','e','x','e',0};
    static const WCHAR plugplayW[] = {'p','l','u','g','p','l','a','y','.','e','x','e',0};
    static const WCHAR rpcssW[] = {'r','p','c','s','s','.','e','x','e',0};
    static const WCHAR winebootW[] = {'w','i','n','e','b','o','o','t','.','e','x','e',0};
    static const WCHAR winemenubuilderW[] =
        {'w','i','n','e','m','e','n','u','b','u','i','l','d','e','r','.','e','x','e',0};
    static const WCHAR *const names[] =
        { winedeviceW, plugplayW, rpcssW, winebootW, winemenubuilderW };
    unsigned int i;

    for (i = 0; i < ARRAY_SIZE(names); i++)
    {
        unsigned int l = wcslen( names[i] );

        if (len == l && !wcsnicmp( name, names[i], l )) return TRUE;
    }
    return FALSE;
}


/* Padding the list out is worse than a short list: a caller that opens each
 * entry finds these do not exist, which proves the list was made up. */
#undef FAKE_PROC

static unsigned int get_system_process_info( SYSTEM_INFORMATION_CLASS class, void *info, ULONG size, ULONG *len )
{
    unsigned int process_count, total_thread_count, total_name_len, i, j;
    unsigned int thread_info_size;
    unsigned int pos = 0;
    char *buffer = NULL;
    unsigned int ret;
    SYSTEM_PROCESS_INFORMATION *last_entry = NULL;

C_ASSERT( sizeof(struct thread_info) <= sizeof(SYSTEM_THREAD_INFORMATION) );
C_ASSERT( sizeof(struct process_info) <= sizeof(SYSTEM_PROCESS_INFORMATION) );

    if (class == SystemExtendedProcessInformation)
        thread_info_size = sizeof(SYSTEM_EXTENDED_THREAD_INFORMATION);
    else
        thread_info_size = sizeof(SYSTEM_THREAD_INFORMATION);

    *len = 0;
    if (size && !(buffer = malloc( size ))) return STATUS_NO_MEMORY;

    SERVER_START_REQ( list_processes )
    {
        wine_server_set_reply( req, buffer, size );
        ret = wine_server_call( req );
        total_thread_count = reply->total_thread_count;
        total_name_len = reply->total_name_len;
        process_count = reply->process_count;
    }
    SERVER_END_REQ;

    if (ret)
    {
        if (ret == STATUS_INFO_LENGTH_MISMATCH)
        {
            *len = sizeof(SYSTEM_PROCESS_INFORMATION) * process_count
                  + (total_name_len + process_count) * sizeof(WCHAR)
                  + total_thread_count * thread_info_size;
        }

        free( buffer );
        return ret;
    }

    for (i = 0; i < process_count; i++)
    {
        SYSTEM_PROCESS_INFORMATION *nt_process = (SYSTEM_PROCESS_INFORMATION *)((char *)info + *len);
        const struct process_info *server_process;
        const WCHAR *server_name, *file_part;
        ULONG proc_len;
        ULONG name_len = 0;
        BOOL hidden;

        pos = (pos + 7) & ~7;
        server_process = (const struct process_info *)(buffer + pos);
        pos += sizeof(*server_process);

        server_name = (const WCHAR *)(buffer + pos);
        file_part = server_name + (server_process->name_len / sizeof(WCHAR));
        pos += server_process->name_len;
        while (file_part > server_name && file_part[-1] != '\\')
        {
            file_part--;
            name_len++;
        }

        hidden = is_our_own_process( file_part, name_len ) &&
                 server_process->pid != HandleToULong( NtCurrentTeb()->ClientId.UniqueProcess );

        proc_len = sizeof(*nt_process) + server_process->thread_count * thread_info_size
                     + (name_len + 1) * sizeof(WCHAR);
        proc_len = (proc_len + 7) & ~(ULONG_PTR)7;
        if (!hidden) *len += proc_len;

        if (!hidden && *len <= size)
        {
            memset(nt_process, 0, proc_len);
            /* tentatively chain to whatever comes next (another real entry, then the
             * fake padding below); the true last entry gets its NextEntryOffset reset
             * to 0 once everything has been written, via last_entry below */
            nt_process->NextEntryOffset = proc_len;
            nt_process->CreationTime.QuadPart = server_process->start_time;
            nt_process->dwThreadCount = server_process->thread_count;
            nt_process->dwBasePriority = server_process->priority;
            nt_process->UniqueProcessId = UlongToHandle(server_process->pid);
            nt_process->ParentProcessId = UlongToHandle(server_process->parent_pid);
            nt_process->SessionId = server_process->session_id;
            nt_process->HandleCount = server_process->handle_count;
            get_thread_times( server_process->unix_pid, -1, &nt_process->KernelTime, &nt_process->UserTime, NULL );
            fill_vm_counters( &nt_process->vmCounters, server_process->unix_pid );
            last_entry = nt_process;
        }

        pos = (pos + 7) & ~7;
        for (j = 0; j < server_process->thread_count; j++)
        {
            const struct thread_info *server_thread = (const struct thread_info *)(buffer + pos);
            SYSTEM_EXTENDED_THREAD_INFORMATION *ti;

            if (!hidden && *len <= size)
            {
                ti = (SYSTEM_EXTENDED_THREAD_INFORMATION *)((BYTE *)nt_process->ti + j * thread_info_size);
                ti->ThreadInfo.CreateTime.QuadPart = server_thread->start_time;
                ti->ThreadInfo.ClientId.UniqueProcess = UlongToHandle(server_process->pid);
                ti->ThreadInfo.ClientId.UniqueThread = UlongToHandle(server_thread->tid);
                ti->ThreadInfo.dwCurrentPriority = server_thread->current_priority;
                ti->ThreadInfo.dwBasePriority = server_thread->base_priority;
                /* the state comes off /proc; the wait reason is the one a user
                 * thread waits for, and Windows leaves the last one in place
                 * while a thread runs rather than clearing it. Both used to be
                 * left at zero, which reads as a thread that never started. */
                get_thread_times( server_process->unix_pid, server_thread->unix_tid,
                                  &ti->ThreadInfo.KernelTime, &ti->ThreadInfo.UserTime,
                                  &ti->ThreadInfo.dwThreadState );
                ti->ThreadInfo.dwWaitReason = 15;   /* WrUserRequest */
                if (class == SystemExtendedProcessInformation)
                {
                    ti->Win32StartAddress = wine_server_get_ptr( server_thread->entry_point );
                    ti->TebBase = wine_server_get_ptr( server_thread->teb );
                }
            }

            pos += sizeof(*server_thread);
        }

        if (!hidden && *len <= size)
        {
            nt_process->ProcessName.Buffer = (WCHAR *)((BYTE *)nt_process->ti
                                                       + server_process->thread_count * thread_info_size);
            nt_process->ProcessName.Length = name_len * sizeof(WCHAR);
            nt_process->ProcessName.MaximumLength = (name_len + 1) * sizeof(WCHAR);
            memcpy(nt_process->ProcessName.Buffer, file_part, name_len * sizeof(WCHAR));
            nt_process->ProcessName.Buffer[name_len] = 0;
        }
    }


    if (last_entry) last_entry->NextEntryOffset = 0;

    if (*len > size) ret = STATUS_INFO_LENGTH_MISMATCH;
    free( buffer );
    return ret;
}

/******************************************************************************
 *              NtQuerySystemInformation  (NTDLL.@)
 */
static NTSTATUS query_system_information( SYSTEM_INFORMATION_CLASS class,
                                          void *info, ULONG size, ULONG *ret_size )
{
    unsigned int ret = STATUS_SUCCESS;
    ULONG len = 0;

    TRACE( "(0x%08x,%p,0x%08x,%p)\n", class, info, size, ret_size );

    /* Diagnostic-only: log every info class queried, not just the ones the
     * cases below already call tuxblox_trace_record for, so a class this
     * tracer doesn't otherwise know about still shows up in the trace.
     *
     * Guarded on tuxblox_trace_enabled() rather than left to
     * tuxblox_trace_record()'s own early-out: the snprintf is an *argument*,
     * so it runs before that early-out ever gets a chance to, making every
     * caller pay a full varargs format even with tracing off -- on the one
     * choke point tuxblox_trace.c's own cached-state comment calls out by
     * name as the reason that caching exists. */
    if (tuxblox_trace_enabled())
    {
        char class_buf[32];

        snprintf( class_buf, sizeof(class_buf), "class=%u", class );
        tuxblox_trace_record( "NtQuerySystemInformation", class_buf );
    }

    {
        unsigned int forced_status, forced_len;

        if (diag_sysclass_forced( class, info, size, &forced_status, &forced_len ))
        {
            if (ret_size) *ret_size = forced_len;
            return forced_status;
        }
    }

    switch (class)
    {
    case SystemNativeBasicInformation:  /* 114 */
        if (!is_win64) return STATUS_INVALID_INFO_CLASS;
        /* fall through */
    case SystemBootEnvironmentInformation:  /* 90 */
    {
        /* The boot identifier has to be the same for every process in a boot
         * and different in the next one, so it cannot be a constant copied
         * from the reference machine: that would give every install running
         * this build the same one. Linux keeps exactly such a value. */
        struct
        {
            GUID  BootIdentifier;
            ULONG FirmwareType;
            ULONG pad;
            ULONGLONG BootFlags;
        } out;

        len = sizeof(out);
        if (size < len) { ret = STATUS_INFO_LENGTH_MISMATCH; break; }
        if (info)
        {
            char id[64];
            int fd;

            memset( &out, 0, sizeof(out) );
            if ((fd = open( "/proc/sys/kernel/random/boot_id", O_RDONLY )) != -1)
            {
                int n = read( fd, id, sizeof(id) - 1 );

                close( fd );
                if (n > 0)
                {
                    unsigned char *b = (unsigned char *)&out.BootIdentifier;
                    int i, k = 0;

                    id[n] = 0;
                    for (i = 0; id[i] && k < 16; i++)
                    {
                        if (id[i] == '-') continue;
                        if (!id[i + 1]) break;
                        b[k++] = (unsigned char)strtoul( (char[3]){ id[i], id[i + 1], 0 }, NULL, 16 );
                        i++;
                    }
                }
            }
            /* 1 is BIOS, 2 is UEFI, which is what the boot actually was */
            out.FirmwareType = access( "/sys/firmware/efi", F_OK ) ? 1 : 2;
            /* the four bytes after FirmwareType are left alone, as on the
             * reference machine, so this copies the two halves and not the
             * padding between them */
            memcpy( info, &out, FIELD_OFFSET( typeof(out), pad ) );
            memcpy( (char *)info + FIELD_OFFSET( typeof(out), BootFlags ),
                    &out.BootFlags, sizeof(out.BootFlags) );
        }
        break;
    }

    case SystemPageFileInformation:    /* 18 */
    case SystemPageFileInformationEx:  /* 144 */
    {
        /* Windows always has one, and reports its name inline behind the
         * record. There is no page file here, so the sizes are the ones a
         * default install of this much memory would have. */
        static const WCHAR nameW[] = {'\\','?','?','\\','C',':','\\','p','a','g','e',
                                      'f','i','l','e','.','s','y','s',0};
        const ULONG name_len = sizeof(nameW) - sizeof(WCHAR);
        const BOOL ex = (class == SystemPageFileInformationEx);
        const ULONG hdr = ex ? 40 : 32;
        SYSTEM_BASIC_INFORMATION sbi;
        ULONG total;

        len = hdr + name_len + sizeof(WCHAR);
        if (size < len)
        {
            /* The reference machine answers a first probe with the fixed
             * record size and only reports the whole thing, name included,
             * once the caller has come back with at least that much. */
            if (size < hdr) len = hdr;
            ret = STATUS_INFO_LENGTH_MISMATCH;
            break;
        }
        if (info)
        {
            UNICODE_STRING *name;
            WCHAR *text = (WCHAR *)((char *)info + hdr);
            ULONG *fields = info;

            virtual_get_system_info( &sbi, is_wow64() );
            total = sbi.MmNumberOfPhysicalPages / 2;
            memset( info, 0, hdr );
            fields[0] = 0;        /* NextEntryOffset: the only one */
            fields[1] = total;    /* TotalSize, in pages */
            fields[2] = 0;        /* TotalInUse */
            fields[3] = 0;        /* PeakUsage */
            name = (UNICODE_STRING *)(fields + 4);
            name->Length = name_len;
            name->MaximumLength = name_len + sizeof(WCHAR);
            name->Buffer = text;
            if (ex)
            {
                fields[8] = total;      /* MinimumSize */
                fields[9] = total * 3;  /* MaximumSize */
            }
            memcpy( text, nameW, sizeof(nameW) );
        }
        break;
    }

    case SystemBasicInformation:  /* 0 */
    {
        SYSTEM_BASIC_INFORMATION sbi;

        virtual_get_system_info( &sbi, FALSE );
        len = sizeof(sbi);
        if (size == len)
        {
            if (!info) ret = STATUS_ACCESS_VIOLATION;
            else memcpy( info, &sbi, len);
        }
        else ret = STATUS_INFO_LENGTH_MISMATCH;
        break;
    }

    case SystemCpuInformation:  /* 1 */
        if (size >= (len = sizeof(SYSTEM_CPU_INFORMATION)))
        {
            SYSTEM_CPU_INFORMATION cpu = get_cpuinfo();
            memcpy( info, &cpu, len );
        }
        else ret = STATUS_INFO_LENGTH_MISMATCH;
        break;

    case SystemPerformanceInformation:  /* 2 */
    {
        SYSTEM_PERFORMANCE_INFORMATION spi;
        static BOOL fixme_written = FALSE;

        get_performance_info( &spi );
        len = sizeof(spi);
        if (size >= len)
        {
            if (!info) ret = STATUS_ACCESS_VIOLATION;
            else memcpy( info, &spi, len);
        }
        else ret = STATUS_INFO_LENGTH_MISMATCH;
        if(!fixme_written) {
            FIXME("info_class SYSTEM_PERFORMANCE_INFORMATION\n");
            fixme_written = TRUE;
        }
        break;
    }

    case SystemTimeOfDayInformation:  /* 3 */
    {
        SYSTEM_TIMEOFDAY_INFORMATION sti = {{{ 0 }}};

        sti.BootTime.QuadPart = server_start_time;
        sti.TimeZoneBias.QuadPart = get_current_tz_bias();

        NtQuerySystemTime( &sti.SystemTime );

        if (size <= sizeof(sti))
        {
            len = size;
            if (!info) ret = STATUS_ACCESS_VIOLATION;
            else memcpy( info, &sti, size);
        }
        else ret = STATUS_INFO_LENGTH_MISMATCH;
        break;
    }

    case SystemProcessInformation:  /* 5 */
        ret = get_system_process_info( class, info, size, &len );
        break;

    case SystemProcessorPerformanceInformation:  /* 8 */
    {
        SYSTEM_PROCESSOR_PERFORMANCE_INFORMATION *sppi = NULL;
        unsigned int cpus = 0;
        int out_cpus = size / sizeof(SYSTEM_PROCESSOR_PERFORMANCE_INFORMATION);

        if (out_cpus == 0)
        {
            /* Windows says how much room to bring even when it is refusing the
             * call for not having enough: one entry per processor. Reporting
             * nothing needed for a buffer just refused as too small is not an
             * answer any kernel gives, and it is one of the three places this
             * build did it. */
            len = peb->NumberOfProcessors * sizeof(SYSTEM_PROCESSOR_PERFORMANCE_INFORMATION);
            ret = STATUS_INFO_LENGTH_MISMATCH;
            break;
        }
        if (!(sppi = calloc( out_cpus, sizeof(*sppi) )))
        {
            ret = STATUS_NO_MEMORY;
            break;
        }
        else
#ifdef __APPLE__
        {
            processor_cpu_load_info_data_t *pinfo;
            mach_msg_type_number_t info_count;
            host_name_port_t host = mach_host_self ();

            if (host_processor_info( host,
                                     PROCESSOR_CPU_LOAD_INFO,
                                     &cpus,
                                     (processor_info_array_t*)&pinfo,
                                     &info_count) == 0)
            {
                int i;
                cpus = min(cpus,out_cpus);
                for (i = 0; i < cpus; i++)
                {
                    sppi[i].IdleTime.QuadPart = (ULONGLONG)pinfo[i].cpu_ticks[CPU_STATE_IDLE] * 100000;
                    sppi[i].KernelTime.QuadPart = (ULONGLONG)pinfo[i].cpu_ticks[CPU_STATE_SYSTEM] * 100000 +
                                                  sppi[i].IdleTime.QuadPart;
                    sppi[i].UserTime.QuadPart = (ULONGLONG)pinfo[i].cpu_ticks[CPU_STATE_USER] * 100000;
                }
                vm_deallocate (mach_task_self (), (vm_address_t) pinfo, info_count * sizeof(natural_t));
            }

            mach_port_deallocate (mach_task_self (), host);
        }
#elif defined(linux)
        {
            FILE *cpuinfo = fopen("/proc/stat", "r");
            if (cpuinfo)
            {
                unsigned long clk_tck = sysconf(_SC_CLK_TCK);
                unsigned long usr,nice,sys,idle,remainder[8];
                int i, count, id;
                char name[32];
                char line[255];

                /* first line is combined usage */
                while (fgets(line,255,cpuinfo))
                {
                    count = sscanf(line, "%s %lu %lu %lu %lu %lu %lu %lu %lu %lu %lu %lu %lu",
                                   name, &usr, &nice, &sys, &idle,
                                   &remainder[0], &remainder[1], &remainder[2], &remainder[3],
                                   &remainder[4], &remainder[5], &remainder[6], &remainder[7]);

                    if (count < 5 || strncmp( name, "cpu", 3 )) break;
                    for (i = 0; i + 5 < count; ++i) sys += remainder[i];
                    sys += idle;
                    usr += nice;
                    id = atoi( name + 3 ) + 1;
                    if (id > out_cpus) break;
                    if (id > cpus) cpus = id;
                    sppi[id-1].IdleTime.QuadPart   = (ULONGLONG)idle * 10000000 / clk_tck;
                    sppi[id-1].KernelTime.QuadPart = (ULONGLONG)sys * 10000000 / clk_tck;
                    sppi[id-1].UserTime.QuadPart   = (ULONGLONG)usr * 10000000 / clk_tck;
                }
                fclose(cpuinfo);
            }
        }
#elif defined(__FreeBSD__) || defined (__FreeBSD_kernel__)
        {
            static int clockrate_name[] = { CTL_KERN, KERN_CLOCKRATE };
            size_t size = 0;
            struct clockinfo clockrate;
            int have_clockrate;
            long *ptimes;

            size = sizeof(clockrate);
            have_clockrate = !sysctl(clockrate_name, 2, &clockrate, &size, NULL, 0);
            size = out_cpus * CPUSTATES * sizeof(long);
            ptimes = malloc(size + 1);
            if (ptimes)
            {
                if (have_clockrate && (!sysctlbyname("kern.cp_times", ptimes, &size, NULL, 0) || errno == ENOMEM))
                {
                    for (cpus = 0; cpus < out_cpus; cpus++)
                    {
                        if (cpus * CPUSTATES * sizeof(long) >= size) break;
                        sppi[cpus].IdleTime.QuadPart = (ULONGLONG)ptimes[cpus*CPUSTATES + CP_IDLE] * 10000000 / clockrate.stathz;
                        sppi[cpus].KernelTime.QuadPart = (ULONGLONG)ptimes[cpus*CPUSTATES + CP_SYS] * 10000000 / clockrate.stathz +
                                                         sppi[cpus].IdleTime.QuadPart;
                        sppi[cpus].UserTime.QuadPart = (ULONGLONG)ptimes[cpus*CPUSTATES + CP_USER] * 10000000 / clockrate.stathz;
                    }
                }
                free(ptimes);
            }
        }
#endif
        if (cpus == 0)
        {
            static int i = 1;
            unsigned int n;
            cpus = min(peb->NumberOfProcessors, out_cpus);
            FIXME("stub info_class SYSTEM_PROCESSOR_PERFORMANCE_INFORMATION\n");
            /* many programs expect these values to change so fake change */
            for (n = 0; n < cpus; n++)
            {
                sppi[n].KernelTime.QuadPart = 1 * i;
                sppi[n].UserTime.QuadPart   = 2 * i;
                sppi[n].IdleTime.QuadPart   = 3 * i;
            }
            i++;
        }

        len = sizeof(*sppi) * cpus;
        if (size >= len)
        {
            if (!info) ret = STATUS_ACCESS_VIOLATION;
            else memcpy( info, sppi, len);
        }
        else ret = STATUS_INFO_LENGTH_MISMATCH;

        free( sppi );
        break;
    }

    case SystemModuleInformation:  /* 11 */
    {
        ULONG i;
        RTL_PROCESS_MODULES *smi = info;

        tuxblox_trace_record( "SystemModuleInformation", "" );

        len = offsetof( RTL_PROCESS_MODULES, Modules[ARRAY_SIZE(kernel_modules)] );
        if (len <= size)
        {
            memset( smi, 0, len );
            for (i = 0; i < ARRAY_SIZE(kernel_modules); i++)
                fill_module_info( &smi->Modules[i], i );
            smi->ModulesCount = i;
        }
        else ret = STATUS_INFO_LENGTH_MISMATCH;

        break;
    }

    case SystemHandleInformation:  /* 16 */
    {
        struct handle_info *handle_info;
        DWORD i, num_handles;

        if (size < sizeof(SYSTEM_HANDLE_INFORMATION))
        {
            len = sizeof(SYSTEM_HANDLE_INFORMATION);
            ret = STATUS_INFO_LENGTH_MISMATCH;
            break;
        }

        if (!info)
        {
            ret = STATUS_ACCESS_VIOLATION;
            break;
        }

        num_handles = (size - FIELD_OFFSET( SYSTEM_HANDLE_INFORMATION, Handle )) / sizeof(SYSTEM_HANDLE_ENTRY);
        if (!(handle_info = malloc( sizeof(*handle_info) * num_handles ))) return STATUS_NO_MEMORY;

        SERVER_START_REQ( get_system_handles )
        {
            wine_server_set_reply( req, handle_info, sizeof(*handle_info) * num_handles );
            if (!(ret = wine_server_call( req )))
            {
                SYSTEM_HANDLE_INFORMATION *shi = info;
                shi->Count = wine_server_reply_size( req ) / sizeof(*handle_info);
                len = FIELD_OFFSET( SYSTEM_HANDLE_INFORMATION, Handle[shi->Count] );
                for (i = 0; i < shi->Count; i++)
                {
                    memset( &shi->Handle[i], 0, sizeof(shi->Handle[i]) );
                    shi->Handle[i].OwnerPid     = handle_info[i].owner;
                    shi->Handle[i].HandleValue  = handle_info[i].handle;
                    shi->Handle[i].AccessMask   = handle_info[i].access;
                    shi->Handle[i].HandleFlags  = handle_info[i].attributes;
                    shi->Handle[i].ObjectType   = handle_info[i].type;
                    shi->Handle[i].ObjectPointer = wine_server_get_ptr( handle_info[i].object );
                }
            }
            else if (ret == STATUS_BUFFER_TOO_SMALL)
            {
                len = FIELD_OFFSET( SYSTEM_HANDLE_INFORMATION, Handle[reply->count] );
                ret = STATUS_INFO_LENGTH_MISMATCH;
            }
        }
        SERVER_END_REQ;

        free( handle_info );
        break;
    }

    case SystemFileCacheInformation:  /* 21 */
    {
        SYSTEM_CACHE_INFORMATION sci = { 0 };

        len = sizeof(sci);
        if (size >= len)
        {
            if (!info) ret = STATUS_ACCESS_VIOLATION;
            else memcpy( info, &sci, len);
        }
        else ret = STATUS_INFO_LENGTH_MISMATCH;
        FIXME("info_class SYSTEM_CACHE_INFORMATION\n");
        break;
    }

    case SystemInterruptInformation: /* 23 */
    {
        len = peb->NumberOfProcessors * sizeof(SYSTEM_INTERRUPT_INFORMATION);
        if (size >= len)
        {
            if (!info) ret = STATUS_ACCESS_VIOLATION;
            else get_random( info, len );
        }
        else ret = STATUS_INFO_LENGTH_MISMATCH;
        break;
    }

    case SystemTimeAdjustmentInformation:  /* 28 */
    {
        SYSTEM_TIME_ADJUSTMENT_QUERY query = { 156250, 156250, TRUE };

        len = sizeof(query);
        if (size == len)
        {
            if (!info) ret = STATUS_ACCESS_VIOLATION;
            else memcpy( info, &query, len );
        }
        else ret = STATUS_INFO_LENGTH_MISMATCH;
        break;
    }

    case SystemKernelDebuggerInformation:  /* 35 */
    {
        SYSTEM_KERNEL_DEBUGGER_INFORMATION skdi;

        skdi.DebuggerEnabled = FALSE;
        skdi.DebuggerNotPresent = TRUE;
        len = sizeof(skdi);
        if (size >= len)
        {
            if (!info) ret = STATUS_ACCESS_VIOLATION;
            else memcpy( info, &skdi, len);
        }
        else ret = STATUS_INFO_LENGTH_MISMATCH;
        break;
    }

    case SystemRegistryQuotaInformation:  /* 37 */
    {
        /* Something to do with the size of the registry             *
         * Since we don't have a size limitation, fake it            *
         * This is almost certainly wrong.                           *
         * This sets each of the three words in the struct to 32 MB, *
         * which is enough to make the IE 5 installer happy.         */
        SYSTEM_REGISTRY_QUOTA_INFORMATION srqi;

        srqi.RegistryQuotaAllowed = 0x2000000;
        srqi.RegistryQuotaUsed = 0x200000;
        srqi.Reserved1 = (void*)0x200000;
        len = sizeof(srqi);

        if (size >= len)
        {
            if (!info) ret = STATUS_ACCESS_VIOLATION;
            else
            {
                FIXME("SystemRegistryQuotaInformation: faking max registry size of 32 MB\n");
                memcpy( info, &srqi, len);
            }
        }
        else ret = STATUS_INFO_LENGTH_MISMATCH;
        break;
    }

    case SystemCurrentTimeZoneInformation:  /* 44 */
    {
        RTL_DYNAMIC_TIME_ZONE_INFORMATION tz;

        get_timezone_info( &tz );
        len = sizeof(RTL_TIME_ZONE_INFORMATION);
        if (size >= len)
        {
            if (!info) ret = STATUS_ACCESS_VIOLATION;
            else memcpy( info, &tz, len);
        }
        else ret = STATUS_INFO_LENGTH_MISMATCH;
        break;
    }

    case SystemExtendedProcessInformation:  /* 57 */
        ret = get_system_process_info( class, info, size, &len );
        break;

    case SystemRecommendedSharedDataAlignment:  /* 58 */
    {
        len = sizeof(DWORD);
        if (size >= len)
        {
            if (!info) ret = STATUS_ACCESS_VIOLATION;
            else
            {
#ifdef __arm__
                *((DWORD *)info) = 32;
#elif defined __aarch64__
                *((DWORD *)info) = 128;
#else
                *((DWORD *)info) = 64;
#endif
            }
        }
        else ret = STATUS_INFO_LENGTH_MISMATCH;
        break;
    }

    case SystemEmulationBasicInformation:  /* 62 */
    {
        SYSTEM_BASIC_INFORMATION sbi;

        virtual_get_system_info( &sbi, is_wow64() );
        len = sizeof(sbi);
        if (size == len)
        {
            if (!info) ret = STATUS_ACCESS_VIOLATION;
            else memcpy( info, &sbi, len);
        }
        else ret = STATUS_INFO_LENGTH_MISMATCH;
        break;
    }

    case SystemEmulationProcessorInformation:  /* 63 */
        if (size >= (len = sizeof(SYSTEM_CPU_INFORMATION)))
        {
            SYSTEM_CPU_INFORMATION cpu = get_cpuinfo();
            if (is_win64)
            {
                if (cpu.ProcessorArchitecture == PROCESSOR_ARCHITECTURE_AMD64)
                    cpu.ProcessorArchitecture = PROCESSOR_ARCHITECTURE_INTEL;
                else if (cpu.ProcessorArchitecture == PROCESSOR_ARCHITECTURE_ARM64)
                    cpu.ProcessorArchitecture = PROCESSOR_ARCHITECTURE_ARM;
            }
            memcpy(info, &cpu, len);
        }
        else ret = STATUS_INFO_LENGTH_MISMATCH;
        break;

    case SystemExtendedHandleInformation:  /* 64 */
    {
        struct handle_info *handle_info;
        DWORD i, num_handles;

        if (size < sizeof(SYSTEM_HANDLE_INFORMATION_EX))
        {
            len = sizeof(SYSTEM_HANDLE_INFORMATION_EX);
            ret = STATUS_INFO_LENGTH_MISMATCH;
            break;
        }

        if (!info)
        {
            ret = STATUS_ACCESS_VIOLATION;
            break;
        }

        num_handles = (size - FIELD_OFFSET( SYSTEM_HANDLE_INFORMATION_EX, Handles ))
                      / sizeof(SYSTEM_HANDLE_TABLE_ENTRY_INFO_EX);
        if (!(handle_info = malloc( sizeof(*handle_info) * num_handles ))) return STATUS_NO_MEMORY;

        SERVER_START_REQ( get_system_handles )
        {
            wine_server_set_reply( req, handle_info, sizeof(*handle_info) * num_handles );
            if (!(ret = wine_server_call( req )))
            {
                SYSTEM_HANDLE_INFORMATION_EX *shi = info;
                shi->NumberOfHandles = wine_server_reply_size( req ) / sizeof(*handle_info);
                len = FIELD_OFFSET( SYSTEM_HANDLE_INFORMATION_EX, Handles[shi->NumberOfHandles] );
                for (i = 0; i < shi->NumberOfHandles; i++)
                {
                    memset( &shi->Handles[i], 0, sizeof(shi->Handles[i]) );
                    shi->Handles[i].UniqueProcessId  = handle_info[i].owner;
                    shi->Handles[i].HandleValue      = handle_info[i].handle;
                    shi->Handles[i].GrantedAccess    = handle_info[i].access;
                    shi->Handles[i].HandleAttributes = handle_info[i].attributes;
                    shi->Handles[i].ObjectTypeIndex  = handle_info[i].type;
                    shi->Handles[i].Object           = wine_server_get_ptr( handle_info[i].object );
                }
            }
            else if (ret == STATUS_BUFFER_TOO_SMALL)
            {
                len = FIELD_OFFSET( SYSTEM_HANDLE_INFORMATION_EX, Handles[reply->count] );
                ret = STATUS_INFO_LENGTH_MISMATCH;
            }
        }
        SERVER_END_REQ;

        free( handle_info );
        break;
    }

    case SystemLogicalProcessorInformation:  /* 73 */
        pthread_once( &logical_proc_init_once, init_logical_proc_info );
        if (!logical_proc_info)
        {
            ret = STATUS_NOT_IMPLEMENTED;
            break;
        }
        len = logical_proc_info_len * sizeof(*logical_proc_info);
        if (size >= len)
        {
            if (!info) ret = STATUS_ACCESS_VIOLATION;
            else memcpy( info, logical_proc_info, len);
        }
        else ret = STATUS_INFO_LENGTH_MISMATCH;
        break;

    case SystemFirmwareTableInformation:  /* 76 */
    {
        SYSTEM_FIRMWARE_TABLE_INFORMATION *sfti = info;

        len = FIELD_OFFSET(SYSTEM_FIRMWARE_TABLE_INFORMATION, TableBuffer);
        if (size < len)
        {
            tuxblox_trace_record( "SystemFirmwareTableInformation", "short" );
            ret = STATUS_INFO_LENGTH_MISMATCH;
            break;
        }
        len = 0;

        if (tuxblox_trace_enabled())
        {
            /* which firmware table is being asked for is the whole content of
             * the question -- recording only that it was asked says nothing */
            char detail[64];
            snprintf( detail, sizeof(detail), "provider=%c%c%c%c table=%08x action=%d",
                      (char)(sfti->ProviderSignature >> 24), (char)(sfti->ProviderSignature >> 16),
                      (char)(sfti->ProviderSignature >> 8), (char)sfti->ProviderSignature,
                      (unsigned int)sfti->TableID, (int)sfti->Action );
            tuxblox_trace_record( "SystemFirmwareTableInformation", detail );
        }

        switch (sfti->Action)
        {
        case SystemFirmwareTable_Enumerate:
            ret = enum_firmware_info(sfti, size, &len);
            break;
        case SystemFirmwareTable_Get:
            ret = get_firmware_info(sfti, size, &len);
            break;
        default:
            ret = STATUS_NOT_IMPLEMENTED;
            FIXME("info_class SYSTEM_FIRMWARE_TABLE_INFORMATION action %d\n", sfti->Action);
        }
        break;
    }

    case SystemModuleInformationEx:  /* 77 */
    {
        RTL_PROCESS_MODULE_INFORMATION_EX *module_info = info;
        ULONG i;

        tuxblox_trace_record( "SystemModuleInformationEx", "" );

        /* The list ends with a real entry whose NextOffset is zero, not with an
         * extra stub after the last one. Writing a stub made the walk hand out
         * one more module than there are -- a blank one, and one more than the
         * count SystemModuleInformation reports for the same machine. */
        len = sizeof(*module_info) * ARRAY_SIZE(kernel_modules);
        if (len <= size)
        {
            memset( info, 0, len );
            for (i = 0; i < ARRAY_SIZE(kernel_modules); i++)
            {
                fill_module_info( &module_info[i].BaseInfo, i );
                module_info[i].NextOffset = (i + 1 < ARRAY_SIZE(kernel_modules)) ? sizeof(*module_info) : 0;
            }
        }
        else ret = STATUS_INFO_LENGTH_MISMATCH;

        break;
    }

    case SystemProcessorIdleCycleTimeInformation: /* 83 */
    {
        ULONG group_id = 0; /* FIXME: should probably be current CPU group id. */

        return NtQuerySystemInformationEx( class, &group_id, sizeof(group_id), info, size, ret_size );
    }

    case SystemProcessIdInformation: /* 88 */
    {
        SYSTEM_PROCESS_ID_INFORMATION *id = info;
        UNICODE_STRING *str = &id->ImageName;
        ULONG name_len = 0;
        void *buffer;

        len = sizeof(*id);
        if (ret_size) *ret_size = len;

        if (len > size)                ret = STATUS_INFO_LENGTH_MISMATCH;
        else if (id->ImageName.Length) ret = STATUS_INVALID_PARAMETER;
        else if (!id->ProcessId)       ret = STATUS_INVALID_CID;

        if (ret) return ret;

        buffer = malloc( str->MaximumLength );
        SERVER_START_REQ( get_process_image_name )
        {
            req->pid = id->ProcessId;
            wine_server_set_reply( req, buffer, str->MaximumLength );
            ret = wine_server_call( req );
            name_len = reply->len;
        }
        SERVER_END_REQ;

        if (ret == STATUS_BUFFER_TOO_SMALL) ret = STATUS_INFO_LENGTH_MISMATCH;
        if (!ret && name_len + sizeof(WCHAR) > str->MaximumLength) ret = STATUS_INFO_LENGTH_MISMATCH;
        if (!ret || ret == STATUS_INFO_LENGTH_MISMATCH) str->MaximumLength = name_len + sizeof(WCHAR);
        if (!ret)
        {
            str->Length = name_len;
            memcpy( str->Buffer, buffer, str->Length );
            str->Buffer[str->Length / sizeof(WCHAR)] = 0;
        }
        free( buffer );
        return ret;
    }

    case SystemDynamicTimeZoneInformation:  /* 102 */
    {
        RTL_DYNAMIC_TIME_ZONE_INFORMATION tz;

        get_timezone_info( &tz );
        len = sizeof(tz);
        if (size >= len)
        {
            if (!info) ret = STATUS_ACCESS_VIOLATION;
            else memcpy( info, &tz, len);
        }
        else ret = STATUS_INFO_LENGTH_MISMATCH;
        break;
    }

    case SystemCodeIntegrityInformation:  /* 103 */
    {
        SYSTEM_CODEINTEGRITY_INFORMATION *integrity_info = info;

        len = sizeof(SYSTEM_CODEINTEGRITY_INFORMATION);

        if (size < len)
        {
            ret = STATUS_INFO_LENGTH_MISMATCH;
            break;
        }
        /* The caller fills in Length itself and Windows checks it: measured on
         * Windows 11 25H2 with workspace/tests/infoprobe, an 8-byte buffer whose
         * Length field still held the caller's filler was refused with
         * STATUS_INFO_LENGTH_MISMATCH rather than answered. Only the size of the
         * buffer was being checked here, so any caller got an answer. */
        if (integrity_info->Length != len)
        {
            ret = STATUS_INFO_LENGTH_MISMATCH;
            break;
        }
        integrity_info->CodeIntegrityOptions = CODEINTEGRITY_OPTION_ENABLED;
        break;
    }

    case SystemProcessorBrandString:  /* 105 */
        if (!cpu_name[0]) return STATUS_NOT_SUPPORTED;
        if ((ULONG_PTR)info & 3) return STATUS_DATATYPE_MISALIGNMENT;
        len = sizeof(cpu_name);
        if (size >= len)
            memcpy( info, cpu_name, len );
        else
            ret = STATUS_INFO_LENGTH_MISMATCH;
        break;

    case SystemKernelDebuggerInformationEx:  /* 149 */
    {
        SYSTEM_KERNEL_DEBUGGER_INFORMATION_EX skdi;

        skdi.DebuggerAllowed = FALSE;
        skdi.DebuggerEnabled = FALSE;
        skdi.DebuggerPresent = FALSE;

        len = sizeof(skdi);
        if (size >= len)
        {
            if (!info) ret = STATUS_ACCESS_VIOLATION;
            else memcpy( info, &skdi, len );
        }
        else ret = STATUS_INFO_LENGTH_MISMATCH;
        break;
    }

    case SystemProcessorFeaturesInformation:  /* 154 */
        len = sizeof(SYSTEM_PROCESSOR_FEATURES_INFORMATION);
        if (size >= len)
        {
            SYSTEM_PROCESSOR_FEATURES_INFORMATION features = { .ProcessorFeatureBits = get_cpu_features() };
            memcpy( info, &features, len );
        }
        else ret = STATUS_INFO_LENGTH_MISMATCH;
        break;

    case SystemIsolatedUserModeInformation:  /* 165 */
    {
        /* Reports virtualisation-based security. None of it is running here,
         * which is also what an ordinary machine with it turned off reports. */
        struct
        {
            BYTE     flags;
            BYTE     flags2;
            BYTE     spare0[6];
            ULONGLONG spare1;
        } isolated = { 0 };

        C_ASSERT( sizeof(isolated) == 16 );
        len = sizeof(isolated);
        if (size >= len)
        {
            if (!info) ret = STATUS_ACCESS_VIOLATION;
            else memcpy( info, &isolated, len );
        }
        else ret = STATUS_INFO_LENGTH_MISMATCH;
        break;
    }

    case SystemPoolTagInformation:  /* 22 */
    {
        /* Kernel pool usage per tag. There is no kernel pool here, so report a
         * plausible set of the tags a Windows kernel and its inbox drivers use;
         * the exact set varies with the drivers loaded on any real machine.
         * Layout confirmed against Windows 10 22H2: a count and 40 bytes per
         * tag, so a single-entry buffer is 48. */
        static const char tags[][4] =
        {
            "Ntff","Ntfr","NtFs","NtFB","MmSt","MmCa","MmCm","MmDb","MmIn","Mm  ",
            "CM  ","CMap","CMkb","CMnb","CMsb","CMvi","CMdc","File","Filt","Thre",
            "Proc","Job ","Even","Sema","Muta","Time","Symb","Toke","Key ","SeSd",
            "SeTa","Obtb","ObDi","ObSq","ObNm","ObHt","Io  ","Irp ","IoNm","IoSL",
            "IoDa","Devi","Driv","Sect","Vad ","VadS","VadL","Pool","PsJb","PsTk",
            "PsWs","Ntfn","NDpp","NDnb","NDsi","Tcpc","TcpD","TcpT","Udpa","AfdB",
            "AfdC","AfdE","Wmip","WmiR","Ttfd","Gh05","Gla1","Uspc","UsQt","Usqm",
            "Ustm","Dxgk","DxgC","VidM","PciB","PcIe","USBp","Uhcd","HidP","Kbdc",
            "Moup","Ndis","NDpb","Srv ","SrvE","LSwi","LStr","Fatf","Udfs","Cdrm",
            "Vol ","Ftdc","RxCa","MRxS","SmSt","SmSb","Perf","Ppmp","Etwp","EtwB",
            "Wdf ","Wdfp","VfPd","Vrfy","Ipng","IpFw","Nsi ","Wfp ","Fwpm","Klbg"
        };
        struct pooltag
        {
            ULONG  tag;
            ULONG  paged_allocs;
            ULONG  paged_frees;
            SIZE_T paged_used;
            ULONG  nonpaged_allocs;
            ULONG  nonpaged_frees;
            SIZE_T nonpaged_used;
        };
        ULONG count = ARRAY_SIZE(tags), i;
        struct pooltag *entry;

        C_ASSERT( sizeof(struct pooltag) == (sizeof(SIZE_T) == 8 ? 40 : 28) );

        len = offsetof( struct { ULONG count; struct pooltag tags[1]; }, tags[count] );
        if (size < offsetof( struct { ULONG count; struct pooltag tags[1]; }, tags[1] ))
        {
            len = offsetof( struct { ULONG count; struct pooltag tags[1]; }, tags[1] );
            ret = STATUS_INFO_LENGTH_MISMATCH;
            break;
        }
        if (size < len)
        {
            ret = STATUS_INFO_LENGTH_MISMATCH;
            break;
        }

        memset( info, 0, len );
        *(ULONG *)info = count;
        entry = (struct pooltag *)((char *)info + offsetof( struct { ULONG count; struct pooltag tags[1]; }, tags[0] ));
        for (i = 0; i < count; i++)
        {
            /* deterministic, and in the range these counters normally sit in */
            ULONG n = (i + 1) * 977;

            memcpy( &entry[i].tag, tags[i], 4 );
            entry[i].paged_allocs    = n * 3;
            entry[i].paged_frees     = n * 3 - (n % 64);
            entry[i].paged_used      = (SIZE_T)(n % 64) * 176;
            entry[i].nonpaged_allocs = n;
            entry[i].nonpaged_frees  = n - (n % 32);
            entry[i].nonpaged_used   = (SIZE_T)(n % 32) * 224;
        }
        break;
    }

    case SystemBigPoolInformation:  /* 66 */
    {
        /* Large kernel pool allocations. There is no kernel pool here to
         * describe, and describing invented ones is worse than describing none:
         * every entry carries the address it was allocated at, and a caller that
         * looks at those addresses can tell they were made up. Report an empty
         * list. Layout confirmed against Windows 10 22H2: a count, then 24 bytes
         * per allocation, so an empty list is the count on its own. */
        struct bigpool_entry
        {
            void  *address;
            SIZE_T size;
            ULONG  tag;
            ULONG  pad;
        };

        C_ASSERT( sizeof(struct bigpool_entry) == (sizeof(void *) == 8 ? 24 : 16) );

        len = offsetof( struct { ULONG count; struct bigpool_entry e[1]; }, e[0] );
        if (size < len)
        {
            len = offsetof( struct { ULONG count; struct bigpool_entry e[1]; }, e[1] );
            ret = STATUS_INFO_LENGTH_MISMATCH;
            break;
        }
        memset( info, 0, len );
        break;
    }

    case SystemTrustedPlatformModuleInformation:  /* 162 */
        /* Windows refuses this one outright rather than calling it unknown. */
        ret = STATUS_ACCESS_DENIED;
        break;

    case SystemKernelDebuggerFlags:  /* 163 */
        len = sizeof(BYTE);
        if (size >= len) *(BYTE *)info = 0;  /* no kernel debugger */
        else ret = STATUS_INFO_LENGTH_MISMATCH;
        break;

    case SystemCodeIntegrityPolicyInformation:  /* 164 */
        len = 32;
        if (size >= len) memset( info, 0, len );
        else ret = STATUS_INFO_LENGTH_MISMATCH;
        break;

    case SystemCpuSetInformation:  /* 175 */
        return NtQuerySystemInformationEx(class, NULL, 0, info, size, ret_size);

    case SystemLeapSecondInformation:  /* 206 */
    {
        SYSTEM_LEAP_SECOND_INFORMATION *leap = info;

        len = sizeof(*leap);
        if (size >= len)
        {
            FIXME( "SystemLeapSecondInformation - stub\n" );
            leap->Enabled = TRUE;
            leap->Flags   = 0;
        }
        else ret = STATUS_INFO_LENGTH_MISMATCH;
        break;
    }

    case SystemProcessorFeaturesBitMapInformation:  /* 250 */
        len = sizeof(cpu_features_bitmap);
        if (size == len) memcpy( info, cpu_features_bitmap, len );
        else ret = STATUS_INFO_LENGTH_MISMATCH;
        break;

    /* Wine extensions */

    case SystemWineVersionInformation:  /* 1000 */
    {
        static const char version[] = PACKAGE_VERSION;
        struct utsname buf;
        char result_buf[64];

        uname( &buf );
        snprintf( info, size, "%s%c%s%c%s%c%s", version, 0, wine_build, 0, buf.sysname, 0, buf.release );
        len = strlen(version) + strlen(wine_build) + strlen(buf.sysname) + strlen(buf.release) + 4;
        if (size < len) ret = STATUS_INFO_LENGTH_MISMATCH;
        /* Diagnostic-only: this class only exists as a Wine extension (real
         * Windows returns STATUS_INVALID_INFO_CLASS for it, the default case
         * below), so a caller reaching this point at all -- regardless of
         * whether the buffer was actually big enough -- has already learned
         * "this is Wine" from the status code alone. Log size/len/ret so a
         * trace capture can tell success from a too-small-buffer probe. */
        snprintf( result_buf, sizeof(result_buf), "size=%u len=%u ret=0x%x", (unsigned)size, (unsigned)len, ret );
        tuxblox_trace_record( "SystemWineVersionInformation", result_buf );
        break;
    }

    case SystemRootSiloInformation:  /* 174 */
        /* The only system class measured to refuse a short buffer with
         * STATUS_BUFFER_TOO_SMALL and report a length of zero rather than the
         * size it wants -- which is why it cannot be described by the table
         * below, whose one length field is both. Twenty-four bytes as read
         * from the reference machine with workspace/tests/infoprobe.exe. */
        if (size < sizeof(sys_174_data))
        {
            if (ret_size) *ret_size = 0;
            return STATUS_BUFFER_TOO_SMALL;
        }
        memcpy( info, sys_174_data, sizeof(sys_174_data) );
        len = sizeof(sys_174_data);
        break;

    case SystemHypervisorSharedPageInformation:
        /* Where QueryPerformanceCounter's scaling is published. The reference
         * machine answers with a pointer to a page its kernel mapped; this
         * reports our own, because handing out that machine's address left the
         * caller a pointer to nothing -- a state no Windows is ever in.
         *
         * The refusal below is reasoned, not measured: this machine has the
         * page, so what one without it answers was never captured. */
        if (!hypervisor_shared_data) return STATUS_NOT_SUPPORTED;
        if (size < sizeof(void *))
        {
            if (ret_size) *ret_size = sizeof(void *);
            return STATUS_INFO_LENGTH_MISMATCH;
        }
        *(void **)info = hypervisor_shared_data;
        len = sizeof(void *);
        break;

    default:
        const struct known_class *entry;

        entry = find_known_class( known_system_classes, ARRAY_SIZE(known_system_classes), class );
        if (entry)
        {
            if (entry->len == NO_LENGTH) return entry->status;
            /* A class recorded with a length of zero reports a zero length and
             * still refuses -- it is not asking for a zero-byte buffer, so the
             * branch below must not treat every size as big enough for it.
             * Without this guard five classes the reference machine answers
             * with a length mismatch came back "not implemented" instead. */
            if (entry->len && entry->status == STATUS_INFO_LENGTH_MISMATCH && size >= entry->len)
            {
                /* Reporting the size a caller needs and then refusing that
                 * exact size is an answer no real system gives, so a class we
                 * measured is answered rather than only sized. */
                if (entry->full_status)
                {
                    len = entry->len;
                    ret = entry->full_status;
                    break;
                }
                if (!entry->data) return STATUS_NOT_IMPLEMENTED;
                if (entry->data_len) memcpy( info, entry->data, entry->data_len );
                len = entry->len;
                ret = STATUS_SUCCESS;
                break;
            }
            len = entry->len;
            ret = entry->status;
            break;
        }

	FIXME( "(0x%08x,%p,0x%08x,%p) stub\n", class, info, size, ret_size );

        /* Several Information Classes are not implemented on Windows and return 2 different values
         * STATUS_NOT_IMPLEMENTED or STATUS_INVALID_INFO_CLASS
         * in 95% of the cases it's STATUS_INVALID_INFO_CLASS, so use this as the default
         *
         * Returned rather than broken out of: a class the system does not have
         * leaves the caller's returned length alone on the reference machine,
         * where falling through to the assignment below wrote a zero into it.
         */
        return STATUS_INVALID_INFO_CLASS;
    }

    if (ret_size) *ret_size = len;
    return ret;
}

NTSTATUS WINAPI NtQuerySystemInformation( SYSTEM_INFORMATION_CLASS class,
                                          void *info, ULONG size, ULONG *ret_size )
{
    ULONG reported = 0;
    NTSTATUS status = query_system_information( class, info, size, ret_size ? ret_size : &reported );

    if (ret_size) reported = *ret_size;
    tuxblox_diag_class( "sys", class, size, reported, status );
    return status;
}


/******************************************************************************
 *              NtQuerySystemInformationEx  (NTDLL.@)
 */
NTSTATUS WINAPI NtQuerySystemInformationEx( SYSTEM_INFORMATION_CLASS class,
                                            void *query, ULONG query_len,
                                            void *info, ULONG size, ULONG *ret_size )
{
    ULONG len = 0;
    unsigned int ret = STATUS_NOT_IMPLEMENTED;

    TRACE( "(0x%08x,%p,%u,%p,%u,%p) stub\n", class, query, query_len, info, size, ret_size );

    pthread_once( &logical_proc_init_once, init_logical_proc_info );

    switch (class)
    {
    case SystemProcessorIdleCycleTimeInformation:
        len = peb->NumberOfProcessors * sizeof(ULONG64);
        if (!query || query_len < sizeof(USHORT) || *(USHORT *)query) return STATUS_INVALID_PARAMETER;
        if (size < len)
        {
            ret = STATUS_BUFFER_TOO_SMALL;
            break;
        }
        get_cpu_idle_cycle_times( info );
        ret = STATUS_SUCCESS;
        break;

    case SystemLogicalProcessorInformationEx:
    {
        SYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX *p;
        DWORD relation;

        if (!query || query_len < sizeof(DWORD))
        {
            ret = STATUS_INVALID_PARAMETER;
            break;
        }
        if (!logical_proc_info_ex)
        {
            ret = STATUS_NOT_IMPLEMENTED;
            break;
        }

        relation = *(DWORD *)query;
        len = 0;
        p = logical_proc_info_ex;
        while ((char *)p != (char *)logical_proc_info_ex + logical_proc_info_ex_size)
        {
            if (relation == RelationAll || p->Relationship == relation)
            {
                if (len + p->Size <= size)
                    memcpy( (char *)info + len, p, p->Size );
                len += p->Size;
            }
            p = (SYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX *)((char *)p + p->Size);
        }
        ret = size >= len ? STATUS_SUCCESS : STATUS_INFO_LENGTH_MISMATCH;
        break;
    }

    case SystemCpuSetInformation:
    {
        unsigned int cpu_count = peb->NumberOfProcessors;
        PROCESS_BASIC_INFORMATION pbi;
        HANDLE process;

        if (!query || query_len < sizeof(HANDLE))
            return STATUS_INVALID_PARAMETER;

        process = *(HANDLE *)query;
        if (process && (ret = NtQueryInformationProcess(process, ProcessBasicInformation, &pbi, sizeof(pbi), NULL)))
            return ret;

        if (size < (len = cpu_count * sizeof(SYSTEM_CPU_SET_INFORMATION)))
        {
            ret = STATUS_BUFFER_TOO_SMALL;
            break;
        }
        if (!info)
            return STATUS_ACCESS_VIOLATION;

        if ((ret = create_cpuset_info(info)))
            return ret;
        break;
    }

    case SystemSupportedProcessorArchitectures:
    {
        SYSTEM_SUPPORTED_PROCESSOR_ARCHITECTURES_INFORMATION *machines = info;
        HANDLE process;
        ULONG i;
        USHORT machine = 0;

        if (!query || query_len < sizeof(HANDLE)) return STATUS_INVALID_PARAMETER;
        process = *(HANDLE *)query;
        if (process)
        {
            SERVER_START_REQ( get_process_info )
            {
                req->handle = wine_server_obj_handle( process );
                if (!(ret = wine_server_call( req ))) machine = reply->machine;
            }
            SERVER_END_REQ;
            if (ret) return ret;
        }

        len = (supported_machines_count + 1) * sizeof(*machines);
        if (size < len)
        {
            ret = STATUS_BUFFER_TOO_SMALL;
            break;
        }
        memset( machines, 0, len );

        /* native machine */
        machines[0].Machine = supported_machines[0];
        machines[0].UserMode = 1;
        machines[0].KernelMode = 1;
        machines[0].Native = 1;
        machines[0].Process = (supported_machines[0] == machine || is_machine_64bit( machine ));
        machines[0].WoW64Container = 0;
        machines[0].ReservedZero0 = 0;
        /* wow64 machines */
        for (i = 1; i < supported_machines_count; i++)
        {
            machines[i].Machine = supported_machines[i];
            machines[i].UserMode = 1;
            machines[i].Process = supported_machines[i] == machine;
            machines[i].WoW64Container = 1;
        }
        ret = STATUS_SUCCESS;
        break;
    }

    default:
        FIXME( "(0x%08x,%p,%u,%p,%u,%p) stub\n", class, query, query_len, info, size, ret_size );
        break;
    }
    if (ret_size) *ret_size = len;
    return ret;
}


/******************************************************************************
 *              NtSetSystemInformation  (NTDLL.@)
 */
NTSTATUS WINAPI NtSetSystemInformation( SYSTEM_INFORMATION_CLASS class, void *info, ULONG length )
{
    FIXME( "(0x%08x,%p,0x%08x) stub\n", class, info, length );
    return STATUS_SUCCESS;
}


/******************************************************************************
 *              NtQuerySystemEnvironmentValue  (NTDLL.@)
 */
NTSTATUS WINAPI NtQuerySystemEnvironmentValue( UNICODE_STRING *name, WCHAR *buffer, ULONG length,
                                               ULONG *retlen )
{
    FIXME( "(%s, %p, %u, %p), stub\n", debugstr_us(name), buffer, length, retlen );
    return STATUS_NOT_IMPLEMENTED;
}


/******************************************************************************
 *              NtQuerySystemEnvironmentValueEx  (NTDLL.@)
 */
NTSTATUS WINAPI NtQuerySystemEnvironmentValueEx( UNICODE_STRING *name, GUID *vendor, void *buffer,
                                                 ULONG *retlen, ULONG *attrib )
{
    FIXME( "(%s, %s, %p, %p, %p), stub\n", debugstr_us(name),
           debugstr_guid(vendor), buffer, retlen, attrib );
    return STATUS_NOT_IMPLEMENTED;
}


/******************************************************************************
 *              NtSystemDebugControl  (NTDLL.@)
 */
NTSTATUS WINAPI NtSystemDebugControl( SYSDBG_COMMAND command, void *in_buff, ULONG in_len,
                                      void *out_buff, ULONG out_len, ULONG *retlen )
{
    FIXME( "(%d, %p, %d, %p, %d, %p), stub\n",
           command, in_buff, in_len, out_buff, out_len, retlen );

    return STATUS_DEBUGGER_INACTIVE;
}


/******************************************************************************
 *              NtShutdownSystem  (NTDLL.@)
 */
NTSTATUS WINAPI NtShutdownSystem( SHUTDOWN_ACTION action )
{
    FIXME( "%d\n", action );
    return STATUS_SUCCESS;
}


#ifdef linux

/* Fallback using /proc/cpuinfo for Linux systems without cpufreq. For
 * most distributions on recent enough hardware, this is only likely to
 * happen while running in virtualized environments such as QEMU. */
static ULONG mhz_from_cpuinfo(void)
{
    char line[512];
    char *s, *value;
    double cmz = 0;
    FILE *f = fopen("/proc/cpuinfo", "r");
    if(f)
    {
        while (fgets(line, sizeof(line), f) != NULL)
        {
            if (!(value = strchr(line,':'))) continue;
            s = value - 1;
            while ((s >= line) && (*s == ' ' || *s == '\t')) s--;
            s[1] = 0;
            value++;
            if (!strcmp( line, "cpu MHz" ))
            {
                sscanf(value, " %lf", &cmz);
                break;
            }
        }
        fclose( f );
    }
    return cmz;
}

static const char * get_sys_str(const char *dirname, const char *basename, char *s)
{
    char path[64];
    FILE *f;
    const char *ret = NULL;

    if (snprintf(path, sizeof(path), "%s/%s", dirname, basename) >= sizeof(path)) return NULL;
    if ((f = fopen(path, "r")))
    {
        if (fgets(s, 16, f)) ret = s;
        fclose(f);
    }
    return ret;
}

static int get_sys_int(const char *dirname, const char *basename)
{
    char s[16];
    return get_sys_str(dirname, basename, s) ? atoi(s) : 0;
}

static NTSTATUS fill_battery_state( SYSTEM_BATTERY_STATE *bs )
{
    DIR *d = opendir("/sys/class/power_supply");
    struct dirent *de;
    char s[16], path[64];
    BOOL found_ac = FALSE;
    LONG64 voltage; /* microvolts */

    bs->AcOnLine = TRUE;
    if (!d) return STATUS_SUCCESS;

    while ((de = readdir(d)))
    {
        if (strcmp(de->d_name, ".") == 0 || strcmp(de->d_name, "..") == 0) continue;
        if (snprintf(path, sizeof(path), "/sys/class/power_supply/%s", de->d_name) >= sizeof(path)) continue;
        if (get_sys_str(path, "scope", s) && strcmp(s, "Device\n") == 0) continue;
        if (!get_sys_str(path, "type", s)) continue;

        if (strcmp(s, "Mains\n") == 0)
        {
            if (!get_sys_str(path, "online", s)) continue;
            if (found_ac)
            {
                FIXME("Multiple mains found, only reporting on the first\n");
            }
            else
            {
                bs->AcOnLine = atoi(s);
                found_ac = TRUE;
            }
        }
        else if (strcmp(s, "Battery\n") == 0)
        {
            if (!get_sys_str(path, "status", s)) continue;
            if (bs->BatteryPresent)
            {
                FIXME("Multiple batteries found, only reporting on the first\n");
            }
            else
            {
                bs->Charging = (strcmp(s, "Charging\n") == 0);
                bs->Discharging = (strcmp(s, "Discharging\n") == 0);
                bs->BatteryPresent = TRUE;
                voltage = get_sys_int(path, "voltage_now");
                bs->MaxCapacity = get_sys_int(path, "charge_full") * voltage / 1e9;
                bs->RemainingCapacity = get_sys_int(path, "charge_now") * voltage / 1e9;
                bs->Rate = -get_sys_int(path, "current_now") * voltage / 1e9;
                if (!bs->Charging && (LONG)bs->Rate < 0)
                    bs->EstimatedTime = 3600 * bs->RemainingCapacity / -(LONG)bs->Rate;
                else
                    bs->EstimatedTime = ~0u;
            }
        }
    }

    closedir(d);
    return STATUS_SUCCESS;
}

#elif defined(__APPLE__)

static NTSTATUS fill_battery_state( SYSTEM_BATTERY_STATE *bs )
{
    CFTypeRef blob = IOPSCopyPowerSourcesInfo();
    CFArrayRef sources = IOPSCopyPowerSourcesList( blob );
    CFIndex count, i;
    CFDictionaryRef source = NULL;
    CFTypeRef prop;
    Boolean is_charging, is_internal, is_present;
    int32_t value, voltage;

    if (!sources)
    {
        if (blob) CFRelease( blob );
        return STATUS_ACCESS_DENIED;
    }

    count = CFArrayGetCount( sources );

    for (i = 0; i < count; i++)
    {
        source = IOPSGetPowerSourceDescription( blob, CFArrayGetValueAtIndex( sources, i ) );

        if (!source)
            continue;

        prop = CFDictionaryGetValue( source, CFSTR(kIOPSTransportTypeKey) );
        is_internal = !CFStringCompare( prop, CFSTR(kIOPSInternalType), 0 );

        prop = CFDictionaryGetValue( source, CFSTR(kIOPSIsPresentKey) );
        is_present = CFBooleanGetValue( prop );

        if (is_internal && is_present)
            break;
    }

    CFRelease( blob );

    if (!source)
    {
        /* Just assume we're on AC with no internal power source. */
        bs->AcOnLine = TRUE;
        CFRelease( sources );
        return STATUS_SUCCESS;
    }

    bs->BatteryPresent = TRUE;

    prop = CFDictionaryGetValue( source, CFSTR(kIOPSIsChargingKey) );
    is_charging = CFBooleanGetValue( prop );

    prop = CFDictionaryGetValue( source, CFSTR(kIOPSPowerSourceStateKey) );

    if (!CFStringCompare( prop, CFSTR(kIOPSACPowerValue), 0 ))
    {
        bs->AcOnLine = TRUE;
        if (is_charging)
            bs->Charging = TRUE;
    }
    else
        bs->Discharging = TRUE;

    /* We'll need the voltage to be able to interpret the other values. */
    prop = CFDictionaryGetValue( source, CFSTR(kIOPSVoltageKey) );
    if (prop)
        CFNumberGetValue( prop, kCFNumberIntType, &voltage );
    else
        /* kIOPSVoltageKey is optional and might not be populated.
         * Assume 11.4 V then, which is a common value for Apple laptops. */
        voltage = 11400;

    prop = CFDictionaryGetValue( source, CFSTR(kIOPSMaxCapacityKey) );
    CFNumberGetValue( prop, kCFNumberIntType, &value );
    bs->MaxCapacity = value * voltage;
    /* Apple uses "estimated time < 10:00" and "22%" for these, but we'll follow
     * Windows for now (5% and 33%). */
    bs->DefaultAlert1 = bs->MaxCapacity / 20;
    bs->DefaultAlert2 = bs->MaxCapacity / 3;

    prop = CFDictionaryGetValue( source, CFSTR(kIOPSCurrentCapacityKey) );
    CFNumberGetValue( prop, kCFNumberIntType, &value );
    bs->RemainingCapacity = value * voltage;

    prop = CFDictionaryGetValue( source, CFSTR(kIOPSCurrentKey) );
    if (prop)
        CFNumberGetValue( prop, kCFNumberIntType, &value );
    else
        /* kIOPSCurrentKey is optional and might not be populated. */
        value = 0;

    bs->Rate = value * voltage / 1000;

    prop = CFDictionaryGetValue( source, CFSTR(kIOPSTimeToEmptyKey) );
    if (prop)
    {
        CFNumberGetValue( prop, kCFNumberIntType, &value );
        if (value > 0)
            /*  A value of -1 indicates "Still Calculating the Time",
             * otherwise estimated minutes left on the battery. */
            bs->EstimatedTime = value * 60;
    }

    CFRelease( sources );
    return STATUS_SUCCESS;
}

#elif defined(__FreeBSD__)

#include <dev/acpica/acpiio.h>

static NTSTATUS fill_battery_state( SYSTEM_BATTERY_STATE *bs )
{
    size_t len;
    int state = 0;
    int rate_mW = 0;
    int time_mins = -1;
    int life_percent = 0;

    bs->BatteryPresent = TRUE;
    len = sizeof(state);
    bs->BatteryPresent &= !sysctlbyname("hw.acpi.battery.state", &state, &len, NULL, 0);
    len = sizeof(rate_mW);
    bs->BatteryPresent &= !sysctlbyname("hw.acpi.battery.rate", &rate_mW, &len, NULL, 0);
    len = sizeof(time_mins);
    bs->BatteryPresent &= !sysctlbyname("hw.acpi.battery.time", &time_mins, &len, NULL, 0);
    len = sizeof(life_percent);
    bs->BatteryPresent &= !sysctlbyname("hw.acpi.battery.life", &life_percent, &len, NULL, 0);

    if (bs->BatteryPresent)
    {
        bs->AcOnLine = (time_mins == -1);
        bs->Charging = state & ACPI_BATT_STAT_CHARGING;
        bs->Discharging = state & ACPI_BATT_STAT_DISCHARG;

        bs->Rate = (rate_mW >= 0 ? -rate_mW : 0);
        if (time_mins >= 0 && life_percent > 0)
        {
            bs->EstimatedTime = 60 * time_mins;
            bs->RemainingCapacity = bs->EstimatedTime * rate_mW / 3600;
            bs->MaxCapacity = bs->RemainingCapacity * 100 / life_percent;
        }
        else
        {
            bs->EstimatedTime = ~0u;
            bs->RemainingCapacity = life_percent;
            bs->MaxCapacity = 100;
        }
    }
    return STATUS_SUCCESS;
}

#else

static NTSTATUS fill_battery_state( SYSTEM_BATTERY_STATE *bs )
{
	FIXME("SystemBatteryState not implemented on this platform\n");
	return STATUS_NOT_IMPLEMENTED;
}

#endif

/******************************************************************************
 *              NtPowerInformation  (NTDLL.@)
 */
NTSTATUS WINAPI NtPowerInformation( POWER_INFORMATION_LEVEL level, void *input, ULONG in_size,
                                    void *output, ULONG out_size )
{
    TRACE( "(%d,%p,%d,%p,%d)\n", level, input, in_size, output, out_size );
    switch (level)
    {
    case SystemPowerCapabilities:
    {
        PSYSTEM_POWER_CAPABILITIES PowerCaps = output;
        FIXME("semi-stub: SystemPowerCapabilities\n");
        if (out_size < sizeof(SYSTEM_POWER_CAPABILITIES)) return STATUS_BUFFER_TOO_SMALL;
        /* FIXME: These values are based off a native XP desktop, should probably use APM/ACPI to get the 'real' values */
        PowerCaps->PowerButtonPresent = TRUE;
        PowerCaps->SleepButtonPresent = FALSE;
        PowerCaps->LidPresent = FALSE;
        PowerCaps->SystemS1 = TRUE;
        PowerCaps->SystemS2 = FALSE;
        PowerCaps->SystemS3 = FALSE;
        PowerCaps->SystemS4 = TRUE;
        PowerCaps->SystemS5 = TRUE;
        PowerCaps->HiberFilePresent = TRUE;
        PowerCaps->FullWake = TRUE;
        PowerCaps->VideoDimPresent = FALSE;
        PowerCaps->ApmPresent = FALSE;
        PowerCaps->UpsPresent = FALSE;
        PowerCaps->ThermalControl = FALSE;
        PowerCaps->ProcessorThrottle = FALSE;
        PowerCaps->ProcessorMinThrottle = 100;
        PowerCaps->ProcessorMaxThrottle = 100;
        PowerCaps->DiskSpinDown = TRUE;
        PowerCaps->SystemBatteriesPresent = FALSE;
        PowerCaps->BatteriesAreShortTerm = FALSE;
        PowerCaps->BatteryScale[0].Granularity = 0;
        PowerCaps->BatteryScale[0].Capacity = 0;
        PowerCaps->BatteryScale[1].Granularity = 0;
        PowerCaps->BatteryScale[1].Capacity = 0;
        PowerCaps->BatteryScale[2].Granularity = 0;
        PowerCaps->BatteryScale[2].Capacity = 0;
        PowerCaps->AcOnLineWake = PowerSystemUnspecified;
        PowerCaps->SoftLidWake = PowerSystemUnspecified;
        PowerCaps->RtcWake = PowerSystemSleeping1;
        PowerCaps->MinDeviceWakeState = PowerSystemUnspecified;
        PowerCaps->DefaultLowLatencyWake = PowerSystemUnspecified;
        return STATUS_SUCCESS;
    }

    case SystemBatteryState:
    {
        if (out_size < sizeof(SYSTEM_BATTERY_STATE)) return STATUS_BUFFER_TOO_SMALL;
        memset(output, 0, sizeof(SYSTEM_BATTERY_STATE));
        return fill_battery_state(output);
    }

    case SystemExecutionState:
    {
        ULONG *state = output;
        WARN("semi-stub: SystemExecutionState\n"); /* Needed for .NET Framework, but using a FIXME is really noisy. */
        if (input != NULL) return STATUS_INVALID_PARAMETER;
        /* FIXME: The actual state should be the value set by SetThreadExecutionState which is not currently implemented. */
        *state = ES_USER_PRESENT;
        return STATUS_SUCCESS;
    }

    case ProcessorInformation:
    {
        const int cannedMHz = 1000; /* We fake a 1GHz processor if we can't conjure up real values */
        PROCESSOR_POWER_INFORMATION* cpu_power = output;
        int i, out_cpus;

        if ((output == NULL) || (out_size == 0)) return STATUS_INVALID_PARAMETER;
        out_cpus = peb->NumberOfProcessors;
        if ((out_size / sizeof(PROCESSOR_POWER_INFORMATION)) < out_cpus) return STATUS_BUFFER_TOO_SMALL;
#if defined(linux)
        {
            unsigned int val;
            char filename[128];
            FILE* f;

            for(i = 0; i < out_cpus; i++) {
                snprintf(filename, sizeof(filename), "/sys/devices/system/cpu/cpu%d/cpufreq/cpuinfo_max_freq", i);
                f = fopen(filename, "r");
                if (f && (fscanf(f, "%u", &val) == 1)) {
                    cpu_power[i].MaxMhz = val / 1000;
                    fclose(f);
                    cpu_power[i].CurrentMhz = cpu_power[i].MaxMhz;
                }
                else {
                    if(i == 0) {
                        cpu_power[0].CurrentMhz = mhz_from_cpuinfo();
                        if(cpu_power[0].CurrentMhz == 0)
                            cpu_power[0].CurrentMhz = cannedMHz;
                    }
                    else
                        cpu_power[i].CurrentMhz = cpu_power[0].CurrentMhz;
                    cpu_power[i].MaxMhz = cpu_power[i].CurrentMhz;
                    if(f) fclose(f);
                }

                snprintf(filename, sizeof(filename), "/sys/devices/system/cpu/cpu%d/cpufreq/scaling_max_freq", i);
                f = fopen(filename, "r");
                if(f && (fscanf(f, "%u", &val) == 1)) {
                    cpu_power[i].MhzLimit = val / 1000;
                    fclose(f);
                }
                else
                {
                    cpu_power[i].MhzLimit = cpu_power[i].MaxMhz;
                    if(f) fclose(f);
                }

                cpu_power[i].Number = i;
                cpu_power[i].MaxIdleState = 0;     /* FIXME */
                cpu_power[i].CurrentIdleState = 0; /* FIXME */
            }
        }
#elif defined(__FreeBSD__) || defined (__FreeBSD_kernel__) || defined(__DragonFly__)
        {
            int num;
            size_t valSize = sizeof(num);
            if (sysctlbyname("hw.clockrate", &num, &valSize, NULL, 0))
                num = cannedMHz;
            for(i = 0; i < out_cpus; i++) {
                cpu_power[i].CurrentMhz = num;
                cpu_power[i].MaxMhz = num;
                cpu_power[i].MhzLimit = num;
                cpu_power[i].Number = i;
                cpu_power[i].MaxIdleState = 0;     /* FIXME */
                cpu_power[i].CurrentIdleState = 0; /* FIXME */
            }
        }
#elif defined (__APPLE__)
        {
            size_t valSize;
            unsigned long long currentMhz;
            unsigned long long maxMhz;

            valSize = sizeof(currentMhz);
            if (!sysctlbyname("hw.cpufrequency", &currentMhz, &valSize, NULL, 0))
                currentMhz /= 1000000;
            else
                currentMhz = cannedMHz;

            valSize = sizeof(maxMhz);
            if (!sysctlbyname("hw.cpufrequency_max", &maxMhz, &valSize, NULL, 0))
                maxMhz /= 1000000;
            else
                maxMhz = currentMhz;

            for(i = 0; i < out_cpus; i++) {
                cpu_power[i].CurrentMhz = currentMhz;
                cpu_power[i].MaxMhz = maxMhz;
                cpu_power[i].MhzLimit = maxMhz;
                cpu_power[i].Number = i;
                cpu_power[i].MaxIdleState = 0;     /* FIXME */
                cpu_power[i].CurrentIdleState = 0; /* FIXME */
            }
        }
#else
        for(i = 0; i < out_cpus; i++) {
            cpu_power[i].CurrentMhz = cannedMHz;
            cpu_power[i].MaxMhz = cannedMHz;
            cpu_power[i].MhzLimit = cannedMHz;
            cpu_power[i].Number = i;
            cpu_power[i].MaxIdleState = 0; /* FIXME */
            cpu_power[i].CurrentIdleState = 0; /* FIXME */
        }
        WARN("Unable to detect CPU MHz for this platform. Reporting %d MHz.\n", cannedMHz);
#endif
        for(i = 0; i < out_cpus; i++) {
            TRACE("cpu_power[%d] = %u %u %u %u %u %u\n", i, cpu_power[i].Number,
                  cpu_power[i].MaxMhz, cpu_power[i].CurrentMhz, cpu_power[i].MhzLimit,
                  cpu_power[i].MaxIdleState, cpu_power[i].CurrentIdleState);
        }
        return STATUS_SUCCESS;
    }

    default:
        /* FIXME: Needed by .NET Framework */
        WARN( "Unimplemented NtPowerInformation action: %d\n", level );
        return STATUS_NOT_IMPLEMENTED;
    }
}


/******************************************************************************
 *              NtLoadDriver  (NTDLL.@)
 */
NTSTATUS WINAPI NtLoadDriver( const UNICODE_STRING *name )
{
    FIXME( "(%s), stub!\n", debugstr_us(name) );
    return STATUS_NOT_IMPLEMENTED;
}


/******************************************************************************
 *              NtUnloadDriver  (NTDLL.@)
 */
NTSTATUS WINAPI NtUnloadDriver( const UNICODE_STRING *name )
{
    FIXME( "(%s), stub!\n", debugstr_us(name) );
    return STATUS_NOT_IMPLEMENTED;
}


/******************************************************************************
 *              NtDisplayString  (NTDLL.@)
 */
NTSTATUS WINAPI NtDisplayString( UNICODE_STRING *string )
{
    ERR( "%s\n", debugstr_us(string) );
    return STATUS_SUCCESS;
}


/******************************************************************************
 *              NtRaiseHardError  (NTDLL.@)
 */
/* Wine has nowhere to put a message box, so a program that reports a problem
 * this way disappears without the user ever being told why. Hand the text to
 * the desktop instead, which is the nearest thing we have to showing it. */
/* Whether the notifier can put buttons on a notification. notify-send grew
 * -A/--action in libnotify 0.8; an older one treats it as a bad option and
 * shows nothing at all, so ask before using it rather than losing the message
 * entirely on those systems. Both KDE and GNOME render the button. */
static BOOL notifier_has_actions(void)
{
    static int cached = -1;
    int fds[2], st;
    pid_t pid;
    char buf[4096];
    ssize_t n, total = 0;

    if (cached != -1) return cached;
    cached = 0;
    if (pipe( fds ) == -1) return cached;

    if (!(pid = fork()))
    {
        close( fds[0] );
        dup2( fds[1], 1 );
        dup2( fds[1], 2 );
        close( fds[1] );
        execlp( "notify-send", "notify-send", "--help", (char *)NULL );
        _exit( 127 );
    }
    close( fds[1] );
    if (pid < 0) { close( fds[0] ); return cached; }

    while (total < (ssize_t)sizeof(buf) - 1 && (n = read( fds[0], buf + total, sizeof(buf) - 1 - total )) > 0)
        total += n;
    buf[total > 0 ? total : 0] = 0;
    close( fds[0] );
    waitpid( pid, &st, 0 );

    if (strstr( buf, "--action" )) cached = 1;
    return cached;
}


/* Show the message. With a button when the caller is waiting for an answer:
 * Roblox's crash notice says to press OK to collect its support files, and
 * without a button there was no way to say yes -- the message named an action
 * the desktop could not offer. Returns TRUE if the button was pressed.
 *
 * The wait is bounded. notify-send with an action stays up until the user
 * answers or the notification expires, and this runs on the thread that is
 * reporting a crash, so it must not be able to hang the process for good. */
static BOOL notify_desktop( const UNICODE_STRING *text, const UNICODE_STRING *caption, BOOL want_answer )
{
    char body[1024], title[256];
    BOOL pressed = FALSE;
    int fds[2] = { -1, -1 };
    pid_t pid;

    if (!text || !text->Buffer) return FALSE;
    if (!ntdll_wcstoumbs( text->Buffer, text->Length / sizeof(WCHAR), body, sizeof(body) - 1, FALSE ))
        return FALSE;
    body[min( text->Length / sizeof(WCHAR), sizeof(body) - 1 )] = 0;

    strcpy( title, "Roblox" );
    if (caption && caption->Buffer &&
        ntdll_wcstoumbs( caption->Buffer, caption->Length / sizeof(WCHAR), title, sizeof(title) - 1, FALSE ))
        title[min( caption->Length / sizeof(WCHAR), sizeof(title) - 1 )] = 0;

    /* A hard error is the program saying what went wrong in its own words, and
     * it is the only place it ever says it. The notification is seen once and
     * then gone, so put it in the log too. */
    ERR( "hard error: %s: %s\n", title, body );

    if (want_answer && !notifier_has_actions()) want_answer = FALSE;
    if (want_answer && pipe( fds ) == -1) want_answer = FALSE;

    if (!(pid = fork()))
    {
        if (want_answer)
        {
            close( fds[0] );
            dup2( fds[1], 1 );
            close( fds[1] );
            /* -A prints the action's name on stdout when it is pressed, and
             * implies --wait. -t bounds how long the notification lives. */
            execlp( "notify-send", "notify-send", "-a", "TuxBlox",
                    "-i", "dialog-error", "-t", "30000", "-A", "ok=OK",
                    title, body, (char *)NULL );
            _exit( 1 );
        }
        /* fork once more so the notifier is not ours to wait for */
        if (!fork())
        {
            execlp( "notify-send", "notify-send", "-a", "TuxBlox",
                    "-i", "dialog-error", title, body, (char *)NULL );
            _exit( 1 );
        }
        _exit( 0 );
    }

    if (pid < 0)
    {
        if (fds[0] != -1) { close( fds[0] ); close( fds[1] ); }
        return FALSE;
    }

    if (want_answer)
    {
        struct pollfd pfd = { fds[0], POLLIN, 0 };
        char answer[64];
        ssize_t n;

        close( fds[1] );
        /* A little beyond the notification's own lifetime, then give up and
         * take the notifier down with us rather than wait on a desktop that
         * is never going to answer. */
        if (poll( &pfd, 1, 35000 ) > 0 && (n = read( fds[0], answer, sizeof(answer) - 1 )) > 0)
        {
            answer[n] = 0;
            if (!strncmp( answer, "ok", 2 )) pressed = TRUE;
        }
        else kill( pid, SIGTERM );
        close( fds[0] );
    }

    waitpid( pid, NULL, 0 );
    return pressed;
}


NTSTATUS WINAPI NtRaiseHardError( NTSTATUS status, ULONG count,
                                  ULONG params_mask, void **params,
                                  HARDERROR_RESPONSE_OPTION option, HARDERROR_RESPONSE *response )
{
    BOOL answered = FALSE;

    tuxblox_trace_record( "NtRaiseHardError", "" );

    /* the first two parameters of a message box are its text and its title */
    if (params && count >= 2 && (params_mask & 3) == 3)
        answered = notify_desktop( params[0], params[1], response && !getenv( "TUXBLOX_HARDERROR_RESPONSE" ) );

    /* A hard error carries the caller's own diagnostic text: the bits set in
     * params_mask say which parameters are UNICODE_STRING pointers rather than
     * plain values. Recording them is the only way to see what the process was
     * trying to tell the user before Wine's stub swallowed the message. */
    if (tuxblox_trace_enabled() && params)
    {
        ULONG i;

        for (i = 0; i < count && i < 8 * sizeof(params_mask); i++)
        {
            if (params_mask & (1u << i))
                tuxblox_trace_record_us( "NtRaiseHardError.param", params[i] );
            else
            {
                char detail[32];

                snprintf( detail, sizeof(detail), "%p", params[i] );
                tuxblox_trace_record( "NtRaiseHardError.param", detail );
            }
        }
    }

    /* Wine never shows the message box, so a caller waiting on the user's answer
     * gets nothing back. TUXBLOX_HARDERROR_RESPONSE supplies one, which is how a
     * crash handler can be made to take its "user pressed OK" path under Wine. */
    if (response)
    {
        const char *forced = getenv( "TUXBLOX_HARDERROR_RESPONSE" );

        if (forced && *forced)
        {
            *response = atoi( forced );
            FIXME( "%#08x %u %#x %p %u %p: answering %u\n",
                   status, count, params_mask, params, option, response, *response );
            return STATUS_SUCCESS;
        }

        /* The button on the notification is the user's answer. Anything else --
         * dismissed, expired, or a desktop that cannot show buttons -- is not
         * an answer, and the caller is told so rather than being handed a
         * "no" it never heard. */
        if (answered)
        {
            *response = ResponseOk;
            return STATUS_SUCCESS;
        }
    }

    FIXME( "%#08x %u %#x %p %u %p: stub\n", status, count, params_mask, params, option, response );
    return STATUS_NOT_IMPLEMENTED;
}


/******************************************************************************
 *              NtInitiatePowerAction  (NTDLL.@)
 */
NTSTATUS WINAPI NtInitiatePowerAction( POWER_ACTION action, SYSTEM_POWER_STATE state,
                                       ULONG flags, BOOLEAN async )
{
    FIXME( "(%d,%d,0x%08x,%d),stub\n", action, state, flags, async );
    return STATUS_NOT_IMPLEMENTED;
}


/******************************************************************************
 *              NtSetThreadExecutionState  (NTDLL.@)
 */
NTSTATUS WINAPI NtSetThreadExecutionState( EXECUTION_STATE new_state, EXECUTION_STATE *old_state )
{
    static EXECUTION_STATE current = ES_SYSTEM_REQUIRED | ES_DISPLAY_REQUIRED | ES_USER_PRESENT;

    WARN( "(0x%x, %p): stub, harmless.\n", new_state, old_state );
    *old_state = current;
    if (!(current & ES_CONTINUOUS) || (new_state & ES_CONTINUOUS)) current = new_state;
    return STATUS_SUCCESS;
}
