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

"""Generate the C++ translation tables from the .po catalogues.

Each component ships as one file with nothing beside it, so there is nowhere
for .mo files to live at runtime. The catalogues are turned into plain C++
tables instead, and this script is the only place that does it. It runs at
build time and writes one header into the build directory.

A catalogue with nothing translated in it is left out, so a language somebody
has started but not finished does not appear in the picker as English.
"""

from __future__ import annotations

import argparse
import pathlib
import sys

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent))
import po  # noqa: E402


def cpp_string(value: str) -> str:
    out = ['"']
    for char in value:
        code = ord(char)
        if char == "\\":
            out.append("\\\\")
        elif char == '"':
            out.append('\\"')
        elif char == "\n":
            out.append("\\n")
        elif char == "\r":
            out.append("\\r")
        elif char == "\t":
            out.append("\\t")
        elif code < 0x20:
            raise SystemExit(f"control character U+{code:04X} in translation: {value!r}")
        else:
            out.append(char)
    out.append('"')
    return "".join(out)


def symbol_for(code: str) -> str:
    return "".join(part.capitalize() for part in code.replace("-", "_").split("_"))


HEADER = """// Generated at build time by shared/i18n/tools/embed-translations.py. Do not edit.
#pragma once
#include <cstddef>

struct I18nEntry {
    const char *context;
    const char *source;
    const char *translation;
};

struct I18nPlural {
    const char *context;
    const char *singular;
    const char *const *forms;
    size_t formCount;
};

struct I18nCatalog {
    const char *code;
    const char *nativeName;
    const I18nEntry *entries;
    size_t count;
    const I18nPlural *plurals;
    size_t pluralCount;
    // The language's own Plural-Forms rule, resolved here rather than parsed
    // at runtime.
    unsigned long (*pluralIndex)(unsigned long long n);
};
"""


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--input-dir", required=True, type=pathlib.Path)
    parser.add_argument("--output", required=True, type=pathlib.Path)
    args = parser.parse_args()

    lines = [HEADER]
    catalogs = []

    for path in sorted(args.input_dir.glob("*.po")):
        catalog = po.parse(path)
        entries = [entry for entry in catalog.entries if entry.translated]
        if not entries:
            print(f"-- {path.name}: nothing translated yet, left out of this build")
            continue

        code = catalog.language
        name = symbol_for(code)
        nplurals, expression = catalog.plural_rule()

        lines.append(f"static unsigned long kI18nPlural{name}(unsigned long long n) {{")
        lines.append(f"    return static_cast<unsigned long>({expression});")
        lines.append("}")
        lines.append("")

        singles = [entry for entry in entries if not entry.is_plural]
        plurals = [entry for entry in entries if entry.is_plural]

        lines.append(f"static const I18nEntry kI18n{name}Entries[] = {{")
        for entry in sorted(singles, key=lambda item: item.key):
            translation = entry.translations[0] if entry.translations else ""
            lines.append("    {" + cpp_string(entry.context) + ", " + cpp_string(entry.msgid) + ", "
                         + cpp_string(translation) + "},")
        if not singles:
            lines.append('    {"", "", ""},')
        lines.append("};")
        lines.append("")

        for number, entry in enumerate(sorted(plurals, key=lambda item: item.key)):
            forms = list(entry.translations) + [""] * (nplurals - len(entry.translations))
            lines.append(f"static const char *const kI18n{name}Forms{number}[] = {{"
                         + ", ".join(cpp_string(form) for form in forms[:nplurals]) + "};")
        lines.append(f"static const I18nPlural kI18n{name}Plurals[] = {{")
        for number, entry in enumerate(sorted(plurals, key=lambda item: item.key)):
            lines.append("    {" + cpp_string(entry.context) + ", " + cpp_string(entry.msgid) + ", "
                         + f"kI18n{name}Forms{number}, {nplurals}}},")
        if not plurals:
            lines.append('    {"", "", nullptr, 0},')
        lines.append("};")
        lines.append("")

        catalogs.append((code, catalog.native_name, name, len(singles), len(plurals)))

    lines.append("static const I18nCatalog kI18nCatalogs[] = {")
    for code, native, name, singles, plurals in catalogs:
        lines.append(f"    {{{cpp_string(code)}, {cpp_string(native)}, kI18n{name}Entries, {singles}, "
                     f"kI18n{name}Plurals, {plurals}, kI18nPlural{name}}},")
    if not catalogs:
        # An empty array is not valid C++, and a build with no catalogue at all
        # is a legitimate state: English is the source text, not a catalogue.
        lines.append("    {\"\", \"\", nullptr, 0, nullptr, 0, nullptr},")
    lines.append("};")
    lines.append(f"static const size_t kI18nCatalogCount = {len(catalogs)};")
    lines.append("")

    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text("\n".join(lines), encoding="utf-8")
    print(f"-- embedded {len(catalogs)} catalogue(s): " + ", ".join(code for code, *_ in catalogs))
    return 0


if __name__ == "__main__":
    sys.exit(main())
