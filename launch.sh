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
mkdir -p ~/.tuxblox/steamapps
mkdir -p runtime
mkdir -p logs

PREFIX_PATH="$(pwd)/runtime"
CLIENT_PATH="$HOME/.tuxblox"
protonLogDir="$(pwd)/logs"

choice="$1"
if [ -z "$choice" ]; then
    echo "Which do you want to launch? [player/studio]"
    read -r choice
fi

protocolUri="$2"
userAgent="TuxBlox-Client/1.0"

# Copies a host-side exe (living outside the prefix entirely, e.g. a freshly
# downloaded installer) into the prefix's own C: before it's ever handed to
# proton/wine. Launching an exe via its raw host path relies on ntdll's
# \??\unix\ bridge, which stopped fully working for the executable IMAGE
# itself once the Z: drive was removed (see item 20 in plan.txt) -- the
# process's own working-directory bridging was fixed separately, but the
# target-exe path itself silently failed to launch at all (no window, no
# trace, just an early exit) rather than erroring visibly. Staging a real
# copy inside C: sidesteps the bridge entirely instead of chasing every edge
# case in it.
stageInPrefix() {
    local srcPath="$1"
    local stageDir
    stageDir="$(pwd)/runtime/pfx/drive_c/TuxBloxStaging"
    mkdir -p "$stageDir"
    local dest="$stageDir/$(basename "$srcPath")"
    cp -f "$srcPath" "$dest"
    printf '%s' "$dest"
}

ensureWebView2() {
    local wv2AppDir="runtime/pfx/drive_c/Program Files (x86)/Microsoft/EdgeWebView/Application"
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
    timeout 180 env "${protonEnv[@]}" "$(pwd)/ProtonBuild/dist/proton" run "$stagedWv2Installer" /silent /install
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
            find "runtime/pfx/drive_c" -ipath "*${relPath}" -type f 2>/dev/null | head -n1 && return 0
        fi
    fi

    find "runtime/pfx/drive_c" -name "$targetExe" -type f | grep -v "Installer" | head -n1
}

case "$choice" in
    ""|0|p|P|player|Player|PLAYER)
        exePath=$(findExe "RobloxPlayerBeta.exe" "runtime/pfx/drive_c/users/user/Desktop/Roblox Player.lnk")
        label="Roblox Client"
        installer="RobloxPlayer/RobloxPlayerInstaller.exe"
        url="https://setup.rbxcdn.com/RobloxPlayerInstaller.exe"
        needsWebView2=1
        ;;
    1|s|S|studio|Studio|STUDIO)
        exePath=$(findExe "RobloxStudioBeta.exe" "runtime/pfx/drive_c/users/user/Desktop/Roblox Studio.lnk")
        label="Roblox Studio"
        installer="RobloxStudio/RobloxStudioInstaller.exe"
        url="https://setup.rbxcdn.com/RobloxStudioInstaller.exe"
        needsWebView2=1
        dxvkConfig="dxgi.enableDummyCompositionSwapchain=True"
        ;;
    w|wd|W|WD|shady|winedetector|wine|shadywine)
        exePath="otherapps/winescore.exe"
        label="WineScore"++
        ;;
    *) exit 1 ;;
esac

protonEnv=(
    "STEAM_COMPAT_DATA_PATH=$PREFIX_PATH"
    "STEAM_COMPAT_CLIENT_INSTALL_PATH=$CLIENT_PATH"
    "PROTON_LOG_DIR=$protonLogDir"
    "DXVK_ASYNC=1"
)
[ -n "$dxvkConfig" ] && protonEnv+=("DXVK_CONFIG=$dxvkConfig")

# Opt-in fingerprint tracing: set TUXBLOX_TRACE=1 to turn it on for a
# specific launch (e.g. `TUXBLOX_TRACE=1 ./launch.sh studio` when actively
# investigating something like the WebView2 login hang). Off by default --
# the per-syscall tallying/logging this does has real overhead and was the
# likely cause of reports of slow/unstable launches when it was previously
# always-on. Merged with (not replacing) any WINEDEBUG the caller already
# set, so a manual `WINEDEBUG=+ole,+relay ./launch.sh studio`-style one-off
# investigation still works alongside this.
if [ -n "$TUXBLOX_TRACE" ]; then
    tuxbloxDebugFlags="+tuxblox,+timestamp,+service"
    if [ -n "$WINEDEBUG" ]; then
        protonEnv+=("WINEDEBUG=$WINEDEBUG,$tuxbloxDebugFlags")
    else
        protonEnv+=("WINEDEBUG=$tuxbloxDebugFlags")
    fi

    # One trace log per launch, newest 20 kept (older ones pruned before
    # writing a new one) so this can't grow without bound across repeated
    # investigation runs.
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

if [[ "$exePath" == runtime/pfx/drive_c/* ]]; then
    # Already inside the prefix's C: (an installed game found by findExe) --
    # just make it absolute, since proton's own subprocess cwd is pinned
    # inside the prefix now (see ProtonSource/proton's run_proc) and this
    # script's cwd no longer matches it.
    exePath="$(pwd)/$exePath"
else
    # Lives outside the prefix (a freshly downloaded installer, or
    # otherapps/) -- stage a copy inside C: rather than launching it via its
    # raw host path (see stageInPrefix above for why).
    exePath=$(stageInPrefix "$exePath")
fi

echo "==== START OF OUTPUT ===="
if [ -n "$traceLogFile" ]; then
    # Tracing enabled (TUXBLOX_TRACE=1) -- tee stderr into the trace log as
    # well as passing it through normally.
    if [ -n "$protocolUri" ]; then
        env "${protonEnv[@]}" "$(pwd)/ProtonBuild/dist/proton" run "$exePath" "$protocolUri" 2> >(tee -a "$traceLogFile" >&2)
    else
        env "${protonEnv[@]}" "$(pwd)/ProtonBuild/dist/proton" run "$exePath" 2> >(tee -a "$traceLogFile" >&2)
    fi
else
    # Common path: run directly with no process substitution, so Ctrl+C
    # signals this foreground job the normal, simple way.
    if [ -n "$protocolUri" ]; then
        env "${protonEnv[@]}" "$(pwd)/ProtonBuild/dist/proton" run "$exePath" "$protocolUri"
    else
        env "${protonEnv[@]}" "$(pwd)/ProtonBuild/dist/proton" run "$exePath"
    fi
fi
exitCode=$?
echo "====  END OF OUTPUT  ===="
echo "Exit code: $exitCode"