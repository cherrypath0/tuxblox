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

source "$(dirname "$0")/versions.env"

Prefix=/opt/tuxblox-ui
LibRel=lib/x86_64-linux-gnu
StageDir=/tmp/ui-stack-stage
OutDir=/out
Tarball="ui-stack-${UI_STACK_VERSION}-x86_64.tar.zst"
export PATH="$Prefix/bin:$PATH"

# Libraries safe to assume on an arbitrary Linux desktop with no GTK installed; anything else must be shipped in the stack
HostAllowList=(
    '^libc\.so\.6$' '^libm\.so\.6$' '^libdl\.so\.2$' '^libpthread\.so\.0$' '^librt\.so\.1$' '^libresolv\.so\.2$'
    '^ld-linux-x86-64\.so\.2$'
    '^libX11\.so\.6$' '^libX11-xcb\.so\.1$' '^libXau\.so\.6$' '^libXdmcp\.so\.6$' '^libXext\.so\.6$' '^libXrender\.so\.1$'
    '^libXi\.so\.6$' '^libXrandr\.so\.2$' '^libXcursor\.so\.1$' '^libXdamage\.so\.1$' '^libXfixes\.so\.3$' '^libXinerama\.so\.1$'
    '^libxcb\.so\.1$' '^libxcb-render\.so\.0$' '^libxcb-shm\.so\.0$'
    '^libGL\.so\.1$' '^libEGL\.so\.1$' '^libGLdispatch\.so\.0$'
    '^libz\.so\.1$' '^libexpat\.so\.1$' '^libpng16\.so\.16$' '^libpcre2-8\.so\.0$'
)

isElf() {
    [ "$(head -c4 "$1" | od -An -tx1 | tr -d ' \n')" = "7f454c46" ]
}

isAllowedHostLibrary() {
    local name="$1" pattern
    for pattern in "${HostAllowList[@]}"; do
        if [[ "$name" =~ $pattern ]]; then
            return 0
        fi
    done
    return 1
}

stageRuntime() {
    printf ':: Staging runtime files\n'
    rm -rf "$StageDir"
    mkdir -p "$StageDir/$LibRel" "$StageDir/share/glib-2.0" "$StageDir/fonts"
    cp -a "$Prefix/$LibRel/." "$StageDir/$LibRel/"
    rm -rf "$StageDir/$LibRel/pkgconfig" "$StageDir/$LibRel/glib-2.0/include" "$StageDir/$LibRel/gdk-pixbuf-2.0"
    rm -f "$StageDir/$LibRel"/libgirepository-2.0.so* "$StageDir/$LibRel"/libxkbregistry.so*
    cp -a "$Prefix/fonts/." "$StageDir/fonts/"
    # Relative to this file, so the bundled fonts are found wherever the payload is unpacked; the host's own configuration is only added for fallback fonts
    cat > "$StageDir/fonts/fonts.conf" <<'FONTS'
<?xml version="1.0"?>
<!DOCTYPE fontconfig SYSTEM "urn:fontconfig:fonts.dtd">
<fontconfig>
    <dir prefix="relative">.</dir>
    <cachedir prefix="xdg">tuxblox/fontconfig</cachedir>
    <include ignore_missing="yes">/etc/fonts/fonts.conf</include>
</fontconfig>
FONTS
    cp -a "$Prefix/share/glib-2.0/schemas" "$StageDir/share/glib-2.0/schemas"
    glib-compile-schemas "$StageDir/share/glib-2.0/schemas"
    find "$StageDir" \( -name '*.a' -o -name '*.la' \) -delete
}

rewriteRpaths() {
    local f dir up part parts
    printf ':: Rewriting RPATH on every shipped ELF file\n'
    while IFS= read -r -d '' f; do
        isElf "$f" || continue
        patchelf --print-rpath "$f" >/dev/null 2>&1 || continue
        dir="$(dirname "${f#"$StageDir"/}")"
        up=""
        if [ "$dir" != "." ]; then
            IFS=/ read -ra parts <<<"$dir"
            for part in "${parts[@]}"; do up="../$up"; done
        fi
        strip --strip-unneeded "$f"
        patchelf --force-rpath --set-rpath "\$ORIGIN:\$ORIGIN/${up}${LibRel}" "$f"
    done < <(find "$StageDir/lib" -type f -print0)
}

checkNeededLibraries() {
    local f needed failed=0
    local hostLines=""
    printf ':: Checking every DT_NEEDED entry is shipped or on the host allow-list\n'
    while IFS= read -r -d '' f; do
        isElf "$f" || continue
        while IFS= read -r needed; do
            [ -n "$needed" ] || continue
            if [ -e "$StageDir/$LibRel/$needed" ]; then
                continue
            fi
            if isAllowedHostLibrary "$needed"; then
                hostLines+="$needed"$'\n'
                continue
            fi
            printf 'ERROR: %s needs %s, which is neither shipped nor on the allow-list\n' "${f#"$StageDir"/}" "$needed" >&2
            failed=1
        done < <(readelf -d "$f" 2>/dev/null | sed -n 's/.*(NEEDED).*\[\(.*\)\]/\1/p')
    done < <(find "$StageDir/lib" -type f -print0)
    if [ "$failed" -ne 0 ]; then
        exit 1
    fi
    printf ':: Host libraries the stack relies on:\n'
    printf '%s' "$hostLines" | sort -u | sed 's/^/::   /'
}

maxSymbolVersion() {
    local prefix="$1" f
    while IFS= read -r -d '' f; do
        isElf "$f" || continue
        objdump -T "$f" 2>/dev/null | grep -o "${prefix}_[0-9][0-9.]*" || true
    done < <(find "$StageDir/lib" -type f -print0) | sort -uV | tail -1
}

checkSymbolVersions() {
    local glibcMax glibcxxMax
    printf ':: Checking the symbol versions the stack requires\n'
    glibcMax="$(maxSymbolVersion GLIBC)"
    glibcxxMax="$(maxSymbolVersion GLIBCXX)"
    printf '::   highest GLIBC: %s\n' "${glibcMax:-none}"
    printf '::   highest GLIBCXX: %s\n' "${glibcxxMax:-none}"
    if [ -n "$glibcxxMax" ]; then
        printf 'ERROR: something in the stack needs libstdc++ symbols (%s); it must be linked statically instead\n' "$glibcxxMax" >&2
        exit 1
    fi
    if [ "$(printf '%s\n%s\n' "$glibcMax" "GLIBC_2.31" | sort -V | tail -1)" != "GLIBC_2.31" ]; then
        printf 'ERROR: the stack needs %s, above the glibc 2.31 floor\n' "$glibcMax" >&2
        exit 1
    fi
}

writeTarball() {
    printf ':: Writing %s\n' "$Tarball"
    mkdir -p "$OutDir"
    # The commit rides inside the tarball so the copyright notice names the libadwaita that was actually built, however old the tarball is
    printf '%s\n' "${LIBADWAITA_COMMIT:?build.sh passes the full submodule commit}" > "$StageDir/LIBADWAITA_COMMIT"
    tar -C "$StageDir" -cf - lib share fonts LIBADWAITA_COMMIT | zstd -19 -T0 -q -o "$OutDir/$Tarball" -f
}

writeDevTree() {
    local devDir="$OutDir/dev" pc
    printf ':: Writing the dev tree\n'
    rm -rf "$devDir"
    mkdir -p "$devDir/lib/pkgconfig"
    cp -a "$Prefix/include" "$devDir/include"
    cp -a "$StageDir/$LibRel" "$devDir/$LibRel"
    for arch in "$Prefix/$LibRel"/*/include; do
        [ -d "$arch" ] || continue
        mkdir -p "$devDir/$LibRel/$(basename "$(dirname "$arch")")"
        cp -a "$arch" "$devDir/$LibRel/$(basename "$(dirname "$arch")")/"
    done
    for pc in "$Prefix/$LibRel"/pkgconfig/*.pc "$Prefix"/lib/pkgconfig/*.pc "$Prefix"/share/pkgconfig/*.pc; do
        [ -e "$pc" ] || continue
        sed "s|$Prefix|\${pcfiledir}/../..|g" "$pc" > "$devDir/lib/pkgconfig/$(basename "$pc")"
    done
}

stageRuntime
rewriteRpaths
checkNeededLibraries
checkSymbolVersions
writeTarball
writeDevTree
