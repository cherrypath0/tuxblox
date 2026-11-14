#!/usr/bin/env bash
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

libadwaitaCommit() {
    local uiDir="$1" tarball commit dir
    for tarball in "$uiDir"/dist/ui-stack-*.tar.zst; do
        [[ -f "$tarball" ]] || continue
        commit="$(zstd -dc "$tarball" 2>/dev/null | tar -xO LIBADWAITA_COMMIT 2>/dev/null | tr -d '[:space:]' || true)"
        if [[ -n "$commit" ]]; then
            printf '%s' "$commit"
            return 0
        fi
    done
    dir="$(cd "$uiDir" 2>/dev/null && pwd -P)/libadwaita"
    if [[ -d "$dir" && "$(git -C "$dir" rev-parse --show-toplevel 2>/dev/null)" == "$dir" ]]; then
        commit="$(git -C "$dir" rev-parse HEAD)"
        if [[ -n "$(git -C "$dir" status --porcelain)" ]]; then
            commit="$commit-dirty"
        fi
        printf '%s' "$commit"
    fi
}
