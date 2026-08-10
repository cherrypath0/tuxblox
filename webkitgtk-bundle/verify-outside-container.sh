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

# webkitgtk-bundle/verify-outside-container.sh
#
# Task 8 post-review correction (2026-08-10): every dependency-closure check in this
# plan, through Task 8's own original pass, ran `ldd`/ELF inspection FROM INSIDE the
# tuxblox-webkitgtk-builder container -- where every -dev package this Containerfile
# installs has its matching runtime .so sitting right there on the default library
# search path. That made it structurally impossible to notice a shipped bundle
# depending on one of those system libraries at runtime, since the check itself was
# always run somewhere those libraries happened to already be present. This script
# is the fix: it runs the same dependency-closure sweep from TWO places that are NOT
# the build container --
#   1. a bare `debian:12-slim` container with nothing installed beyond what that
#      base image ships (closest available stand-in for "a minimal target system"),
#   2. this repo's own host machine, whatever distro that happens to be (the
#      closest available stand-in for "a real end-user's desktop" -- this is
#      genuinely how Task 8's review caught the original bugs, run on this
#      project's Arch Linux host).
#
# Usage:
#   webkitgtk-bundle/verify-outside-container.sh [path/to/webkitgtk-*.tar.xz]
#   (defaults to the newest tarball in webkitgtk-bundle/out/)
#
# Exit 0 only if every ELF file's dependency closure resolves cleanly in BOTH
# environments, after excluding the documented "system-provided, assumed present on
# any real target Linux desktop" boundary (X11/Wayland/DRM/gcrypt/nghttp2/tiff/udev,
# extended by the Task 8 post-review correction to also cover selinux/mount/brotli/
# zstd/systemd/atomic/xshmfence, plus bubblewrap/xdg-dbus-proxy as external
# executables rather than linked libraries) -- see README.md's
# "System-provided libraries (not vendored)" section for the full, current list and
# reasoning. Anything NOT on that list that still shows up here is a real bug the
# same class as the ones this correction round fixed (libjpeg/libxml2/libxslt/
# libmanette/libenchant/libhyphen/libproxy) -- it means something in this bundle is
# still depending on a library this plan never decided was safe to assume present.
set -uo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
TARBALL="${1:-$(ls -t "$SCRIPT_DIR"/out/webkitgtk-*-x86_64.tar.xz 2>/dev/null | head -1)}"

if [ -z "$TARBALL" ] || [ ! -f "$TARBALL" ]; then
    echo "Usage: $0 [path/to/webkitgtk-*.tar.xz]" >&2
    echo "No tarball found in $SCRIPT_DIR/out/ and none given on the command line." >&2
    exit 2
fi

echo ":: Verifying $TARBALL"

# Documented system-provided boundary (README.md's "System-provided libraries" list)
# -- an unresolved NEEDED entry matching one of these basenames is expected/accepted,
# not a bug, PROVIDED it's genuinely present on the target this bundle ends up on.
# grep -E pattern, matched against the bare library basename (e.g. "libX11.so.6").
#
# NOTE this list is NOT limited to the Task 8 post-review correction's own additions
# (selinux/mount/brotli/zstd/systemd/atomic/xshmfence) -- it has to also include every
# OLDER system-provided boundary this plan already established (Task 1's foundational
# zlib/libpng/freetype/fontconfig/expat/pixman, Task 4's X11/Wayland/DRM/epoxy/
# xkbcommon/gcrypt/nghttp2/tiff/udev extension), or a bare debian:12-slim container --
# which lacks ALL desktop libraries, not just the ones this correction round is about
# -- produces false "REAL BUG" positives for perfectly legitimate, already-decided
# system dependencies. Caught and fixed while first testing this script itself: an
# earlier draft missing this full list flagged libfreetype/libexpat/libpixman/
# libpng16/libepoxy/libXxf86vm as bugs, none of which have anything to do with this
# correction round.
SYSTEM_PROVIDED_PATTERN='^lib(z|png16|freetype|fontconfig|expat|pixman-1|X11|Xext|Xrender|Xi|Xrandr|Xcursor|Xdamage|Xfixes|Xinerama|Xxf86vm|xcb.*|X11-xcb|wayland-(client|server|egl|cursor)|epoxy|xkbcommon.*|drm.*|gcrypt|nghttp2|tiff|udev|selinux|mount|brotli(dec|enc|common)|zstd|systemd|atomic|xshmfence)\.so'

# The interpreter (ld-linux*.so.2) and libc/libm/libdl/libpthread/librt/libresolv/
# libutil/libgcc_s/libstdc++/libz -- the C library + toolchain runtime itself, on the
# same "present on literally every Linux system" footing as the kernel ABI libraries
# above, just not re-derived from the same X11/Wayland/DRM Task 4 precedent.
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

# All findings (both sections) also get written here, not just echoed, so the final
# exit code can be computed correctly -- the classification loops below run inside
# piped subshells, whose exit status doesn't propagate to this top-level shell.
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
