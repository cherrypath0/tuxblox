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

"""Reading .po catalogues, for the tools that embed and check them.

Small on purpose: TuxBlox needs msgctxt, msgid, msgid_plural and msgstr, and
nothing else gettext's format allows. Entries marked fuzzy are reported as
untranslated, which is what gettext itself does with them.
"""

from __future__ import annotations

import dataclasses
import pathlib
import re

# \n, \t, \", \\ and the octal escapes gettext writes. Everything else in a
# catalogue is plain UTF-8.
ESCAPES = {"n": "\n", "t": "\t", "r": "\r", '"': '"', "\\": "\\", "a": "\a", "b": "\b", "f": "\f", "v": "\v"}


@dataclasses.dataclass
class Entry:
    context: str = ""
    msgid: str = ""
    msgid_plural: str | None = None
    # One per plural form; a non-plural entry has exactly one.
    translations: list[str] = dataclasses.field(default_factory=list)
    fuzzy: bool = False
    line: int = 0

    @property
    def is_plural(self) -> bool:
        return self.msgid_plural is not None

    @property
    def translated(self) -> bool:
        return not self.fuzzy and any(text for text in self.translations)

    @property
    def key(self) -> tuple[str, str]:
        return (self.context, self.msgid)


@dataclasses.dataclass
class Catalog:
    path: pathlib.Path
    headers: dict[str, str] = dataclasses.field(default_factory=dict)
    entries: list[Entry] = dataclasses.field(default_factory=list)

    @property
    def language(self) -> str:
        # The file name is the authority: msgmerge rewrites headers, and the
        # name is what the picker and settings.json agree on.
        return self.path.stem

    @property
    def native_name(self) -> str:
        return self.headers.get("X-Native-Name", "").strip() or self.language

    def plural_rule(self) -> tuple[int, str]:
        return parse_plural_forms(self.headers.get("Plural-Forms", ""))

    def by_key(self) -> dict[tuple[str, str], Entry]:
        return {entry.key: entry for entry in self.entries}


def decode(value: str, path: pathlib.Path, line: int) -> str:
    out: list[str] = []
    i = 0
    while i < len(value):
        char = value[i]
        if char != "\\":
            out.append(char)
            i += 1
            continue
        i += 1
        if i >= len(value):
            raise SystemExit(f"{path}:{line}: a string ends in a backslash")
        marker = value[i]
        if marker in ESCAPES:
            out.append(ESCAPES[marker])
            i += 1
        elif marker.isdigit():
            digits = ""
            while i < len(value) and value[i].isdigit() and len(digits) < 3:
                digits += value[i]
                i += 1
            out.append(chr(int(digits, 8)))
        elif marker == "x":
            i += 1
            digits = ""
            while i < len(value) and value[i] in "0123456789abcdefABCDEF":
                digits += value[i]
                i += 1
            out.append(chr(int(digits, 16)))
        else:
            raise SystemExit(f"{path}:{line}: unknown escape \\{marker}")
    return "".join(out)


def encode(value: str) -> str:
    out = []
    for char in value:
        if char == "\\":
            out.append("\\\\")
        elif char == '"':
            out.append('\\"')
        elif char == "\n":
            out.append("\\n")
        elif char == "\t":
            out.append("\\t")
        elif char == "\r":
            out.append("\\r")
        else:
            out.append(char)
    return "".join(out)


STRING_RE = re.compile(r'^\s*"(.*)"\s*$')
KEYWORD_RE = re.compile(r'^(msgctxt|msgid|msgid_plural|msgstr(?:\[(\d+)\])?)\s+"(.*)"\s*$')


def parse(path: pathlib.Path) -> Catalog:
    catalog = Catalog(path=path)
    entry = Entry()
    field: str | None = None
    index = 0
    started = False

    def finish() -> None:
        nonlocal entry, field, started
        if started:
            if entry.msgid == "" and not entry.context:
                catalog.headers.update(parse_headers(entry.translations[0] if entry.translations else ""))
            else:
                catalog.entries.append(entry)
        entry = Entry()
        field = None
        started = False

    for number, raw in enumerate(path.read_text(encoding="utf-8").splitlines(), 1):
        line = raw.rstrip()
        if not line.strip():
            finish()
            continue
        if line.startswith("#~"):
            # An obsolete entry msgmerge kept for reference. Not shipped.
            finish()
            continue
        if line.startswith("#"):
            if line.startswith("#,") and "fuzzy" in line:
                entry.fuzzy = True
            continue
        match = KEYWORD_RE.match(line)
        if match:
            keyword, plural_index, value = match.group(1), match.group(2), match.group(3)
            text = decode(value, path, number)
            if keyword == "msgctxt":
                # A catalogue written without blank lines between entries is
                # still well formed, so a new keyword can be what ends one.
                if started and entry.translations:
                    finish()
                entry.context = text
                field = "context"
            elif keyword == "msgid":
                if started and entry.translations:
                    finish()
                entry.msgid = text
                entry.line = number
                field = "msgid"
            elif keyword == "msgid_plural":
                entry.msgid_plural = text
                field = "msgid_plural"
            else:
                index = int(plural_index) if plural_index is not None else 0
                while len(entry.translations) <= index:
                    entry.translations.append("")
                entry.translations[index] = text
                field = "msgstr"
            started = True
            continue
        match = STRING_RE.match(line)
        if match and field is not None:
            text = decode(match.group(1), path, number)
            if field == "context":
                entry.context += text
            elif field == "msgid":
                entry.msgid += text
            elif field == "msgid_plural":
                entry.msgid_plural = (entry.msgid_plural or "") + text
            else:
                entry.translations[index] += text
            continue
        raise SystemExit(f"{path}:{number}: this line is not something a catalogue may hold: {raw!r}")

    finish()
    return catalog


def parse_headers(block: str) -> dict[str, str]:
    headers: dict[str, str] = {}
    for line in block.splitlines():
        if ":" not in line:
            continue
        name, _, value = line.partition(":")
        headers[name.strip()] = value.strip()
    return headers


# The expression is evaluated by generated C++, so it is held to C's own
# operators and nothing else: no calls, no names but n, no statements.
SAFE_EXPRESSION = re.compile(r"^[0-9n\s()?:%!=<>&|+\-*/]*$")
DEFAULT_RULE = (2, "n != 1")


def parse_plural_forms(header: str) -> tuple[int, str]:
    if not header:
        return DEFAULT_RULE
    count_match = re.search(r"nplurals\s*=\s*(\d+)", header)
    plural_match = re.search(r"plural\s*=\s*([^;]+)", header)
    if count_match is None or plural_match is None:
        return DEFAULT_RULE
    expression = plural_match.group(1).strip()
    if not SAFE_EXPRESSION.match(expression) or "n" not in expression:
        return DEFAULT_RULE
    return (int(count_match.group(1)), expression)
