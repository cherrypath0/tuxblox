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
LibDir="$Prefix/lib/x86_64-linux-gnu"
Markers="$Prefix/.done"
JOBS="${JOBS:-$(nproc)}"
mkdir -p "$Prefix" "$Markers" /build

# Our own libraries must win over the builder's older ones, and meson installs into the multiarch libdir while autotools would use lib
export PKG_CONFIG_PATH="$LibDir/pkgconfig:$Prefix/lib/pkgconfig:$Prefix/share/pkgconfig"
export LDFLAGS="-Wl,-rpath,$LibDir -L$LibDir"
export CPPFLAGS="-I$Prefix/include"
export CFLAGS="-O2"
export CXXFLAGS="-O2"
export PATH="$Prefix/bin:$PATH"

fetchAndExtract() {
    local url="$1" destDir="$2"
    shift 2
    local fallbacks=("$@")
    local tmpArchive u
    tmpArchive="$(mktemp)"
    rm -rf "$destDir"
    mkdir -p "$destDir"
    for u in "$url" "${fallbacks[@]}"; do
        if curl -fL --connect-timeout 30 --retry 3 --retry-all-errors "$u" -o "$tmpArchive"; then
            tar -xf "$tmpArchive" -C "$destDir" --strip-components=1
            rm -f "$tmpArchive"
            return 0
        fi
        printf ':: download failed: %s -- trying next source\n' "$u" >&2
    done
    rm -f "$tmpArchive"
    printf 'ERROR: every download source failed for %s\n' "$url" >&2
    return 1
}

mesonBuild() {
    local name="$1"
    shift
    meson setup "/build/$name/_b" "/build/$name" --prefix="$Prefix" "$@"
    ninja -C "/build/$name/_b" -j"$JOBS" install
}

# Runs a build function once; the marker carries the version so a bump rebuilds it
runStep() {
    local name="$1" version="$2" func="$3" marker="$Markers/$1-$2"
    if [ -e "$marker" ]; then
        printf ':: Skipping %s %s (already built)\n' "$name" "$version"
        return 0
    fi
    printf ':: Building %s %s\n' "$name" "$version"
    "$func" "$name"
    touch "$marker"
}

buildLibffi() {
    fetchAndExtract "https://github.com/libffi/libffi/releases/download/v${LIBFFI_VERSION}/libffi-${LIBFFI_VERSION}.tar.gz" /build/libffi
    cd /build/libffi
    ./configure --prefix="$Prefix" --libdir="$LibDir" --disable-static --disable-docs
    make -j"$JOBS"
    make install
    # libffi's own makefile installs to $(libdir)/../lib, not $(libdir)
    mv "$Prefix"/lib/lib/libffi* "$LibDir/"
    rmdir "$Prefix/lib/lib"
    sed -i 's|^toolexeclibdir=.*|toolexeclibdir=${libdir}|' "$LibDir/pkgconfig/libffi.pc"
    cd /build
}

buildGlib() {
    fetchAndExtract "https://download.gnome.org/sources/glib/${GLIB_VERSION%.*}/glib-${GLIB_VERSION}.tar.xz" /build/glib
    # selinux would add a libselinux.so.1 dependency that distros like Arch do not ship
    # libmount would pull libselinux back in through the host's libmount
    mesonBuild glib -Dtests=false -Dselinux=disabled -Dlibmount=disabled
}

buildZstd() {
    fetchAndExtract "https://github.com/facebook/zstd/releases/download/v${ZSTD_VERSION}/zstd-${ZSTD_VERSION}.tar.gz" /build/zstd
    # The builder has no libzstd.so or .a, and the multiarch libdir is the only one the meson builds search
    make -C /build/zstd/lib -j"$JOBS" install PREFIX="$Prefix" libdir="$LibDir"
    make -C /build/zstd/programs -j"$JOBS" install PREFIX="$Prefix"
}

buildLibtiff() {
    fetchAndExtract "https://download.osgeo.org/libtiff/tiff-${TIFF_VERSION}.tar.gz" /build/libtiff
    # GTK requires libtiff, and the host's soname differs between distros (5 on Debian 11, 6 on Arch), so it is built here with only zlib
    cd /build/libtiff
    ./configure --prefix="$Prefix" --libdir="$LibDir" --disable-static --disable-jpeg --disable-old-jpeg --disable-jbig \
        --disable-lerc --disable-lzma --disable-zstd --disable-webp --disable-libdeflate --disable-cxx \
        --disable-tools --disable-tests --disable-contrib --disable-docs
    make -j"$JOBS"
    make install
    cd /build
}

buildLibjpeg() {
    fetchAndExtract "https://github.com/libjpeg-turbo/libjpeg-turbo/releases/download/${JPEG_TURBO_VERSION}/libjpeg-turbo-${JPEG_TURBO_VERSION}.tar.gz" /build/libjpeg
    # gdk-pixbuf compiles its JPEG loader in, and the host's soname differs between distros (62 on Debian and Fedora, 8 on Arch and openSUSE)
    cmake -S /build/libjpeg -B /build/libjpeg/_b -G Ninja -DCMAKE_INSTALL_PREFIX="$Prefix" -DCMAKE_INSTALL_LIBDIR=lib/x86_64-linux-gnu \
        -DCMAKE_BUILD_TYPE=Release -DENABLE_STATIC=OFF -DWITH_TURBOJPEG=OFF -DWITH_JAVA=OFF
    ninja -C /build/libjpeg/_b -j"$JOBS" install
}

buildPixman() {
    fetchAndExtract "https://cairographics.org/releases/pixman-${PIXMAN_VERSION}.tar.gz" /build/pixman
    # The host's pixman is not guaranteed to exist on a machine without cairo installed
    mesonBuild pixman -Dtests=disabled -Ddemos=disabled -Dgtk=disabled -Dlibpng=disabled
}

buildFribidi() {
    fetchAndExtract "https://github.com/fribidi/fribidi/releases/download/v${FRIBIDI_VERSION}/fribidi-${FRIBIDI_VERSION}.tar.xz" /build/fribidi
    # Pango, GTK and libadwaita all link it, and it is not guaranteed on a machine without GTK installed
    mesonBuild fribidi -Ddocs=false -Dbin=false -Dtests=false
}

buildFreetype() {
    local harfbuzz="disabled"
    [ "$1" = "freetype-pass2" ] && harfbuzz="enabled"
    if [ ! -e /build/freetype/meson.build ]; then
        fetchAndExtract "https://download.savannah.gnu.org/releases/freetype/freetype-${FREETYPE_VERSION}.tar.xz" /build/freetype \
            "https://downloads.sourceforge.net/project/freetype/freetype2/${FREETYPE_VERSION}/freetype-${FREETYPE_VERSION}.tar.xz"
    fi
    # freetype and harfbuzz need each other, so freetype is built once without harfbuzz, then again with it; brotli only adds WOFF2, which nothing here uses
    rm -rf /build/freetype/_b
    mesonBuild freetype -Dharfbuzz="$harfbuzz" -Dbrotli=disabled -Dpng=enabled -Dzlib=system -Dbzip2=disabled
}

buildCairo() {
    fetchAndExtract "https://cairographics.org/releases/cairo-${CAIRO_VERSION}.tar.xz" /build/cairo
    mesonBuild cairo -Dtests=disabled
}

buildHarfbuzz() {
    fetchAndExtract "https://github.com/harfbuzz/harfbuzz/releases/download/${HARFBUZZ_VERSION}/harfbuzz-${HARFBUZZ_VERSION}.tar.xz" /build/harfbuzz
    # ICU would add libicuuc, whose soname changes with every ICU release
    mesonBuild harfbuzz -Dicu=disabled -Dutilities=disabled -Dtests=disabled -Ddocs=disabled -Dintrospection=disabled
}

buildPango() {
    fetchAndExtract "https://download.gnome.org/sources/pango/${PANGO_VERSION%.*}/pango-${PANGO_VERSION}.tar.xz" /build/pango
    # libthai and Xft are left out: they exist on a host mostly because Pango pulls them in
    mesonBuild pango -Dintrospection=disabled -Dbuild-testsuite=false -Dbuild-examples=false -Dlibthai=disabled -Dxft=disabled
}

buildGdkPixbuf() {
    fetchAndExtract "https://download.gnome.org/sources/gdk-pixbuf/${GDK_PIXBUF_VERSION%.*}/gdk-pixbuf-${GDK_PIXBUF_VERSION}.tar.xz" /build/gdkpixbuf
    # A loader that is merely left out of builtin_loaders is still built as a dlopenable module, so the unwanted ones are disabled outright
    mesonBuild gdkpixbuf -Dbuiltin_loaders=png,jpeg -Dtiff=disabled -Dgif=disabled \
        -Dtests=false -Dman=false -Dintrospection=disabled -Dinstalled_tests=false
}

buildGraphene() {
    # 1.10.8 has no uploaded release asset, so this is the tag archive
    fetchAndExtract "https://github.com/ebassi/graphene/archive/refs/tags/${GRAPHENE_VERSION}.tar.gz" /build/graphene
    mesonBuild graphene -Dtests=false -Dinstalled_tests=false -Dintrospection=disabled -Dgtk_doc=false
}

buildLibepoxy() {
    fetchAndExtract "https://github.com/anholt/libepoxy/archive/refs/tags/${EPOXY_VERSION}.tar.gz" /build/libepoxy
    mesonBuild libepoxy -Dtests=false -Ddocs=false
}

buildLibxkbcommon() {
    fetchAndExtract "https://xkbcommon.org/download/libxkbcommon-${XKBCOMMON_VERSION}.tar.xz" /build/libxkbcommon
    mesonBuild libxkbcommon -Denable-docs=false -Denable-tools=false -Denable-x11=false -Denable-wayland=false
}

buildWayland() {
    # The builder ships 1.18, below GTK's floor, and a missing Wayland silently drops the backend instead of failing
    fetchAndExtract "https://gitlab.freedesktop.org/wayland/wayland/-/releases/${WAYLAND_VERSION}/downloads/wayland-${WAYLAND_VERSION}.tar.xz" /build/wayland \
        "https://wayland.freedesktop.org/releases/wayland-${WAYLAND_VERSION}.tar.xz"
    mesonBuild wayland -Ddocumentation=false -Dtests=false -Ddtd_validation=false
}

buildWaylandprotocols() {
    fetchAndExtract "https://gitlab.freedesktop.org/wayland/wayland-protocols/-/releases/${WAYLAND_PROTOCOLS_VERSION}/downloads/wayland-protocols-${WAYLAND_PROTOCOLS_VERSION}.tar.xz" /build/wayland-protocols \
        "https://wayland.freedesktop.org/releases/wayland-protocols-${WAYLAND_PROTOCOLS_VERSION}.tar.xz"
    mesonBuild wayland-protocols -Dtests=false
}

buildGtk4() {
    fetchAndExtract "https://download.gnome.org/sources/gtk/${GTK4_VERSION%.*}/gtk-${GTK4_VERSION}.tar.xz" /build/gtk4
    # Both backends, and no Mesa: glvnd resolves the host's GL driver, which is what we want here
    mesonBuild gtk4 -Dx11-backend=true -Dwayland-backend=true -Dvulkan=disabled \
        -Dmedia-gstreamer=disabled -Dbuild-tests=false -Dbuild-demos=false \
        -Dbuild-examples=false -Dbuild-testsuite=false -Dintrospection=disabled \
        -Ddocumentation=false -Dprint-cups=disabled -Dcloudproviders=disabled \
        -Dsysprof=disabled -Dcolord=disabled
}

buildLibadwaita() {
    # Not fetched: libadwaita is the one library TuxBlox maintains, and it comes from the submodule at shared/ui/libadwaita, whose commit is the pin
    # Copied out of the read-only mount rather than built in place, so the checkout the repo owner edits never collects build artefacts
    rm -rf /build/libadwaita
    cp -a /src/libadwaita /build/libadwaita
    rm -rf /build/libadwaita/.git
    mesonBuild libadwaita -Dintrospection=disabled -Dvapi=false -Dtests=false -Dexamples=false
}

buildAdwaitaFonts() {
    fetchAndExtract "https://download.gnome.org/sources/adwaita-fonts/${ADWAITA_FONTS_VERSION%%.*}/adwaita-fonts-${ADWAITA_FONTS_VERSION}.tar.xz" /build/adwaita-fonts
    mkdir -p "$Prefix/fonts"
    find /build/adwaita-fonts -name 'AdwaitaSans-*.ttf' -exec cp -f {} "$Prefix/fonts/" \;
    if ! ls "$Prefix"/fonts/AdwaitaSans-*.ttf >/dev/null 2>&1; then
        printf 'ERROR: no AdwaitaSans fonts in the adwaita-fonts archive\n' >&2
        exit 1
    fi
}

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
runStep gtk4 "$GTK4_VERSION" buildGtk4
runStep libadwaita "$LIBADWAITA_VERSION" buildLibadwaita
runStep adwaita-fonts "$ADWAITA_FONTS_VERSION" buildAdwaitaFonts

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
