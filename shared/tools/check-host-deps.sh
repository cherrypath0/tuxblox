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

set -euo pipefail

BuildDir="${1:-build}"
StackDirName="libtuxblox"
GlibcFloor="GLIBC_2.29"

GlibcNames='^(libc|libm|libdl|libpthread|librt|libresolv|libutil|libanl)\.so\.[0-9]+$'
HostGraphicsNames='^lib(X11|X11-xcb|Xau|Xdmcp|Xext|Xrender|Xi|Xrandr|Xcursor|Xdamage|Xfixes|Xinerama|Xtst|Xss|xcb|xcb-render|xcb-shm|xcb-dri2|xcb-dri3|xcb-glx|xcb-present|xcb-sync|xcb-xfixes|xcb-randr|xcb-shape|xcb-util|xshmfence|drm|gbm|GL|GLX|EGL|GLdispatch|OpenGL|wayland-client|wayland-cursor|wayland-egl|xkbcommon|xkbcommon-x11)\.so\.[0-9]+$'

Binaries=(
    "TuxBloxInstaller:no"
    "studio-mcp:no"
    "compat/main:no"
    "TuxBloxLauncher:yes"
)

Failures=0

note() {
    printf '%s\n' "$1"
}

problem() {
    printf '%s\n' "$1" >&2
    Failures=$((Failures + 1))
}

resolveLoaded() {
    local binary="$1"
    ldd "$binary" 2>/dev/null | awk '
        /=>/ {
            path = $3
            if ($3 == "not" && $4 == "found") path = "not-found"
            else if (path == "" || path ~ /^\(0x/) path = "kernel-or-loader"
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

        if [[ "$libPath" == "kernel-or-loader" || "$libName" == *"ld-linux"* ]]; then
            continue
        fi
        if [[ "$libPath" == "not-found" ]]; then
            problem "   !! $name needs $libName and it resolves nowhere"
            continue
        fi

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

while IFS= read -r lib; do
    if objdump -p "$lib" 2>/dev/null | grep -q 'NEEDED.*libstdc++'; then
        problem "!! $(basename "$lib") needs a C++ runtime, so carrying our own in the programs is no longer safe"
    fi
done < <(find "$BuildDir/$StackDirName" -name '*.so*' -type f 2>/dev/null)

for entry in "${Binaries[@]}"; do
    name="${entry%%:*}"
    binary="$BuildDir/$name"
    [[ -x "$binary" ]] || continue
    if objdump -T "$binary" 2>/dev/null | grep -q 'GLIBCXX_'; then
        problem "!! $name still asks the machine for a GLIBCXX version"
    fi
    maxGlibc="$(objdump -T "$binary" 2>/dev/null | grep -oE 'GLIBC_[0-9.]+' | sort -V | tail -1 || true)"
    if [[ -z "$maxGlibc" ]]; then
        note "   $name asks for no C library version at all"
        continue
    fi
    note "   $name needs at most $maxGlibc"
    if [[ "$(printf '%s\n' "${maxGlibc#GLIBC_}" "${GlibcFloor#GLIBC_}" | sort -V | tail -1)" != "${GlibcFloor#GLIBC_}" ]]; then
        problem "!! $name needs $maxGlibc, above the $GlibcFloor floor TuxBlox promises"
    fi
done

if [[ $Failures -gt 0 ]]; then
    printf '!! %s problems\n' "$Failures" >&2
    exit 1
fi
printf ':: every binary loads only what it is expected to\n'
