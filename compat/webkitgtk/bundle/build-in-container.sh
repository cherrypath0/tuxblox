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
mkdir -p "$PREFIX"
export PKG_CONFIG_PATH="$PREFIX/lib/pkgconfig:$PREFIX/lib/x86_64-linux-gnu/pkgconfig:$PREFIX/share/pkgconfig"
export LDFLAGS="-Wl,-rpath,$PREFIX/lib -Wl,-rpath,$PREFIX/lib/x86_64-linux-gnu -L$PREFIX/lib -L$PREFIX/lib/x86_64-linux-gnu"
export CPPFLAGS="-I$PREFIX/include"
export CFLAGS="-O2"
export CXXFLAGS="-O2"
export PATH="$PREFIX/bin:$PATH"
JOBS="${JOBS:-$(nproc)}"
export CCACHE_DIR="${CCACHE_DIR:-/ccache}"
mkdir -p "$CCACHE_DIR"

SOURCES_DIR="${SOURCES_DIR:-/sources}"
SUMS_FILE="${SUMS_FILE:-$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)/sha256sums}"

if [ ! -d "$SOURCES_DIR" ]; then
    echo "ERROR: no source archives mounted at $SOURCES_DIR" >&2
    echo "       Run compat/webkitgtk/bundle/fetch-sources.sh on the host, then mount" >&2
    echo "       that sources/ directory read-only at $SOURCES_DIR." >&2
    exit 1
fi
if [ ! -f "$SUMS_FILE" ]; then
    echo "ERROR: no recorded checksums at $SUMS_FILE" >&2
    exit 1
fi

export MESON_PACKAGE_CACHE_DIR="$SOURCES_DIR"

extract_source() {
    local fname="$1" dest_dir="$2"
    local archive="$SOURCES_DIR/$fname"
    local expected actual

    if [ ! -f "$archive" ]; then
        echo "ERROR: $fname is not in $SOURCES_DIR" >&2
        echo "       Run compat/webkitgtk/bundle/fetch-sources.sh on the host first." >&2
        return 1
    fi

    expected="$(awk -v f="$fname" '$2 == f || $2 == "*" f { print $1; exit }' "$SUMS_FILE")"
    if [ -z "$expected" ]; then
        echo "ERROR: no recorded checksum for $fname -- refusing to build it" >&2
        return 1
    fi
    actual="$(sha256sum "$archive" | awk '{print $1}')"
    if [ "$actual" != "$expected" ]; then
        echo "ERROR: checksum mismatch for $fname" >&2
        echo "       recorded: $expected" >&2
        echo "       on disk:  $actual" >&2
        return 1
    fi

    mkdir -p "$dest_dir"
    tar -xf "$archive" -C "$dest_dir" --strip-components=1
}

echo ":: Building zlib $ZLIB_VERSION"
extract_source "zlib-${ZLIB_VERSION}.tar.gz" /build/zlib
cd /build/zlib
./configure --prefix="$PREFIX"
make -j"$JOBS"
make install
cd /build

echo ":: Building bzip2 $BZIP2_VERSION"
extract_source "bzip2-${BZIP2_VERSION}.tar.gz" /build/bzip2
cd /build/bzip2
make -f Makefile-libbz2_so -j"$JOBS"
mkdir -p "$PREFIX/lib" "$PREFIX/include"
cp -a "libbz2.so.${BZIP2_VERSION}" "$PREFIX/lib/"
ln -sf "libbz2.so.${BZIP2_VERSION}" "$PREFIX/lib/libbz2.so.1.0"
ln -sf "libbz2.so.${BZIP2_VERSION}" "$PREFIX/lib/libbz2.so.1"
ln -sf "libbz2.so.${BZIP2_VERSION}" "$PREFIX/lib/libbz2.so"
cp -a bzlib.h "$PREFIX/include/"
cd /build

echo ":: Building zstd $ZSTD_VERSION"
extract_source "zstd-${ZSTD_VERSION}.tar.gz" /build/zstd
cd /build/zstd
make -j"$JOBS" PREFIX="$PREFIX"
make PREFIX="$PREFIX" install
cd /build

echo ":: Building brotli $BROTLI_VERSION"
extract_source "brotli-${BROTLI_VERSION}.tar.gz" /build/brotli
cmake -B /build/brotli/_build -S /build/brotli -GNinja -DCMAKE_INSTALL_PREFIX="$PREFIX" \
    -DCMAKE_BUILD_TYPE=Release -DBUILD_SHARED_LIBS=ON -DBROTLI_DISABLE_TESTS=ON
ninja -C /build/brotli/_build -j"$JOBS" install
cd /build

echo ":: Building expat $EXPAT_VERSION"
extract_source "expat-${EXPAT_VERSION}.tar.xz" /build/expat
cd /build/expat
./configure --prefix="$PREFIX" --disable-static --without-docbook --without-examples --without-tests
make -j"$JOBS"
make install
cd /build

echo ":: Building libpng $LIBPNG_VERSION"
extract_source "libpng-${LIBPNG_VERSION}.tar.xz" /build/libpng
cd /build/libpng
./configure --prefix="$PREFIX" --disable-static
make -j"$JOBS"
make install
cd /build

echo ":: Building pcre2 $PCRE2_VERSION"
extract_source "pcre2-${PCRE2_VERSION}.tar.gz" /build/pcre2
cd /build/pcre2
./configure --prefix="$PREFIX" --disable-static --enable-unicode --enable-jit
make -j"$JOBS"
make install
cd /build

echo ":: Building libgpg-error $LIBGPG_ERROR_VERSION"
extract_source "libgpg-error-${LIBGPG_ERROR_VERSION}.tar.bz2" /build/libgpg-error
cd /build/libgpg-error
./configure --prefix="$PREFIX" --disable-static --disable-doc --disable-tests
make -j"$JOBS"
make install
cd /build

echo ":: Building libgcrypt $LIBGCRYPT_VERSION"
extract_source "libgcrypt-${LIBGCRYPT_VERSION}.tar.bz2" /build/libgcrypt
cd /build/libgcrypt
./configure --prefix="$PREFIX" --disable-static --disable-doc \
    --with-libgpg-error-prefix="$PREFIX"
make -j"$JOBS"
make install
cd /build

echo ":: Building libidn2 $LIBIDN2_VERSION"
extract_source "libidn2-${LIBIDN2_VERSION}.tar.gz" /build/libidn2
cd /build/libidn2
./configure --prefix="$PREFIX" --disable-static --disable-doc --disable-gcc-warnings
make -j"$JOBS"
make install
cd /build

echo ":: Building fribidi $FRIBIDI_VERSION"
extract_source "fribidi-${FRIBIDI_VERSION}.tar.xz" /build/fribidi
meson setup /build/fribidi/_build /build/fribidi --prefix="$PREFIX" \
    -Ddocs=false -Dbin=false -Dtests=false
ninja -C /build/fribidi/_build -j"$JOBS" install

echo ":: Building libdatrie $LIBDATRIE_VERSION"
extract_source "libdatrie-${LIBDATRIE_VERSION}.tar.xz" /build/libdatrie
cd /build/libdatrie
./configure --prefix="$PREFIX" --disable-static --disable-doxygen-doc
make -j"$JOBS"
make install
cd /build

echo ":: Building libthai $LIBTHAI_VERSION"
extract_source "libthai-${LIBTHAI_VERSION}.tar.xz" /build/libthai
cd /build/libthai
./configure --prefix="$PREFIX" --disable-static --disable-doxygen-doc
make -j"$JOBS"
make install
cd /build

echo ":: Building pixman $PIXMAN_VERSION"
extract_source "pixman-${PIXMAN_VERSION}.tar.gz" /build/pixman
meson setup /build/pixman/_build /build/pixman --prefix="$PREFIX" \
    -Dtests=disabled -Ddemos=disabled
ninja -C /build/pixman/_build -j"$JOBS" install

echo ":: Building xorgproto $XORGPROTO_VERSION"
extract_source "xorgproto-${XORGPROTO_VERSION}.tar.xz" /build/xorgproto
cd /build/xorgproto
./configure --prefix="$PREFIX"
make -j"$JOBS"
make install
cd /build

echo ":: Building libXau $LIBXAU_VERSION"
extract_source "libXau-${LIBXAU_VERSION}.tar.xz" /build/libXau
cd /build/libXau
./configure --prefix="$PREFIX" --disable-static
make -j"$JOBS"
make install
cd /build

echo ":: Building libXdmcp $LIBXDMCP_VERSION"
extract_source "libXdmcp-${LIBXDMCP_VERSION}.tar.xz" /build/libXdmcp
cd /build/libXdmcp
./configure --prefix="$PREFIX" --disable-static --without-docs
make -j"$JOBS"
make install
cd /build

echo ":: Building xcb-proto $XCBPROTO_VERSION"
extract_source "xcb-proto-${XCBPROTO_VERSION}.tar.xz" /build/xcb-proto
cd /build/xcb-proto
./configure --prefix="$PREFIX"
make -j"$JOBS"
make install
cd /build
export PYTHONPATH="$(echo "$PREFIX"/lib/python3*/site-packages):${PYTHONPATH:-}"

echo ":: Building libxcb $LIBXCB_VERSION"
extract_source "libxcb-${LIBXCB_VERSION}.tar.xz" /build/libxcb
cd /build/libxcb
./configure --prefix="$PREFIX" --disable-static --without-doxygen
make -j"$JOBS"
make install
cd /build

echo ":: Building libX11 $LIBX11_VERSION"
extract_source "libX11-${LIBX11_VERSION}.tar.xz" /build/libX11
cd /build/libX11
./configure --prefix="$PREFIX" --disable-static --without-xmlto --without-fop --without-xsltproc
make -j"$JOBS"
make install
cd /build

for xlib in \
    "libXext-${LIBXEXT_VERSION}" \
    "libXrender-${LIBXRENDER_VERSION}" \
    "libXfixes-${LIBXFIXES_VERSION}" \
    "libXdamage-${LIBXDAMAGE_VERSION}" \
    "libXi-${LIBXI_VERSION}" \
    "libXrandr-${LIBXRANDR_VERSION}" \
    "libXcursor-${LIBXCURSOR_VERSION}" \
    "libXinerama-${LIBXINERAMA_VERSION}" \
    "libXxf86vm-${LIBXXF86VM_VERSION}" \
    "libXtst-${LIBXTST_VERSION}" \
    "libXv-${LIBXV_VERSION}" \
    "libxshmfence-${LIBXSHMFENCE_VERSION}"
do
    echo ":: Building $xlib"
    extract_source "${xlib}.tar.xz" "/build/$xlib"
    cd "/build/$xlib"
    ./configure --prefix="$PREFIX" --disable-static --without-xmlto --without-fop --without-xsltproc
    make -j"$JOBS"
    make install
    cd /build
done

echo ":: Building libxkbcommon $XKBCOMMON_VERSION"
extract_source "libxkbcommon-${XKBCOMMON_VERSION}.tar.xz" /build/libxkbcommon
meson setup /build/libxkbcommon/_build /build/libxkbcommon --prefix="$PREFIX" \
    -Denable-docs=false -Denable-wayland=false -Denable-xkbregistry=false
ninja -C /build/libxkbcommon/_build -j"$JOBS" install

echo ":: Building libglvnd $LIBGLVND_VERSION"
extract_source "libglvnd-v${LIBGLVND_VERSION}.tar.gz" /build/libglvnd
meson setup /build/libglvnd/_build /build/libglvnd --prefix="$PREFIX" \
    -Dx11=enabled -Degl=true -Dglx=enabled -Dhgl=false
ninja -C /build/libglvnd/_build -j"$JOBS" install

echo ":: Building libepoxy $LIBEPOXY_VERSION"
extract_source "libepoxy-${LIBEPOXY_VERSION}.tar.xz" /build/libepoxy
meson setup /build/libepoxy/_build /build/libepoxy --prefix="$PREFIX" \
    -Dtests=false -Dglx=yes -Degl=yes
ninja -C /build/libepoxy/_build -j"$JOBS" install

echo ":: Building libffi $LIBFFI_VERSION"
extract_source "libffi-${LIBFFI_VERSION}.tar.gz" /build/libffi
cd /build/libffi
./configure --prefix="$PREFIX" --disable-static --disable-docs
make -j"$JOBS"
make install
cd /build

echo ":: Building wayland $WAYLAND_VERSION"
extract_source "wayland-${WAYLAND_VERSION}.tar.xz" /build/wayland
meson setup /build/wayland/_build /build/wayland --prefix="$PREFIX" \
    -Dlibraries=true -Dscanner=true -Dtests=false -Ddocumentation=false \
    -Ddtd_validation=false
ninja -C /build/wayland/_build -j"$JOBS" install

echo ":: Building wayland-protocols $WAYLAND_PROTOCOLS_VERSION"
extract_source "wayland-protocols-${WAYLAND_PROTOCOLS_VERSION}.tar.xz" /build/wayland-protocols
meson setup /build/wayland-protocols/_build /build/wayland-protocols --prefix="$PREFIX" \
    -Dtests=false
ninja -C /build/wayland-protocols/_build -j"$JOBS" install

echo ":: Building glib $GLIB_VERSION"
extract_source "glib-${GLIB_VERSION}.tar.xz" /build/glib
meson setup /build/glib/_build /build/glib --prefix="$PREFIX" -Dtests=false -Dselinux=disabled
ninja -C /build/glib/_build -j"$JOBS" install

echo ":: Building icu $ICU_VERSION"
extract_source "icu4c-${ICU_VERSION//-/_}-src.tgz" /build/icu
cd /build/icu/source
./configure --prefix="$PREFIX"
make -j"$JOBS"
make install
cd /build

echo ":: Building libtasn1 $LIBTASN1_VERSION"
extract_source "libtasn1-${LIBTASN1_VERSION}.tar.gz" /build/libtasn1
cd /build/libtasn1 && ./configure --prefix="$PREFIX" && make -j"$JOBS" && make install && cd /build

echo ":: Building sqlite $SQLITE_VERSION"
extract_source "sqlite-autoconf-${SQLITE_VERSION}.tar.gz" /build/sqlite
cd /build/sqlite && ./configure --prefix="$PREFIX" && make -j"$JOBS" && make install && cd /build

echo ":: Building lcms2 $LCMS2_VERSION"
extract_source "lcms2-${LCMS2_VERSION}.tar.gz" /build/lcms2
cd /build/lcms2 && ./configure --prefix="$PREFIX" && make -j"$JOBS" && make install && cd /build

echo ":: Building libwebp $LIBWEBP_VERSION"
extract_source "libwebp-${LIBWEBP_VERSION}.tar.gz" /build/libwebp
cd /build/libwebp && ./configure --prefix="$PREFIX" && make -j"$JOBS" && make install && cd /build

echo ":: Building openjpeg $OPENJPEG_VERSION"
extract_source "openjpeg-${OPENJPEG_VERSION}.tar.gz" /build/openjpeg
cmake -B /build/openjpeg/_build -S /build/openjpeg -DCMAKE_INSTALL_PREFIX="$PREFIX" \
    -DBUILD_SHARED_LIBS=ON
cmake --build /build/openjpeg/_build -j"$JOBS"
cmake --install /build/openjpeg/_build

echo ":: Building libjpeg-turbo $LIBJPEG_TURBO_VERSION"
extract_source "libjpeg-turbo-${LIBJPEG_TURBO_VERSION}.tar.gz" /build/libjpeg-turbo
cmake -B /build/libjpeg-turbo/_build -S /build/libjpeg-turbo \
    -DCMAKE_INSTALL_PREFIX="$PREFIX" -DCMAKE_BUILD_TYPE=Release \
    -DWITH_JPEG8=1 -DENABLE_SHARED=ON -DENABLE_STATIC=OFF
cmake --build /build/libjpeg-turbo/_build -j"$JOBS"
cmake --install /build/libjpeg-turbo/_build

echo ":: Building libtiff $LIBTIFF_VERSION"
extract_source "tiff-${LIBTIFF_VERSION}.tar.gz" /build/libtiff
cmake -B /build/libtiff/_build -S /build/libtiff \
    -DCMAKE_INSTALL_PREFIX="$PREFIX" -DCMAKE_PREFIX_PATH="$PREFIX" -DCMAKE_BUILD_TYPE=Release \
    -DBUILD_SHARED_LIBS=ON -Dtiff-tools=OFF -Dtiff-tests=OFF -Dtiff-docs=OFF \
    -Djpeg=OFF -Dold-jpeg=OFF -Dwebp=OFF -Dzstd=OFF -Dlzma=OFF -Dlibdeflate=OFF -Djbig=OFF
cmake --build /build/libtiff/_build -j"$JOBS"
cmake --install /build/libtiff/_build

echo ":: Building libxml2 $LIBXML2_VERSION"
extract_source "libxml2-${LIBXML2_VERSION}.tar.xz" /build/libxml2
meson setup /build/libxml2/_build /build/libxml2 --prefix="$PREFIX" -Dpython=disabled
ninja -C /build/libxml2/_build -j"$JOBS" install

echo ":: Building libxslt $LIBXSLT_VERSION"
extract_source "libxslt-${LIBXSLT_VERSION}.tar.xz" /build/libxslt
cd /build/libxslt
./configure --prefix="$PREFIX" --without-python
make -j"$JOBS"
make install
cd /build

echo ":: Building freetype $FREETYPE_VERSION (pass 1, no harfbuzz)"
extract_source "freetype-${FREETYPE_VERSION}.tar.xz" /build/freetype
meson setup /build/freetype/_build /build/freetype --prefix="$PREFIX" \
    -Dharfbuzz=disabled -Dbrotli=enabled -Dpng=enabled -Dzlib=system -Dbzip2=disabled
ninja -C /build/freetype/_build -j"$JOBS" install

echo ":: Building fontconfig $FONTCONFIG_VERSION"
extract_source "fontconfig-${FONTCONFIG_VERSION}.tar.xz" /build/fontconfig
meson setup /build/fontconfig/_build /build/fontconfig --prefix="$PREFIX" \
    -Dcache-build=disabled -Ddoc=disabled -Ddoc-man=disabled -Dtests=disabled
ninja -C /build/fontconfig/_build -j"$JOBS" install

echo ":: Building cairo $CAIRO_VERSION"
extract_source "cairo-${CAIRO_VERSION}.tar.xz" /build/cairo
meson setup /build/cairo/_build /build/cairo --prefix="$PREFIX"
ninja -C /build/cairo/_build -j"$JOBS" install

echo ":: Building harfbuzz $HARFBUZZ_VERSION"
extract_source "harfbuzz-${HARFBUZZ_VERSION}.tar.xz" /build/harfbuzz
meson setup /build/harfbuzz/_build /build/harfbuzz --prefix="$PREFIX"
ninja -C /build/harfbuzz/_build -j"$JOBS" install

echo ":: Rebuilding freetype $FREETYPE_VERSION (pass 2, with harfbuzz)"
meson setup --wipe /build/freetype/_build /build/freetype --prefix="$PREFIX" \
    -Dharfbuzz=enabled -Dbrotli=enabled -Dpng=enabled -Dzlib=system -Dbzip2=disabled
ninja -C /build/freetype/_build -j"$JOBS" install

echo ":: Building pango $PANGO_VERSION"
extract_source "pango-${PANGO_VERSION}.tar.xz" /build/pango
meson setup /build/pango/_build /build/pango --prefix="$PREFIX" \
    -Dxft=disabled
ninja -C /build/pango/_build -j"$JOBS" install

echo ":: Building gdk-pixbuf $GDK_PIXBUF_VERSION"
extract_source "gdk-pixbuf-${GDK_PIXBUF_VERSION}.tar.xz" /build/gdk-pixbuf
meson setup /build/gdk-pixbuf/_build /build/gdk-pixbuf --prefix="$PREFIX" -Dman=false
ninja -C /build/gdk-pixbuf/_build -j"$JOBS" install

echo ":: Building graphene $GRAPHENE_VERSION"
extract_source "graphene-${GRAPHENE_VERSION}.tar.gz" /build/graphene
meson setup /build/graphene/_build /build/graphene --prefix="$PREFIX"
ninja -C /build/graphene/_build -j"$JOBS" install

echo ":: Building libpsl $LIBPSL_VERSION"
extract_source "libpsl-${LIBPSL_VERSION}.tar.gz" /build/libpsl
meson setup /build/libpsl/_build /build/libpsl --prefix="$PREFIX" \
    -Druntime=libicu -Dbuiltin=true
ninja -C /build/libpsl/_build -j"$JOBS" install

echo ":: Building libgudev $LIBGUDEV_VERSION"
extract_source "libgudev-${LIBGUDEV_VERSION}.tar.xz" /build/libgudev
meson setup /build/libgudev/_build /build/libgudev --prefix="$PREFIX"
ninja -C /build/libgudev/_build -j"$JOBS" install

echo ":: Building libsecret $LIBSECRET_VERSION"
extract_source "libsecret-${LIBSECRET_VERSION}.tar.xz" /build/libsecret
meson setup /build/libsecret/_build /build/libsecret --prefix="$PREFIX" -Dmanpage=false -Dgtk_doc=false -Dintrospection=false
ninja -C /build/libsecret/_build -j"$JOBS" install

echo ":: Building libnghttp2 $LIBNGHTTP2_VERSION"
extract_source "nghttp2-${LIBNGHTTP2_VERSION}.tar.xz" /build/nghttp2
cmake -B /build/nghttp2/_build -S /build/nghttp2 \
    -DCMAKE_INSTALL_PREFIX="$PREFIX" -DCMAKE_BUILD_TYPE=Release \
    -DENABLE_LIB_ONLY=ON -DBUILD_SHARED_LIBS=ON -DBUILD_STATIC_LIBS=OFF
cmake --build /build/nghttp2/_build -j"$JOBS"
cmake --install /build/nghttp2/_build

echo ":: Building libsoup $LIBSOUP_VERSION"
extract_source "libsoup-${LIBSOUP_VERSION}.tar.xz" /build/libsoup
sed -i 's/if (soup2_is_loaded ())/if (0 \&\& soup2_is_loaded ())/' /build/libsoup/libsoup/soup-init.c
grep -q 'if (0 && soup2_is_loaded ())' /build/libsoup/libsoup/soup-init.c || {
    echo "ERROR: soup2_is_loaded() patch did not apply -- libsoup source layout changed, update the sed pattern above" >&2
    exit 1
}
meson setup /build/libsoup/_build /build/libsoup --prefix="$PREFIX" \
    -Dvapi=disabled -Dgssapi=disabled -Dsysprof=disabled -Dtls_check=false
ninja -C /build/libsoup/_build -j"$JOBS" install

echo ":: Building gtk4 $GTK4_VERSION"
extract_source "gtk-${GTK4_VERSION}.tar.xz" /build/gtk4
python3 - <<'GTKPATCH_EOF'
import sys
path = "/build/gtk4/gdk/x11/gdksurface-x11.c"
src = open(path).read()

old = """  if (!foreign_destroy)
    {
      gdk_surface_set_egl_native_window (surface, NULL);
      gdk_x11_surface_destroy_glx_drawable (impl);
"""

new = """  /* TuxBlox: release the EGL native window on BOTH destroy paths, not just the
   * one where GDK itself calls XDestroyWindow -- gdksurface.c's own
   * _gdk_surface_destroy_hierarchy asserts it is NULL right after this function
   * returns, with no foreign_destroy exemption. Also stops the EGLSurface from
   * being leaked on the foreign path. */
  gdk_surface_set_egl_native_window (surface, NULL);

  if (!foreign_destroy)
    {
      gdk_x11_surface_destroy_glx_drawable (impl);
"""

if src.count(old) != 1:
    sys.exit("ERROR: gdk_x11_surface_destroy EGL-teardown patch did not match (%d hits) -- "
             "gtk4 source layout changed, update this patch in build-in-container.sh" % src.count(old))

open(path, "w").write(src.replace(old, new))
GTKPATCH_EOF
grep -q 'TuxBlox: release the EGL native window on BOTH destroy paths' /build/gtk4/gdk/x11/gdksurface-x11.c || {
    echo "ERROR: gtk4 EGL-teardown patch did not apply -- see the comment above this check" >&2
    exit 1
}
meson setup /build/gtk4/_build /build/gtk4 --prefix="$PREFIX" \
    -Dmedia-gstreamer=disabled -Dvulkan=disabled -Dbuild-tests=false -Dbuild-demos=false -Dbuild-examples=false \
    -Dprint-cups=disabled
ninja -C /build/gtk4/_build -j"$JOBS" install

echo ":: Building gmp $GMP_VERSION"
extract_source "gmp-${GMP_VERSION}.tar.xz" /build/gmp
cd /build/gmp && ./configure --prefix="$PREFIX" --enable-fat && make -j"$JOBS" && make install && cd /build
gmp_dynsyms="$(nm -D "$PREFIX/lib/libgmp.so")"
if [[ "$gmp_dynsyms" != *__gmpn_cpuvec* ]]; then
    echo "ERROR: libgmp was built without --enable-fat; the bundle will SIGILL on pre-Broadwell CPUs" >&2
    exit 1
fi

echo ":: Building nettle $NETTLE_VERSION"
extract_source "nettle-${NETTLE_VERSION}.tar.gz" /build/nettle
cd /build/nettle
./configure --prefix="$PREFIX" --libdir="$PREFIX/lib" --disable-documentation
make -j"$JOBS"
make install
cd /build

echo ":: Building p11-kit $P11KIT_VERSION"
extract_source "p11-kit-${P11KIT_VERSION}.tar.xz" /build/p11-kit
meson setup /build/p11-kit/_build /build/p11-kit --prefix="$PREFIX"
ninja -C /build/p11-kit/_build -j"$JOBS" install

echo ":: Building gnutls $GNUTLS_VERSION"
extract_source "gnutls-${GNUTLS_VERSION}.tar.xz" /build/gnutls
cd /build/gnutls
./configure --prefix="$PREFIX" \
    --with-default-trust-store-file=/etc/ssl/certs/ca-certificates.crt \
    --with-default-trust-store-dir=/etc/ssl/certs \
    --disable-doc --disable-manpages --disable-guile --without-tpm2 --without-tpm \
    --with-included-unistring
make -j"$JOBS"
make install
cd /build

echo ":: Building glib-networking $GLIB_NETWORKING_VERSION"
extract_source "glib-networking-${GLIB_NETWORKING_VERSION}.tar.xz" /build/glib-networking
meson setup /build/glib-networking/_build /build/glib-networking --prefix="$PREFIX" \
    -Dgnome_proxy=disabled -Dlibproxy=disabled
ninja -C /build/glib-networking/_build -j"$JOBS" install

echo ":: Building libdrm $LIBDRM_VERSION"
extract_source "libdrm-${LIBDRM_VERSION}.tar.xz" /build/libdrm
meson setup /build/libdrm/_build /build/libdrm --prefix="$PREFIX" \
    -Dcairo-tests=disabled -Dvalgrind=disabled
ninja -C /build/libdrm/_build -j"$JOBS" install

echo ":: Building mesa $MESA_VERSION"
extract_source "mesa-${MESA_VERSION}.tar.xz" /build/mesa
meson setup /build/mesa/_build /build/mesa --prefix="$PREFIX" \
    -Dplatforms=x11,wayland -Dgallium-drivers=llvmpipe -Dvulkan-drivers=[] \
    -Dllvm=enabled -Dshared-llvm=disabled
ninja -C /build/mesa/_build -j"$JOBS" install

echo ":: Building gstreamer $GSTREAMER_VERSION"
extract_source "gstreamer-${GSTREAMER_VERSION}.tar.xz" /build/gstreamer
meson setup /build/gstreamer/_build /build/gstreamer --prefix="$PREFIX" \
    -Dexamples=disabled -Dtests=disabled -Dbenchmarks=disabled
ninja -C /build/gstreamer/_build -j"$JOBS" install

echo ":: Building gst-plugins-base $GSTREAMER_VERSION"
extract_source "gst-plugins-base-${GSTREAMER_VERSION}.tar.xz" /build/gst-plugins-base
meson setup /build/gst-plugins-base/_build /build/gst-plugins-base --prefix="$PREFIX" \
    -Dexamples=disabled -Dtests=disabled
ninja -C /build/gst-plugins-base/_build -j"$JOBS" install

echo ":: Building gst-plugins-good $GSTREAMER_VERSION"
extract_source "gst-plugins-good-${GSTREAMER_VERSION}.tar.xz" /build/gst-plugins-good
meson setup /build/gst-plugins-good/_build /build/gst-plugins-good --prefix="$PREFIX" \
    -Dflac=disabled -Dvpx=disabled -Dsoup=disabled -Dgtk3=disabled \
    -Dexamples=disabled -Dtests=disabled
ninja -C /build/gst-plugins-good/_build -j"$JOBS" install

echo ":: Building gst-plugins-bad $GSTREAMER_VERSION"
extract_source "gst-plugins-bad-${GSTREAMER_VERSION}.tar.xz" /build/gst-plugins-bad
meson setup /build/gst-plugins-bad/_build /build/gst-plugins-bad --prefix="$PREFIX" \
    -Dopenal=disabled -Dcurl=disabled -Ddtls=disabled -Daes=disabled -Dgtk3=disabled \
    -Dexamples=disabled -Dtests=disabled
ninja -C /build/gst-plugins-bad/_build -j"$JOBS" install

echo ":: Building webkitgtk $WEBKITGTK_VERSION"
mkdir -p /build/webkitgtk
rsync -a -c --delete --exclude=_build /src-webkitgtk/ /build/webkitgtk/

rm -f /build/webkitgtk/_build/CMakeCache.txt
cmake -B /build/webkitgtk/_build -S /build/webkitgtk -GNinja \
    -DPORT=GTK -DUSE_GTK4=ON -DCMAKE_INSTALL_PREFIX="$PREFIX" \
    -DCMAKE_BUILD_TYPE=Release -DENABLE_INTROSPECTION=OFF -DENABLE_DOCUMENTATION=OFF \
    -DENABLE_MINIBROWSER=OFF \
    -DUSE_AVIF=OFF -DUSE_JPEGXL=OFF -DENABLE_SPEECH_SYNTHESIS=OFF -DUSE_LIBBACKTRACE=OFF \
    -DUSE_SYSTEM_SYSPROF_CAPTURE=OFF \
    -DENABLE_SPELLCHECK=OFF -DENABLE_GAMEPAD=OFF -DUSE_LIBHYPHEN=OFF \
    -DCMAKE_PREFIX_PATH="$PREFIX" -DICU_ROOT="$PREFIX" \
    -DCMAKE_C_COMPILER_LAUNCHER=ccache -DCMAKE_CXX_COMPILER_LAUNCHER=ccache \
    -DENABLE_BUBBLEWRAP_SANDBOX=OFF \
    -DENABLE_JOURNALD_LOG=OFF
cmake --build /build/webkitgtk/_build -j"$JOBS"
cmake --install /build/webkitgtk/_build

webkit_freetype_inc="$(sed -n 's/^FREETYPE_INCLUDE_DIR_freetype2:PATH=//p' \
    /build/webkitgtk/_build/CMakeCache.txt)"
case "$webkit_freetype_inc" in
    "$PREFIX"/*) ;;
    *)
        echo "ERROR: WebKitGTK configured against freetype headers at" \
             "'${webkit_freetype_inc:-<unset>}', not inside $PREFIX -- the bundled" \
             "freetype is not the one being compiled against" >&2
        exit 1
        ;;
esac
host_freetype_refs="$(grep -n '/usr/include/freetype2\|/usr/lib/[^ ;]*/libfreetype' \
    /build/webkitgtk/_build/CMakeCache.txt || true)"
if [ -n "$host_freetype_refs" ]; then
    echo "ERROR: WebKitGTK's CMake cache still references the HOST freetype -- some" \
         "translation units would compile against 2.12-era headers while linking the" \
         "bundled 2.14 library. Offending entries:" >&2
    echo "$host_freetype_refs" >&2
    exit 1
fi

webkitgtk_so_strings="$(strings "$PREFIX"/lib/libwebkitgtk-6.0.so.* 2>/dev/null || true)"
if ! grep -q '^WEBKIT_EXEC_PATH$' <<<"$webkitgtk_so_strings"; then
    echo "ERROR: WEBKIT_EXEC_PATH string not found in installed libwebkitgtk-6.0.so -- the Task 8 patch did not take effect" >&2
    exit 1
fi

PREFIX="$PREFIX" bash "$(dirname "$0")/host-build.sh"
