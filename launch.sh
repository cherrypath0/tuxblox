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
mkdir -p ~/.local/share/tuxblox/steamapps
mkdir -p runtime
mkdir -p logs

# Kept as plain (non-exported) shell vars, not `export`ed, so nothing this
# script spawns other than Proton itself (e.g. the `curl` calls below) ever
# sees Wine/Proton/Steam-Play-shaped env vars in its own environment. They're
# only turned into the real STEAM_COMPAT_* names Proton requires as an
# inline env prefix on the actual "proton run" invocations further down.
# See item 33 in plan/plan.txt.
PREFIX_PATH="$(pwd)/runtime"
CLIENT_PATH="$HOME/.local/share/tuxblox"
protonLogDir="$(pwd)/logs"

choice="$1"
if [ -z "$choice" ]; then
    echo "Which do you want to launch? [player/studio]"
    read -r choice
fi

# Set when invoked as a roblox-player:/roblox-studio: URL handler (see
# install-handler.sh); forwarded to the exe verbatim, same as the real
# Windows launcher would receive it via the registry's %1.
protocolUri="$2"

userAgent="TuxBlox-Client/1.0"

# Roblox's own installer bundles a copy of the real Microsoft Edge WebView2
# Runtime bootstrapper and runs it itself on a fresh prefix, but that's a
# blocking, silent, easy-to-miss ~30-120s network-dependent step buried
# inside the installer's own progress bar (real Microsoft binary, downloads
# the actual runtime from Microsoft's CDN -- not something Wine/TuxBlox can
# avoid or speed up, same cost a fresh install pays on real Windows machines
# that don't already have WebView2 preinstalled by the OS). Doing it here
# instead, before Roblox's installer runs, means it happens once per fresh
# prefix with a clearer "installing WebView2" message instead of silently
# eating time inside Roblox's own installer, and Roblox's installer then
# detects it's already present and skips its own copy of this step.
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
    # The bootstrapper's actual runtime install finishes well before the
    # process itself exits (it lingers doing update-service registration
    # afterward) -- bound it so a slow/stuck exit can never block the rest
    # of this script from launching Roblox.
    timeout 180 env "${protonEnv[@]}" "$(pwd)/ProtonBuild/dist/proton" run "$wv2Installer" /silent /install
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
        # Studio's embedded WebView2 (login/create.roblox.com UI) appears to rely
        # on a DXGI composition swapchain; DXVK's CreateSwapChainForComposition
        # returns E_NOTIMPL unless this is enabled. Scoped to Studio only -- see
        # dxgi_factory.cpp's enableDummyCompositionSwapchain option.
        dxvkConfig="dxgi.enableDummyCompositionSwapchain=True"
        ;;
    w|wd|W|WD|shady|winedetector|wine|shadywine)
        exePath="otherapps/winescore.exe"
        label="WineScore"++
        ;;
    *) exit 1 ;;
esac

# Turned into the real STEAM_COMPAT_*/DXVK_* names Proton requires, but only
# for the "proton run" invocations below (via `env`) -- never exported into
# this script's own environment, so it can't leak into the `curl` calls
# above/below or any other non-Proton program this script runs. See item 33
# in plan/plan.txt.
protonEnv=(
    "STEAM_COMPAT_DATA_PATH=$PREFIX_PATH"
    "STEAM_COMPAT_CLIENT_INSTALL_PATH=$CLIENT_PATH"
    "PROTON_LOG_DIR=$protonLogDir"
    "DXVK_ASYNC=1"
)
[ -n "$dxvkConfig" ] && protonEnv+=("DXVK_CONFIG=$dxvkConfig")

[ -n "$needsWebView2" ] && ensureWebView2

if [ -z "$exePath" ]; then
    echo "$label not found. Running installer..."
    mkdir -p "$(dirname "$installer")"
    [ ! -f "$installer" ] && curl -fL -A "$userAgent" -o "$installer" "$url"
    exePath="$installer"
fi

echo "Launching $label from: $exePath"
echo "==== START OF OUTPUT ===="
if [ -n "$protocolUri" ]; then
    env "${protonEnv[@]}" "$(pwd)/ProtonBuild/dist/proton" run "$exePath" "$protocolUri"
else
    env "${protonEnv[@]}" "$(pwd)/ProtonBuild/dist/proton" run "$exePath"
fi
exitCode=$?
echo "====  END OF OUTPUT  ===="
echo "Exit code: $exitCode"