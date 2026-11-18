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

"""Write one release's notes into the Flatpak's metainfo, from the same sources the GitHub release carries."""

import argparse
import datetime
import os
import re
import sys
import xml.etree.ElementTree as ET

# The names a reader knows them by, in the order the download page lists them. libtuxblox keeps no
# changelog: its version tracks GTK's and says nothing anyone can act on.
TITLES = (("compat", "Runtime"), ("launcher", "Launcher"),
          ("installer", "Installer"), ("studio-mcp", "Studio MCP"))

BULLET = re.compile(r"^\s*[*-]\s+(.*)$")
NUMBERED = re.compile(r"^\s*\d+[.)]\s+(.*)$")
HEADING = re.compile(r"^\s*#{1,6}\s+(.*)$")
FENCE = re.compile(r"^\s*```")


def inline(text):
    """Markdown to the inline subset AppStream allows, escaped before any tag is introduced."""
    text = re.sub(r"<(https?://[^>\s]+)>", r"\1", text)
    text = text.replace("&", "&amp;").replace("<", "&lt;").replace(">", "&gt;")
    text = re.sub(r"\[([^\]]+)\]\(([^)]+)\)", r"\1 (\2)", text)
    text = re.sub(r"\*\*(.+?)\*\*", r"<em>\1</em>", text)
    text = re.sub(r"`(.+?)`", r"<code>\1</code>", text)
    return text.strip()


def verbatim(text):
    """A fenced line, which AppStream can only carry as code inside a paragraph."""
    return text.replace("&", "&amp;").replace("<", "&lt;").replace(">", "&gt;").strip()


def to_appstream(markdown):
    """The description's children: ("p", xml) or ("list", "ul"/"ol", [xml]). A blank line does not break a list."""
    out = []
    items = []
    kind = "ul"
    paragraph = []
    fenced = False

    def flush_list():
        if items:
            out.append(("list", kind, list(items)))
            del items[:]

    def flush_paragraph():
        if paragraph:
            out.append(("p", " ".join(paragraph)))
            del paragraph[:]

    for line in markdown.splitlines():
        if FENCE.match(line):
            flush_paragraph()
            flush_list()
            fenced = not fenced
            continue
        if fenced:
            if line.strip():
                out.append(("p", "<code>%s</code>" % verbatim(line)))
            continue
        if not line.strip():
            flush_paragraph()
            continue
        heading = HEADING.match(line)
        if heading:
            flush_paragraph()
            flush_list()
            out.append(("p", inline(heading.group(1))))
            continue
        bullet = BULLET.match(line)
        numbered = None if bullet else NUMBERED.match(line)
        if bullet or numbered:
            flush_paragraph()
            wanted = "ul" if bullet else "ol"
            if items and wanted != kind:
                flush_list()
            kind = wanted
            items.append(inline((bullet or numbered).group(1)))
            continue
        flush_list()
        paragraph.append(inline(line))

    flush_paragraph()
    flush_list()
    return out


def render(blocks, prefix):
    """Indented XML text. A paragraph keeps its own text on one line, so no whitespace joins it."""
    lines = []
    for block in blocks:
        if block[0] == "p":
            lines.append("%s<p>%s</p>" % (prefix, block[1]))
            continue
        lines.append("%s<%s>" % (prefix, block[1]))
        lines += ["%s  <li>%s</li>" % (prefix, item) for item in block[2]]
        lines.append("%s</%s>" % (prefix, block[1]))
    return "\n".join(lines)


def gather(root, notes_file):
    if notes_file is not None:
        return sys.stdin.read() if notes_file == "-" else read(notes_file)

    parts = [read(os.path.join(root, "CHANGELOG.md")).strip()]
    for key, title in TITLES:
        said = read(os.path.join(root, key, "changelogs.md")).strip()
        if not said:
            continue
        version = read(os.path.join(root, key, "VERSION")).strip()
        parts.append("## %s" % " ".join(x for x in (title, version) if x))
        parts.append(said)
    return "\n\n".join(p for p in parts if p)


def read(path):
    try:
        with open(path, encoding="utf-8") as handle:
            return handle.read()
    except OSError:
        return ""


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("version")
    parser.add_argument("--date", default=datetime.date.today().isoformat())
    parser.add_argument("--notes-file", help="read the notes from here, or - for standard input")
    parser.add_argument("--replace", action="store_true", help="rewrite an entry this version already has")
    parser.add_argument("--metainfo", default=os.path.join(os.path.dirname(os.path.abspath(__file__)),
                                                           "..", "net.tuxblox.TuxBlox.metainfo.xml"))
    parser.add_argument("--root", default=os.path.join(os.path.dirname(os.path.abspath(__file__)),
                                                       "..", "..", ".."))
    args = parser.parse_args()

    body = to_appstream(gather(args.root, args.notes_file))
    if not body:
        sys.exit("no notes to publish for %s: every changelog was empty, and a release with no "
                 "notes must not be written" % args.version)

    text = read(args.metainfo)
    if not text:
        sys.exit("cannot read %s" % args.metainfo)

    existing = re.search(r"[ \t]*<release version=\"%s\".*?</release>\n" % re.escape(args.version),
                         text, re.S)
    if existing and not args.replace:
        sys.exit("%s is already in %s: pass --replace to rewrite it" % (args.version, args.metainfo))
    if existing:
        text = text[:existing.start()] + text[existing.end():]

    block = ("    <release version=\"%s\" date=\"%s\">\n      <description>\n%s\n"
             "      </description>\n    </release>\n") % (args.version, args.date, render(body, "        "))

    if "<releases/>" in text:
        text = text.replace("<releases/>", "<releases>\n%s  </releases>" % block)
    elif "<releases>" in text:
        text = text.replace("<releases>\n", "<releases>\n%s" % block, 1)
    else:
        sys.exit("%s has no <releases> element" % args.metainfo)

    ET.fromstring(text)
    with open(args.metainfo, "w", encoding="utf-8") as handle:
        handle.write(text)


if __name__ == "__main__":
    main()
