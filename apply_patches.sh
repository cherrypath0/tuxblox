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

# Resets every ProtonSource submodule to the commit recorded in the index
# (discarding any leftover local edits, e.g. a patch applied by a previous
# build), then overlays patches/<submodule>/<relpath> onto
# ProtonSource/<submodule>/<relpath> for whichever submodules have patches
# staged. Wine is excluded on purpose: it's a separately maintained fork
# checked in directly rather than a submodule (see .gitmodules), so it's
# patched by editing ProtonSource/wine in place, not through this mechanism.
set -eo pipefail
cd "$(dirname "$0")"

step() {
    echo -e "\e[34m:: $1\e[0m"
}

step "Reloading submodules to their recorded commit"
git submodule update --init --force

patches_dir="patches"
proton_source="ProtonSource"

if [[ ! -d "$patches_dir" ]]; then
    exit 0
fi

step "Applying patches from $patches_dir/"
shopt -s nullglob
applied=0
for submodule_dir in "$patches_dir"/*/; do
    submodule_name="$(basename "$submodule_dir")"

    if [[ "$submodule_name" == "wine" ]]; then
        echo "!! patches/wine is not supported -- wine is patched directly in ProtonSource/wine, not through patches/" >&2
        continue
    fi

    target_dir="$proton_source/$submodule_name"
    if [[ ! -d "$target_dir" ]]; then
        echo "!! patches/$submodule_name has no matching ProtonSource/$submodule_name -- skipping" >&2
        continue
    fi

    while IFS= read -r -d '' file; do
        rel_path="${file#"$submodule_dir"}"
        dest="$target_dir/$rel_path"
        mkdir -p "$(dirname "$dest")"
        cp -f "$file" "$dest"
        echo ":: Applied patch: $file -> $dest"
        applied=$((applied + 1))
    done < <(find "$submodule_dir" -type f -print0)
done
shopt -u nullglob

step "Applied $applied patch file(s)"
