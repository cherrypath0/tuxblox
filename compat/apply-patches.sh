#!/bin/bash
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

set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT"

echo ":: Reloading the compatibility layer's submodules to their recorded commit"

# FEX is 1.4GB and Makefile.in builds it only when aarch64 is in the target architectures, so an
# x86_64 build checks it out and never compiles a line of it.
if [ "${TARGET_ARCH:-x86_64}" = arm64 ]; then
    git submodule update --init --force -- compat
else
    mapfile -t CompatSubmodules < <(git config -f .gitmodules --get-regexp '^submodule\..*\.path$' |
        awk '{print $2}' | grep '^compat/' | grep -v '/FEX$')
    git submodule update --init --force -- "${CompatSubmodules[@]}"
fi

echo ":: Making sure the shared libraries are checked out"
git submodule update --init -- shared

PatchesDir="compat/patches"
[ -d "$PatchesDir" ] || exit 0

Known="$(sed -n 's/.*rules-source,\([^,]*\),.*/\1/p' compat/Makefile.in | sort -u)"

shopt -s nullglob
count=0
bad=0
for dir in "$PatchesDir"/*/; do
    name="$(basename "$dir")"

    if [ "$name" = wine ]; then
        echo "!! patches/wine is not supported: wine is a maintained fork, patched directly in compat/wine" >&2
        bad=1
        continue
    fi

    if ! grep -qx "$name" <<<"$Known"; then
        echo "!! patches/$name matches no package in the build -- it would never be applied." >&2
        echo "!! Patch directories are named after the build's package (the src-<name> folder)," >&2
        echo "!! which is not always the submodule folder name. Known packages:" >&2
        echo "$Known" | sed 's/^/!!   /' >&2
        bad=1
        continue
    fi

    n="$(find "$dir" -type f | wc -l)"
    count=$(( count + n ))
    echo ":: patches/$name will be overlaid onto src-$name ($n file(s))"
done
shopt -u nullglob

[ "$bad" -eq 0 ] || exit 1
echo ":: $count patch file(s) queued, applied by the Proton build into build/.artifacts/"
