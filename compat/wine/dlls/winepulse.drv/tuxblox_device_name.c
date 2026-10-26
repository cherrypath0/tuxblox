/*
 * Audio device names
 *
 * PulseAudio describes a device as "<device> <profile>" and labels a loopback
 * capture of an output "Monitor of <device>", neither of which Windows does.
 * Windows names the device alone, and calls the same loopback capture Stereo
 * Mix. This reshapes a description into that form; the device itself, its
 * channels and its form factor are reported exactly as they are.
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

#include <string.h>

#ifndef ARRAY_SIZE
#define ARRAY_SIZE(x) (sizeof(x) / sizeof((x)[0]))
#endif

static const char monitor_prefix[] = "Monitor of ";
static const char stereo_mix_open[] = "Stereo Mix (";

static void append( char *buffer, unsigned int size, unsigned int *pos,
                    const char *text, unsigned int len )
{
    unsigned int i;

    for (i = 0; i < len && *pos + 1 < size; i++) buffer[(*pos)++] = text[i];
    buffer[*pos] = 0;
}

/* PCI vendors Windows has a named driver for; anything else gets its generic one */
static const struct
{
    const char *id;
    const char *name;
}
pci_vendors[] =
{
    { "10de", "NVIDIA High Definition Audio" },
    { "1002", "AMD High Definition Audio Device" },
};

static const char generic_hd_audio[] = "High Definition Audio Device";

static int same_vendor_id( const char *vendor, const char *id )
{
    unsigned int i;

    if (vendor[0] == '0' && (vendor[1] == 'x' || vendor[1] == 'X')) vendor += 2;
    for (i = 0; id[i]; i++)
    {
        char c = vendor[i];

        if (c >= 'A' && c <= 'Z') c = (char)(c + ('a' - 'A'));
        if (c != id[i]) return 0;
    }
    return !vendor[i];
}

/* A card on the PCI bus carries the Linux name of its controller die, which
   Windows never shows -- there it is named after whichever driver claims it. */
static const char *pci_card_name( const char *vendor )
{
    unsigned int i;

    if (!vendor) return generic_hd_audio;
    for (i = 0; i < ARRAY_SIZE(pci_vendors); i++)
        if (same_vendor_id( vendor, pci_vendors[i].id )) return pci_vendors[i].name;
    return generic_hd_audio;
}

void tuxblox_windows_device_name( char *buffer, unsigned int size, const char *desc,
                                  const char *profile, const char *bus, const char *vendor,
                                  int monitor )
{
    unsigned int pos = 0, len;

    if (!size) return;
    buffer[0] = 0;
    if (!desc) desc = "";

    if (bus && !strcmp( bus, "pci" ))
    {
        desc = pci_card_name( vendor );
        profile = NULL;
    }
    len = strlen( desc );

    if (monitor && len >= sizeof(monitor_prefix) - 1 &&
        !strncmp( desc, monitor_prefix, sizeof(monitor_prefix) - 1 ))
    {
        desc += sizeof(monitor_prefix) - 1;
        len -= sizeof(monitor_prefix) - 1;
    }

    /* Only a whole trailing word is the profile, and never the whole name */
    if (profile && profile[0])
    {
        unsigned int profile_len = strlen( profile );

        if (len > profile_len + 1 && desc[len - profile_len - 1] == ' ' &&
            !strcmp( desc + len - profile_len, profile ))
            len -= profile_len + 1;
    }

    if (monitor) append( buffer, size, &pos, stereo_mix_open, sizeof(stereo_mix_open) - 1 );
    append( buffer, size, &pos, desc, len );
    if (monitor) append( buffer, size, &pos, ")", 1 );
}
