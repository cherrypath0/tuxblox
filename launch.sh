#!/bin/bash
cd "$(dirname "$0")" || exit 1
mkdir -p ~/.local/share/tuxblox/steamapps
mkdir -p prefix
export STEAM_COMPAT_DATA_PATH="$(pwd)/prefix"
export STEAM_COMPAT_CLIENT_INSTALL_PATH="$HOME/.local/share/tuxblox"

choice="$1"

if [ -z "$choice" ]; then
    echo "Which do you want to launch? [0/p/player = Player (default), 1/s/studio = Studio]"
    read -r choice
fi

studioLnk="prefix/pfx/drive_c/users/steamuser/Desktop/Roblox Studio.lnk"
studioInstaller="RobloxStudio/RobloxStudioInstaller.exe"
studioInstallerUrl="https://setup.rbxcdn.com/RobloxStudioInstaller.exe"
studioUserAgent="TuxBlox-Client/1.0"

# Pull the embedded Windows target path out of the .lnk and check that the exe it
# points to actually still exists on disk. This catches stale shortcuts left over
# after Roblox updates to a new version-xxxx folder.
#
# The LocalBasePath inside a .lnk's LinkInfo structure is stored as plain ANSI
# bytes (not UTF-16), so plain `strings` finds it; UTF-16LE is tried as a fallback
# in case a given .lnk variant encodes it as Unicode instead.
resolveLnkTarget() {
    local lnkPath="$1"
    local targetWin
    targetWin=$(strings -a "$lnkPath" 2>/dev/null | grep -oi '[A-Za-z]:\\[^"]*RobloxStudioBeta\.exe' | head -n1)
    if [ -z "$targetWin" ]; then
        targetWin=$(strings -el "$lnkPath" 2>/dev/null | grep -oi '[A-Za-z]:\\[^"]*RobloxStudioBeta\.exe' | head -n1)
    fi
    [ -z "$targetWin" ] && return 1

    local relPath
    relPath=$(printf '%s' "$targetWin" | sed -e 's/\\/\//g' -e 's/^[A-Za-z]://')

    # Windows paths in the .lnk are cased like "Users", but Wine prefixes use
    # lowercase "users" on disk, so a plain -f test would false-negative here.
    # Match case-insensitively against what's actually on disk instead.
    find "prefix/pfx/drive_c" -ipath "*${relPath}" -type f 2>/dev/null | head -n1
}

case "$choice" in
    ""|0|p|P|player|Player|PLAYER)
        exePath="RobloxPlayer/RobloxPlayerBeta.exe"
        label="Roblox Client"
        ;;
    1|s|S|studio|Studio|STUDIO)
        studioTargetUnix=""
        if [ -f "$studioLnk" ]; then
            studioTargetUnix="$(resolveLnkTarget "$studioLnk")"
        fi

        if [ -n "$studioTargetUnix" ]; then
            exePath="$studioTargetUnix"
            label="Roblox Studio"
        else
            echo "Existing shortcut missing or stale, reinstalling."
            mkdir -p RobloxStudio
            if [ ! -f "$studioInstaller" ]; then
                echo "Roblox Studio not found. Downloading installer..."
                if ! curl -fL -A "$studioUserAgent" -o "$studioInstaller" "$studioInstallerUrl"; then
                    echo "Download failed."
                    rm -f "$studioInstaller"
                    exit 1
                fi
            fi
            exePath="$studioInstaller"
            label="Roblox Studio Installer"
        fi
        ;;
    *)
        exePath="RobloxPlayer/RobloxPlayerBeta.exe"
        label="Roblox Client"
        ;;
esac

echo "Launching $label"
echo "$exePath"
PROTON_LOG=1 "$(pwd)/ProtonBuild/dist/proton" run "$exePath"
echo "Exit code: $?"