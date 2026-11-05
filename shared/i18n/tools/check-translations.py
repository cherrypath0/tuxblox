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

"""Check that every tr()/trf()/trn()/trc() string has both translations.

Usage:
    check-translations.py --check
    check-translations.py --write-en

`--write-en` adds keys that the source calls but en.json is missing. It does
not delete stale keys; --check reports those so they can be reviewed.
"""

from __future__ import annotations

import argparse
import json
import pathlib
import re
import shutil
import subprocess
import sys

ENGLISH_CATALOG = "en.json"
PLACEHOLDER_RE = re.compile(r"\{(\d+)\}")


def parse_po(text: str) -> set[tuple[str, str]]:
    keys: set[tuple[str, str]] = set()
    for block in re.split(r"\n\s*\n", text):
        context = ""
        msgid = None
        msgid_plural = None
        current = None
        for line in block.splitlines():
            match = re.match(r'^(msgctxt|msgid|msgid_plural)\s+"(.*)"$', line)
            if match:
                current = match.group(1)
                value = decode_po_string(match.group(2))
                if current == "msgctxt":
                    context = value
                elif current == "msgid":
                    msgid = value
                elif current == "msgid_plural":
                    msgid_plural = value
                continue
            match = re.match(r'^"(.*)"$', line)
            if match and current is not None:
                value = decode_po_string(match.group(1))
                if current == "msgctxt":
                    context += value
                elif current == "msgid":
                    msgid = (msgid or "") + value
                elif current == "msgid_plural":
                    msgid_plural = (msgid_plural or "") + value
        if msgid and any(marker in msgid for marker in (
                "Project-Id-Version:", "POT-Creation-Date:", "PO-Revision-Date:",
                "Last-Translator:", "Language-Team:", "MIME-Version:", "Content-Type:",
                "Content-Transfer-Encoding:", "Plural-Forms:")):
            continue
        if msgid:
            keys.add((context, msgid))
        if msgid_plural:
            keys.add((context, msgid_plural))
    return keys


def decode_po_string(value: str) -> str:
    # gettext uses C-style escapes. JSON's string syntax covers the ones that
    # appear in these catalogs, and leaves ordinary UTF-8 untouched.
    try:
        return json.loads('"' + value + '"')
    except json.JSONDecodeError:
        return value


def extract_source_keys(root: pathlib.Path, source_dirs: list[pathlib.Path]) -> set[tuple[str, str]]:
    xgettext = shutil.which("xgettext")
    if xgettext is None:
        raise SystemExit("xgettext is not installed (package: gettext)")
    sources: list[str] = []
    directories = source_dirs or [
        root / "launcher" / "src",
        root / "installer" / "src",
        root / "shared" / "i18n" / "src",
    ]
    for directory in directories:
        if not directory.is_dir():
            continue
        for suffix in ("*.cpp", "*.h"):
            sources.extend(str(path) for path in sorted(directory.rglob(suffix)))
    if not sources:
        raise SystemExit(f"no source files under {root}")
    command = [
        xgettext,
        "--language=C++",
        "--from-code=UTF-8",
        "--no-location",
        "--keyword=tr",
        "--keyword=trc:1,2",
        "--keyword=trf",
        "--keyword=trn:1,2",
        "--output=-",
        *sources,
    ]
    result = subprocess.run(command, text=True, capture_output=True)
    if result.returncode != 0:
        raise SystemExit(result.stderr.strip() or "xgettext failed")
    return parse_po(result.stdout)


def load_catalog(path: pathlib.Path) -> tuple[dict[str, str], dict[str, dict[str, str]]]:
    with path.open("r", encoding="utf-8") as handle:
        catalog = json.load(handle)
    strings = catalog.get("strings", {})
    contexts = catalog.get("contexts", {})
    if not isinstance(strings, dict) or not isinstance(contexts, dict):
        raise SystemExit(f"{path}: strings and contexts must be objects")
    return strings, contexts


def catalog_keys(strings: dict[str, str], contexts: dict[str, dict[str, str]]) -> set[tuple[str, str]]:
    keys = {("", source) for source in strings}
    for context, entries in contexts.items():
        keys.update((context, source) for source in entries)
    return keys


def lookup(strings: dict[str, str], contexts: dict[str, dict[str, str]], key: tuple[str, str]) -> str | None:
    context, source = key
    if context:
        return contexts.get(context, {}).get(source)
    return strings.get(source)


def placeholders(text: str) -> tuple[set[str], int]:
    return set(PLACEHOLDER_RE.findall(text)), text.count("{}")


def write_catalog(path: pathlib.Path, strings: dict[str, str], contexts: dict[str, dict[str, str]]) -> None:
    catalog = {
        "language": path.stem,
        "strings": dict(sorted(strings.items())),
        "contexts": {
            context: dict(sorted(entries.items()))
            for context, entries in sorted(contexts.items())
        },
    }
    path.write_text(json.dumps(catalog, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")


def main() -> int:
    parser = argparse.ArgumentParser()
    group = parser.add_mutually_exclusive_group(required=True)
    group.add_argument("--check", action="store_true")
    group.add_argument("--write-en", action="store_true")
    parser.add_argument("--root", type=pathlib.Path, default=pathlib.Path(__file__).resolve().parents[3])
    parser.add_argument("--i18n", type=pathlib.Path, default=pathlib.Path(__file__).resolve().parents[1])
    parser.add_argument("--source-dir", action="append", type=pathlib.Path, default=[])
    parser.add_argument("--allow-extra", action="store_true",
                        help="do not fail on catalog keys this source tree does not use")
    args = parser.parse_args()

    translations_dir = args.i18n / "translations"
    english_path = translations_dir / ENGLISH_CATALOG
    source_keys = extract_source_keys(args.root, args.source_dir)
    en_strings, en_contexts = load_catalog(english_path)

    if args.write_en:
        for context, source in sorted(source_keys):
            if context:
                en_contexts.setdefault(context, {}).setdefault(source, source)
            else:
                en_strings.setdefault(source, source)
        write_catalog(english_path, en_strings, en_contexts)
        print(f"en.json now has {len(catalog_keys(en_strings, en_contexts))} keys")
        return 0

    failures: list[str] = []
    en_keys = catalog_keys(en_strings, en_contexts)
    for key in sorted(source_keys - en_keys):
        failures.append(f"en.json is missing {format_key(key)}")
    if not args.allow_extra:
        for key in sorted(en_keys - source_keys):
            failures.append(f"en.json has a stale key {format_key(key)}")

    catalog_data = {"en": (en_strings, en_contexts)}
    for path in sorted(translations_dir.glob("*.json")):
        if path.name == ENGLISH_CATALOG:
            continue
        strings, contexts = load_catalog(path)
        catalog_data[path.name] = (strings, contexts)
        keys = catalog_keys(strings, contexts)
        for key in sorted(en_keys - keys):
            failures.append(f"{path.name} is missing {format_key(key)}")
        for key in sorted(keys - en_keys):
            failures.append(f"{path.name} has an extra key {format_key(key)}")

    for code, (strings, contexts) in catalog_data.items():
        for key in sorted(en_keys):
            source = key[1]
            translation = lookup(strings, contexts, key)
            if translation is None:
                continue
            source_slots, source_plain = placeholders(source)
            translated_slots, translated_plain = placeholders(translation)
            if source_slots != translated_slots or source_plain != translated_plain:
                failures.append(
                    f"{code}: placeholder mismatch for {format_key(key)}: "
                    f"{sorted(source_slots)}/{source_plain} vs {sorted(translated_slots)}/{translated_plain}"
                )

    if failures:
        for failure in failures:
            print(failure, file=sys.stderr)
        print(f"{len(failures)} translation problem(s)", file=sys.stderr)
        return 1
    print(f"translations OK: {len(en_keys)} keys")
    return 0


def format_key(key: tuple[str, str]) -> str:
    context, source = key
    return f"{context!r}:{source!r}" if context else repr(source)


if __name__ == "__main__":
    sys.exit(main())
