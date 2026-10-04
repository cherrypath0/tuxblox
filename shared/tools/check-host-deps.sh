#!/usr/bin/env bash
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

# Checks what each shipped binary actually loads, so a change cannot quietly put back a dependency on the machine it runs on.

set -euo pipefail

BuildDir="${1:-build}"
StackDirName="libtuxblox"

# Libraries that have to come from the machine: the C library and the loader.
GlibcNames='^(libc|libm|libdl|libpthread|librt|libresolv|libutil|libanl)\.so\.[0-9]+$'
# Driver- and display-coupled libraries, which must match what is installed rather than be shipped.
HostGraphicsNames='^lib(X11|X11-xcb|Xau|Xdmcp|Xext|Xrender|Xi|Xrandr|Xcursor|Xdamage|Xfixes|Xinerama|Xtst|Xss|xcb|xcb-render|xcb-shm|xcb-dri2|xcb-dri3|xcb-glx|xcb-present|xcb-sync|xcb-xfixes|xcb-randr|xcb-shape|xcb-util|xshmfence|drm|gbm|GL|GLX|EGL|GLdispatch|OpenGL|wayland-client|wayland-cursor|wayland-egl|xkbcommon|xkbcommon-x11)\.so\.[0-9]+$'

# Each binary, and whether it may load the interface stack and the display libraries alongside the C library.
Binaries=(
    "TuxBloxInstaller:no"
    "studio-mcp:no"
    "compat/main:no"
    "TuxBloxLauncher:yes"
    "TuxBloxBootstrapper:yes"
)

Failures=0

note() {
    printf '%s\n' "$1"
}

problem() {
    printf '%s\n' "$1" >&2
    Failures=$((Failures + 1))
}

# Reports "name<TAB>resolved path" for everything the binary loads, with RPATH and RUNPATH honoured.
resolveLoaded() {
    local binary="$1"
    ldd "$binary" 2>/dev/null | awk '
        /=>/ {
            path = $3
            if (path == "" || path == "(0x0)") path = "not-found"
            sub(/^\(/, "", path)
            print $1 "\t" path
            next
        }
        /ld-linux|linux-vdso/ { print $1 "\tkernel-or-loader" }
    '
}

checkBinary() {
    local name="$1" stackAllowed="$2"
    local binary="$BuildDir/$name"
    local stackRoot="$BuildDir/$StackDirName"
    local line libName libPath seen=0 fromStack=0 fromHost=0

    if [[ ! -x "$binary" ]]; then
        problem "!! missing binary: $binary"
        return
    fi

    if ! ldd "$binary" >/dev/null 2>&1; then
        note ":: $name (not dynamically linked, nothing to resolve)"
        return
    fi

    note ":: $name"
    while IFS=$'\t' read -r libName libPath; do
        [[ -z "$libName" ]] && continue
        seen=$((seen + 1))

        # The loader appears either as a bare soname or as the absolute path baked into the binary.
        if [[ "$libPath" == "kernel-or-loader" || "$libName" == *"ld-linux"* ]]; then
            continue
        fi
        if [[ "$libPath" == "not-found" ]]; then
            problem "   !! $name needs $libName and it resolves nowhere"
            continue
        fi

        # Anything resolving inside the install's own folder is shipped, not borrowed.
        if [[ "$libPath" == "$stackRoot/"* || "$libPath" == *"/$StackDirName/"* ]]; then
            fromStack=$((fromStack + 1))
            if [[ "$stackAllowed" != "yes" ]]; then
                problem "   + $name must not use the interface stack, but loads $libName"
            fi
            continue
        fi

        fromHost=$((fromHost + 1))
        if printf '%s' "$libName" | grep -qE "$GlibcNames"; then
            continue
        fi
        if [[ "$stackAllowed" == "yes" ]] && printf '%s' "$libName" | grep -qE "$HostGraphicsNames"; then
            continue
        fi
        problem "   + unexpected dependency on the machine: $libName ($libPath)"
    done < <(resolveLoaded "$binary")

    if [[ $seen -eq 0 ]]; then
        problem "   !! read nothing from $name -- ldd gave no output"
        return
    fi
    note "   $fromStack shipped, $fromHost from the machine"
}

for entry in "${Binaries[@]}"; do
    checkBinary "${entry%%:*}" "${entry##*:}"
done

# Carrying our own C++ runtime is only safe while the interface stack stays pure C: a C++ library
# in there could throw across the boundary into a different runtime than the one it was built with.
for lib in "$BuildDir/$StackDirName"/lib/x86_64-linux-gnu/*.so.* "$BuildDir/$StackDirName"/lib/*.so.*; do
    [[ -e "$lib" ]] || continue
    if objdump -p "$lib" 2>/dev/null | grep -q 'NEEDED.*libstdc++'; then
        problem "!! $(basename "$lib") needs a C++ runtime, so carrying our own in the programs is no longer safe"
    fi
done

# No program may ask the machine for a C++ runtime version any more, and the C library floor must not have risen.
for entry in "${Binaries[@]}"; do
    name="${entry%%:*}"
    binary="$BuildDir/$name"
    [[ -x "$binary" ]] || continue
    if objdump -T "$binary" 2>/dev/null | grep -q 'GLIBCXX_'; then
        problem "!! $name still asks the machine for a GLIBCXX version"
    fi
    maxGlibc="$(objdump -T "$binary" 2>/dev/null | grep -oE 'GLIBC_[0-9.]+' | sort -V | tail -1)"
    note "   $name needs at most ${maxGlibc:-no} C library version"
done

if [[ $Failures -gt 0 ]]; then
    printf '!! %s problems\n' "$Failures" >&2
    exit 1
fi
printf ':: every binary loads only what it is expected to\n'
