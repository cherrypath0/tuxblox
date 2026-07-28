#!/bin/bash
set -eo pipefail
cd "$(dirname "$0")"

JOBS="${TUXBLOX_MAKE_JOBS:-$(nproc 2>/dev/null || echo 1)}"

detect_pkg_manager() {
    if command -v apt-get >/dev/null 2>&1; then echo apt
    elif command -v dnf >/dev/null 2>&1; then echo dnf
    elif command -v pacman >/dev/null 2>&1; then echo pacman
    elif command -v brew >/dev/null 2>&1; then echo brew
    elif command -v apk >/dev/null 2>&1; then echo apk
    else echo unknown
    fi
}

install_deps() {
    local mgr
    mgr="$(detect_pkg_manager)"
    case "$mgr" in
        apt)
            sudo apt-get update
            sudo apt-get install -y build-essential cmake pkg-config \
                libsdl2-dev libcurl4-openssl-dev libarchive-dev libssl-dev \
                librsvg2-bin git
            ;;
        dnf)
            sudo dnf install -y gcc-c++ cmake pkgconfig \
                SDL2-devel libcurl-devel libarchive-devel openssl-devel \
                librsvg2-tools git
            ;;
        pacman)
            sudo pacman -S --needed --noconfirm base-devel cmake pkgconf \
                sdl2 curl libarchive openssl librsvg git
            ;;
        brew)
            brew install cmake pkg-config sdl2 curl libarchive openssl librsvg git
            ;;
        apk)
            sudo apk add build-base cmake pkgconf sdl2-dev curl-dev \
                libarchive-dev openssl-dev librsvg git
            ;;
        *)
            echo "!! Unknown package manager. Install manually: cmake, pkg-config, SDL2, libcurl, libarchive, openssl, rsvg-convert (librsvg), git" >&2
            ;;
    esac
}

echo ":: Checking build dependencies"
install_deps

echo ":: Vendoring third-party sources"
./vendor.sh

echo ":: Configuring"
cmake -B build -S . -DCMAKE_BUILD_TYPE=Release

echo ":: Building"
cmake --build build -j"$JOBS"

echo ":: Done. Binary at build/TuxBloxLauncher"
