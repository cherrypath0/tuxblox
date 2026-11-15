/*
 * Authenticode verification for the image a process was started with
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

#ifndef __WINE_TUXBLOX_INTEGRITY_H
#define __WINE_TUXBLOX_INTEGRITY_H

#include <stddef.h>

enum tuxblox_integrity_status
{
    /* The file matches its signature, and the signature chains to a root we
     * trust. The only value that means "safe to run". */
    TUXBLOX_INTEGRITY_VERIFIED,
    TUXBLOX_INTEGRITY_NOT_SIGNED,
    /* Signed, but its contents no longer match what was signed. */
    TUXBLOX_INTEGRITY_MODIFIED,
    /* A broken signature, or a chain that does not reach a root we trust. */
    TUXBLOX_INTEGRITY_BAD_SIGNATURE,
    /* Signed and intact, but by somebody else. */
    TUXBLOX_INTEGRITY_WRONG_PUBLISHER,
    /* Not readable, or not a Windows program at all. */
    TUXBLOX_INTEGRITY_UNREADABLE,
    /* The crypto library could not be loaded, so nothing was checked. Never
     * treat this as a pass: it is the answer when the check could not run. */
    TUXBLOX_INTEGRITY_UNAVAILABLE
};

const char *tuxblox_integrity_status_name( enum tuxblox_integrity_status status );

/* Which step refused the last file, for the line this gets logged on. */
const char *tuxblox_integrity_detail(void);

/* Verifies one PE image. roots_pem is the set of roots a chain may end at, and
 * accepted_publishers the organisations the leaf may be signed by, compared
 * without regard to case. At least one is required, and an empty list refuses
 * every file rather than accepting any publisher. publisher receives the
 * organisation that signed it whenever the file got far enough for that to be
 * known. */
enum tuxblox_integrity_status tuxblox_verify_authenticode( const char *path, const char *gnutls_path,
                                                           const char *roots_pem, size_t roots_length,
                                                           const char **accepted_publishers,
                                                           unsigned int accepted_count,
                                                           char *publisher, size_t publisher_size );

#endif /* __WINE_TUXBLOX_INTEGRITY_H */
