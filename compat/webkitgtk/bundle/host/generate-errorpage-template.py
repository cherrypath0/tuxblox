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

# Turns host/errorpage.html into host/errorpage_template.h, so the page is
# compiled into the webview2loader-host binary and nothing has to ship beside
# it. Run after editing the HTML:
#
#     python3 compat/webkitgtk/bundle/host/generate-errorpage-template.py
#
# build-in-container.sh runs this itself before compiling, so a real build
# cannot pick up a stale page. The generated header is committed as well, so a
# plain gcc invocation still works without running this first.

import pathlib
import re
import sys

HERE = pathlib.Path(__file__).resolve().parent
SOURCE = HERE / "errorpage.html"

# Writes next to the HTML by default, which is the committed copy. The build
# passes an output directory instead, because it mounts the source read-only.
OUTPUT = (pathlib.Path(sys.argv[1]) if len(sys.argv) > 1 else HERE) / "errorpage_template.h"

# Every slot errorpage.c fills in. Kept here as a checklist: a typo in the HTML
# turns into a placeholder that silently survives into the page, so the
# generator fails instead of letting that happen.
EXPECTED_SLOTS = {
    "FONT_INTER_400", "FONT_INTER_600", "FONT_MONTSERRAT_700",
    "KICKER", "HEADING", "BODY", "URL", "DETAILS",
}

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

/* compat/webkitgtk/bundle/host/errorpage_template.h
 *
 * GENERATED FILE -- do not edit by hand. Edit host/errorpage.html and run:
 *
 *     python3 compat/webkitgtk/bundle/host/generate-errorpage-template.py
 *
 * The error page, compiled in so the binary carries it and nothing ships
 * beside it. errorpage.c fills the {{SLOTS}} and drops the IF_URL block when
 * there is no URL to offer.
 */
#ifndef WV2L_HOST_ERRORPAGE_TEMPLATE_H
#define WV2L_HOST_ERRORPAGE_TEMPLATE_H

static const char errorpage_template[] =
"""


def c_string_lines(text):
    """Escapes the HTML into C string literals, one per source line."""
    out = []
    for line in text.split("\n"):
        escaped = line.replace("\\", "\\\\").replace('"', '\\"')
        out.append('    "%s\\n"' % escaped)
    return "\n".join(out)


def main():
    html = SOURCE.read_text()

    # Maintainer notes are written as <!--STRIP ... --> and dropped here, so
    # they cost nothing in the binary and never turn up in the page's source.
    # Everything else, including the licence header and the IF_URL markers the
    # page needs at runtime, is left alone.
    html = re.sub(r"<!--STRIP\b.*?-->\n?", "", html, flags=re.DOTALL)

    slots = set(re.findall(r"\{\{([A-Z_0-9]+)\}\}", html))
    unknown = slots - EXPECTED_SLOTS
    missing = EXPECTED_SLOTS - slots
    if unknown:
        raise SystemExit("errorpage.html uses unknown slots: %s" % ", ".join(sorted(unknown)))
    if missing:
        raise SystemExit("errorpage.html is missing slots: %s" % ", ".join(sorted(missing)))

    for marker in ("<!--IF_URL-->", "<!--END_URL-->"):
        if html.count(marker) != 1:
            raise SystemExit("errorpage.html must contain exactly one %s" % marker)
    if html.index("<!--IF_URL-->") > html.index("<!--END_URL-->"):
        raise SystemExit("errorpage.html has <!--END_URL--> before <!--IF_URL-->")

    body = c_string_lines(html)
    OUTPUT.write_text(LICENSE_HEADER + body + ";\n\n#endif /* WV2L_HOST_ERRORPAGE_TEMPLATE_H */\n")
    print("wrote %s (%d bytes from %d bytes of HTML)"
          % (OUTPUT.name, OUTPUT.stat().st_size, len(html)))


if __name__ == "__main__":
    main()
