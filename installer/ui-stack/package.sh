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

PREFIX=/opt/tuxblox-ui
LIBREL=lib/x86_64-linux-gnu
export PATH="$PREFIX/bin:$PATH"
Stage=/tmp/ui-stack-stage
Out=/out
Tarball="ui-stack-${UI_STACK_VERSION}-x86_64.tar.zst"

rm -rf "$Stage"
mkdir -p "$Stage/$LIBREL" "$Stage/share/glib-2.0" "$Stage/fonts"

printf ':: Staging runtime files\n'
cp -a "$PREFIX/$LIBREL/." "$Stage/$LIBREL/"
rm -rf "$Stage/$LIBREL/pkgconfig"
cp -a "$PREFIX/fonts/." "$Stage/fonts/"
cp -a "$PREFIX/share/glib-2.0/schemas" "$Stage/share/glib-2.0/schemas"
glib-compile-schemas "$Stage/share/glib-2.0/schemas"
find "$Stage" -name '*.a' -delete
find "$Stage" -name '*.la' -delete
rm -rf "$Stage/$LIBREL/glib-2.0/include" "$Stage/$LIBREL/gdk-pixbuf-2.0"
rm -f "$Stage/$LIBREL"/libgirepository-2.0.so* "$Stage/$LIBREL"/libxkbregistry.so*

printf ':: Rewriting RPATH on every shipped ELF file\n'
while IFS= read -r -d '' f; do
    [ "$(head -c4 "$f" | od -An -tx1 | tr -d ' \n')" = "7f454c46" ] || continue
    patchelf --print-rpath "$f" >/dev/null 2>&1 || continue
    dir="$(dirname "${f#"$Stage"/}")"
    up=""
    if [ "$dir" != "." ]; then
        IFS=/ read -ra parts <<<"$dir"
        for _ in "${parts[@]}"; do up="../$up"; done
    fi
    strip --strip-unneeded "$f"
    patchelf --force-rpath --set-rpath "\$ORIGIN:\$ORIGIN/${up}${LIBREL}" "$f"
done < <(find "$Stage/lib" -type f -print0)

printf ':: Checking nothing still names the build prefix\n'
if grep -rl --binary-files=text "$PREFIX" "$Stage" >/dev/null 2>&1; then
    printf 'NOTE: files that still mention %s:\n' "$PREFIX" >&2
    grep -rl --binary-files=text "$PREFIX" "$Stage" >&2 || true
fi

printf ':: Writing %s\n' "$Tarball"
mkdir -p "$Out"
tar -C "$Stage" -cf - lib share fonts | zstd -19 -T0 -q -o "$Out/$Tarball" -f

printf ':: Writing the dev tree\n'
Dev="$Out/dev"
rm -rf "$Dev"
mkdir -p "$Dev/lib/pkgconfig"
cp -a "$PREFIX/include" "$Dev/include"
cp -a "$Stage/$LIBREL" "$Dev/$LIBREL"
for pc in "$PREFIX/$LIBREL"/pkgconfig/*.pc "$PREFIX"/lib/pkgconfig/*.pc "$PREFIX"/share/pkgconfig/*.pc; do
    [ -e "$pc" ] || continue
    sed "s|$PREFIX|\${pcfiledir}/../..|g" "$pc" > "$Dev/lib/pkgconfig/$(basename "$pc")"
done
