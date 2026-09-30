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

set -eo pipefail
cd "$(dirname "$0")"

# Every dependency fetched here is compiled into the shipped bootstrapper binary, so each fetch is pinned to an immutable-as-possible ref and has NO fallback:
# if a pinned fetch fails we want the build to stop loudly rather than silently substitute an unreviewed newer revision 
# (set -e makes an unguarded failure exit the script).

mkdir -p third_party

if [ ! -f third_party/json.hpp ]; then
    echo ":: Vendoring nlohmann/json (pinned v3.11.3)"
    curl -fsSL -o third_party/json.hpp \
        https://raw.githubusercontent.com/nlohmann/json/v3.11.3/single_include/nlohmann/json.hpp
fi

echo ":: Vendoring complete"
