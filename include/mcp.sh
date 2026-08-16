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

cd "$(dirname "$0")" || exit 1

PREFIX_PATH="$(pwd)/runtime"
protonLogDir="$(pwd)/logs"

findExe() {
    local targetExe="$1"
    local lnkPath="$2"
    if [ -f "$lnkPath" ]; then
        local targetWin
        targetWin=$(strings -a "$lnkPath" 2>/dev/null | grep -oiE "[A-Za-z]:\\\\.*$targetExe" | head -n1)
        [ -z "$targetWin" ] && targetWin=$(strings -el "$lnkPath" 2>/dev/null | grep -oiE "[A-Za-z]:\\\\.*$targetExe" | head -n1)
        if [ -n "$targetWin" ]; then
            local relPath
            relPath=$(printf '%s' "$targetWin" | sed -e 's/\\/\//g' -e 's/^[A-Za-z]://')
            find "runtime/pfx/drive_c" -ipath "*${relPath}" -type f 2>/dev/null | head -n1 && return 0
        fi
    fi

    find "runtime/pfx/drive_c" -name "$targetExe" -type f | grep -v "Installer" | head -n1
}

findStudioMCP() {
    local studioExe
    studioExe=$(findExe "RobloxStudioBeta.exe" "runtime/pfx/drive_c/users/user/Desktop/Roblox Studio.lnk")

    local mcpExe=""

    if [ -n "$studioExe" ] && [ -f "$(dirname "$studioExe")/StudioMCP.exe" ]; then
        mcpExe="$(dirname "$studioExe")/StudioMCP.exe"
    fi

    if [ -z "$mcpExe" ] && [ -f "runtime/pfx/user.reg" ]; then
        local contentFolderWin
        contentFolderWin=$(awk '
            /^\[Software\\\\Roblox\\\\RobloxStudio\]/ { f=1; next }
            /^\[/ { f=0 }
            f && /"ContentFolder"=/ { print; exit }
        ' "runtime/pfx/user.reg" | sed -E 's/.*"ContentFolder"="([^"]*)".*/\1/' | sed 's/\\\\/\\/g')

        if [ -n "$contentFolderWin" ]; then
            local contentFolderRel
            contentFolderRel=$(printf '%s' "$contentFolderWin" | sed -e 's/\\/\//g' -e 's/^[A-Za-z]://')
            local contentFolderUnix
            contentFolderUnix=$(find "runtime/pfx/drive_c" -ipath "*${contentFolderRel}" -type d 2>/dev/null | head -n1)

            if [ -n "$contentFolderUnix" ] && [ -f "$contentFolderUnix/../StudioMCP.exe" ]; then
                mcpExe="$(cd "$contentFolderUnix/.." && pwd)/StudioMCP.exe"
            fi
        fi
    fi

    [ -z "$mcpExe" ] && mcpExe=$(find "runtime/pfx/drive_c" -name "StudioMCP.exe" -type f 2>/dev/null | head -n1)

    printf '%s' "$mcpExe"
}

mcpExe=$(findStudioMCP)

if [ -z "$mcpExe" ]; then
    echo "StudioMCP.exe not found"
    exit 1
fi

mcpExe="$(pwd)/$mcpExe"

protonEnv=(
    "TUXBLOX_PREFIX=$PREFIX_PATH"
    "PROTON_LOG_DIR=$protonLogDir"
    "DXVK_ASYNC=1"
)

# Lives next to proton/ in both homes this script ships to (build/ in the
# repo, ~/.tuxblox when installed), so the path is relative to this script.
env "${protonEnv[@]}" "$(pwd)/proton/main" run "$mcpExe" "$@"