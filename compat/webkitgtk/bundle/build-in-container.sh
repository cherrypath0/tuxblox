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

# compat/webkitgtk/bundle/build-in-container.sh
# Runs INSIDE the tuxblox-webkitgtk-builder container. Builds the full WebKitGTK
# dependency chain from source into a private, relocatable prefix.
set -euo pipefail

source "$(dirname "$0")/versions.env"

PREFIX=/opt/tuxblox-webview
mkdir -p "$PREFIX"
# share/pkgconfig is where the X11 protocol packages put their .pc files -- xorgproto
# and xcb-proto install headers and protocol descriptions rather than a library, so
# they follow datarootdir rather than libdir. Without it on this path libxcb's
# configure fails with "No package 'xcb-proto' found" immediately after xcb-proto
# installed successfully, and the whole X11 chain below it stops.
export PKG_CONFIG_PATH="$PREFIX/lib/pkgconfig:$PREFIX/lib/x86_64-linux-gnu/pkgconfig:$PREFIX/share/pkgconfig"
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
# -L (not just -Wl,-rpath,) and CPPFLAGS's -I are needed too, not discovered until
# the GnuTLS/nettle chain (Task 4's TLS backend addition): every from-source
# cross-dependency up to that point was either found via pkg-config (meson builds,
# which compute their own -I/-L from PKG_CONFIG_PATH automatically) or didn't
# depend on another from-source library at all. nettle's configure.ac checks for
# GMP with a bare `AC_CHECK_LIB(gmp, __gmpn_zero_p, ...)` -- no pkg-config, no
# --with-gmp-* flag -- so without an explicit -L search path the compiler falls
# through to system default paths only, silently decides GMP isn't present, and
# nettle builds its own bundled mini-gmp instead of linking the real GMP this
# script builds a few steps earlier. (GMP itself ships no .pc file, so this can't
# be fixed by PKG_CONFIG_PATH alone the way the meson-based gaps were.) CPPFLAGS's
# -I mirrors the same reasoning for header lookup during compilation, not just the
# configure-time link check.
export LDFLAGS="-Wl,-rpath,$PREFIX/lib -Wl,-rpath,$PREFIX/lib/x86_64-linux-gnu -L$PREFIX/lib -L$PREFIX/lib/x86_64-linux-gnu"
export CPPFLAGS="-I$PREFIX/include"
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
# ccache (Task 7 addition, see the matching Containerfile comment for the full "every
# container run is --rm, so nothing survives without this" rationale). Defaults to
# /ccache so the common invocation is just `-v webkitgtk-ccache:/ccache`, matching
# webkitgtk-prefix's own convention; still works with no mount at all (ccache just
# writes into the container's own throwaway filesystem and provides zero benefit
# across runs in that case, but never errors). Only wired into the WebKitGTK cmake
# invocation below (via CMAKE_C_COMPILER_LAUNCHER/CMAKE_CXX_COMPILER_LAUNCHER) --
# the earlier meson/autotools dependency builds each run at most once per from-scratch
# script execution already, so there's nothing to gain caching them, and retrofitting
# every one of them was out of scope for what this task actually needed.
export CCACHE_DIR="${CCACHE_DIR:-/ccache}"
mkdir -p "$CCACHE_DIR"

# The archives this script builds from are mounted read-only from the host, filled by
# fetch-sources.sh beforehand. Nothing here downloads: see that script for why the two
# halves are deliberately separate.
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

# The other way a build here could reach the network: a meson project declares its
# optional dependencies as wrap files and downloads them at configure time when the
# dependency is not already present. Nearly every wrap in these tarballs resolves
# against a library this bundle vendors itself and is never fetched, but cairo ships no
# subproject sources at all and genuinely builds fontconfig as one. Pointing meson's
# package cache at the same read-only directory lets it find that archive on disk and
# check it against the hash in cairo's own fontconfig.wrap. The container is run with
# no network at all (see build.sh), so anything this misses fails loudly instead of
# quietly fetching.
export MESON_PACKAGE_CACHE_DIR="$SOURCES_DIR"

# extract_source <archive filename> <dest_dir>
#
# Unpacks one archive out of the read-only sources directory, after checking its bytes
# against the checksum recorded for it. A missing archive, or one that does not match,
# stops the build instead of being fetched or built anyway -- so a release can always
# be traced back to a known set of source bytes, and an upstream that starts serving
# something else cannot end up inside a shipped binary unnoticed.
#
# Every package here used to be a `curl` straight into `tar` at build time. That made
# each build depend on a dozen upstreams being up and serving the same thing, and
# verified none of it. It bit once already: Savannah's freetype download URL is a
# redirector onto a rotating pool of volunteer mirrors, and the one it picked was down,
# which killed a build 40 minutes in. The retries and fallback URLs that used to live
# here moved to fetch-sources.sh, where a failure costs a re-run instead of a build.
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
    # tar picks gzip vs xz vs bzip2 out of the archive's own magic bytes, which it can
    # only do on a seekable file -- these archives mix all three formats.
    tar -xf "$archive" -C "$dest_dir" --strip-components=1
}

# --- Vendored host dependencies ---------------------------------------------------
# Everything in this section used to be taken from whatever the user's distro shipped.
# Borrowing a library brings the host's SONAME and build options with it, which is how
# the libtiff.so.6 and libffi.so.7-vs-.so.8 breakages happened -- fine on this machine,
# unloadable on the distros most users run. After this section the only libraries left
# on the host boundary are glibc, libudev, the Vulkan/VA-API driver loaders and the
# audio daemons; README.md's "System-provided libraries" section is the current list.
#
# Order matters here and is not alphabetical: pcre2 must precede glib, fribidi and
# libthai must precede pango, pixman must precede cairo, and the X11 chain builds
# bottom-up from xorgproto. Anything inserted into this section needs placing against
# those, not appended to the end.

echo ":: Building zlib $ZLIB_VERSION"
extract_source "zlib-${ZLIB_VERSION}.tar.gz" /build/zlib
cd /build/zlib
./configure --prefix="$PREFIX"
make -j"$JOBS"
make install
cd /build

echo ":: Building bzip2 $BZIP2_VERSION"
extract_source "bzip2-${BZIP2_VERSION}.tar.gz" /build/bzip2
# bzip2 ships no configure, and its default target builds only the static archive.
# Makefile-libbz2_so is the one that produces the shared object; it installs nothing,
# so the library, its SONAME links and the header are placed by hand. The SONAME it
# records is libbz2.so.1.0, which is the name everything else here links against.
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
# glib links pcre2 unconditionally for GRegex; without this the bundle's own glib
# resolves libpcre2-8 against the host, which is the single most load-bearing
# unvendored dependency the bundle had -- nothing starts if it is absent.
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
# pango links libthai for Thai line breaking whether or not anybody types Thai, so an
# absent libthai.so.0 stops pango loading at all -- and with it GTK and the whole host.
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

# --- X11 client libraries ---------------------------------------------------------
# Built bottom-up: protocol headers, then the authentication helpers libxcb needs,
# then libxcb, then Xlib, then the extension libraries on top of it. These are client
# libraries speaking a stable wire protocol to whatever X server is already running,
# so vendoring them pins our ABI without pinning the user's display server.

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
# libxcb's build imports xcbgen, the Python package xcb-proto just installed into this
# prefix rather than into the container's own site-packages. Without this on PYTHONPATH
# libxcb's configure fails with "xcbgen not found" even though xcb-proto is installed.
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
# The keymap data itself stays on the host (/usr/share/X11/xkb): it has to match the
# layouts the user's desktop actually configured, so only the library is vendored.
extract_source "libxkbcommon-${XKBCOMMON_VERSION}.tar.xz" /build/libxkbcommon
meson setup /build/libxkbcommon/_build /build/libxkbcommon --prefix="$PREFIX" \
    -Denable-docs=false -Denable-wayland=false -Denable-xkbregistry=false
ninja -C /build/libxkbcommon/_build -j"$JOBS" install

echo ":: Building libglvnd $LIBGLVND_VERSION"
# The bundle already ships its own Mesa and pins it through __EGL_VENDOR_LIBRARY_FILENAMES,
# but the glvnd dispatch layer in front of it -- libEGL.so.1/libGLX.so.0 -- was still the
# host's, so the vendor library we select was being loaded by a loader we do not control.
# Vendoring glvnd closes that: the loader and the driver behind it are now both ours.
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
# Vendored because libgobject links it and the SONAME moves between distros:
# Debian 11 (this builder) ships libffi.so.7, while Ubuntu 22.04, Arch and Fedora
# ship .so.8. Borrowing the builder's copy makes the bundle unloadable on exactly
# the distros most users run -- the mirror image of the libtiff .so.6 failure.
cd /build/libffi
./configure --prefix="$PREFIX" --disable-static --disable-docs
make -j"$JOBS"
make install
cd /build

echo ":: Building wayland $WAYLAND_VERSION"
# The shipped 3.7.0 bundle already contained libwayland 1.23.0 and nothing here built it:
# it came out of a builder image that had been modified by hand, so a library inside a
# published release existed nowhere in this repository and could not be rebuilt from it.
# Debian 11 has 1.18.0 and gtk4 requires ">= 1.23.0", which is why the image had to be
# modified in the first place; building it here is what makes the bundle reproducible.
#
# Placed after libffi because libwayland-server marshals through it, and before gtk4 and
# mesa, which are the consumers. Shipping a newer libwayland than the user's compositor is
# safe and is what every bundled runtime does: the client library talks a stable wire
# protocol to the compositor, so the two are not required to match.
#
# dtd_validation is off so this does not have to wait for libxml2 further down; it only
# checks the protocol DTD at build time and changes nothing about what is produced.
extract_source "wayland-${WAYLAND_VERSION}.tar.xz" /build/wayland
meson setup /build/wayland/_build /build/wayland --prefix="$PREFIX" \
    -Dlibraries=true -Dscanner=true -Dtests=false -Ddocumentation=false \
    -Ddtd_validation=false
ninja -C /build/wayland/_build -j"$JOBS" install

echo ":: Building wayland-protocols $WAYLAND_PROTOCOLS_VERSION"
# Protocol XML and a pkg-config file, no library -- it is build-time data, so nothing from
# it is shipped. gtk4 and mesa both require ">= 1.41" and Debian 11 carries 1.20.
extract_source "wayland-protocols-${WAYLAND_PROTOCOLS_VERSION}.tar.xz" /build/wayland-protocols
meson setup /build/wayland-protocols/_build /build/wayland-protocols --prefix="$PREFIX" \
    -Dtests=false
ninja -C /build/wayland-protocols/_build -j"$JOBS" install

echo ":: Building glib $GLIB_VERSION"
extract_source "glib-${GLIB_VERSION}.tar.xz" /build/glib
# -Dselinux=disabled is REQUIRED for portability, not a preference. Debian ships
# libselinux and glib's gio links it when present; Arch does not ship it at all, so
# a bundle built here without this flag dies on Arch with
# "libselinux.so.1: cannot open shared object file" before reaching main().
# Caught by running the freshly built bundle on the maintainer's own Arch box.
# gio's selinux support only annotates file security contexts -- nothing in this
# bundle reads them.
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

# --- Task 8 post-review correction (2026-08-10): libjpeg-turbo, libxml2, libxslt --
# Originally satisfied by this container's system packages (libjpeg62-turbo-dev,
# libxml2-dev, libxslt1-dev in Containerfile) -- Task 8's review ran `ldd` against
# the packaged tarball OUTSIDE this build container for the first time in this whole
# plan and found the shipped bundle depended on the build machine's own system
# copies of all three at runtime, none of which get vendored into $PREFIX by
# installing a -dev package. libjpeg is the worst offender: Debian ships
# `libjpeg.so.62`, a Debian-*specific* soname (most other distros ship
# `libjpeg.so.8`), so this wasn't just "missing on a minimal target," it was
# unsatisfiable on most non-Debian hosts entirely as originally built. All three are
# genuine, unconditional WebKitGTK requirements -- OptionsGTK.cmake has
# `find_package(JPEG REQUIRED)` and `find_package(LibXml2 2.9.13 REQUIRED)` with no
# disable option at all, and `find_package(LibXslt 1.1.13 REQUIRED)` whenever
# ENABLE_XSLT is on (kept on -- real web content still uses XSLT) -- so simply
# turning them off the way spellcheck/gamepad/hyphenation were below wasn't an
# option. Built from source into $PREFIX instead, the same as every other real
# dependency in this chain, which sidesteps the soname/portability problem entirely.
# Placed here (before cairo) rather than right before WebKitGTK itself because
# gdk-pixbuf's JPEG loader (a few steps below) and gtk4's bundled wayland-scanner
# subproject (dtd_validation, needs libxml2 -- see Containerfile's Task 4 comment,
# now superseded) both need these earlier in the pipeline too.
echo ":: Building libjpeg-turbo $LIBJPEG_TURBO_VERSION"
extract_source "libjpeg-turbo-${LIBJPEG_TURBO_VERSION}.tar.gz" /build/libjpeg-turbo
# -DWITH_JPEG8=1: makes the installed libjpeg.so report itself under the modern
# libjpeg-turbo/libjpeg v8 API/ABI (SONAME libjpeg.so.8), matching what most
# non-Debian distros ship and what gdk-pixbuf/WebKitGTK both link against by name
# (`libjpeg`) rather than caring about the exact SONAME -- deliberately NOT
# reproducing Debian's own `.so.62` (the original, Debian/Ubuntu-specific v6.2 ABI
# numbering) that caused this whole correction. -DENABLE_SHARED=ON/-DENABLE_STATIC=OFF:
# only a shared library is needed (RPATH-relocated the same way as everything else
# in this bundle); skip building+installing the static .a nothing here links.
cmake -B /build/libjpeg-turbo/_build -S /build/libjpeg-turbo \
    -DCMAKE_INSTALL_PREFIX="$PREFIX" -DCMAKE_BUILD_TYPE=Release \
    -DWITH_JPEG8=1 -DENABLE_SHARED=ON -DENABLE_STATIC=OFF
cmake --build /build/libjpeg-turbo/_build -j"$JOBS"
cmake --install /build/libjpeg-turbo/_build

echo ":: Building libtiff $LIBTIFF_VERSION"
extract_source "tiff-${LIBTIFF_VERSION}.tar.gz" /build/libtiff
# Vendored for exactly the reason libjpeg-turbo above is: the distro SONAME is not
# portable. Debian 11/Ubuntu 22.04 ship libtiff.so.5, Debian 12/Arch ship .so.6, so
# linking whatever the builder happens to have breaks the other half of the users --
# and it did. Building on debian:12 produced a bundle needing libtiff.so.6, and
# webview2loader-host then failed to start on Ubuntu 22.04 with
# "libtiff.so.6: cannot open shared object file", taking the whole webview with it.
# Shipping our own copy makes the host's version irrelevant either way.
# Pulled in by libgtk-4 and gdk-pixbuf's TIFF loader.
# Every optional codec is OFF on purpose. Left on 'auto', libtiff picks up the
# BUILD container's Debian 11 libjpeg62/libwebp6/libdeflate and drags their
# SONAMEs into the bundle -- libjpeg.so.62 and libwebp.so.6 do not exist on Arch
# or on newer Debians, so the bundle would then fail to load there. Exactly the
# portability trap this vendoring exists to close, just pointed the other way.
# GTK's TIFF loader only needs baseline TIFF; none of these codecs are used.
cmake -B /build/libtiff/_build -S /build/libtiff \
    -DCMAKE_INSTALL_PREFIX="$PREFIX" -DCMAKE_PREFIX_PATH="$PREFIX" -DCMAKE_BUILD_TYPE=Release \
    -DBUILD_SHARED_LIBS=ON -Dtiff-tools=OFF -Dtiff-tests=OFF -Dtiff-docs=OFF \
    -Djpeg=OFF -Dold-jpeg=OFF -Dwebp=OFF -Dzstd=OFF -Dlzma=OFF -Dlibdeflate=OFF -Djbig=OFF
cmake --build /build/libtiff/_build -j"$JOBS"
cmake --install /build/libtiff/_build

echo ":: Building libxml2 $LIBXML2_VERSION"
extract_source "libxml2-${LIBXML2_VERSION}.tar.xz" /build/libxml2
# -Dpython=disabled: libxml2's meson build can also build Python bindings; nothing
# in this C/C++ dependency chain needs them, and building them would pull in a
# python3-dev header dependency this Containerfile doesn't otherwise need.
meson setup /build/libxml2/_build /build/libxml2 --prefix="$PREFIX" -Dpython=disabled
ninja -C /build/libxml2/_build -j"$JOBS" install

echo ":: Building libxslt $LIBXSLT_VERSION"
extract_source "libxslt-${LIBXSLT_VERSION}.tar.xz" /build/libxslt
# libxslt ${LIBXSLT_VERSION} ships both an autotools ./configure and a CMakeLists.txt
# but no meson.build -- autotools chosen to match this script's existing convention
# for every other non-meson dependency (gmp, nettle, gnutls, icu, sqlite, lcms2,
# libwebp, libtasn1, ...). --without-python: same reasoning as libxml2's
# -Dpython=disabled above -- no Python bindings needed here.
cd /build/libxslt
# NO --with-libxml-prefix, deliberately: passing it makes configure locate libxml2
# through $PREFIX/bin/xml2-config, and libxml2 2.15 dropped that script's --shared
# option, so the probe dies with "Unknown option --shared" followed by the
# misleading "Could not find libxml2 anywhere" -- while sitting right next to a
# perfectly good 2.15.3. Without the flag it uses pkg-config, which PKG_CONFIG_PATH
# already points at this same prefix.
#
# This only became visible when the prefix volume was wiped for the sniper rebuild:
# libxslt had been built once, long ago, against a pre-2.15 libxml2 and then simply
# persisted in the volume, so no later run ever re-ran this configure. A from-scratch
# build had been broken for a while without anyone noticing.
./configure --prefix="$PREFIX" --without-python
make -j"$JOBS"
make install
cd /build

echo ":: Building freetype $FREETYPE_VERSION (pass 1, no harfbuzz)"
# Vendored as of 2026-08-18 -- see versions.env's FREETYPE_VERSION comment for the
# COLRv1 ABI crash this fixes. Must come before cairo, which needs cairo-ft, and before
# fontconfig below, which links freetype.
#
# Two passes, because freetype and harfbuzz depend on each other: harfbuzz's hb-ft
# needs freetype, and freetype's autohinter uses harfbuzz to reach glyphs a font's
# cmap doesn't address directly. Every distro resolves this the same way -- build
# freetype without harfbuzz, build harfbuzz against it, rebuild freetype. Pass 2 is
# below, right after harfbuzz. Not optional polish: Debian's own libfreetype6 (what
# this bundle used to pick up from the host) is built with harfbuzz, so shipping
# without it would be a hinting-quality regression against the previous release.
#
# brotli stays enabled: WebKit's OptionsGTK.cmake probes for FreeType's builtin WOFF2
# support and, if it isn't there, hard-requires libwoff2dec instead -- which this
# container doesn't have, so the cmake configure would FATAL_ERROR. libbrotli-dev is
# already present in the image as a transitive dependency, and libbrotlidec.so.1 was
# already a host runtime dependency by way of the system freetype this replaces.
# png is needed for CBDT/CBLC color bitmap fonts (the non-COLRv1 emoji format most
# non-Fedora distros still ship).
extract_source "freetype-${FREETYPE_VERSION}.tar.xz" /build/freetype
meson setup /build/freetype/_build /build/freetype --prefix="$PREFIX" \
    -Dharfbuzz=disabled -Dbrotli=enabled -Dpng=enabled -Dzlib=system -Dbzip2=disabled
ninja -C /build/freetype/_build -j"$JOBS" install

echo ":: Building fontconfig $FONTCONFIG_VERSION"
# Built here rather than left to cairo's meson subproject, which is what used to happen.
# That subproject only triggers when the container's own fontconfig is too old for cairo,
# and Debian 11's 2.13.1 satisfies cairo's ">= 2.13.0" -- so nothing was ever built, and
# cairo and libXft linked the host's copy instead, one built against a different freetype
# than the 2.14.3 above. pango wants ">= 2.15.0" and falls back to a git clone, which an
# offline build cannot serve at all; that is the failure this fixes.
#
# The config paths are deliberately left at their defaults, which put them under $PREFIX.
# That is not incidental: the bundle SHIPS its own etc/fonts, and the webview points
# FONTCONFIG_PATH at it, so pointing these at the host's /etc/fonts instead would ship no
# configuration at all and leave that variable aimed at a directory that does not exist --
# which costs every font rather than saving anything. The shipped fonts.conf already lists
# the host's font directories (/usr/share/fonts, the xdg one, ~/.fonts), so host fonts are
# found either way; what the default loses is the user's own conf.d tweaks, and what it
# buys is a bundle that renders text at all. package.sh drops the one $PREFIX path in that
# file, the build-prefix cachedir, leaving the xdg one it would have fallen through to.
#
# cache-build is off because it runs fc-cache at install time, which builds a cache keyed
# to this container rather than the machine that will use it.
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
# Second half of the dependency cycle described in pass 1 above. Same version, same
# soname, same public ABI -- the harfbuzz option only changes the autofitter's
# internals -- so overwriting the pass-1 library in place is safe for fontconfig and
# cairo, which already linked against it.
meson setup --wipe /build/freetype/_build /build/freetype --prefix="$PREFIX" \
    -Dharfbuzz=enabled -Dbrotli=enabled -Dpng=enabled -Dzlib=system -Dbzip2=disabled
ninja -C /build/freetype/_build -j"$JOBS" install

echo ":: Building pango $PANGO_VERSION"
extract_source "pango-${PANGO_VERSION}.tar.xz" /build/pango
# -Dxft=disabled, and no libXft build above any more. pangoxft was the only thing that
# wanted Xft, nothing in this bundle loads pangoxft -- GTK 4 draws text through
# pangocairo -- and it was not even resolvable in the shipped tarball, which carried
# libpangoxft without the libXft it needs. Left on auto it would link the container's own
# libXft instead, which is a host dependency bought for a module nothing opens.
meson setup /build/pango/_build /build/pango --prefix="$PREFIX" \
    -Dxft=disabled
ninja -C /build/pango/_build -j"$JOBS" install

echo ":: Building gdk-pixbuf $GDK_PIXBUF_VERSION"
extract_source "gdk-pixbuf-${GDK_PIXBUF_VERSION}.tar.xz" /build/gdk-pixbuf
meson setup /build/gdk-pixbuf/_build /build/gdk-pixbuf --prefix="$PREFIX" -Dman=false
ninja -C /build/gdk-pixbuf/_build -j"$JOBS" install

echo ":: Building graphene $GRAPHENE_VERSION"
# graphene's "1.10.8" GitHub release has no uploaded release asset (only 1.10.6 and
# earlier do) -- release-download URLs 404 for it. GitHub's auto-generated tag
# source archive works for every tag, same pattern already used for openjpeg above.
extract_source "graphene-${GRAPHENE_VERSION}.tar.gz" /build/graphene
meson setup /build/graphene/_build /build/graphene --prefix="$PREFIX"
ninja -C /build/graphene/_build -j"$JOBS" install

echo ":: Building libpsl $LIBPSL_VERSION"
extract_source "libpsl-${LIBPSL_VERSION}.tar.gz" /build/libpsl
# IDNA backend pinned to ICU instead of left on 'auto'. On debian:12 'auto' happened
# to find libunistring and silently used it; sniper (Debian 11) has no
# libunistring-dev, so the same 'auto' hard-errors with
# "C shared or static library 'unistring' not found".
#
# Pinning to libicu rather than installing libunistring-dev: ICU is already built
# into this prefix (see the icu step above), so this adds no new dependency at all,
# whereas libunistring would become another host library the bundle links and must
# then be vendored to stay portable -- the exact trap libtiff just cost us.
meson setup /build/libpsl/_build /build/libpsl --prefix="$PREFIX" \
    -Druntime=libicu -Dbuiltin=true
ninja -C /build/libpsl/_build -j"$JOBS" install

echo ":: Building libgudev $LIBGUDEV_VERSION"
extract_source "libgudev-${LIBGUDEV_VERSION}.tar.xz" /build/libgudev
meson setup /build/libgudev/_build /build/libgudev --prefix="$PREFIX"
ninja -C /build/libgudev/_build -j"$JOBS" install

echo ":: Building libsecret $LIBSECRET_VERSION"
extract_source "libsecret-${LIBSECRET_VERSION}.tar.xz" /build/libsecret
# libsecret's meson.options declares `introspection` as a plain boolean (default
# true), unlike the `feature`-typed (auto/enabled/disabled) introspection options
# used everywhere else in this chain (glib, pango, gtk4, libsoup, ...) -- so it's a
# hard, unconditional dependency on the separate gobject-introspection project
# (g-ir-scanner/g-ir-compiler + the gobject-introspection-1.0 pkg-config module),
# which nothing earlier in this script builds. Every other component in the stack
# already resolves its own introspection option to "not found -> skip" the same
# way, since that tooling isn't present; libsecret's option just isn't shaped to
# degrade the same way. Introspection only gates .gir/.typelib generation and
# introspection-only tests (see libsecret/meson.build's `if get_option('introspection')`
# blocks) -- it does not affect the C library or headers WebKitGTK links against, so
# disabling it here doesn't drop any functionality Roblox's pages could depend on;
# it only means libsecret wouldn't be usable from GI-based bindings (Python/gjs),
# which is out of scope for this C/C++ build chain.
meson setup /build/libsecret/_build /build/libsecret --prefix="$PREFIX" -Dmanpage=false -Dgtk_doc=false -Dintrospection=false
ninja -C /build/libsecret/_build -j"$JOBS" install

echo ":: Building libnghttp2 $LIBNGHTTP2_VERSION"
extract_source "nghttp2-${LIBNGHTTP2_VERSION}.tar.xz" /build/nghttp2
# libsoup links this for HTTP/2, and it was being taken from the host. It is not
# installed by default on minimal systems, and the host binary refuses to start
# when it is absent -- "libnghttp2.so.14: cannot open shared object file" was the
# actual failure in a clean Ubuntu 22.04 container. ENABLE_LIB_ONLY: only the
# library is wanted, not nghttp2's tools/tests (which would pull libev, libcares,
# openssl and more host dependencies with them).
cmake -B /build/nghttp2/_build -S /build/nghttp2 \
    -DCMAKE_INSTALL_PREFIX="$PREFIX" -DCMAKE_BUILD_TYPE=Release \
    -DENABLE_LIB_ONLY=ON -DBUILD_SHARED_LIBS=ON -DBUILD_STATIC_LIBS=OFF
cmake --build /build/nghttp2/_build -j"$JOBS"
cmake --install /build/nghttp2/_build

echo ":: Building libsoup $LIBSOUP_VERSION"
extract_source "libsoup-${LIBSOUP_VERSION}.tar.xz" /build/libsoup
# Plan 2 (webview2loader) investigation finding: libsoup/libsoup/soup-init.c's ELF
# constructor calls soup2_is_loaded(), which probes for a libsoup2-only symbol via
# `dlopen(NULL, RTLD_LAZY | RTLD_GLOBAL)` + `dlsym(handle, "soup_uri_new")` and calls
# the fatal g_error() (aborts the process) if it finds a match. This is meant to catch
# a real misconfiguration (libsoup2 and libsoup3 loaded in the same process at once),
# but `dlopen(NULL, ...)` -- the "handle for the whole program" call -- is a documented
# rough edge when made from code running inside a dlmopen()-isolated linker namespace,
# which is exactly how webview2loader's unixlib.c loads this entire bundle (Task 6's
# fix for a separate, already-solved host-glib collision, see unixlib.c's own
# comments). Verified via disassembly + `nm -D` across the whole bundle, the whole
# Wine dist tree, and the host system: nothing anywhere actually defines
# `soup_uri_new` -- there is no real libsoup2/libsoup3 coexistence happening, so this
# is a false-positive-shaped fatal error triggered by the probe mechanism itself, not
# a real conflict. We only ever load exactly one libsoup per namespace in this
# deployment, so the scenario this check guards against cannot occur here regardless
# of whether the check runs -- disable the call at its one call site (leaving the
# function itself, and its dlopen/dlsym calls, untouched/uncalled) rather than
# reimplementing or trying to make the probe namespace-safe.
sed -i 's/if (soup2_is_loaded ())/if (0 \&\& soup2_is_loaded ())/' /build/libsoup/libsoup/soup-init.c
grep -q 'if (0 && soup2_is_loaded ())' /build/libsoup/libsoup/soup-init.c || {
    echo "ERROR: soup2_is_loaded() patch did not apply -- libsoup source layout changed, update the sed pattern above" >&2
    exit 1
}
# -Dtls_check=false: libsoup itself has no bundled TLS implementation in ANY
# configuration -- it always delegates to GIO's pluggable TLS backend, which is
# provided at runtime by the separate glib-networking project (a dynamically loaded
# GIO module, not something libsoup links against at build time). `tls_check` (default
# true) is purely a build-machine sanity assert: it runs a test program and fails the
# *build* if the machine doing the building doesn't already have glib-networking
# installed. Since this container never runs the libsoup it's building, that check
# is meaningless here regardless of its value -- libsoup's own meson_options.txt
# documents exactly this escape hatch for packagers who provide glib-networking as a
# separate runtime dependency instead of a build-time one.
#
# IMPORTANT -- this is NOT the same as "TLS is handled, ship it": nothing in this
# plan's Global Constraints dependency list (glib, cairo, pango, gdk-pixbuf, GTK-4,
# libsoup, Mesa, GStreamer, libgudev, libsecret, libtasn1, libwebp, lcms2, openjpeg,
# sqlite, wpebackend-fdo, ICU) currently builds glib-networking or a TLS backend for
# it (gnutls or openssl). Without glib-networking present as a GIO module in the
# final bundle, GIO falls back to its dummy TLS backend at runtime and every HTTPS
# connection WebKitGTK makes -- which is all of them for Roblox -- will fail. A later
# task MUST build glib-networking (+ gnutls or openssl) and install it into this same
# prefix before the bundle produced by this plan is functionally complete. Flagged
# in this task's report rather than silently building it here, since it's outside
# this task's brief (gtk4/libsoup-3/libsecret/libgudev/libpsl) and pulls in its own
# dependency chain (nettle, p11-kit, gnutls/openssl, etc.) that deserves its own task.
meson setup /build/libsoup/_build /build/libsoup --prefix="$PREFIX" \
    -Dvapi=disabled -Dgssapi=disabled -Dsysprof=disabled -Dtls_check=false
ninja -C /build/libsoup/_build -j"$JOBS" install

echo ":: Building gtk4 $GTK4_VERSION"
extract_source "gtk-${GTK4_VERSION}.tar.xz" /build/gtk4
# TuxBlox patch: fix an upstream GTK crash that aborts this whole helper process
# whenever a reparented, EGL-backed surface's X window is destroyed by someone else.
#
# gdk/gdksurface.c's _gdk_surface_destroy_hierarchy calls the backend's own destroy
# and then asserts, unconditionally:
#
#     GDK_SURFACE_GET_CLASS (surface)->destroy (surface, foreign_destroy);
#     /* backend must have unset this */
#     g_assert (priv->egl_native_window == NULL);
#
# ...but gdk/x11/gdksurface-x11.c's gdk_x11_surface_destroy only unsets it inside
# `if (!foreign_destroy)`. So any EGL-backed X11 surface destroyed by anyone other
# than GDK itself trips a fatal assertion. That is not an exotic case for TuxBlox:
# compat/webkitgtk/bundle/host/geometry.c XReparentWindow's the webview's surface into
# Roblox Studio's own top-level window, so when Studio destroys that window the X
# server destroys this child along with it, and GDK arrives at exactly that path via
# gdk_x11_surface_destroy_notify -> _gdk_surface_destroy (surface, TRUE). Confirmed
# against a real coredump (SIGABRT in g_assertion_message_expr under gtk_window_destroy)
# whose log line -- "GdkSurface 0x... unexpectedly destroyed" -- is the g_warning
# immediately above that same _gdk_surface_destroy call.
#
# host/watchdog.c already races to tear the webview down cooperatively before GDK
# notices, but that is a race it can lose (both its GSource and GDK's event source sit
# at G_PRIORITY_DEFAULT). This patch removes the crash itself rather than improving the
# odds of avoiding it, so losing that race is merely suboptimal instead of fatal.
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
# Brief's original flag was "-Ddemos=false" -- gtk4's meson.options actually names
# this option "build-demos" (there is no bare "demos" option; "-Ddemos=false" fails
# meson setup immediately with "Unknown option: demos"). Confirmed against the real
# gtk-4.18.6 tarball's meson.options before fixing.
# -Dprint-cups=disabled drops libprintbackend-cups.so, the only thing in this bundle
# that pulled in libcups.so.2. Nothing in the Roblox sign-in flow prints.
meson setup /build/gtk4/_build /build/gtk4 --prefix="$PREFIX" \
    -Dmedia-gstreamer=disabled -Dvulkan=disabled -Dbuild-tests=false -Dbuild-demos=false -Dbuild-examples=false \
    -Dprint-cups=disabled
ninja -C /build/gtk4/_build -j"$JOBS" install

# --- TLS backend for libsoup/GIO -------------------------------------------------
# libsoup above was built with -Dtls_check=false, which only skips a *build-machine*
# sanity assert -- it does NOT mean TLS is handled. libsoup has no TLS implementation
# of its own in any configuration; it always delegates to GIO's pluggable TLS backend
# at runtime, which is provided by the separate glib-networking project as a
# dynamically-loaded GIO module. Without it, GIO silently falls back to a dummy
# backend and every HTTPS request WebKitGTK makes -- all of them, for Roblox --
# fails. This section builds glib-networking and its own dependency chain (GnuTLS,
# the GNOME-stack default / "please don't second-guess our defaults" per
# glib-networking's own meson.options, over the OpenSSL alternative) from source, so
# the bundle in this prefix has real, working TLS, not just TLS-shaped libraries.
#
# Chain: GMP (bignum arithmetic) -> Nettle (crypto primitives, links the real GMP,
# not its bundled mini-gmp fallback) -> p11-kit (PKCS#11 + system trust-store
# integration, already have its other dependencies libffi/libtasn1 from earlier in
# this script) -> GnuTLS (the TLS protocol implementation) -> glib-networking (the
# GIO module that makes GnuTLS visible to GIO/libsoup at runtime).

echo ":: Building gmp $GMP_VERSION"
# gmplib.org went unreachable mid-build on 2026-08-18 (recovered on curl's retry).
# ftp.gnu.org is GMP's other official distribution point, not a volunteer mirror.
extract_source "gmp-${GMP_VERSION}.tar.xz" /build/gmp
# --enable-fat is required, not a tuning choice. GMP's configure otherwise probes the
# BUILD machine's CPU and compiles one microarchitecture's mpn assembly with no runtime
# dispatch, so a bundle built on a Zen or Haswell-or-newer machine fills libgmp with
# mulx/adcx/adox and dies with SIGILL on any older CPU the moment GnuTLS asks it for
# bignum work -- which is the first TLS handshake, i.e. signing in. --enable-fat builds
# every variant and picks one from CPUID at startup, as every distro's own gmp does.
cd /build/gmp && ./configure --prefix="$PREFIX" --enable-fat && make -j"$JOBS" && make install && cd /build

echo ":: Building nettle $NETTLE_VERSION"
extract_source "nettle-${NETTLE_VERSION}.tar.gz" /build/nettle
cd /build/nettle
# --libdir="$PREFIX/lib" is required, not cosmetic: nettle's own configure.ac
# silently overrides libdir to "$PREFIX/lib64" on 64-bit ABIs whenever the caller
# hasn't explicitly set --libdir (see its configure.ac's `libdir='${exec_prefix}/lib64'`
# branch) -- a third libdir convention this prefix would otherwise have to track
# alongside plain lib (autotools/cmake) and lib/x86_64-linux-gnu (meson). Forcing it
# to plain lib keeps every autotools build in this script landing in the same place.
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
# --with-default-trust-store-file / --with-default-trust-store-dir: without an
# explicit choice, gnutls's own configure probes a short list of common distro CA
# bundle paths and bakes in whatever it finds on THIS build machine -- fine for the
# container, meaningless for the arbitrary target Linux machine this relocatable
# bundle actually runs on later. /etc/ssl/certs/ca-certificates.crt (Debian/Ubuntu/
# Arch) and the /etc/ssl/certs hash-symlink directory (near-universal across Linux
# distros regardless of each one's own "native" bundle location, since most also
# maintain that directory for OpenSSL/cross-tool compatibility) are the most
# broadly-portable choice available, not merely what happens to exist here.
# --with-included-unistring: gnutls needs libunistring (Unicode string ops); rather
# than adding yet another external from-source library for a single internal
# utility dependency, gnutls's own build explicitly supports compiling a bundled
# copy in directly -- the sanctioned self-contained-build path, not a workaround.
# --disable-doc/--disable-manpages/--disable-guile/--without-tpm2/--without-tpm:
# documentation and Guile bindings are irrelevant to a C library consumed by
# libsoup/WebKitGTK; TPM2/TPM1 hardware-backed key storage is a niche feature this
# embedded-webview use case has no need for and that would otherwise pull in
# tpm2-tss/trousers as further from-source dependencies for no practical benefit.
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
# -Dgnome_proxy=disabled: this option (default 'enabled', not 'auto') needs
# gsettings-desktop-schemas, which Debian 12's package ships as schema data only --
# no .pc file at all, so it can never be satisfied via pkg-config on this baseline
# regardless of whether the apt package is installed.
#
# -Dlibproxy=disabled: Task 8 post-review correction (2026-08-10) -- originally left
# enabled on the theory that its apt dependency (libproxy-dev) was a normal, cheap
# system package and proxy auto-detection is a real feature a corporate-proxy user
# would notice missing. Review found that reasoning incomplete: libproxy-dev
# satisfies this *build-time* check inside the container, but the resulting
# glib-networking libproxy module still links against the *build container's own
# system* libproxy.so.1 at runtime -- never vendored into $PREFIX the way every real
# dependency in this bundle is -- so the shipped tarball carried a dependency on a
# library that would never actually be present on an arbitrary target machine
# (confirmed via `ldd` against the packaged bundle run OUTSIDE this container, the
# corrected verification methodology this whole review round introduced). With both
# proxy resolvers now off, WebKitGTK falls back to no automatic proxy
# auto-detection -- a real, accepted capability loss (same tier as the disabled
# spellcheck/gamepad/hyphenation features below), not a portability workaround.
meson setup /build/glib-networking/_build /build/glib-networking --prefix="$PREFIX" \
    -Dgnome_proxy=disabled -Dlibproxy=disabled
ninja -C /build/glib-networking/_build -j"$JOBS" install

echo ":: Building libdrm $LIBDRM_VERSION"
extract_source "libdrm-${LIBDRM_VERSION}.tar.xz" /build/libdrm
# Vendored for two reasons at once.
#
# Required: Mesa ${MESA_VERSION} wants libdrm >= 2.4.109 and sniper (Debian 11)
# ships 2.4.104, so its configure fails outright without this.
#
# Wanted anyway: libdrm was already one of the host libraries this bundle linked
# rather than shipped, and Mesa is tightly version-coupled to it. Borrowing the
# host's copy meant a bundled Mesa talking to whatever libdrm the user's distro
# happened to have -- fine on the machine it was built on, a coin flip elsewhere.
# Shipping the matching pair removes that variable and one more host dependency.
meson setup /build/libdrm/_build /build/libdrm --prefix="$PREFIX" \
    -Dcairo-tests=disabled -Dvalgrind=disabled
ninja -C /build/libdrm/_build -j"$JOBS" install

echo ":: Building mesa $MESA_VERSION"
extract_source "mesa-${MESA_VERSION}.tar.xz" /build/mesa
# The plan brief's original flags (-Dgallium-drivers=swrast, -Dosmesa=true) don't
# match current Mesa's actual meson.options -- verified directly against the real
# mesa-${MESA_VERSION} source tree, not assumed from the brief:
#   - 'swrast' is not a valid -Dgallium-drivers choice at all (meson.options lists
#     'softpipe' and 'llvmpipe', not 'swrast'). Mesa's own top-level meson.build
#     (see the "compatibility for swrast as an internal-ish driver name" comment
#     there) computes with_gallium_swrast = with_gallium_softpipe or
#     with_gallium_llvmpipe internally -- 'swrast' is the resulting internal
#     DRI/EGL *driver name* (swrast_dri.so) that either gallium driver registers
#     under, not a selectable value itself. 'softpipe' is chosen over 'llvmpipe'
#     specifically to avoid pulling in a from-source or system LLVM: llvmpipe
#     JIT-compiles via libLLVM, which would either need llvm-dev from this
#     container's apt (coupling the relocatable bundle's software GL path to
#     whatever LLVM version happens to be on THIS build machine, the same
#     class of portability problem this whole bundle exists to avoid) or a
#     full from-source LLVM build (a huge, out-of-scope new vendored
#     dependency not in versions.env or the plan's library list). softpipe is
#     a pure-C rasterizer with no such coupling -- slower, but this Mesa copy
#     is explicitly not the GPU-accelerated path per the brief's own note.
#   - '-Dosmesa=true' is a no-op in current Mesa: meson.options marks it
#     'deprecated: true' with description "Does nothing, left here for a
#     while to avoid build breakages" -- the standalone OSMesa off-screen
#     target it used to control was removed from the tree entirely (no
#     src/gallium/targets/osmesa/ directory exists in ${MESA_VERSION}).
#     Dropped rather than passed-but-ignored, to not misrepresent what the
#     flag does to a future reader of this script.
#   - '-Dvulkan-drivers=[]' is kept -- Roblox/WebKitGTK have no Vulkan
#     dependency here, only GL/EGL, and this avoids pulling in
#     glslang/spirv-tools as further build dependencies for an unused driver
#     class.
#
# --- 2026-08-15 correction (software-only-llvmpipe plan): softpipe -> llvmpipe ---
# The 'softpipe over llvmpipe' choice above turned out to be the wrong tradeoff in
# practice: real hands-on testing (before this bundle's separate out-of-process
# webview2loader-host GPU work) traced this bundle's own laggy/unresponsive webview
# feel directly to softpipe's naive pure-C rasterizer being the active GL backend.
# The concern that motivated picking softpipe originally -- coupling this
# relocatable bundle's software GL path to a build-machine-specific LLVM, or
# needing a huge new from-source LLVM vendor chain -- is real but was addressable,
# not a reason to accept the slow path permanently:
#   - LLVM itself is now a pinned, reproducible apt package (`llvm-dev`) inside
#     THIS Containerfile's own `FROM debian:12` build container -- see that file's
#     own comment on the exact version this resolves to. This is the same
#     "ordinary apt package inside the pinned old-glibc-baseline container" class
#     of build-time-only tool as ruby/gperf/unifdef/python3-mako above, not a
#     from-source LLVM build and not a dependency on whatever LLVM the operator's
#     own host machine happens to have.
#   - '-Dshared-llvm=disabled' below avoids the OTHER half of the original
#     concern -- a runtime dependency on the build container's own libLLVM-14.so.1,
#     which is the exact "build container has it, real target desktop doesn't"
#     trap the Task 8 post-review correction already found and fixed for
#     libjpeg/libxml2/libxslt (see this script's own libjpeg-turbo/libxml2/libxslt
#     comments above). Passing `static: not shared-llvm` down into meson's
#     `dependency('llvm', ...)` call (mesa's own meson.build) links LLVM's IR/
#     codegen/JIT machinery statically into llvmpipe's own driver .so instead --
#     no new .so, no new vendoring, no new runtime dependency at all beyond what
#     softpipe already shipped.
# 'llvmpipe' (not 'swrast', still not a real -Dgallium-drivers value -- see above)
# is a real, correctly-spelled choice verified directly against this exact Mesa
# version's own meson.options `choices:` list (softpipe and llvmpipe both listed).
# softpipe is dropped from the build entirely rather than building
# `softpipe,llvmpipe` side by side: Mesa's swrast DRI/EGL target
# (`with_gallium_swrast = with_gallium_softpipe or with_gallium_llvmpipe`,
# mesa/meson.build) compiles into a single shared driver artifact either way, and
# building only llvmpipe removes any runtime ambiguity about which of the two a
# given GALLIUM_DRIVER-unset process would resolve to -- there is exactly one
# software rasterizer in this bundle now, and it is llvmpipe.
#
# This is also the fix for a real, separate, confirmed-reproducible upstream
# WebKitGTK/NVIDIA driver bug (WebKitWebProcess SIGSEGV in a SkiaGPUWorker thread
# inside libnvidia-eglcore.so, 5 real coredumps) that Task 7's real
# glvnd/host-EGL GPU-acceleration work exposed: the repo owner's explicit decision
# was to force software rendering unconditionally (see
# compat/wine/dlls/webview2loader/unixlib.c's spawn_helper(), which no
# longer conditionally prefers the host's own NVIDIA driver at all) rather than
# chase the upstream WebKitGTK/NVIDIA race tonight -- which makes the software
# path's own speed newly load-bearing for the ENTIRE webview experience, not just
# a fallback for hosts with no usable EGL. llvmpipe is the only realistic way to
# force-software-render without reintroducing the original softpipe-era lag.
# Needs python3-mako + python3-yaml (see Containerfile) -- mesa's own configure-time
# python check hard-requires both regardless of driver selection. Needs llvm-dev
# (see Containerfile's own comment) for llvmpipe specifically.
meson setup /build/mesa/_build /build/mesa --prefix="$PREFIX" \
    -Dplatforms=x11,wayland -Dgallium-drivers=llvmpipe -Dvulkan-drivers=[] \
    -Dllvm=enabled -Dshared-llvm=disabled
ninja -C /build/mesa/_build -j"$JOBS" install

# libwpe and wpebackend-fdo used to be built here and are not any more. They are the WPE
# port's windowing abstraction, and this is the GTK port: WebKit's cmake for PORT=GTK
# mentions WPE nowhere at all, no shipped binary carried a single wpe DT_NEEDED entry,
# nothing provided the libWPEBackend-default.so that libwpe dlopens, and no gstreamer wpe
# plugin was built either. They were shipped, unopenable, for every release until now.

# --- GStreamer (native Linux) -----------------------------------------------------
# This is a *native Linux* GStreamer build for WebKitGTK's <video>/<audio> support --
# unrelated to and independent from the existing compat/submodules/gstreamer submodule,
# which cross-compiles GStreamer for the Windows target for use *inside* the Wine
# prefix by Windows apps. Same project, completely different build/purpose/toolchain.
#
# Unlike GTK-4/Mesa/libsecret earlier in this script, all three components here
# configured cleanly with a bare `meson setup --prefix=$PREFIX` -- no flag corrections
# needed. Verified directly: a real build against this same persistent prefix
# completed with exit code 0 and `pkg-config --modversion` printing real version
# strings for all three modules before this was appended here.
#
# The codec gap this block used to flag is now closed -- and it was not cosmetic.
#
# What it said: the Containerfile shipped no ogg/vorbis/theora/alsa dev libraries,
# so gst-plugins-base's 'auto' features silently resolved to disabled, and
# gst-plugins-good (the usual home of the audio sinks and common demuxers) was
# never built at all.
#
# What that cost: playing any audio in Studio's Toolbox killed the WebProcess
# outright. Confirmed from a real coredump -- SIGABRT, with abort() called from
# inside libwebkitgtk on a main-loop dispatch, i.e. a WebKit assertion, not a
# segfault. WebKit's GStreamer backend treats "required element missing" as
# unrecoverable rather than degrading to silence, so no decoder + no audio sink =
# abort. The bundle had GStreamer core and GL-backed video plugins but literally
# no way to decode or output a sound.
#
# So the audio/codec -dev packages are now in the Containerfile (see its header)
# and gst-plugins-good is built above. gst-plugins-ugly and gst-libav are still
# deliberately absent: they are the patent/licensing-sensitive ones, and base +
# good already covers what the Toolbox actually serves. If a format shows up that
# still fails, check the plugin set before assuming a code bug -- a missing plugin
# looks exactly like a crash here.
# Tests and examples are disabled in all four of these, and not to save time: several
# of the examples are GTK3 programs, and they link the container's own libgdk-3, which
# wants Debian's versioned xkb_*@V_0.5.0 symbols. Since this bundle started building its
# own libxkbcommon, -L$PREFIX wins the library search and upstream's unversioned 1.7.0
# answers instead, so the link fails on a program that is never installed or shipped --
# 470 targets into gstreamer, after everything that matters has already built. Same
# reasoning as -Dgtk3=disabled below, one step earlier in the dependency chain.
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
# Plugins disabled because their host libraries are not portable across distros:
# FLAC (libFLAC.so.8 on Debian, .so.12 elsewhere), vpx (libvpx.so.6 vs .so.7/.so.9),
# soup (would pull a second HTTP stack). Shipping a plugin that cannot load anywhere
# but the build distro is dead weight -- GStreamer skips it at registry scan, so the
# only thing it adds is confusion when someone greps the plugin list.
# Toolbox audio needs vorbis/opus/ogg/wav/mp4 + a sink; none of these are on that path.
# gtk3 is disabled for the same reason, one step further on: libgstgtk.so is a video
# sink WebKit never uses, and it was dragging a whole GTK3 stack into a GTK4 bundle.
meson setup /build/gst-plugins-good/_build /build/gst-plugins-good --prefix="$PREFIX" \
    -Dflac=disabled -Dvpx=disabled -Dsoup=disabled -Dgtk3=disabled \
    -Dexamples=disabled -Dtests=disabled
ninja -C /build/gst-plugins-good/_build -j"$JOBS" install

echo ":: Building gst-plugins-bad $GSTREAMER_VERSION"
extract_source "gst-plugins-bad-${GSTREAMER_VERSION}.tar.xz" /build/gst-plugins-bad
# Same portability rule as gst-plugins-good above: openal (libopenal.so.1),
# curl (Debian's libcurl-gnutls.so.4 specifically), and dtls/aes (OpenSSL 1.1's
# libcrypto.so.1.1, while Arch and every current distro are on OpenSSL 3) all pull
# host libraries that do not exist outside the build distro.
# -Dgtk3=disabled for the reason gst-plugins-good disables it above, and it only began to
# matter once this script started building wayland: the option is "auto", so with GTK3 still
# in the container and libwayland now in $PREFIX, gst-plugins-bad builds libgstgtkwayland.so
# -- a GTK *3* video sink. This bundle ships GTK 4, so that plugin went out needing
# libgtk-3/libgdk-3, which are neither vendored here nor on the system boundary README.md
# documents, and verify-outside-container.sh reports it as a real bug. waylandsink is a
# separate option and is still built.
meson setup /build/gst-plugins-bad/_build /build/gst-plugins-bad --prefix="$PREFIX" \
    -Dopenal=disabled -Dcurl=disabled -Ddtls=disabled -Daes=disabled -Dgtk3=disabled \
    -Dexamples=disabled -Dtests=disabled
ninja -C /build/gst-plugins-bad/_build -j"$JOBS" install

# --- WebKitGTK itself -------------------------------------------------------------
# This is the step every prior task (1-6) exists to make possible: build the actual
# webkit2gtk-6.0 library against the full from-source dependency chain above. Flags
# below were verified against the REAL Source/cmake/OptionsGTK.cmake,
# Source/cmake/WebKitFeatures.cmake, Source/cmake/BubblewrapSandboxChecks.cmake, and
# Source/cmake/GStreamerChecks.cmake in the actual webkitgtk-${WEBKITGTK_VERSION}
# source tree -- not assumed from the plan brief, which (correctly, per its own
# instructions) only sketched a starting point.
#
# -DUSE_SOUP2 (in the plan brief's original flags) does not exist as an option in
# this WebKitGTK version at all -- verified by grepping the whole source tree for
# "SOUP2": zero matches anywhere in Source/cmake. Recent WebKitGTK dropped libsoup2
# support entirely; this build already only has libsoup-3 (Task 4), which is the only
# choice available, so the flag is simply omitted rather than passed as a no-op
# (CMake would otherwise warn "Manually-specified variables were not used by the
# project").
#
# -DUSE_GTK4=ON is technically redundant (OptionsGTK.cmake already defaults USE_GTK4
# to ON), kept anyway per the brief for explicitness/self-documentation -- this whole
# bundle's reason to exist is GTK4-based WebKitGTK-6.0, not the GTK3/WebKit2GTK-4.1
# API this source tree can alternatively build.
#
# Real, previously-unlisted hard dependencies turned up by actually reading
# OptionsGTK.cmake's `find_package(... REQUIRED)` / `message(FATAL_ERROR ...)` checks
# (full accounting, including the apt packages this needed, is in the Containerfile's
# Task 7 comment block) are now satisfied by that Containerfile update, EXCEPT the
# four toggled off below, which were judged disproportionately heavy for what they'd
# add (see the Containerfile's matching comment for the full reasoning on each):
#   -DUSE_AVIF=OFF               -- would need libavif + an AV1 decoder (dav1d/libaom)
#   -DUSE_JPEGXL=OFF             -- would need libjxl + Google's `highway` SIMD library
#   -DENABLE_SPEECH_SYNTHESIS=OFF -- would need CMU Flite (full offline TTS + voice data)
#   -DUSE_LIBBACKTRACE=OFF        -- upstream has no tagged release to pin in versions.env
#
# USE_WOFF2 needs NO flag and NO libwoff2 build: OptionsGTK.cmake's own WOFF2Checks
# probe (a real compile-test for FreeType's `FT_CONFIG_OPTION_USE_BROTLI`) detects
# that this container's system FreeType (2.12.1, via apt) already has brotli-based
# WOFF2 decoding built in, and silently prefers that over `USE_WOFF2`/libwoff2 --
# confirmed directly by compiling that exact probe against the container's FreeType
# before writing this step. Real WOFF2 web-font support is retained for free.
#
# USE_GBM (default ON) needing a real Mesa with GBM/EGL/GLX actually built is why the
# Containerfile's libxxf86vm-dev fix matters here specifically -- without it, Mesa
# (Task 5) silently produced zero usable libraries (see that Containerfile comment),
# and this configure step would have failed immediately on "GBM is required for
# USE_GBM".
#
# -DUSE_SYSTEM_SYSPROF_CAPTURE=OFF: the only flag correction found by actually running
# `cmake` rather than just reading source -- `USE_SYSTEM_SYSPROF_CAPTURE` defaults ON
# and Source/CMakeLists.txt hard-requires a system `sysprof-capture-4` pkg-config
# module for it (`CMake Error ... system libsysprof-capture-4 not found, consider
# using USE_SYSTEM_SYSPROF_CAPTURE=NO`), which this container has no apt package for
# (GNOME Sysprof's capture library, a profiling/tracing helper). No new dependency
# needed either way: WebKitGTK vendors its own copy at
# Source/ThirdParty/libsysprof-capture specifically as the non-system fallback for
# this option, so turning it off just builds that bundled copy instead -- exactly
# what the CMake error message itself suggests.
# -DCMAKE_PREFIX_PATH="$PREFIX" and -DICU_ROOT="$PREFIX": a REAL, previously-hidden
# bug found only by actually running the full build, not visible from configure
# output alone. Root cause: unlike the meson/autotools builds earlier in this script,
# CMake's OWN dependency-resolution model does not read PKG_CONFIG_PATH (WebKit's
# hand-written Source/cmake/Find*.cmake modules mostly do, via pkg_check_modules --
# e.g. FindGLib.cmake, FindHarfBuzz.cmake -- but `find_package(ICU ...)` in
# OptionsGTK.cmake uses CMake's own bundled, pkg-config-blind Modules/FindICU.cmake).
# Several of this Task 7 Containerfile's new apt packages (libmanette-0.2-dev,
# libenchant-2-dev, etc.) transitively pulled in Debian's OWN libicu-dev (72.1) and
# libglib2.0-dev (2.74) as dependencies, alongside the real 77.1/2.84.4 copies this
# script builds from source into $PREFIX -- so both a system AND a from-source copy
# of ICU coexist in this container for the first time in this plan. Without a root
# hint, CMake's FindICU resolved ICU_INCLUDE_DIR/ICU_UC_LIBRARY to plain
# /usr/include and /usr/lib/x86_64-linux-gnu/libicuuc.so (system 72.1) -- but actual
# compiles still ended up pulling in $PREFIX/include/unicode/*.h instead (shadowed by
# -I$PREFIX/include already on the compile command for unrelated reasons, e.g. glib.h
# living in the same include dir), producing object code that calls ICU's
# version-suffixed entry points as `_77` (encoded via urename.h's
# U_ICU_VERSION_SHORT macro at compile time) while the linker was only given
# system ICU 72.1's library (which only exports `_72`-suffixed symbols) -- a real,
# reproducible "undefined reference to `u_strToLower_77'" (and ~40 more ICU symbols)
# failure linking libjavascriptcoregtk-6.0.so, confirmed against the real build
# before this fix, and confirmed resolved (CMakeCache.txt's ICU_INCLUDE_DIR/
# ICU_UC_LIBRARY_RELEASE/etc. all consistently pointing at $PREFIX afterward) with
# this fix in place. CMAKE_PREFIX_PATH is the general form (protects every other
# CMake-native, non-pkg-config-aware find_package call in this same tree against the
# identical class of system-vs-$PREFIX ambiguity, e.g. if a future dependency bump
# pulls in another apt package with more transitive system-library collisions);
# ICU_ROOT is FindICU.cmake's own documented, more specific hint variable, added
# redundantly since it's the officially-sanctioned mechanism for exactly this
# scenario. Neither flag affects resolution of the genuinely-system-only libraries
# this bundle deliberately does NOT vendor (X11, zlib, libpng, brotli, expat, etc.)
# -- $PREFIX simply contains no alternate copies of those, so find_package still
# falls through to the system paths for them exactly as before. (freetype, fontconfig,
# libjpeg and libxml2 were all on that list when it was written; all four are vendored
# now -- freetype as of 2026-08-18, see versions.env's FREETYPE_VERSION comment.)
#
# -DENABLE_BUBBLEWRAP_SANDBOX=OFF: a deliberate capability tradeoff, not a build
# workaround (added after this task's initial review round -- the Containerfile's
# libseccomp-dev/bubblewrap/xdg-dbus-proxy packages were originally added believing
# ENABLE_BUBBLEWRAP_SANDBOX's default-ON state was just a normal build-time
# requirement to satisfy, the same as everything else in that comment block).
# Real problem found in review: in this WebKitGTK 6.0 (the "2022 GLib API")
# configuration, the bubblewrap process sandbox has NO runtime opt-out --
# webkit_web_context_set_sandbox_enabled() is compiled out entirely under the 2022
# GLib API, and a failed sandbox spawn at runtime is a hard g_error() abort, not a
# graceful fallback. With the sandbox left enabled, the built libwebkitgtk-6.0.so
# would have a real, undeclared HARD RUNTIME dependency on /usr/bin/bwrap and
# /usr/bin/xdg-dbus-proxy existing on whatever end-user Linux system this bundle is
# unpacked onto -- directly conflicting with this whole plan's relocatable,
# zero-host-package goal (the same reason every other dependency in this pipeline is
# built from source into $PREFIX rather than assumed present on the target machine).
# Disabling it trades away WebKit's process-level sandboxing (defense-in-depth
# against a compromised renderer process reaching the rest of the host) for genuine
# host independence -- consistent with this plan's existing choice to vendor
# everything rather than lean on host packages. This tradeoff is also documented in
# the plan document's Task 8 section. libseccomp-dev/bubblewrap/xdg-dbus-proxy are
# left in the Containerfile even though they're now unused at cmake-configure time
# (BubblewrapSandboxChecks.cmake's whole body is gated behind
# `if (ENABLE_BUBBLEWRAP_SANDBOX)`) -- harmless build-time-only leftovers, not
# something that affects the shipped bundle either way; not cleaned up here to keep
# this fix narrowly scoped to the two review findings it addresses.
echo ":: Building webkitgtk $WEBKITGTK_VERSION"
# 2026-08-15: webkitgtk is now checked directly into this repo, at
# compat/webkitgtk/src (bind-mounted read-only at /src-webkitgtk by build.sh) --
# same "carry our own patches as a real, versioned tree" treatment the repo already
# documents for compat/wine, and for the same reason: this bundle now carries
# real source patches (WEBKIT_EXEC_PATH, WindowIsActive, isInMonitor -- see
# compat/webkitgtk/src/README-TUXBLOX-PATCHES.md), previously applied as fragile
# inline sed/python string-replacements against a freshly downloaded tarball on every
# build. Patches now live as plain edits in the checked-in source itself -- normal git
# diff/log/blame, no more "ERROR: expected original text not found" breakage risk
# every time upstream reflows a comment.
#
# /build/webkitgtk is still backed by a DEDICATED persistent volume
# (webkitgtk-src-build, see build.sh) because this is the one genuinely expensive step
# (hours for a real from-scratch compile, even with ccache -- ccache only speeds up
# recompiling individual translation units it has already seen, not the cmake
# configure + ninja dependency-graph walk). Persisting the actual _build directory
# means ninja's own incremental build (correctness-tracked by real file content/
# mtimes) does the skip-if-unchanged work. rsync -c (checksum, not mtime, comparison
# -- the checked-in tree's mtimes are whatever the local checkout happens to have, not
# meaningful) syncs the read-only bind-mounted source into the volume every run:
# unchanged files keep their PREVIOUS destination mtime from the last successful
# build (exactly what ninja needs to keep skipping them), and only files that actually
# changed (a version bump, a new patch) get touched -- so a real source edit still
# gets a correct, minimal incremental rebuild, without needing a separate version-
# keyed marker file to decide whether to resync at all. --delete keeps a removed
# upstream file from lingering in the volume forever. _build itself is excluded
# (that's the ninja cache this rsync must never touch).
mkdir -p /build/webkitgtk
rsync -a -c --delete --exclude=_build /src-webkitgtk/ /build/webkitgtk/

# TuxBlox's own patches (WEBKIT_EXEC_PATH ungated, WindowIsActive forced-active,
# isInMonitor forced-true) are applied directly to compat/webkitgtk/src -- see
# compat/webkitgtk/src/README-TUXBLOX-PATCHES.md for the full rationale for each.
# No patch-application step needed here anymore; the rsync above already synced the
# already-patched source.

# -DCMAKE_C(XX)_COMPILER_LAUNCHER=ccache: the standard, non-invasive way to interpose
# ccache into a CMake build without renaming/symlinking the compiler itself -- CMake
# prefixes every actual compile command with the launcher. See the ccache comment
# earlier in this script (shared environment) and in the Containerfile for why this
# matters specifically for this --rm-per-run pipeline.
#
# A real memory characteristic worth recording here since it directly caused TWO
# separate build failures while developing this step, at two different JOBS values --
# corrected after an earlier version of this comment understated the second one:
#   - `-e JOBS=3 --memory=5g`: failed early, inside JavaScriptCore, compiling
#     Source/JavaScriptCore/dfg/DFGSpeculativeJIT.cpp -- a real container-cgroup OOM
#     kill (`journalctl -k`: "Memory cgroup out of memory: Killed process ...
#     (cc1plus) ... anon-rss:1948984kB").
#   - `-e JOBS=2 --memory=6g`: a *targeted* rebuild of just JavaScriptCore alone
#     (2705/2705 objects, ending in a clean `Linking CXX shared library
#     lib/libjavascriptcoregtk-6.0.so...`) completed with no kill -- but that is NOT
#     the same claim as "JOBS=2 is safe for the full build". A subsequent FULL build
#     at this same `-e JOBS=2 --memory=6g` got to [8044/9423] (85%, inside WebCore
#     this time, not JSC) before hitting the identical class of container-cgroup OOM
#     kill on one of WebCore's unified-source translation units. WebCore's heaviest
#     unified-source bundles are apparently heavier than JSC's, and JOBS=2 was NOT
#     actually safe for the full tree on this host, even though it looked safe from
#     the JSC-only test alone.
#   - `-e JOBS=1 --memory=6g`: this is the setting the actual full, successful,
#     installed build (the one this repo's persisted results/report are based on)
#     used. Confirmed known-safe end-to-end for the complete ~9423-target tree on
#     this host. Higher JOBS values were not confirmed stable for the full build --
#     do not assume JOBS=2 is safe based on the JSC-only data point above; that was
#     real data but for a smaller, apparently-less-memory-hungry subset of the tree.
# Net guidance for a future caller: JOBS=1 is the known-safe default this script's
# own successful run used. If you have headroom to try higher parallelism (a host
# with more consistently-free RAM than the one this was developed on), that has not
# been validated here and should be treated as unverified, not as a documented-safe
# option -- watch for cc1plus OOM kills specifically inside WebCore, not just JSC.
# -DENABLE_SPELLCHECK=OFF -DENABLE_GAMEPAD=OFF -DUSE_LIBHYPHEN=OFF: Task 8 post-review
# correction (2026-08-10). Originally satisfied by installing libenchant-2-dev/
# libmanette-0.2-dev/libhyphen-dev in the Containerfile (real, unconditional
# find_package()+FATAL_ERROR requirements when these options are left at their
# default ON). Review found -- via `ldd` run against the packaged tarball OUTSIDE
# this build container, this whole review round's corrected methodology -- that all
# three left the shipped bundle depending on the build container's own system copies
# of libenchant-2/libmanette-0.2/libhyphen at runtime, none of which get vendored
# into $PREFIX by installing a -dev package, so none would be present on a real
# target machine. Rather than adding three more libraries to the from-source
# vendored chain for features (in-page spellcheck, Gamepad API, CSS automatic
# hyphenation) this embedded-WebView use case doesn't need, all three are simply
# disabled -- same tier of accepted, documented capability loss as the
# AVIF/JPEG-XL/Speech-Synthesis trims already accepted above.
# /build/webkitgtk is a persistent volume, so CMakeCache.txt survives between runs --
# and a cached dependency path is never re-searched. Discard the cache so every
# dependency is re-resolved against the CURRENT $PREFIX.
#
# This is deliberately the whole cache, not a targeted -U of a few variables. The
# first attempt at this fix used `-U 'FREETYPE_*'`, which does force
# find_package(Freetype) to re-run -- but pkg_check_modules caches its results
# separately, so a stale `GTK_CFLAGS` left over from the pre-vendoring configure kept
# feeding `-I/usr/include/freetype2` to every GTK-dependent translation unit while
# FREETYPE_INCLUDE_DIRS correctly pointed into $PREFIX. That mixes two incompatible
# FT_COLR_Paint layouts inside one library -- the exact bug being fixed here, just
# moved. Verified for real: the cache from that run held
# `GTK_CFLAGS:INTERNAL=...;-I/usr/include/freetype2;...` while `pkg-config --cflags
# gtk4` in the same prefix returned `-I/opt/tuxblox-webview/include/freetype2`.
# Enumerating which cache variables might embed a dependency path is exactly the kind
# of guess that failure punished, so don't -- drop the cache. Object files and the
# ccache are untouched, so the cost is one reconfigure plus whatever genuinely
# changed flags force a recompile.
rm -f /build/webkitgtk/_build/CMakeCache.txt
# ENABLE_JOURNALD_LOG defaults ON in OptionsGTK.cmake and is the only reason
# libwebkitgtk and libjavascriptcoregtk link libsystemd, which simply does not exist on
# Void, Artix, Alpine, Devuan or an OpenRC Gentoo -- WebKit's own libraries could not
# load there at all. It is used in WTF's Logger.h/Assertions.h and nowhere else, so
# turning it off costs the journal as a log destination and nothing more; the output
# goes to stderr instead, which is where TuxBlox's session logs already capture it.
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

# Fail loudly if WebKit resolved freetype anywhere but this prefix -- the whole point
# of vendoring it is that build-time headers and runtime library are the same copy.
#
# Two checks, because the first one alone gave a false pass: find_package(Freetype)
# pointed cleanly into $PREFIX while a stale cached GTK_CFLAGS was still handing
# `-I/usr/include/freetype2` to every GTK-dependent translation unit. So check the
# resolved path AND check that no host freetype path survives anywhere in the cache.
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

# Guard against a future upstream version silently changing shape in a way the Task 8
# patch above no longer actually applies to (the python patch step already fails loudly
# if its exact expected source text isn't found, but this double-checks the *compiled
# result* too, catching e.g. a differently-shaped fix upstream ends up applying itself).
#
# Task 8 (2026-08-14-webview2loader-host-process, real end-to-end verification) bug fix:
# this used to be `strings ... | grep -q '^WEBKIT_EXEC_PATH$'` piped directly, which is
# the classic `producer | grep -q` SIGPIPE-under-pipefail trap (this script sets
# `set -euo pipefail` up top) -- `grep -q` exits the instant it finds its first match,
# without draining the rest of its stdin, which SIGPIPEs `strings` while it's still
# mid-write on a ~130MB shared library (well over a million lines of extracted-string
# output). Under pipefail, that SIGPIPE'd `strings` process's non-zero exit status wins
# out over grep's own successful (0, match-found) exit status, so the pipeline as a
# whole spuriously reports failure -- confirmed for real: this check failed on two
# separate from-scratch container builds even though a standalone, non-piped re-check
# against the exact same just-installed .so immediately afterward found the string fine
# both times. Fixed by capturing `strings`' full output into a variable first (a real
# command substitution runs to completion before anything reads it, so there's no live
# reader to SIGPIPE it), then grepping that already-complete text -- no pipe between a
# still-writing producer and an early-exiting consumer, so no SIGPIPE race left to hit.
webkitgtk_so_strings="$(strings "$PREFIX"/lib/libwebkitgtk-6.0.so.* 2>/dev/null || true)"
if ! grep -q '^WEBKIT_EXEC_PATH$' <<<"$webkitgtk_so_strings"; then
    echo "ERROR: WEBKIT_EXEC_PATH string not found in installed libwebkitgtk-6.0.so -- the Task 8 patch did not take effect" >&2
    exit 1
fi

# --- webview2loader-host ----------------------------------------------------------
# The separate helper process that owns the GTK4/WebKitGTK main loop out-of-process
# from Wine. host-build.sh holds the compile and nothing else, so it can also be run
# on its own when only the host's own sources changed -- see build-host.sh.
PREFIX="$PREFIX" bash "$(dirname "$0")/host-build.sh"
