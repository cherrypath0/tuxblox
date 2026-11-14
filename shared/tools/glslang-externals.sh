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
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
SrcDir="${1:?usage: glslang-externals.sh <src-glslang dir>}"
cd "$SrcDir"

rm -rf External/spirv-tools External/googletest

# known_good.json pins the commits, so its hash is the cache key.
Key="$(sha256sum known_good.json | cut -c1-16)"
Cache="$ROOT/compat/sources/glslang-externals-$Key.tar"

if [ -f "$Cache" ]; then
    echo ":: Using cached glslang externals"
    tar -xf "$Cache"
    exit 0
fi

python3 update_glslang_sources.py
mkdir -p "$(dirname "$Cache")"
tar -cf "$Cache.part" External && mv -f "$Cache.part" "$Cache"
echo ":: Cached glslang externals for future builds"
