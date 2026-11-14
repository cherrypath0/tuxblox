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

set -uo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
TARBALL="${1:-$(ls -t "$SCRIPT_DIR"/out/webkitgtk-*-x86_64.tar.xz 2>/dev/null | head -1)}"

if [ -z "$TARBALL" ] || [ ! -f "$TARBALL" ]; then
    echo "Usage: $0 [path/to/webkitgtk-*.tar.xz]" >&2
    echo "No tarball found in $SCRIPT_DIR/out/ and none given on the command line." >&2
    exit 2
fi

echo ":: Verifying $TARBALL"

SYSTEM_PROVIDED_PATTERN='^lib(udev|mount|atomic|vulkan|va|va-drm|asound|pulse|mpg123|sndfile|ogg|vorbis|vorbisenc|opus|speex|theoradec|theoraenc|v4l2|usb-1\.0)\.so'

LIBC_TOOLCHAIN_PATTERN='^(ld-linux|libc|libm|libdl|libpthread|librt|libresolv|libutil|libgcc_s|libstdc\+\+|libz)\.so'

run_sweep() {
    local root="$1"
    find "$root" -type f | while read -r f; do
        magic=$(head -c4 "$f" 2>/dev/null | od -An -tx1 | tr -d ' \n')
        [ "$magic" = "7f454c46" ] || continue
        ldd "$f" 2>/dev/null | grep 'not found' | while read -r line; do
            libname=$(echo "$line" | awk '{print $1}')
            if echo "$libname" | grep -qE "$SYSTEM_PROVIDED_PATTERN|$LIBC_TOOLCHAIN_PATTERN"; then
                echo "EXPECTED (system-provided): $f: $line"
            else
                echo "!!! REAL BUG: $f: $line"
            fi
        done
    done
}

WORKDIR="$(mktemp -d)"
trap 'rm -rf "$WORKDIR"' EXIT
mkdir -p "$WORKDIR/extracted"
tar -xJf "$TARBALL" -C "$WORKDIR/extracted"
EXTRACTED_ROOT="$WORKDIR/extracted/webkitgtk"

RESULTS="$WORKDIR/results.txt"

echo
echo "############################################################"
echo "## 1. Host machine (this repo's own, whatever distro it is)"
echo "############################################################"
run_sweep "$EXTRACTED_ROOT" | tee -a "$RESULTS"

echo
echo "############################################################"
echo "## 2. Bare debian:12-slim container (nothing extra installed)"
echo "############################################################"
if command -v podman >/dev/null 2>&1; then
    podman run --rm -v "$EXTRACTED_ROOT:/verify:ro" debian:12-slim bash -c '
        find /verify -type f | while read -r f; do
            magic=$(head -c4 "$f" 2>/dev/null | od -An -tx1 | tr -d " \n")
            [ "$magic" = "7f454c46" ] || continue
            ldd "$f" 2>/dev/null | grep "not found" | while read -r line; do
                echo "$f: $line"
            done
        done
    ' | while read -r rawline; do
        libname=$(echo "$rawline" | sed -E 's/^[^:]+: //' | awk '{print $1}')
        if echo "$libname" | grep -qE "$SYSTEM_PROVIDED_PATTERN|$LIBC_TOOLCHAIN_PATTERN"; then
            echo "EXPECTED (system-provided): $rawline"
        else
            echo "!!! REAL BUG: $rawline"
        fi
    done | tee -a "$RESULTS"
else
    echo "SKIPPED: podman not available"
fi

echo
echo "############################################################"
echo "## Summary"
echo "############################################################"
echo "Lines starting '!!! REAL BUG:' above are genuine problems -- something in this"
echo "bundle depends on a library not on the documented system-provided boundary"
echo "(README.md) and not vendored into the bundle either. Lines starting 'EXPECTED'"
echo "are accepted, documented assumptions about the target system, not bugs."

if grep -q '^!!! REAL BUG:' "$RESULTS" 2>/dev/null; then
    echo
    echo "RESULT: FAIL -- real, unresolved dependencies found (see '!!! REAL BUG:' lines above)."
    exit 1
fi
echo
echo "RESULT: PASS -- every unresolved dependency (if any) is on the documented system-provided boundary."
