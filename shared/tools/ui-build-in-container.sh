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

source "$(dirname "$0")/ui-versions.env"

Prefix=/opt/tuxblox-ui
LibDir="$Prefix/lib/x86_64-linux-gnu"
Markers="$Prefix/.done"
JOBS="${JOBS:-$(nproc)}"
SourcesDir="${TUXBLOX_SOURCES_DIR:-/sources}"
mkdir -p "$Prefix" "$Markers" /build

export PKG_CONFIG_PATH="$LibDir/pkgconfig:$Prefix/lib/pkgconfig:$Prefix/share/pkgconfig"
export LDFLAGS="-Wl,-rpath,$LibDir -L$LibDir"
export CPPFLAGS="-I$Prefix/include"
export CFLAGS="-O2"
export CXXFLAGS="-O2"
export PATH="$Prefix/bin:$PATH"

extractSource() {
    local archive="$1" destDir="$2"
    local path="$SourcesDir/$archive"
    if [ ! -f "$path" ]; then
        printf 'ERROR: missing source archive: %s\n' "$archive" >&2
        printf '       run shared/ui/fetch-sources.sh to fill %s\n' "$SourcesDir" >&2
        return 1
    fi
    rm -rf "$destDir"
    mkdir -p "$destDir"
    tar -xf "$path" -C "$destDir" --strip-components=1
}

mesonBuild() {
    local name="$1"
    shift
    meson setup "/build/$name/_b" "/build/$name" --prefix="$Prefix" "$@"
    ninja -C "/build/$name/_b" -j"$JOBS" install
}

runStep() {
    local name="$1" version="$2" func="$3" marker="$Markers/$1-$2-stack$UI_STACK_VERSION"
    if [ -e "$marker" ]; then
        printf ':: Skipping %s %s (already built)\n' "$name" "$version"
        return 0
    fi
    printf ':: Building %s %s\n' "$name" "$version"
    "$func" "$name"
    touch "$marker"
}

buildZlib() {
    extractSource "zlib-${ZLIB_VERSION}.tar.gz" /build/zlib
    cd /build/zlib
    ./configure --prefix="$Prefix" --libdir="$LibDir"
    make -j"$JOBS"
    make install
    rm -f "$LibDir/libz.a"
    cd /build
}

buildPcre2() {
    extractSource "pcre2-${PCRE2_VERSION}.tar.bz2" /build/pcre2
    cd /build/pcre2
    ./configure --prefix="$Prefix" --libdir="$LibDir" --disable-static \
        --enable-pcre2-8 --disable-pcre2-16 --disable-pcre2-32 --enable-unicode
    make -j"$JOBS"
    make install
    cd /build
}

buildExpat() {
    extractSource "expat-${EXPAT_VERSION}.tar.xz" /build/expat
    cd /build/expat
    ./configure --prefix="$Prefix" --libdir="$LibDir" --disable-static --without-docbook --without-examples --without-tests
    make -j"$JOBS"
    make install
    cd /build
}

buildLibpng() {
    extractSource "libpng-${LIBPNG_VERSION}.tar.xz" /build/libpng
    cd /build/libpng
    ./configure --prefix="$Prefix" --libdir="$LibDir" --disable-static
    make -j"$JOBS"
    make install
    rm -f "$LibDir/libpng.a"
    cd /build
}

buildLibffi() {
    extractSource "libffi-${LIBFFI_VERSION}.tar.gz" /build/libffi
    cd /build/libffi
    ./configure --prefix="$Prefix" --libdir="$LibDir" --disable-static --disable-docs
    make -j"$JOBS"
    make install
    mv "$Prefix"/lib/lib/libffi* "$LibDir/"
    rmdir "$Prefix/lib/lib"
    sed -i 's|^toolexeclibdir=.*|toolexeclibdir=${libdir}|' "$LibDir/pkgconfig/libffi.pc"
    cd /build
}

buildGlib() {
    extractSource "glib-${GLIB_VERSION}.tar.xz" /build/glib
    mesonBuild glib -Dtests=false -Dselinux=disabled -Dlibmount=disabled
}

buildZstd() {
    extractSource "zstd-${ZSTD_VERSION}.tar.gz" /build/zstd
    make -C /build/zstd/lib -j"$JOBS" install PREFIX="$Prefix" libdir="$LibDir"
    make -C /build/zstd/programs -j"$JOBS" install PREFIX="$Prefix"
}

buildLibtiff() {
    extractSource "tiff-${TIFF_VERSION}.tar.gz" /build/libtiff
    cd /build/libtiff
    ./configure --prefix="$Prefix" --libdir="$LibDir" --disable-static --disable-jpeg --disable-old-jpeg --disable-jbig \
        --disable-lerc --disable-lzma --disable-zstd --disable-webp --disable-libdeflate --disable-cxx \
        --disable-tools --disable-tests --disable-contrib --disable-docs
    make -j"$JOBS"
    make install
    cd /build
}

buildLibjpeg() {
    extractSource "libjpeg-turbo-${JPEG_TURBO_VERSION}.tar.gz" /build/libjpeg
    cmake -S /build/libjpeg -B /build/libjpeg/_b -G Ninja -DCMAKE_INSTALL_PREFIX="$Prefix" -DCMAKE_INSTALL_LIBDIR=lib/x86_64-linux-gnu \
        -DCMAKE_BUILD_TYPE=Release -DENABLE_STATIC=OFF -DWITH_TURBOJPEG=OFF -DWITH_JAVA=OFF
    ninja -C /build/libjpeg/_b -j"$JOBS" install
}

buildPixman() {
    extractSource "pixman-${PIXMAN_VERSION}.tar.gz" /build/pixman
    mesonBuild pixman -Dtests=disabled -Ddemos=disabled -Dgtk=disabled -Dlibpng=disabled
}

buildFribidi() {
    extractSource "fribidi-${FRIBIDI_VERSION}.tar.xz" /build/fribidi
    mesonBuild fribidi -Ddocs=false -Dbin=false -Dtests=false
}

buildFreetype() {
    local harfbuzz="disabled"
    [ "$1" = "freetype-pass2" ] && harfbuzz="enabled"
    if [ ! -e /build/freetype/meson.build ]; then
        extractSource "freetype-${FREETYPE_VERSION}.tar.xz" /build/freetype
    fi
    rm -rf /build/freetype/_b
    mesonBuild freetype -Dharfbuzz="$harfbuzz" -Dbrotli=disabled -Dpng=enabled -Dzlib=system -Dbzip2=disabled
}

buildCairo() {
    extractSource "cairo-${CAIRO_VERSION}.tar.xz" /build/cairo
    mesonBuild cairo -Dtests=disabled
}

buildHarfbuzz() {
    extractSource "harfbuzz-${HARFBUZZ_VERSION}.tar.xz" /build/harfbuzz
    mesonBuild harfbuzz -Dicu=disabled -Dutilities=disabled -Dtests=disabled -Ddocs=disabled -Dintrospection=disabled
}

buildPango() {
    extractSource "pango-${PANGO_VERSION}.tar.xz" /build/pango
    mesonBuild pango -Dintrospection=disabled -Dbuild-testsuite=false -Dbuild-examples=false -Dlibthai=disabled -Dxft=disabled
}

buildGdkPixbuf() {
    extractSource "gdk-pixbuf-${GDK_PIXBUF_VERSION}.tar.xz" /build/gdkpixbuf
    mesonBuild gdkpixbuf -Dbuiltin_loaders=png,jpeg -Dtiff=disabled -Dgif=disabled \
        -Dtests=false -Dman=false -Dintrospection=disabled -Dinstalled_tests=false
}

buildGraphene() {
    extractSource "graphene-${GRAPHENE_VERSION}.tar.gz" /build/graphene
    mesonBuild graphene -Dtests=false -Dinstalled_tests=false -Dintrospection=disabled -Dgtk_doc=false
}

buildLibepoxy() {
    extractSource "libepoxy-${EPOXY_VERSION}.tar.gz" /build/libepoxy
    mesonBuild libepoxy -Dtests=false -Ddocs=false
}

buildLibxkbcommon() {
    extractSource "libxkbcommon-${XKBCOMMON_VERSION}.tar.xz" /build/libxkbcommon
    mesonBuild libxkbcommon -Denable-docs=false -Denable-tools=false -Denable-x11=false -Denable-wayland=false
}

buildWayland() {
    extractSource "wayland-${WAYLAND_VERSION}.tar.xz" /build/wayland
    mesonBuild wayland -Ddocumentation=false -Dtests=false -Ddtd_validation=false
}

buildWaylandprotocols() {
    extractSource "wayland-protocols-${WAYLAND_PROTOCOLS_VERSION}.tar.xz" /build/wayland-protocols
    mesonBuild wayland-protocols -Dtests=false
}

buildLibxml2() {
    extractSource "libxml2-${LIBXML2_VERSION}.tar.xz" /build/libxml2
    cd /build/libxml2
    ./configure --prefix="$Prefix" --libdir="$LibDir" --disable-static --without-python --without-lzma --without-zlib --without-http --without-modules --without-readline
    make -j"$JOBS"
    make install
    cd /build
}
buildGtk4() {
    extractSource "gtk-${GTK4_VERSION}.tar.xz" /build/gtk4
    mesonBuild gtk4 -Dx11-backend=true -Dwayland-backend=true -Dvulkan=disabled \
        -Dmedia-gstreamer=disabled -Dbuild-tests=false -Dbuild-demos=false \
        -Dbuild-examples=false -Dbuild-testsuite=false -Dintrospection=disabled \
        -Ddocumentation=false -Dprint-cups=disabled -Dcloudproviders=disabled \
        -Dsysprof=disabled -Dcolord=disabled
}

buildLibadwaita() {
    rm -rf /build/libadwaita
    cp -a /src/ui/libadwaita /build/libadwaita
    rm -rf /build/libadwaita/.git
    mesonBuild libadwaita -Dintrospection=disabled -Dvapi=false -Dtests=false -Dexamples=false
}

buildAdwaitaFonts() {
    extractSource "adwaita-fonts-${ADWAITA_FONTS_VERSION}.tar.xz" /build/adwaita-fonts
    mkdir -p "$Prefix/fonts"
    find /build/adwaita-fonts -name 'AdwaitaSans-*.ttf' -exec cp -f {} "$Prefix/fonts/" \;
    if ! ls "$Prefix"/fonts/AdwaitaSans-*.ttf >/dev/null 2>&1; then
        printf 'ERROR: no AdwaitaSans fonts in the adwaita-fonts archive\n' >&2
        exit 1
    fi
    find /build/adwaita-fonts -name 'AdwaitaMono-*.ttf' -exec cp -f {} "$Prefix/fonts/" \;
    if ! ls "$Prefix"/fonts/AdwaitaMono-*.ttf >/dev/null 2>&1; then
        printf 'ERROR: no AdwaitaMono fonts in the adwaita-fonts archive\n' >&2
        exit 1
    fi
}

runStep zlib "$ZLIB_VERSION" buildZlib
runStep pcre2 "$PCRE2_VERSION" buildPcre2
runStep expat "$EXPAT_VERSION" buildExpat
runStep libpng "$LIBPNG_VERSION" buildLibpng
runStep libffi "$LIBFFI_VERSION" buildLibffi
runStep glib "$GLIB_VERSION" buildGlib
runStep zstd "$ZSTD_VERSION" buildZstd
runStep libtiff "$TIFF_VERSION" buildLibtiff
runStep libjpeg "$JPEG_TURBO_VERSION" buildLibjpeg
runStep pixman "$PIXMAN_VERSION" buildPixman
runStep fribidi "$FRIBIDI_VERSION" buildFribidi
runStep freetype-pass1 "$FREETYPE_VERSION" buildFreetype
runStep harfbuzz "$HARFBUZZ_VERSION" buildHarfbuzz
runStep freetype-pass2 "$FREETYPE_VERSION" buildFreetype
runStep cairo "$CAIRO_VERSION" buildCairo
runStep pango "$PANGO_VERSION" buildPango
runStep gdk-pixbuf "$GDK_PIXBUF_VERSION" buildGdkPixbuf
runStep graphene "$GRAPHENE_VERSION" buildGraphene
runStep libepoxy "$EPOXY_VERSION" buildLibepoxy
runStep libxkbcommon "$XKBCOMMON_VERSION" buildLibxkbcommon
runStep wayland "$WAYLAND_VERSION" buildWayland
runStep wayland-protocols "$WAYLAND_PROTOCOLS_VERSION" buildWaylandprotocols
runStep libxml2 "$LIBXML2_VERSION" buildLibxml2
runStep gtk4 "$GTK4_VERSION" buildGtk4
case "${LIBADWAITA_SOURCE:?build.sh passes the submodule commit}" in
    *-dirty) rm -f "$Markers"/libadwaita-* ;;
    *) [ -e "$Markers/libadwaita-$LIBADWAITA_SOURCE" ] || rm -f "$Markers"/libadwaita-* ;;
esac
runStep libadwaita "$LIBADWAITA_SOURCE" buildLibadwaita
runStep adwaita-fonts "$ADWAITA_FONTS_VERSION" buildAdwaitaFonts

printf ':: Assembling the icon theme\n'
themeDir="$Prefix/share/icons/TuxBlox"
for iconSource in /build/gtk4/gtk/icons /build/libadwaita/src/icons; do
    if [ ! -d "$iconSource" ]; then
        printf 'ERROR: %s is missing, so the icon theme cannot be assembled; delete %s and build again\n' "$iconSource" "$Markers" >&2
        exit 1
    fi
done
rm -rf "$themeDir"
mkdir -p "$themeDir"
cp -a /build/gtk4/gtk/icons/. "$themeDir/"
cp -a /build/libadwaita/src/icons/. "$themeDir/"
rm -f "$themeDir/hicolor.index.theme"

printf ':: Rasterising the icons that exist only as SVG\n'
rasterised=0
while IFS= read -r svg; do
    name="$(basename "$svg" .svg)"
    if find "$themeDir" -name "$name.png" -o -name "$name.symbolic.png" | grep -q .; then
        continue
    fi
    category="$(basename "$(dirname "$svg")")"
    case "$name" in
        *-symbolic | *-symbolic-rtl | *-symbolic-ltr) twin="$themeDir/16x16/$category/$name.symbolic.png" ;;
        *) twin="$themeDir/16x16/$category/$name.png" ;;
    esac
    mkdir -p "$(dirname "$twin")"
    if ! rsvg-convert -w 16 -h 16 -o "$twin" "$svg" || [ ! -s "$twin" ]; then
        printf 'ERROR: %s could not be rendered, so that icon would draw as a missing image\n' "$svg" >&2
        exit 1
    fi
    rasterised=$((rasterised + 1))
done < <(find "$themeDir/scalable" -name '*.svg' | sort)
printf ':: %s icons rendered\n' "$rasterised"

iconDirs="$(cd "$themeDir" && find . -mindepth 2 -type d | sed 's#^\./##' | sort)"
{
    printf '[Icon Theme]\nName=TuxBlox\nComment=The icons TuxBlox ships\nInherits=Adwaita,hicolor\n'
    printf 'Directories=%s\n' "$(printf '%s\n' "$iconDirs" | paste -sd,)"
    printf '%s\n' "$iconDirs" | while IFS= read -r iconDir; do
        printf '\n'
        awk -v section="[$iconDir]" '
            $0 == section { inside = 1; print; next }
            inside && /^\[/ { exit }
            inside { print }
        ' /build/gtk4/gtk/icons/hicolor.index.theme
    done
} > "$themeDir/index.theme"

themeFailed=0
sections="$(grep -c '^\[.*\]$' "$themeDir/index.theme")"
sizes="$(grep -c '^Size=' "$themeDir/index.theme")"
if [ "$sections" -ne "$((sizes + 1))" ]; then
    printf 'ERROR: the theme index has %s folder descriptions but %s sizes, so one of them says nothing\n' "$((sections - 1))" "$sizes" >&2
    themeFailed=1
fi
while IFS= read -r iconDir; do
    relDir="${iconDir#"$themeDir"/}"
    if ! grep -q "^\[$relDir\]$" "$themeDir/index.theme"; then
        printf 'ERROR: icon folder %s is not listed in the theme index, so nothing in it can be found\n' "$relDir" >&2
        themeFailed=1
    fi
done < <(find "$themeDir" -mindepth 2 -type d)
while IFS= read -r svg; do
    name="$(basename "$svg" .svg)"
    if ! find "$themeDir" -name "$name.png" -o -name "$name.symbolic.png" | grep -q .; then
        printf 'ERROR: %s has no rendered twin, so that icon would draw as a missing image\n' "$svg" >&2
        themeFailed=1
    fi
done < <(find "$themeDir/scalable" -name '*.svg')
if [ ! -e "$themeDir/16x16/status/image-missing.png" ]; then
    printf 'ERROR: the icon theme has no image-missing\n' >&2
    themeFailed=1
fi
if [ "$themeFailed" -ne 0 ]; then
    exit 1
fi
printf ':: %s icons in %s folders\n' "$(find "$themeDir" -type f ! -name index.theme | wc -l)" "$(find "$themeDir" -mindepth 2 -type d | wc -l)"

printf ':: Verifying both GDK backends are present\n'
gtkSo="$LibDir/libgtk-4.so"
for backend in gdk_wayland gdk_x11; do
    count="$(nm -D --defined-only "$gtkSo" | grep -c "$backend" || true)"
    if [[ "$count" -lt 5 ]]; then
        printf 'ERROR: libgtk-4 exports only %s %s symbols -- that backend did not build\n' "$count" "$backend" >&2
        exit 1
    fi
    printf ':: %s symbols: %s\n' "$backend" "$count"
done
