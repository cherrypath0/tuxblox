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

"""Generate the C++ translation tables from the JSON catalogs.

The catalogs are ordinary UTF-8 JSON, so translators can edit them without a
build tree. This script is the only place that turns them into C++ string
literals. It runs at build time and writes one header into the build directory.
"""

from __future__ import annotations

import argparse
import json
import pathlib
import sys

CATALOGS = (("en", "en.json"), ("pt_BR", "pt_BR.json"), ("es", "es.json"))


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


def load_catalog(path: pathlib.Path) -> list[tuple[str, str, str]]:
    with path.open("r", encoding="utf-8") as handle:
        catalog = json.load(handle)
    strings = catalog.get("strings", {})
    contexts = catalog.get("contexts", {})
    if not isinstance(strings, dict) or not isinstance(contexts, dict):
        raise SystemExit(f"{path}: strings and contexts must be objects")
    rows: list[tuple[str, str, str]] = []
    for source, translation in sorted(strings.items()):
        if not isinstance(source, str) or not isinstance(translation, str):
            raise SystemExit(f"{path}: every string entry must be text")
        rows.append(("", source, translation))
    for context, entries in sorted(contexts.items()):
        if not isinstance(entries, dict):
            raise SystemExit(f"{path}: context {context!r} must be an object")
        for source, translation in sorted(entries.items()):
            if not isinstance(source, str) or not isinstance(translation, str):
                raise SystemExit(f"{path}: every context entry must be text")
            rows.append((context, source, translation))
    return rows


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--input-dir", required=True, type=pathlib.Path)
    parser.add_argument("--output", required=True, type=pathlib.Path)
    args = parser.parse_args()

    lines = [
        "// Generated at build time by shared/i18n/tools/embed-translations.py. Do not edit.",
        "#pragma once",
        "#include <cstddef>",
        "",
        "struct I18nEntry {",
        "    const char *context;",
        "    const char *source;",
        "    const char *translation;",
        "};",
        "",
        "struct I18nCatalog {",
        "    const char *code;",
        "    const I18nEntry *entries;",
        "    size_t count;",
        "};",
        "",
    ]

    catalog_rows = []
    for code, filename in CATALOGS:
        rows = load_catalog(args.input_dir / filename)
        symbol = "kI18n" + "".join(part.capitalize() for part in code.replace("-", "_").split("_"))
        lines.append(f"static const I18nEntry {symbol}[] = {{")
        if rows:
            for context, source, translation in rows:
                lines.append(
                    "    {" + cpp_string(context) + ", " + cpp_string(source) + ", "
                    + cpp_string(translation) + "},"
                )
        else:
            lines.append('    {"", "", ""},')
        lines.append("};")
        lines.append("")
        catalog_rows.append((code, symbol, len(rows)))

    lines.append("static const I18nCatalog kI18nCatalogs[] = {")
    for code, symbol, count in catalog_rows:
        lines.append(f"    {{{cpp_string(code)}, {symbol}, {count}}},")
    lines.append("};")
    lines.append("")

    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text("\n".join(lines), encoding="utf-8")
    return 0


if __name__ == "__main__":
    sys.exit(main())
