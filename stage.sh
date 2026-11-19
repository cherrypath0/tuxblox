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

# Helper script to install the build at ~/.tuxblox

set -eo pipefail
cd "$(dirname "$0")"

ORANGE='\e[38;5;208m'
BLUE='\e[34m'
RED='\e[31m'
RESET='\e[0m'

step() {
    echo -e "${BLUE}:: $1${RESET}"
}

fail() {
    echo -e "${RED}!! $1${RESET}" >&2
    exit 1
}

usage() {
    cat <<'EOF'
Usage: ./stage.sh [--carry-prefix | --fresh-prefix]
  --carry-prefix   replace the prefix there with the one in build/runtime/
  --fresh-prefix   delete the prefix there
EOF
}

prefix_mode="keep"
for arg in "$@"; do
    case "$arg" in
        --carry-prefix|--fresh-prefix)
            requested="${arg#--}"
            requested="${requested%-prefix}"
            [[ "$prefix_mode" == "keep" || "$prefix_mode" == "$requested" ]] ||
                fail "--carry-prefix and --fresh-prefix cannot be used at the same time"
            prefix_mode="$requested"
            ;;
        -h|--help) usage; exit 0 ;;
        *) usage >&2; fail "unknown option: $arg" ;;
    esac
done

[[ -n "$HOME" ]] || fail "HOME is not set, cannot find installation path"
INSTALL_DIR="$HOME/.tuxblox"

COMPONENTS=(compat libtuxblox TuxBloxLauncher TuxBloxInstaller studio-mcp)

[[ -d build/native ]] || fail "build/native/ not found, make sure to run make first"
[[ -f build/native/compat/main ]] || fail "build/native/compat/main not found, make sure to run make first"
for component in "${COMPONENTS[@]}"; do
    [[ -e "build/native/$component" ]] || fail "build/native/$component not found, make sure to run make first"
done

if pgrep -f "$INSTALL_DIR/TuxBloxLauncher" >/dev/null 2>&1; then
    fail "An instance of the TuxBlox launcher is already running! Make sure to close it before running this script"
fi

step "Staging into $INSTALL_DIR"
mkdir -p "$INSTALL_DIR"

# Optional experimental macOS backend. `make rosemary` puts it here; installs
# that never built it keep working exactly as before.
if [[ -f build/native/rosemary/rosemary ]]; then
    step "Staging Rosemary (experimental macOS client)"
    mkdir -p "$INSTALL_DIR/rosemary"
    cp -a build/native/rosemary/rosemary "$INSTALL_DIR/rosemary/rosemary"
fi

for component in "${COMPONENTS[@]}"; do
    target="$INSTALL_DIR/$component"
    if [[ -d "build/native/$component" ]]; then
        rm -rf "$target"
    else
        rm -f "$target"
    fi
    cp -a "build/native/$component" "$target"
    echo "   $component"
done

# used to auto replace "proton" and "mcp.sh" here

if [[ -d include ]]; then
    while IFS= read -r -d '' entry; do
        name="$(basename "$entry")"
        skip=0
        for component in "${COMPONENTS[@]}"; do
            [[ "$name" == "$component" ]] && skip=1 && break
        done
        [[ $skip -eq 1 ]] && continue
        [[ -e "build/native/$name" ]] || continue
        rm -rf "${INSTALL_DIR:?}/$name"
        cp -a "build/native/$name" "$INSTALL_DIR/$name"
        echo "   $name"
    done < <(find include -mindepth 1 -maxdepth 1 -print0)
fi

case "$prefix_mode" in
    keep)
        if [[ -d "$INSTALL_DIR/runtime" ]]; then
            step "Keeping the prefix in $INSTALL_DIR"
            prefix_summary="kept"
        else
            step "No prefix in $INSTALL_DIR yet"
            prefix_summary="none"
        fi
        ;;
    carry)
        [[ -d build/runtime ]] || fail "build/runtime/ not found, no prefix to carry"
        step "Carrying build/runtime/ over the prefix in $INSTALL_DIR"
        rm -rf "$INSTALL_DIR/runtime"
        cp -a build/runtime "$INSTALL_DIR/runtime"
        prefix_summary="carried"
        ;;
    fresh)
        if [[ -d "$INSTALL_DIR/runtime" ]]; then
            step "Deleting the prefix in $INSTALL_DIR"
            rm -rf "$INSTALL_DIR/runtime"
        else
            step "No prefix in $INSTALL_DIR to delete"
        fi
        prefix_summary="deleted"
        ;;
esac

staged_version="$("$INSTALL_DIR/compat/main" --version 2>/dev/null || true)"

echo
echo -e "${ORANGE}Staged${RESET} $INSTALL_DIR"
[[ -n "$staged_version" ]] && echo "  Version:   $staged_version"
echo "  Prefix:    $prefix_summary"
echo "  Run:       $INSTALL_DIR/TuxBloxLauncher"
echo "  Installer: $INSTALL_DIR/TuxBloxInstaller"

flatpak_repo=""
[[ -d build/flatpak/repo ]] && flatpak_repo="$PWD/build/flatpak/repo"
appimage=""
if [[ -e build/appimage/TuxBloxInstaller.AppImage ]]; then
    appimage="$PWD/build/appimage/TuxBloxInstaller.AppImage"
elif [[ -d build/appimage/TuxBloxInstaller.AppDir ]]; then
    appimage="unpacked"
fi

if [[ -n "$flatpak_repo" || -n "$appimage" ]]; then
    echo
    echo -e "${ORANGE}Also built${RESET} not staged, each installs its own way"
    [[ -n "$flatpak_repo" ]] &&
        echo "  Flatpak:   flatpak install --user $flatpak_repo net.tuxblox.TuxBlox"
    if [[ "$appimage" == "unpacked" ]]; then
        appdir="$PWD/build/appimage/TuxBloxInstaller.AppDir"
        echo "  AppImage:  not packed, no appimagetool; run APPDIR=$appdir $appdir/AppRun"
    elif [[ -n "$appimage" ]]; then
        echo "  AppImage:  $appimage"
    fi
fi
