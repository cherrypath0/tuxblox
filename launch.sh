#!/bin/bash
cd "$(dirname "$0")" || exit 1
mkdir -p ~/.local/share/tuxblox/steamapps
mkdir -p runtime
mkdir -p logs
export STEAM_COMPAT_DATA_PATH="$(pwd)/runtime"
export STEAM_COMPAT_CLIENT_INSTALL_PATH="$HOME/.local/share/tuxblox"
export PROTON_LOG_DIR="$(pwd)/logs"
export PROTON_LOG=1 
export PROTON_LOG_LEVEL=debug 
export DXVK_ASYNC=1
export WINEDEBUG=+relay,+seh,+loaddll,+timestamp,+pid

choice="$1"
if [ -z "$choice" ]; then
    echo "Which do you want to launch? [player/studio]"
    read -r choice
fi

userAgent="TuxBlox-Client/1.0"

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
        exePath=$(findExe "RobloxPlayerBeta.exe" "runtime/pfx/drive_c/users/steamuser/Desktop/Roblox Player.lnk")
        label="Roblox Client"
        installer="RobloxPlayer/RobloxPlayerInstaller.exe"
        url="https://setup.rbxcdn.com/RobloxPlayerInstaller.exe"
        ;;
    1|s|S|studio|Studio|STUDIO)
        exePath=$(findExe "RobloxStudioBeta.exe" "runtime/pfx/drive_c/users/steamuser/Desktop/Roblox Studio.lnk")
        label="Roblox Studio"
        installer="RobloxStudio/RobloxStudioInstaller.exe"
        url="https://setup.rbxcdn.com/RobloxStudioInstaller.exe"
        ;;
    *) exit 1 ;;
esac

if [ -z "$exePath" ]; then
    echo "$label not found. Running installer..."
    mkdir -p "$(dirname "$installer")"
    [ ! -f "$installer" ] && curl -fL -A "$userAgent" -o "$installer" "$url"
    exePath="$installer"
fi

echo "Launching $label from: $exePath"
echo "==== START OF OUTPUT ===="
"$(pwd)/ProtonBuild/dist/proton" run "$exePath"
echo "====  END OF OUTPUT  ===="
echo "Exit code: $?"