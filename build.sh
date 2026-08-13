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

set -eo pipefail
cd "$(dirname "$0")"

debug=1
force_deps=0
for arg in "$@"; do
    case "$arg" in
        --nodebug) debug=0 ;;
        --reinstall-deps) force_deps=1 ;;
    esac
done

packages=(
    python3
    podman
    curl
    gcc
    uidmap
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
    if [[ ! -f ../build.log ]]; then
        echo "!! No build.log found to inspect." >&2
        return
    fi

    local pattern='error:|undefined reference|fatal error|No such file|cannot find|ld returned|segmentation fault|core dumped|killed|signal 1[0-9]|Error [0-9]+$'
    local total_lines last_match_from_end last_match_line

    total_lines=$(wc -l < ../build.log)
    last_match_from_end=$(tac ../build.log | grep -n -E -i -m1 "$pattern" | cut -d: -f1)

    if [[ -z "$last_match_from_end" ]]; then
        echo "!! Could not isolate a specific error pattern. Last 60 lines of build.log:" >&2
        tail -n 60 ../build.log >&2
        return
    fi

    last_match_line=$((total_lines - last_match_from_end + 1))

    echo "!! Likely root cause (build.log line $last_match_line), with context:" >&2
    echo "----------------------------------------------------------------------" >&2
    sed -n "$((last_match_line > 5 ? last_match_line - 5 : 1)),$((last_match_line + 15))p" ../build.log >&2
    echo "----------------------------------------------------------------------" >&2
    echo "!! Full log at build.log if you need more context." >&2
}

step "Cleaning up previous build logs"
rm -f build.log

step "Cleaning up old prefix directory"
rm -rf runtime
mkdir -p runtime

step "Reloading submodules and applying patches"
run_step "apply_patches" strict ./apply_patches.sh

step "Cleaning up old proton build"
rm -rf ProtonBuild
mkdir ProtonBuild
cd ProtonBuild

step "Updating dependencies"

declare -A override_apt=()
# uidmap provides newuidmap/newgidmap (needed for podman's --userns=keep-id); on
# dnf/pacman hosts those ship as part of shadow-utils/shadow, which are normally
# already installed by default, so map to those instead of a nonexistent "uidmap"
# package name.
declare -A override_dnf=(
    [uidmap]="shadow-utils"
)
declare -A override_pacman=(
    [uidmap]="shadow"
)
declare -A override_brew=(
    [python3]="python@3"
    # macOS podman runs rootless containers inside a Linux VM ("podman machine"),
    # which already has newuidmap/newgidmap set up internally -- there's no separate
    # host formula for this. Alias to the already-required "podman" formula so the
    # check is a harmless no-op instead of failing on a formula that doesn't exist.
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
run_step "check_podman" strict bash -c 'podman info >/dev/null && podman build -t tuxblox-old-glibc-builder -f ../build-container/Containerfile ../build-container'

step "Configuring Proton (ccache enabled for faster rebuilds)"
run_step "configure_proton" strict bash -c './../ProtonSource/configure.sh --enable-ccache'

step "First-pass build (1/4) (using $JOBS parallel jobs)"
run_step "first_pass_build" allow-fail bash -c "set -o pipefail; make -j$JOBS 2>&1 | tee ../build.log"

step "Fetching external sources (2/4)"
run_step "fetch_external_sources" strict bash -c 'cd src-glslang && rm -rf External/spirv-tools External/googletest && python3 update_glslang_sources.py'

step "Initializing nested submodules (3/4)"
run_step "init_submodules" strict bash -c 'cd ../ProtonSource/dxvk-nvapi && git submodule update --init --recursive'

step "Ensuring wine x86_64 is configured"
run_step "configure_wine_x86_64" strict bash -c 'make wine-x86_64-configure'

step "Ensuring x86_64 NLS data is built"
run_step "build_x86_64_nls" strict bash -c 'cd obj-wine-x86_64 && make nls/locale.nls'

step "Resuming build (4/4) (using $JOBS parallel jobs)"
run_step "resume_build" strict bash -c "set -o pipefail; make -j$JOBS 2>&1 | tee -a ../build.log"

step "Compiling proton launcher to a native binary (podman, old-glibc baseline)"
run_step "compile_proton_native" strict bash -c '
    set -e
    podman build -t tuxblox-old-glibc-builder -f ../build-container/Containerfile ../build-container

    workdir="$(mktemp -d)"
    cp dist/proton "$workdir/proton.py"
    cp dist/filelock.py "$workdir/filelock.py"
    mkdir -p "$workdir/out"

    podman run --rm --userns=keep-id -v "$workdir:/work:Z" -w /work tuxblox-old-glibc-builder \
        nuitka --standalone --no-progressbar \
        --output-filename=proton --output-dir=/work/out /work/proton.py

    rm -f dist/proton dist/filelock.py
    cp -a "$workdir/out/proton.dist/." dist/
    rm -rf "$workdir"
'

step "Recording build version"
if [[ -z "$TUXBLOX_BUILD_VERSION" ]]; then
    read -rp "Enter version for this build: " TUXBLOX_BUILD_VERSION
    while [[ -z "$TUXBLOX_BUILD_VERSION" ]]; do
        read -rp "Version cannot be empty. Enter version for this build: " TUXBLOX_BUILD_VERSION
    done
fi
echo "$(date '+%s') ${TUXBLOX_BUILD_VERSION}" > dist/version

step "Clearing up unnecessary junk"
shopt -s nullglob
rm -rf obj-* dst-*
shopt -u nullglob

echo -e "Successfully compiled TuxBlox-Proton!"