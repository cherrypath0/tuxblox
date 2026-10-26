/*
 * Unit test for the audio device name reshaping
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
#include <string.h>

#include "tuxblox_device_name.c"

static int failures;

static void expect( const char *desc, const char *profile, int monitor, const char *want )
{
    char buffer[64];

    tuxblox_windows_device_name( buffer, sizeof(buffer), desc, profile, NULL, NULL, monitor );
    if (!strcmp( buffer, want )) return;

    failures++;
    printf( "  FAIL: %s\n     got: %s\n    want: %s\n", desc, buffer, want );
}

static void expect_pci( const char *desc, const char *vendor, int monitor, const char *want )
{
    char buffer[64];

    tuxblox_windows_device_name( buffer, sizeof(buffer), desc, NULL, "pci", vendor, monitor );
    if (!strcmp( buffer, want )) return;

    failures++;
    printf( "  FAIL: %s (vendor %s)\n     got: %s\n    want: %s\n",
            desc, vendor ? vendor : "(none)", buffer, want );
}

int main( void )
{
    /* The profile suffix is PulseAudio's; Windows names the device alone */
    expect( "Razer Kraken V4 Pro Analog Stereo", "Analog Stereo", 0, "Razer Kraken V4 Pro" );
    expect( "Starship/Matisse HD Audio Controller Analog Stereo", "Analog Stereo", 0,
            "Starship/Matisse HD Audio Controller" );
    expect( "Some Headset Digital Stereo (IEC958)", "Digital Stereo (IEC958)", 0, "Some Headset" );

    /* Windows calls a loopback capture of the output Stereo Mix */
    expect( "Monitor of Razer Kraken V4 Pro Analog Stereo", "Analog Stereo", 1,
            "Stereo Mix (Razer Kraken V4 Pro)" );
    expect( "Monitor of Some Device", NULL, 1, "Stereo Mix (Some Device)" );

    /* A monitor PulseAudio did not label, so there is no prefix to remove */
    expect( "Some Device", NULL, 1, "Stereo Mix (Some Device)" );

    /* Nothing to strip */
    expect( "Some Device", NULL, 0, "Some Device" );
    expect( "Some Device", "", 0, "Some Device" );
    expect( "Some Device", "Analog Stereo", 0, "Some Device" );

    /* The profile only counts at the end, and only as a whole word */
    expect( "Analog Stereo Thing", "Analog Stereo", 0, "Analog Stereo Thing" );
    expect( "DeviceAnalog Stereo", "Analog Stereo", 0, "DeviceAnalog Stereo" );

    /* A description that is nothing but its profile keeps its name */
    expect( "Analog Stereo", "Analog Stereo", 0, "Analog Stereo" );

    /* Degenerate input must still leave a usable string */
    expect( "", NULL, 0, "" );
    expect( "", NULL, 1, "Stereo Mix ()" );

    /* A card on the PCI bus is named for its vendor, the way Windows names the
       driver, instead of carrying the Linux name of the controller die. */
    expect_pci( "GB206 High Definition Audio Controller Digital Stereo (HDMI) [E27QHD-G]",
            "0x10de", 0, "NVIDIA High Definition Audio" );
    expect_pci( "Navi 31 HDMI/DP Audio Digital Stereo", "0x1002", 0,
            "AMD High Definition Audio Device" );
    expect_pci( "Starship/Matisse HD Audio Controller Digital Stereo (IEC958)", "0x1022", 0,
            "High Definition Audio Device" );

    /* The two cards on this machine must not end up sharing one name */
    expect_pci( "GB206 High Definition Audio Controller", "0x10de", 0,
            "NVIDIA High Definition Audio" );
    expect_pci( "Starship/Matisse HD Audio Controller", "0x1022", 0,
            "High Definition Audio Device" );

    /* An unknown or missing vendor falls back to the name Windows uses for a
       card it has no vendor driver for. */
    expect_pci( "Some Controller", "0xbeef", 0, "High Definition Audio Device" );
    expect_pci( "Some Controller", NULL, 0, "High Definition Audio Device" );
    expect_pci( "Some Controller", "", 0, "High Definition Audio Device" );

    /* However the id is spelled */
    expect_pci( "x", "10DE", 0, "NVIDIA High Definition Audio" );
    expect_pci( "x", "0X10DE", 0, "NVIDIA High Definition Audio" );

    /* A monitor of a PCI card is still Stereo Mix, of the renamed card */
    expect_pci( "Monitor of GB206 High Definition Audio Controller", "0x10de", 1,
            "Stereo Mix (NVIDIA High Definition Audio)" );

    /* USB keeps its product name, which is what Windows shows too */
    {
        char buffer[64];
        tuxblox_windows_device_name( buffer, sizeof(buffer),
                "Razer Kraken V4 Pro Analog Stereo", "Analog Stereo", "usb", "0x1532", 0 );
        if (strcmp( buffer, "Razer Kraken V4 Pro" ))
        {
            failures++;
            printf( "  FAIL: usb device\n     got: %s\n    want: Razer Kraken V4 Pro\n", buffer );
        }
    }

    if (failures)
    {
        printf( "device name: %d failed\n", failures );
        return 1;
    }
    printf( "device name: all passed\n" );
    return 0;
}
