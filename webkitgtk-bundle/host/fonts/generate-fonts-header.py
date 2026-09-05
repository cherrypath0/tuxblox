#!/usr/bin/env python3
# TuxBlox - Linux Compatibility Layer for the Roblox Engine
# Copyright (C) 2026 TuxBlox Developers
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program. If not, see <https://www.gnu.org/licenses/>.

# Turns the .woff2 files next to this script into host/errorpage_fonts.h.
#
# The error page has to render with no network at all -- that is the whole
# situation it exists for -- so its two faces travel inside the document as
# data: URIs rather than being fetched. Run this after replacing a .woff2:
#
#     python3 webkitgtk-bundle/host/fonts/generate-fonts-header.py

import base64
import pathlib
import sys

HERE = pathlib.Path(__file__).resolve().parent

# Writes into host/ by default, which is the committed copy. The build passes an
# output directory instead, because it mounts the source read-only.
OUTPUT = (pathlib.Path(sys.argv[1]) if len(sys.argv) > 1 else HERE.parent) / "errorpage_fonts.h"

# Latin subsets downloaded from the Google Fonts css2 endpoint. Both families
# are SIL OFL 1.1; their licence texts ship in third_party_licenses/.
FACES = [
    ("errorpage_font_inter_400", "Inter-Regular.woff2", "Inter Regular (400)"),
    ("errorpage_font_inter_600", "Inter-SemiBold.woff2", "Inter SemiBold (600)"),
    ("errorpage_font_montserrat_700", "Montserrat-Bold.woff2", "Montserrat Bold (700)"),
]

LICENSE_HEADER = """/* TuxBlox - Linux Compatibility Layer for the Roblox Engine
 * Copyright (C) 2026 TuxBlox Developers
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <https://www.gnu.org/licenses/>.
 */
"""

PREAMBLE = """
/* webkitgtk-bundle/host/errorpage_fonts.h
 *
 * GENERATED FILE -- do not edit by hand. Regenerate with:
 *
 *     python3 webkitgtk-bundle/host/fonts/generate-fonts-header.py
 *
 * The two faces the error page is set in, base64'd so they can travel inside
 * the document as data: URIs. They are embedded rather than fetched because
 * the page appears precisely when the network is unreachable, and rather than
 * installed into the bundle's fontconfig because that would change how every
 * other page the webview renders is drawn, not just this one.
 *
 * Both families are SIL Open Font License 1.1; the licence texts ship in
 * third_party_licenses/. The .woff2 sources are in host/fonts/ so a
 * regeneration is reproducible without going back to Google Fonts.
 */
#ifndef WV2L_HOST_ERRORPAGE_FONTS_H
#define WV2L_HOST_ERRORPAGE_FONTS_H
"""

WIDTH = 76


def emit(name, filename, description):
    data = base64.b64encode((HERE / filename).read_bytes()).decode("ascii")
    lines = [data[i:i + WIDTH] for i in range(0, len(data), WIDTH)]
    body = "\n".join('    "%s"' % line for line in lines)
    return "\n/* %s -- %s, %d bytes base64 */\nstatic const char %s[] =\n%s;\n" % (
        description, filename, len(data), name, body)


def main():
    parts = [LICENSE_HEADER, PREAMBLE]
    for name, filename, description in FACES:
        parts.append(emit(name, filename, description))
    parts.append("\n#endif /* WV2L_HOST_ERRORPAGE_FONTS_H */\n")

    OUTPUT.write_text("".join(parts))
    print("wrote %s (%d bytes)" % (OUTPUT, OUTPUT.stat().st_size))


if __name__ == "__main__":
    main()
