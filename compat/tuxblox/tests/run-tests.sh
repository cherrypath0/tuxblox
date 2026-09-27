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

set -euo pipefail

# The compatibility layer is built by one g++ over everything under src/, so it
# has no test target. These compile against the units they cover directly.
cd "$(dirname "$0")"
out="$(mktemp -d)"
trap 'rm -rf "$out"' EXIT

g++ -std=c++17 -O0 -g -Wall -Wextra -UNDEBUG \
    -o "$out/test_account_name" test_account_name.cpp ../src/support/account_name.cpp
"$out/test_account_name"

g++ -std=c++17 -O0 -g -Wall -Wextra -UNDEBUG \
    -o "$out/test_account_migration" test_account_migration.cpp ../src/support/account_name.cpp
"$out/test_account_migration"

g++ -std=c++17 -O0 -g -Wall -Wextra -UNDEBUG \
    -o "$out/test_host_folders" test_host_folders.cpp ../src/support/host_folders.cpp
"$out/test_host_folders"

./check-rule-parity.py

printf 'compat tests: all passed\n'
