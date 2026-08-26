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

# Builds everything, in order: installer, launcher, proton. All output lands in build/

set -eo pipefail
cd "$(dirname "$0")"

export ROOT="$(pwd)"
export BUILD_LOG="$ROOT/build.log"

debug=1
force_deps=0
log_enabled=0
for arg in "$@"; do
    case "$arg" in
        --nodebug) debug=0 ;;
        --reinstall-deps) force_deps=1 ;;
        --log) log_enabled=1 ;;
    esac
done

version_file="$ROOT/ProtonSource/VERSION"
if [[ -z "$TUXBLOX_BUILD_VERSION" && -r "$version_file" ]]; then
    TUXBLOX_BUILD_VERSION="$(sed -n '1p' "$version_file" | tr -d '[:space:]')"
fi
if [[ -z "$TUXBLOX_CHANNEL" && -r "$version_file" ]]; then
    TUXBLOX_CHANNEL="$(sed -n '2p' "$version_file" | tr -d '[:space:]')"
fi

if [[ -z "$TUXBLOX_BUILD_VERSION" ]]; then
    read -rp "Enter version for this build: " TUXBLOX_BUILD_VERSION
    while [[ -z "$TUXBLOX_BUILD_VERSION" ]]; do
        read -rp "Version cannot be empty. Enter version for this build: " TUXBLOX_BUILD_VERSION
    done
fi

TUXBLOX_CHANNEL="${TUXBLOX_CHANNEL:-stable}"
case "$TUXBLOX_CHANNEL" in
    stable|canary|dev) ;;
    *)
        echo "!! Unknown channel \"$TUXBLOX_CHANNEL\" -- expected stable, canary or dev" >&2
        exit 1
        ;;
esac

echo ":: Building TuxBlox $TUXBLOX_BUILD_VERSION ($TUXBLOX_CHANNEL)"
export TUXBLOX_BUILD_VERSION TUXBLOX_CHANNEL

packages=(
    python3
    podman
    curl
    gcc
    uidmap
    git
)

JOBS="${TUXBLOX_MAKE_JOBS:-$(nproc 2>/dev/null || echo 1)}"

ORANGE='\e[38;5;208m'
BLUE='\e[34m'
RESET='\e[0m'

step() {
    echo -e "${BLUE}:: $1${RESET}"
}

run_step() {
    local label="$1"
    local allow_failure="$2"
    shift 2
    local start_time end_time elapsed minutes seconds status

    start_time=$(date +%s)

    set +e
    if [[ $debug -eq 1 ]]; then
        "$@"
    else
        "$@" >/dev/null
    fi
    status=$?
    set -e

    end_time=$(date +%s)
    elapsed=$((end_time - start_time))
    minutes=$((elapsed / 60))
    seconds=$((elapsed % 60))

    printf "Task completed in: %02d minutes, %02d seconds\n" "$minutes" "$seconds"
    sleep 1

    if [[ $status -ne 0 ]]; then
        if [[ "$allow_failure" == "allow-fail" ]]; then
            echo "!! Step '$label' failed as expected (exit $status) — continuing to next step." >&2
        else
            echo "" >&2
            echo "!! Step '$label' failed (exit $status)" >&2
            report_failure_cause
            exit "$status"
        fi
    fi
}

report_failure_cause() {
    if [[ ! -f "$BUILD_LOG" ]]; then
        echo "!! No build.log to inspect (re-run with --log to capture one)." >&2
        return
    fi

    local pattern='error:|undefined reference|fatal error|No such file|cannot find|ld returned|segmentation fault|core dumped|killed|signal 1[0-9]|Error [0-9]+$'
    local total_lines last_match_from_end last_match_line

    total_lines=$(wc -l < "$BUILD_LOG")
    last_match_from_end=$(tac "$BUILD_LOG" | grep -n -E -i -m1 "$pattern" | cut -d: -f1)

    if [[ -z "$last_match_from_end" ]]; then
        echo "!! Could not isolate a specific error pattern. Last 60 lines of build.log:" >&2
        tail -n 60 "$BUILD_LOG" >&2
        return
    fi

    last_match_line=$((total_lines - last_match_from_end + 1))

    echo "!! Likely root cause (build.log line $last_match_line), with context:" >&2
    echo "----------------------------------------------------------------------" >&2
    sed -n "$((last_match_line > 5 ? last_match_line - 5 : 1)),$((last_match_line + 15))p" "$BUILD_LOG" >&2
    echo "----------------------------------------------------------------------" >&2
    echo "!! Full log at build.log if you need more context." >&2
}

logged() {
    if [[ $log_enabled -eq 1 ]]; then
        "$@" 2>&1 | tee -a "$BUILD_LOG"
    else
        "$@"
    fi
}

apply_patches() {
    echo ":: Reloading submodules to their recorded commit"
    git submodule update --init --force

    local patches_dir="patches"
    local proton_source="ProtonSource/submodules"

    if [[ ! -d "$patches_dir" ]]; then
        return 0
    fi

    echo ":: Applying patches from $patches_dir/"
    shopt -s nullglob
    local applied=0
    local submodule_dir submodule_name target_dir file rel_path dest
    for submodule_dir in "$patches_dir"/*/; do
        submodule_name="$(basename "$submodule_dir")"

        if [[ "$submodule_name" == "wine" ]]; then
            echo "!! patches/wine is not supported -- wine is patched directly in ProtonSource/wine, not through patches/" >&2
            continue
        fi

        target_dir="$proton_source/$submodule_name"
        if [[ ! -d "$target_dir" ]]; then
            echo "!! patches/$submodule_name has no matching $target_dir -- skipping" >&2
            continue
        fi

        while IFS= read -r -d '' file; do
            rel_path="${file#"$submodule_dir"}"
            dest="$target_dir/$rel_path"
            mkdir -p "$(dirname "$dest")"
            cp -f "$file" "$dest"
            echo ":: Applied patch: $file -> $dest"
            applied=$((applied + 1))
        done < <(find "$submodule_dir" -type f -print0)
    done
    shopt -u nullglob

    echo ":: Applied $applied patch file(s)"
}

step "Cleaning up previous build logs"
rm -f "$BUILD_LOG"

step "Cleaning up old build output"
rm -rf build
mkdir -p build/.artifacts build/runtime

step "Updating dependencies"

declare -A override_apt=()

declare -A override_dnf=(
    [uidmap]="shadow-utils"
)
declare -A override_pacman=(
    [uidmap]="shadow"
)
declare -A override_brew=(
    [python3]="python@3"
    [uidmap]="podman"
)
declare -A override_apk=()

detect_pm() {
    if command -v apt-get &>/dev/null; then echo "apt"
    elif command -v dnf &>/dev/null; then echo "dnf"
    elif command -v pacman &>/dev/null; then echo "pacman"
    elif command -v brew &>/dev/null; then echo "brew"
    elif command -v apk &>/dev/null; then echo "apk"
    else echo "unknown"
    fi
}

pkg_name() {
    local pm="$1" pkg="$2"
    local -n override_map="override_${pm}"
    echo "${override_map[$pkg]:-$pkg}"
}

is_installed() {
    local pm="$1" pkg="$2"
    case "$pm" in
        apt)    dpkg -s "$pkg" &>/dev/null ;;
        dnf)    rpm -q "$pkg" &>/dev/null ;;
        pacman) pacman -Qi "$pkg" &>/dev/null ;;
        brew)   brew list "$pkg" &>/dev/null ;;
        apk)    apk info -e "$pkg" &>/dev/null ;;
    esac
}

pm=$(detect_pm)
echo ":: Detected package manager: $pm"
to_install=()
for pkg in "${packages[@]}"; do
    resolved_pkg="$(pkg_name "$pm" "$pkg")"
    if is_installed "$pm" "$resolved_pkg"; then
        echo ":: $resolved_pkg already installed "
    else
        to_install+=("$resolved_pkg")
    fi
done

if [[ ${#to_install[@]} -eq 0 && $force_deps -eq 0 && -t 0 ]]; then
    read -rp ":: All dependencies already installed. Reinstall/check for updates anyway? [y/N] " reinstall_choice || true
    [[ "$reinstall_choice" =~ ^[Yy]$ ]] && force_deps=1
fi

if [[ ${#to_install[@]} -gt 0 || $force_deps -eq 1 ]]; then
    [[ $force_deps -eq 1 ]] && to_install=("${packages[@]}")
    case "$pm" in
        apt)
            sudo apt-get update
            sudo apt-get install -y "${to_install[@]}"
            ;;
        dnf)
            sudo dnf install -y "${to_install[@]}"
            ;;
        pacman)
            sudo pacman -Sy --needed --noconfirm "${to_install[@]}"
            ;;
        brew)
            brew update
            brew install "${to_install[@]}"
            ;;
        apk)
            sudo apk update
            sudo apk add "${to_install[@]}"
            ;;
        *)
            echo "!! Unsupported or undetected package manager. Install manually: ${packages[*]}"
            exit 1
            ;;
    esac
else
    echo ":: All dependencies satisfied, skipping package manager."
fi

step "Checking rootless podman and warming the old-glibc builder image"
run_step "check_podman" strict bash -c 'podman info >/dev/null && podman build -t tuxblox-old-glibc-builder -f Containerfile .'

step "Reloading submodules and applying patches"
run_step "apply_patches" strict apply_patches

step "Building TuxBlox Installer (podman, old-glibc baseline)"
run_step "build_installer" strict logged env TUXBLOX_SKIP_DEPS=1 ./installer/build.sh

step "Staging installer output into build/"

rm -f build/TuxBloxInstaller
mv installer/build build/.artifacts/installer
mv build/.artifacts/installer/TuxBloxInstaller build/TuxBloxInstaller

step "Building TuxBlox Launcher (podman, old-glibc baseline)"
run_step "build_launcher" strict logged env TUXBLOX_SKIP_DEPS=1 ./launcher/build.sh

step "Staging launcher output into build/"

rm -f build/TuxBloxLauncher
rm -rf build/libtuxblox
mv launcher/build build/.artifacts/launcher
mv build/.artifacts/launcher/TuxBloxLauncher build/TuxBloxLauncher

mv build/.artifacts/launcher/libtuxblox build/libtuxblox

PROTON_BUILD_DIR="$ROOT/build/.artifacts/proton"
mkdir -p "$PROTON_BUILD_DIR"
cd "$PROTON_BUILD_DIR"

step "Configuring Proton (ccache enabled for faster rebuilds)"
run_step "configure_proton" strict logged "$ROOT/ProtonSource/configure.sh" --enable-ccache

step "First-pass build (1/4) (using $JOBS parallel jobs)"
run_step "first_pass_build" allow-fail logged make -j"$JOBS"

step "Fetching external sources (2/4)"
run_step "fetch_external_sources" strict bash -c 'cd src-glslang && rm -rf External/spirv-tools External/googletest && python3 update_glslang_sources.py'

step "Initializing nested submodules (3/4)"
run_step "init_submodules" strict bash -c 'cd "$ROOT/ProtonSource/submodules/dxvk-nvapi" && git submodule update --init --recursive'

step "Ensuring wine x86_64 is configured"
run_step "configure_wine_x86_64" strict logged make wine-x86_64-configure

step "Ensuring x86_64 NLS data is built"
run_step "build_x86_64_nls" strict bash -c 'cd obj-wine-x86_64 && make nls/locale.nls'

step "Resuming build (4/4) (using $JOBS parallel jobs)"
run_step "resume_build" strict logged make -j"$JOBS"

step "Compiling proton launcher"
run_step "compile_proton_native" strict bash -c '
    set -e
    podman build -t tuxblox-old-glibc-builder -f "$ROOT/Containerfile" "$ROOT"

    workdir="$(mktemp -d)"
    mkdir -p "$workdir/src/third_party" "$workdir/out"
    cp "$ROOT"/ProtonSource/*.cpp "$ROOT"/ProtonSource/*.h "$workdir/src/"
    cp "$ROOT/ProtonSource/third_party/json.hpp" "$workdir/src/third_party/"

    # Run through sh so the *.cpp glob is expanded inside the container.
    podman run --rm --userns=keep-id -v "$workdir:/work:Z" -w /work/src \
        -e TUXBLOX_BUILD_VERSION -e TUXBLOX_CHANNEL \
        tuxblox-old-glibc-builder sh -c \
        '"'"'g++ -std=c++17 -O2 -Wall -Wextra \
            -DTUXBLOX_VERSION="\"$TUXBLOX_BUILD_VERSION\"" \
            -DTUXBLOX_CHANNEL="\"$TUXBLOX_CHANNEL\"" \
            -o /work/out/main ./*.cpp'"'"'

    install -m 755 "$workdir/out/main" dist/main
    rm -rf "$workdir"
'

step "Clearing up unnecessary junk"
shopt -s nullglob
if [ -z "$TUXBLOX_KEEP_OBJ" ]; then
    rm -rf obj-* dst-*
fi
shopt -u nullglob

cd "$ROOT"

step "Staging Proton"
mv "$PROTON_BUILD_DIR/dist" build/proton

step "Copying licenses into Proton"
cp -a LICENSE build/proton/LICENSE
rm -rf build/proton/third_party_licenses
cp -a third_party_licenses build/proton/third_party_licenses

step "Copying include/ into build/"
if [[ -d include ]]; then
    cp -a include/. build/
fi

echo -e "Successfully built TuxBlox!"
