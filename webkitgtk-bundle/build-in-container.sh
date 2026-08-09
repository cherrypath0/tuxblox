#!/bin/bash
# webkitgtk-bundle/build-in-container.sh
# Runs INSIDE the tuxblox-webkitgtk-builder container. Builds the full WebKitGTK
# dependency chain from source into a private, relocatable prefix.
set -euo pipefail

source "$(dirname "$0")/versions.env"

PREFIX=/opt/tuxblox-webview
mkdir -p "$PREFIX"
export PKG_CONFIG_PATH="$PREFIX/lib/pkgconfig:$PREFIX/lib/x86_64-linux-gnu/pkgconfig"
export LDFLAGS="-Wl,-rpath,$PREFIX/lib"
export CFLAGS="-O2"
export CXXFLAGS="-O2"
JOBS="${JOBS:-$(nproc)}"

fetch_and_extract() {
    local url="$1" dest_dir="$2"
    local tmp_archive
    tmp_archive="$(mktemp)"
    mkdir -p "$dest_dir"
    # Download to a regular file rather than piping curl straight into tar: GNU tar
    # only auto-detects gzip vs. xz vs. bzip2 from the archive's magic bytes when
    # reading a seekable file, not a stdin pipe -- and these URLs mix compression
    # formats (glib ships .tar.xz, everything else here ships .tar.gz/.tgz).
    curl -fL "$url" -o "$tmp_archive"
    tar -xf "$tmp_archive" -C "$dest_dir" --strip-components=1
    rm -f "$tmp_archive"
}

echo ":: Building glib $GLIB_VERSION"
fetch_and_extract \
    "https://download.gnome.org/sources/glib/${GLIB_VERSION%.*}/glib-${GLIB_VERSION}.tar.xz" \
    /build/glib
meson setup /build/glib/_build /build/glib --prefix="$PREFIX" -Dtests=false
ninja -C /build/glib/_build -j"$JOBS" install

echo ":: Building icu $ICU_VERSION"
fetch_and_extract \
    "https://github.com/unicode-org/icu/releases/download/release-${ICU_VERSION//./-}/icu4c-${ICU_VERSION//-/_}-src.tgz" \
    /build/icu
cd /build/icu/source
./configure --prefix="$PREFIX"
make -j"$JOBS"
make install
cd /build

echo ":: Building libtasn1 $LIBTASN1_VERSION"
fetch_and_extract \
    "https://ftp.gnu.org/gnu/libtasn1/libtasn1-${LIBTASN1_VERSION}.tar.gz" /build/libtasn1
cd /build/libtasn1 && ./configure --prefix="$PREFIX" && make -j"$JOBS" && make install && cd /build

echo ":: Building sqlite $SQLITE_VERSION"
mkdir -p /build/sqlite
curl -fL "https://www.sqlite.org/${SQLITE_YEAR}/sqlite-autoconf-${SQLITE_VERSION}.tar.gz" \
    | tar -xz -C /build/sqlite --strip-components=1
cd /build/sqlite && ./configure --prefix="$PREFIX" && make -j"$JOBS" && make install && cd /build

echo ":: Building lcms2 $LCMS2_VERSION"
fetch_and_extract \
    "https://github.com/mm2/Little-CMS/releases/download/lcms${LCMS2_VERSION}/lcms2-${LCMS2_VERSION}.tar.gz" \
    /build/lcms2
cd /build/lcms2 && ./configure --prefix="$PREFIX" && make -j"$JOBS" && make install && cd /build

echo ":: Building libwebp $LIBWEBP_VERSION"
fetch_and_extract \
    "https://storage.googleapis.com/downloads.webmproject.org/releases/webp/libwebp-${LIBWEBP_VERSION}.tar.gz" \
    /build/libwebp
cd /build/libwebp && ./configure --prefix="$PREFIX" && make -j"$JOBS" && make install && cd /build

echo ":: Building openjpeg $OPENJPEG_VERSION"
fetch_and_extract \
    "https://github.com/uclouvain/openjpeg/archive/refs/tags/v${OPENJPEG_VERSION}.tar.gz" \
    /build/openjpeg
cmake -B /build/openjpeg/_build -S /build/openjpeg -DCMAKE_INSTALL_PREFIX="$PREFIX" \
    -DBUILD_SHARED_LIBS=ON
cmake --build /build/openjpeg/_build -j"$JOBS"
cmake --install /build/openjpeg/_build
