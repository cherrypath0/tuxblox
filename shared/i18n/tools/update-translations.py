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

"""Re-extract the translatable strings and bring every catalogue forward.

Run this by hand after changing user-facing text, then commit what it wrote.
It is deliberately not part of the build: the build must not write into the
tree, because a release records the commit it was built from and a dirty tree
cannot be published.

    python3 shared/i18n/tools/update-translations.py

Needs xgettext and msgmerge, from the gettext package.
"""

from __future__ import annotations

import argparse
import pathlib
import shutil
import subprocess
import sys

KEYWORDS = ["tr", "trc:1c,2", "trf", "trn:1,2"]
SOURCE_DIRS = ["launcher/src", "installer/src", "shared/i18n/src"]


def tool(name: str) -> str:
    found = shutil.which(name)
    if found is None:
        raise SystemExit(f"{name} is not installed (package: gettext)")
    return found


def sources(root: pathlib.Path) -> list[str]:
    found: list[str] = []
    for relative in SOURCE_DIRS:
        directory = root / relative
        if not directory.is_dir():
            continue
        for suffix in ("*.cpp", "*.h"):
            found.extend(str(path.relative_to(root)) for path in sorted(directory.rglob(suffix)))
    if not found:
        raise SystemExit(f"no source files under {root}")
    return found


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--root", type=pathlib.Path, default=pathlib.Path(__file__).resolve().parents[3])
    parser.add_argument("--translations", type=pathlib.Path, default=None)
    args = parser.parse_args()

    root = args.root.resolve()
    translations = args.translations or root / "shared" / "translations"
    pot = translations / "tuxblox.pot"

    command = [
        tool("xgettext"),
        "--language=C++",
        "--from-code=UTF-8",
        "--no-location",
        "--sort-output",
        "--package-name=TuxBlox",
        "--copyright-holder=TuxBlox Developers",
        "--msgid-bugs-address=cherry@tuxblox.net",
        "--keyword=",
        *(f"--keyword={keyword}" for keyword in KEYWORDS),
        "--output=" + str(pot),
        *sources(root),
    ]
    result = subprocess.run(command, text=True, capture_output=True, cwd=root)
    if result.returncode != 0:
        raise SystemExit(result.stderr.strip() or "xgettext failed")
    print(f"{pot.relative_to(root)}: rewritten")

    for path in sorted(translations.glob("*.po")):
        merge = subprocess.run(
            [tool("msgmerge"), "--update", "--backup=none", "--quiet", str(path), str(pot)],
            text=True, capture_output=True)
        if merge.returncode != 0:
            raise SystemExit(merge.stderr.strip() or f"msgmerge failed for {path.name}")
        print(f"{path.relative_to(root)}: merged")

    check = root / "shared" / "i18n" / "tools" / "check-translations.py"
    return subprocess.run([sys.executable, str(check), "--check", "--translations", str(translations)]).returncode


if __name__ == "__main__":
    sys.exit(main())
