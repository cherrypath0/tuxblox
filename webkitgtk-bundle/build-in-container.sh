#!/bin/bash
# webkitgtk-bundle/build-in-container.sh
# Runs INSIDE the tuxblox-webkitgtk-builder container. Builds the full WebKitGTK
# dependency chain from source into a private, relocatable prefix.
set -euo pipefail

source "$(dirname "$0")/versions.env"

PREFIX=/opt/tuxblox-webview
mkdir -p "$PREFIX"
export PKG_CONFIG_PATH="$PREFIX/lib/pkgconfig:$PREFIX/lib/x86_64-linux-gnu/pkgconfig"
# meson defaults to Debian's multiarch libdir (lib/x86_64-linux-gnu) for its own
# installs, while the autotools/cmake builds earlier in this script (icu, libtasn1,
# sqlite, lcms2, libwebp, openjpeg) land in plain lib -- so both directories need to
# be on the rpath, not just plain lib. Without lib/x86_64-linux-gnu here, any binary
# that actually executes during a later build step (not just links) and pulls in a
# meson-installed lib -- e.g. gdk-pixbuf's own build running
# gdk-pixbuf-query-loaders, which is dynamically linked against libgobject-2.0 and
# (transitively) libffi, both installed to lib/x86_64-linux-gnu -- fails at runtime
# with "error while loading shared libraries". This also isn't just a build-time
# concern: the whole point of this prefix is to ship it as a relocatable tarball, so
# an incomplete rpath here would leave the final WebKitGTK bundle broken at runtime.
export LDFLAGS="-Wl,-rpath,$PREFIX/lib -Wl,-rpath,$PREFIX/lib/x86_64-linux-gnu"
export CFLAGS="-O2"
export CXXFLAGS="-O2"
# From-source libraries (starting with glib) install build-time tools -- e.g.
# glib-compile-resources, glib-genmarshal, glib-mkenums -- into $PREFIX/bin. Later
# meson-based builds in this script invoke those tools by bare name (as
# dependencies discovered via meson's `find_program()`), so $PREFIX/bin must be on
# PATH or those lookups fail with "Program 'glib-compile-resources' not found or
# not executable" once a build (gdk-pixbuf, and everything after it) actually
# needs one of GLib's own tools rather than just its headers/libs.
export PATH="$PREFIX/bin:$PATH"
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

echo ":: Building cairo $CAIRO_VERSION"
fetch_and_extract "https://cairographics.org/releases/cairo-${CAIRO_VERSION}.tar.xz" /build/cairo
meson setup /build/cairo/_build /build/cairo --prefix="$PREFIX"
ninja -C /build/cairo/_build -j"$JOBS" install

echo ":: Building harfbuzz $HARFBUZZ_VERSION"
fetch_and_extract \
    "https://github.com/harfbuzz/harfbuzz/releases/download/${HARFBUZZ_VERSION}/harfbuzz-${HARFBUZZ_VERSION}.tar.xz" \
    /build/harfbuzz
meson setup /build/harfbuzz/_build /build/harfbuzz --prefix="$PREFIX"
ninja -C /build/harfbuzz/_build -j"$JOBS" install

echo ":: Building pango $PANGO_VERSION"
fetch_and_extract \
    "https://download.gnome.org/sources/pango/${PANGO_VERSION%.*}/pango-${PANGO_VERSION}.tar.xz" \
    /build/pango
meson setup /build/pango/_build /build/pango --prefix="$PREFIX"
ninja -C /build/pango/_build -j"$JOBS" install

echo ":: Building gdk-pixbuf $GDK_PIXBUF_VERSION"
fetch_and_extract \
    "https://download.gnome.org/sources/gdk-pixbuf/${GDK_PIXBUF_VERSION%.*}/gdk-pixbuf-${GDK_PIXBUF_VERSION}.tar.xz" \
    /build/gdk-pixbuf
meson setup /build/gdk-pixbuf/_build /build/gdk-pixbuf --prefix="$PREFIX" -Dman=false
ninja -C /build/gdk-pixbuf/_build -j"$JOBS" install

echo ":: Building graphene $GRAPHENE_VERSION"
# graphene's "1.10.8" GitHub release has no uploaded release asset (only 1.10.6 and
# earlier do) -- release-download URLs 404 for it. GitHub's auto-generated tag
# source archive works for every tag, same pattern already used for openjpeg above.
fetch_and_extract \
    "https://github.com/ebassi/graphene/archive/refs/tags/${GRAPHENE_VERSION}.tar.gz" \
    /build/graphene
meson setup /build/graphene/_build /build/graphene --prefix="$PREFIX"
ninja -C /build/graphene/_build -j"$JOBS" install
