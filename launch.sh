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
mkdir -p ~/.tuxblox
mkdir -p build/runtime
mkdir -p logs

PREFIX_PATH="$(pwd)/build/runtime"
protonLogDir="$(pwd)/logs"

choice="$1"
if [ -z "$choice" ]; then
    echo "Which do you want to launch? [player/studio]"
    read -r choice
fi

protocolUri="$2"
userAgent="TuxBlox-Client/1.0"

stageInPrefix() {
    local srcPath="$1"
    local stageDir
    stageDir="$(pwd)/build/runtime/pfx/drive_c/TuxBloxStaging"
    mkdir -p "$stageDir"
    local dest="$stageDir/$(basename "$srcPath")"
    cp -f "$srcPath" "$dest"
    printf '%s' "$dest"
}

ensureWebView2() {
    local wv2AppDir="build/runtime/pfx/drive_c/Program Files (x86)/Microsoft/EdgeWebView/Application"
    if [ -d "$wv2AppDir" ] && [ -n "$(find "$wv2AppDir" -mindepth 1 -maxdepth 1 -type d 2>/dev/null | head -n1)" ]; then
        return 0
    fi

    echo "WebView2 Runtime not found in prefix, preinstalling..."
    local wv2Installer="webview2/MicrosoftEdgeWebview2Setup.exe"
    mkdir -p "$(dirname "$wv2Installer")"
    if [ ! -f "$wv2Installer" ]; then
        curl -fL -A "$userAgent" -o "$wv2Installer" "https://go.microsoft.com/fwlink/p/?LinkId=2124703" || {
            echo "WebView2 Runtime download failed, continuing anyway (Roblox's own installer will retry this)."
            return 1
        }
    fi
    local stagedWv2Installer
    stagedWv2Installer=$(stageInPrefix "$wv2Installer")
    timeout 180 env "${protonEnv[@]}" "$(pwd)/build/proton/main" run "$stagedWv2Installer" /silent /install
}

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
            find "build/runtime/pfx/drive_c" -ipath "*${relPath}" -type f 2>/dev/null | head -n1 && return 0
        fi
    fi

    find "build/runtime/pfx/drive_c" -name "$targetExe" -type f | grep -v "Installer" | head -n1
}

case "$choice" in
    ""|0|p|P|player|Player|PLAYER)
        exePath=$(findExe "RobloxPlayerBeta.exe" "build/runtime/pfx/drive_c/users/user/Desktop/Roblox Player.lnk")
        label="Roblox Client"
        installer="RobloxPlayer/RobloxPlayerInstaller.exe"
        url="https://setup.rbxcdn.com/RobloxPlayerInstaller.exe"
        needsWebView2=1
        ;;
    1|s|S|studio|Studio|STUDIO)
        exePath=$(findExe "RobloxStudioBeta.exe" "build/runtime/pfx/drive_c/users/user/Desktop/Roblox Studio.lnk")
        label="Roblox Studio"
        installer="RobloxStudio/RobloxStudioInstaller.exe"
        url="https://setup.rbxcdn.com/RobloxStudioInstaller.exe"
        needsWebView2=1
        dxvkConfig="dxgi.enableDummyCompositionSwapchain=True"
        ;;
    *) exit 1 ;;
esac

protonEnv=(
    "TUXBLOX_PREFIX=$PREFIX_PATH"
    "PROTON_LOG_DIR=$protonLogDir"
)
[ -n "$dxvkConfig" ] && protonEnv+=("DXVK_CONFIG=$dxvkConfig")

[ -n "$needsWebView2" ] && protonEnv+=("WINEDLLOVERRIDES=webview2loader=b")

if [ -n "$TUXBLOX_TRACE" ]; then
    tuxbloxDebugFlags="+tuxblox,+timestamp,+service"
    if [ -n "$WINEDEBUG" ]; then
        protonEnv+=("WINEDEBUG=$WINEDEBUG,$tuxbloxDebugFlags")
    else
        protonEnv+=("WINEDEBUG=$tuxbloxDebugFlags")
    fi

    traceLogDir="$(pwd)/logs/tuxblox-traces"
    mkdir -p "$traceLogDir"
    find "$traceLogDir" -maxdepth 1 -name 'trace-*.log' -type f 2>/dev/null | sort | head -n -19 | xargs -r rm -f
    traceLogFile="$traceLogDir/trace-$(date +%Y%m%dT%H%M%S).log"
fi

[ -n "$needsWebView2" ] && ensureWebView2

if [ -z "$exePath" ]; then
    echo "$label not found. Running installer..."
    mkdir -p "$(dirname "$installer")"
    [ ! -f "$installer" ] && curl -fL -A "$userAgent" -o "$installer" "$url"
    exePath="$installer"
fi

echo "Launching $label from: $exePath"

if [[ "$exePath" == build/runtime/pfx/drive_c/* ]]; then
    exePath="$(pwd)/$exePath"
else
    exePath=$(stageInPrefix "$exePath")
fi

echo "==== START OF OUTPUT ===="
if [ -n "$traceLogFile" ]; then
    if [ -n "$protocolUri" ]; then
        env "${protonEnv[@]}" "$(pwd)/build/proton/main" run "$exePath" "$protocolUri" 2> >(tee -a "$traceLogFile" >&2)
    else
        env "${protonEnv[@]}" "$(pwd)/build/proton/main" run "$exePath" 2> >(tee -a "$traceLogFile" >&2)
    fi
else
    if [ -n "$protocolUri" ]; then
        env "${protonEnv[@]}" "$(pwd)/build/proton/main" run "$exePath" "$protocolUri"
    else
        env "${protonEnv[@]}" "$(pwd)/build/proton/main" run "$exePath"
    fi
fi
exitCode=$?
echo "====  END OF OUTPUT  ===="
echo "Exit code: $exitCode"