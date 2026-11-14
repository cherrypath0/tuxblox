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
Files="${1:?usage: alias-graphics-driver.sh <build/compat/files>}"

for arch in x86_64 i386; do
    winArch="$arch-windows"
    pe="$Files/lib/wine/$winArch"
    so="$Files/lib/wine/$arch-unix"

    [ -e "$pe/winex11.drv" ] && [ -e "$so/winex11.so" ] || continue
    ln -sfn winex11.drv "$pe/dispumd.dll"
    ln -sfn winex11.so "$so/dispumd.so"

    case "$arch" in
        x86_64) sysDir=system32 ;;
        i386)   sysDir=syswow64 ;;
    esac

    for pfx in "$Files"/share/default_pfx*/drive_c/windows/"$sysDir"; do
        [ -d "$pfx" ] || continue
        ln -sfn "../../../../../lib/wine/$winArch/winex11.drv" "$pfx/dispumd.dll"
        rm -f "$pfx/winex11.drv"
    done
done
