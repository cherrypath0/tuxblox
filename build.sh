#!/bin/bash
set -eo pipefail
cd "$(dirname "$0")"

packages=(
    python3
    podman
    curl
)

echo ":: Cleaning up previous build logs"
rm -f build.log

echo ":: Cleaning up old prefix directory"
rm -rf prefix
mkdir -p prefix

echo ":: Cleaning up old proton build"
rm -rf ProtonBuild
mkdir ProtonBuild
cd ProtonBuild

echo ":: Updating dependencies"

declare -A override_apt=()
declare -A override_dnf=()
declare -A override_pacman=()
declare -A override_brew=(
    [python3]="python@3"
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
to_upgrade=()
for pkg in "${packages[@]}"; do
    resolved_pkg="$(pkg_name "$pm" "$pkg")"
    if is_installed "$pm" "$resolved_pkg"; then
        echo ":: $resolved_pkg already installed, will check for updates"
        to_upgrade+=("$resolved_pkg")
    else
        to_install+=("$resolved_pkg")
    fi
done
 
case "$pm" in
    apt)
        sudo apt-get update
        [ ${#to_install[@]} -gt 0 ] && sudo apt-get install -y "${to_install[@]}"
        [ ${#to_upgrade[@]} -gt 0 ] && sudo apt-get install -y --only-upgrade "${to_upgrade[@]}"
        ;;
    dnf)
        [ ${#to_install[@]} -gt 0 ] && sudo dnf install -y "${to_install[@]}"
        [ ${#to_upgrade[@]} -gt 0 ] && sudo dnf upgrade -y "${to_upgrade[@]}"
        ;;
    pacman)
        sudo pacman -Sy --needed --noconfirm "${packages[@]}"
        ;;
    brew)
        brew update
        [ ${#to_install[@]} -gt 0 ] && brew install "${to_install[@]}"
        for p in "${to_upgrade[@]}"; do
            brew outdated "$p" &>/dev/null && brew upgrade "$p" || echo ":: $p already up to date"
        done
        ;;
    apk)
        sudo apk update
        [ ${#to_install[@]} -gt 0 ] && sudo apk add "${to_install[@]}"
        [ ${#to_upgrade[@]} -gt 0 ] && sudo apk add --upgrade "${to_upgrade[@]}"
        ;;
    *)
        echo "!! Unsupported or undetected package manager. Install manually: ${packages[*]}"
        exit 1
        ;;
esac

echo ":: Configuring Proton"
./../ProtonSource/configure.sh

echo ":: First-pass build"
make 2>&1 | tee ../build.log || true

echo ":: Fetching external sources"
(cd src-glslang && rm -rf External/spirv-tools External/googletest && python3 update_glslang_sources.py)

echo ":: Initializing nested submodules"
(cd src-dxvk-nvapi && git submodule update --init --recursive)

echo ":: Resuming build"
make 2>&1 | tee -a ../build.log

echo ":: Clearing up unnecessary junk"
shopt -s nullglob
rm -rf obj-* dst-*
shopt -u nullglob