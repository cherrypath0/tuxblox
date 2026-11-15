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

set -euo pipefail
source "$(dirname "$0")/versions.env"

PREFIX=/opt/tuxblox-webview
cd "$PREFIX"

gmp_dynsyms="$(nm -D "$PREFIX/lib/libgmp.so")"
if [[ "$gmp_dynsyms" != *__gmpn_cpuvec* ]]; then
    echo "ERROR: the libgmp about to be packaged exports no __gmpn_cpuvec, so it is not a" >&2
    echo "       fat build and will SIGILL on CPUs older than this builder. Refusing to package." >&2
    exit 1
fi

mkdir -p libexec/gdk-pixbuf-tools
cp -a bin/gdk-pixbuf-query-loaders libexec/gdk-pixbuf-tools/

cp -a bin/webview2loader-host libexec/

echo ":: Relocating bundle's own libEGL/libGL/libGLESv1_CM/libGLESv2 into gl-fallback/"
mkdir -p lib/x86_64-linux-gnu/gl-fallback
moved=0
if [ -e share/glvnd/egl_vendor.d/50_mesa.json ]; then
    echo ":: glvnd vendor description present -- loader stays resolvable, nothing relocated"
else
for pattern in 'libEGL.so*' 'libGL.so*' 'libGLESv1_CM.so*' 'libGLESv2.so*'; do
    for f in lib/x86_64-linux-gnu/$pattern; do
        [ -e "$f" ] || continue
        mv -f "$f" lib/x86_64-linux-gnu/gl-fallback/
        moved=$((moved + 1))
    done
done
fi
if [ "$moved" -eq 0 ] && [ ! -e lib/x86_64-linux-gnu/share/glvnd ] \
   && [ ! -e share/glvnd/egl_vendor.d/50_mesa.json ]; then
    echo "!! Mesa installed neither a libEGL.so.1 to relocate nor a glvnd vendor" >&2
    echo "!! description at share/glvnd/egl_vendor.d/50_mesa.json -- nothing would" >&2
    echo "!! point this bundle's WebKit at its own GL, so it would fall back to the" >&2
    echo "!! host's driver on every machine. Check Mesa's meson configuration." >&2
    exit 1
fi

echo ":: Dropping the build-prefix cachedir from etc/fonts/fonts.conf"
if [ -f etc/fonts/fonts.conf ]; then
    sed -i "\|<cachedir>$PREFIX/var/cache/fontconfig</cachedir>|d" etc/fonts/fonts.conf
    if grep -q "$PREFIX" etc/fonts/fonts.conf; then
        echo "ERROR: etc/fonts/fonts.conf still references the build prefix $PREFIX" >&2
        exit 1
    fi
fi

echo ":: Rewriting RPATH/RUNPATH on every ELF file in the packaged tree"
find lib libexec etc share -type f \
    -not -path 'libexec/installed-tests/*' -not -path 'share/installed-tests/*' \
    -print0 2>/dev/null | while IFS= read -r -d '' f; do
    magic=$(head -c4 "$f" 2>/dev/null | od -An -tx1 | tr -d ' \n')
    [ "$magic" = "7f454c46" ] || continue

    patchelf --print-rpath "$f" >/dev/null 2>&1 || continue

    while IFS= read -r needed; do
        case "$needed" in
            "$PREFIX"/*)
                patchelf --replace-needed "$needed" "$(basename "$needed")" "$f"
                ;;
        esac
    done < <(patchelf --print-needed "$f" 2>/dev/null)

    dir=$(dirname "$f")
    depth=$(python3 -c "import sys; d=sys.argv[1]; print(0 if d=='.' else len(d.split('/')))" "$dir")
    up=""
    for ((i = 0; i < depth; i++)); do up="../$up"; done
    strip --strip-unneeded "$f"

    patchelf --force-rpath --set-rpath "\$ORIGIN:\$ORIGIN/${up}lib:\$ORIGIN/${up}lib/x86_64-linux-gnu" "$f"

done

mkdir -p /out
tar -cJf "/out/webkitgtk-${WEBKITGTK_VERSION}-x86_64.tar.xz" \
    --sort=name --mtime="@${SOURCE_DATE_EPOCH:?build.sh passes the commit date}" \
    --owner=0 --group=0 --numeric-owner \
    --exclude='libexec/installed-tests' --exclude='share/installed-tests' \
    --exclude='*.a' --exclude='*.pc' \
    --exclude='share/locale' --exclude='share/man' --exclude='share/info' \
    --exclude='share/doc' --exclude='share/gtk-doc' --exclude='share/gir-1.0' \
    --exclude='lib/libtiffxx.so*' --exclude='lib/libgnutlsxx.so*' \
    --exclude='lib/libturbojpeg.so*' --exclude='lib/libexslt.so*' \
    --exclude='lib/libicuio.so*' --exclude='lib/libicutest.so*' --exclude='lib/libicutu.so*' \
    --exclude='lib/x86_64-linux-gnu/cairo' \
    --exclude='lib/x86_64-linux-gnu/libgirepository-2.0.so*' \
    --exclude='lib/x86_64-linux-gnu/libgthread-2.0.so*' \
    --exclude='lib/x86_64-linux-gnu/libharfbuzz-cairo.so*' \
    --exclude='lib/x86_64-linux-gnu/libharfbuzz-gobject.so*' \
    --exclude='lib/x86_64-linux-gnu/libharfbuzz-gpu.so*' \
    --exclude='lib/x86_64-linux-gnu/libharfbuzz-raster.so*' \
    --exclude='lib/x86_64-linux-gnu/libharfbuzz-vector.so*' \
    --exclude='lib/x86_64-linux-gnu/libgstbadaudio-1.0.so*' \
    --exclude='lib/x86_64-linux-gnu/libgstcheck-1.0.so*' \
    --exclude='lib/x86_64-linux-gnu/libgstplay-1.0.so*' \
    --exclude='lib/x86_64-linux-gnu/libgstplayer-1.0.so*' \
    --exclude='lib/x86_64-linux-gnu/libgsttranscoder-1.0.so*' \
    --exclude='lib/x86_64-linux-gnu/libgstwebrtc-1.0.so*' \
    --exclude='*.la' --exclude='lib/cmake' --exclude='lib/icu' \
    --exclude='share/aclocal' --exclude='share/pkgconfig' --exclude='share/cmake' \
    --exclude='share/bash-completion' --exclude='share/zsh' --exclude='share/gettext' \
    --exclude='share/gdb' --exclude='share/thumbnailers' \
    --exclude='share/wayland' --exclude='share/wayland-protocols' \
    --exclude='lib/python3.9' --exclude='share/xcb' \
    --exclude='share/common-lisp' --exclude='libexec/xkbcommon' \
    --exclude='share/glib-2.0/codegen' --exclude='share/glib-2.0/dtds' \
    --exclude='share/glib-2.0/gdb' --exclude='share/glib-2.0/gettext' \
    --exclude='share/glib-2.0/valgrind' \
    --exclude='share/gtk-4.0/emoji' --exclude='share/gtk-4.0/valgrind' \
    --exclude='share/gtk-4.0/gtk4builder.rng' \
    --transform 's,^,webkitgtk/,S' \
    lib libexec etc share

python3 /build-scripts/check-archive.py --epoch "$SOURCE_DATE_EPOCH" \
    "/out/webkitgtk-${WEBKITGTK_VERSION}-x86_64.tar.xz"

echo ":: Wrote /out/webkitgtk-${WEBKITGTK_VERSION}-x86_64.tar.xz"
