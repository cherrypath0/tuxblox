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

"""Check every catalogue against the extracted template.

    check-translations.py --check

Fails when a catalogue has drifted from the template, and when a translation
drops or invents a placeholder -- a button reading "Stopped" with no number is
the failure this exists to prevent. Untranslated entries are not failures:
they fall back to the English source, which is what a part-finished language
is supposed to do. Needs nothing but python3, so it runs inside the build
containers; update-translations.py is the step that needs gettext.
"""

from __future__ import annotations

import argparse
import pathlib
import re
import sys

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent))
import po  # noqa: E402

NUMBERED = re.compile(r"\{(\d+)\}")


def placeholders(text: str) -> tuple[set[str], int]:
    return set(NUMBERED.findall(text)), text.count("{}")


def describe(key: tuple[str, str]) -> str:
    context, source = key
    return f"{context!r}:{source!r}" if context else repr(source)


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--check", action="store_true", required=True)
    parser.add_argument("--translations", type=pathlib.Path,
                        default=pathlib.Path(__file__).resolve().parents[3] / "shared" / "translations")
    args = parser.parse_args()

    template_path = args.translations / "tuxblox.pot"
    if not template_path.is_file():
        print(f"{template_path} is missing; run update-translations.py", file=sys.stderr)
        return 1

    template = po.parse(template_path)
    expected = template.by_key()
    failures: list[str] = []
    # A fuzzy entry is not rendered -- gettext treats it as untranslated -- so a placeholder it
    # dropped cannot reach a screen until somebody clears the mark, and that is when it has to
    # fail rather than now.
    warnings: list[str] = []
    summary: list[str] = []

    for path in sorted(args.translations.glob("*.po")):
        catalog = po.parse(path)
        present = catalog.by_key()

        for key in sorted(expected.keys() - present.keys()):
            failures.append(f"{path.name} is missing {describe(key)}")
        for key in sorted(present.keys() - expected.keys()):
            failures.append(f"{path.name} has {describe(key)}, which no source string matches "
                            f"(run update-translations.py so msgmerge can retire it)")

        for key, entry in sorted(present.items()):
            source = expected.get(key)
            if source is None:
                continue
            if source.is_plural != entry.is_plural:
                failures.append(f"{path.name}: {describe(key)} is a plural in one file and not the other")
                continue
            forms = [source.msgid] if not source.is_plural else [source.msgid, source.msgid_plural or ""]
            for index, translation in enumerate(entry.translations):
                if not translation:
                    continue
                # A plural form may legitimately use either English form's
                # placeholders, so it is checked against the one it renders.
                reference = forms[min(index, len(forms) - 1)]
                want_slots, want_plain = placeholders(reference)
                got_slots, got_plain = placeholders(translation)
                if want_slots != got_slots or want_plain != got_plain:
                    report = (f"{path.name}: placeholder mismatch for {describe(key)}: "
                              f"{sorted(want_slots)}/{want_plain} in English, "
                              f"{sorted(got_slots)}/{got_plain} in the translation")
                    (warnings if entry.fuzzy else failures).append(
                        report + (" (needs review, so it renders in English until it is fixed)"
                                  if entry.fuzzy else ""))

        done = sum(1 for entry in catalog.entries if entry.translated)
        total = len(expected)
        fuzzy = sum(1 for entry in catalog.entries if entry.fuzzy)
        summary.append(f"{path.stem}: {done}/{total} translated"
                       + (f", {fuzzy} needing review" if fuzzy else ""))

    for line in summary:
        print(line)
    for warning in warnings:
        print("warning: " + warning)
    if failures:
        for failure in failures:
            print(failure, file=sys.stderr)
        print(f"{len(failures)} translation problem(s)", file=sys.stderr)
        return 1
    print(f"translations OK: {len(expected)} source strings")
    return 0


if __name__ == "__main__":
    sys.exit(main())
