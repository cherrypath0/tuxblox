#!/usr/bin/env bash

# A CLI launcher for TuxBlox
# NOT A FULL LAUNCHER! It will only work if you actually built the compatibility layer here.

cd "$(dirname "$0")" || exit 1
mkdir -p ~/.tuxblox
mkdir -p build/runtime
mkdir -p build/installers
mkdir -p build/logs

PREFIX_PATH="$(pwd)/build/runtime"
protonLogDir="$(pwd)/build/logs"

choice="$1"
if [ -z "$choice" ]; then
    echo "Which do you want to launch? [player/studio]"
    read -r choice
fi

protocolUri="$2"
userAgent="TuxBlox-Client/1.0" # maybe update the version in the user agent some day? has been 1.0 since day 1 lol

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
    local wv2Installer="build/installers/MicrosoftEdgeWebview2Setup.exe" # we override the exe file for the setup with our own, i wonder what effect this has if any
    mkdir -p "$(dirname "$wv2Installer")"
    if [ ! -f "$wv2Installer" ]; then
        curl -fL -A "$userAgent" -o "$wv2Installer" "https://go.microsoft.com/fwlink/p/?LinkId=2124703" || {
            echo "WebView2 Runtime download failed, continuing anyway (Roblox's own installer will retry this)."
            return 1
        }
    fi
    local stagedWv2Installer
    stagedWv2Installer=$(stageInPrefix "$wv2Installer")
    timeout 180 env "${protonEnv[@]}" "$(pwd)/build/compat/main" run "$stagedWv2Installer" /silent /install
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
        installer="build/installers/RobloxPlayerInstaller.exe"
        url="https://setup.rbxcdn.com/RobloxPlayerInstaller.exe"
        needsWebView2=1
        ;;
    1|s|S|studio|Studio|STUDIO)
        exePath=$(findExe "RobloxStudioBeta.exe" "build/runtime/pfx/drive_c/users/user/Desktop/Roblox Studio.lnk")
        label="Roblox Studio"
        installer="build/installers/RobloxStudioInstaller.exe"
        url="https://setup.rbxcdn.com/RobloxStudioInstaller.exe"
        needsWebView2=1
        # previously this line below existed, but it does absolutely nothing, so I removed it.
        # dxvkConfig="dxgi.enableDummyCompositionSwapchain=True"
        ;;
    *) exit 1 ;;
esac

protonEnv=(
    "TUXBLOX_PREFIX=$PREFIX_PATH"
    "TUXBLOX_LOG_DIR=$protonLogDir"
    "TUXBLOX_WEBVIEW_GPU=${TUXBLOX_WEBVIEW_GPU:-1}"
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

describeExit() { # just in case i forget
    case "$1" in
        0)           echo "OK" ;;
        -1073741819) echo "0xC0000005 access violation" ;;
        -1073741790) echo "0xC0000022 access denied" ;;
        -1073741571) echo "0xC00000FD stack overflow" ;;
        -1073741510) echo "0xC000013A interrupted" ;;
        -2147467260) echo "0x80004004 aborted" ;;
        *)           echo "unrecognised" ;;
    esac
}

exitCapture=$(mktemp -t tuxblox-exit.XXXXXX)
trap 'rm -f "$exitCapture"' EXIT

teeTargets=("$exitCapture")
[ -n "$traceLogFile" ] && teeTargets+=("$traceLogFile")

echo "==== START OF OUTPUT ===="
if [ -n "$protocolUri" ]; then
    env "${protonEnv[@]}" "$(pwd)/build/compat/main" run "$exePath" "$protocolUri" 2>&1 | tee -a "${teeTargets[@]}"
else
    env "${protonEnv[@]}" "$(pwd)/build/compat/main" run "$exePath" 2>&1 | tee -a "${teeTargets[@]}"
fi
wrapperCode=${PIPESTATUS[0]}
echo "====  END OF OUTPUT  ===="

realCode=$(sed -n 's/.*TUXBLOX_REAL_EXIT_CODE=\(-\{0,1\}[0-9]\{1,\}\).*/\1/p' "$exitCapture" | tail -1)

if [ -n "$realCode" ]; then
    exitCode=$realCode
    echo "Exit code: $exitCode ($(describeExit "$exitCode"))"
elif [ "$wrapperCode" -eq 0 ]; then
    exitCode=0
    echo "Exit code: 0 (OK)"
else
    exitCode=$wrapperCode
    echo "Exit code: $exitCode (from compat)"
fi