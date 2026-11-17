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

# Builds libwpe and wpebackend-fdo into the existing dependency prefix, then configures
# WPE WebKit against it. Configure only: the question is whether PORT=WPE can be built
# here at all, and compiling the engine would answer that hours later than necessary.
set -euo pipefail

PREFIX=/opt/tuxblox-webview
SOURCES_DIR=/sources
export PKG_CONFIG_PATH="$PREFIX/lib/pkgconfig:$PREFIX/lib/x86_64-linux-gnu/pkgconfig:$PREFIX/share/pkgconfig"
export LDFLAGS="-Wl,-rpath,$PREFIX/lib -Wl,-rpath,$PREFIX/lib/x86_64-linux-gnu -L$PREFIX/lib -L$PREFIX/lib/x86_64-linux-gnu"
export CPPFLAGS="-I$PREFIX/include"
export CFLAGS="-O2"
export CXXFLAGS="-O2"
export PATH="$PREFIX/bin:$PATH"
export MESON_PACKAGE_CACHE_DIR="$SOURCES_DIR"
JOBS="${JOBS:-$(nproc)}"

set -a; . "$SOURCES_DIR/versions.env"; set +a

step() { printf '\n:: %s\n' "$1"; }

# A real unified diff, so a context mismatch on a version bump fails here rather than
# silently misapplying.
applyPatches() {
    local name="$1" p
    for p in /patches/"$name"-*.patch; do
        [ -e "$p" ] || continue
        printf '   applying %s\n' "$(basename "$p")"
        patch -p1 -d "/build/$name" < "$p"
    done
}

mesonInto() {
    local name="$1" archive="$2"; shift 2
    step "Building $name"
    rm -rf "/build/$name"
    mkdir -p "/build/$name"
    tar -xf "$SOURCES_DIR/$archive" -C "/build/$name" --strip-components=1
    applyPatches "$name"
    meson setup "/build/$name/_b" "/build/$name" --prefix="$PREFIX" --buildtype=release "$@"
    ninja -C "/build/$name/_b" -j"$JOBS" install
}

mesonInto libwpe "libwpe-${LIBWPE_VERSION}.tar.xz"
mesonInto wpebackend-fdo "wpebackend-fdo-${WPEBACKEND_FDO_VERSION}.tar.xz"

# A re-run would otherwise delete a part-finished build, which is how an interrupted one is lost.
if [ -f /build/wpewebkit/_build/build.ninja ] && [ -z "${FORCE_UNPACK:-}" ]; then
    step "Reusing the existing WPE WebKit tree (FORCE_UNPACK=1 to start over)"
else

step "Unpacking WPE WebKit ${WPEWEBKIT_VERSION}"
rm -rf /build/wpewebkit
mkdir -p /build/wpewebkit
tar -xf "$SOURCES_DIR/wpewebkit-${WPEWEBKIT_VERSION}.tar.xz" -C /build/wpewebkit --strip-components=1
applyPatches wpewebkit

fi

step "Configuring PORT=WPE"
cmake -S /build/wpewebkit -B /build/wpewebkit/_build -GNinja \
    -DPORT=WPE -DCMAKE_INSTALL_PREFIX="$PREFIX" \
    -DCMAKE_BUILD_TYPE=Release -DENABLE_INTROSPECTION=OFF -DENABLE_DOCUMENTATION=OFF \
    -DENABLE_MINIBROWSER=OFF \
    -DUSE_AVIF=OFF -DUSE_JPEGXL=OFF -DENABLE_SPEECH_SYNTHESIS=OFF -DUSE_LIBBACKTRACE=OFF \
    -DUSE_SYSTEM_SYSPROF_CAPTURE=OFF \
    -DENABLE_SPELLCHECK=OFF -DENABLE_GAMEPAD=OFF -DUSE_LIBHYPHEN=OFF \
    -DENABLE_WEBDRIVER=OFF -DENABLE_PDFJS=OFF -DENABLE_XSLT=OFF \
    -DUSE_ATK=OFF -DUSE_LCMS=OFF \
    -DCMAKE_PREFIX_PATH="$PREFIX" -DICU_ROOT="$PREFIX" \
    -DCMAKE_C_COMPILER_LAUNCHER=ccache -DCMAKE_CXX_COMPILER_LAUNCHER=ccache \
    -DENABLE_BUBBLEWRAP_SANDBOX=OFF \
    -DENABLE_JOURNALD_LOG=OFF

step "GATE PASSED: PORT=WPE configured"
echo "Build dir: /build/wpewebkit/_build"

# Deliberately no cmake --install: the dependency prefix is shared with the GTK bundle and
# this is an experiment. Compiling is what proves the port links.
if [ -n "${BUILD_ENGINE:-}" ]; then
    step "Building WPE WebKit (hours)"
    cmake --build /build/wpewebkit/_build -j"$JOBS"
    step "BUILD PASSED: WPE WebKit compiled and linked"
    ls -la /build/wpewebkit/_build/lib/*.so* 2>/dev/null | head
fi
