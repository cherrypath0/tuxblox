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
fetch_and_extract \
    "https://github.com/libjpeg-turbo/libjpeg-turbo/releases/download/${LIBJPEG_TURBO_VERSION}/libjpeg-turbo-${LIBJPEG_TURBO_VERSION}.tar.gz" \
    /build/libjpeg-turbo
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

echo ":: Building libxml2 $LIBXML2_VERSION"
fetch_and_extract \
    "https://download.gnome.org/sources/libxml2/${LIBXML2_VERSION%.*}/libxml2-${LIBXML2_VERSION}.tar.xz" \
    /build/libxml2
# -Dpython=disabled: libxml2's meson build can also build Python bindings; nothing
# in this C/C++ dependency chain needs them, and building them would pull in a
# python3-dev header dependency this Containerfile doesn't otherwise need.
meson setup /build/libxml2/_build /build/libxml2 --prefix="$PREFIX" -Dpython=disabled
ninja -C /build/libxml2/_build -j"$JOBS" install

echo ":: Building libxslt $LIBXSLT_VERSION"
fetch_and_extract \
    "https://download.gnome.org/sources/libxslt/${LIBXSLT_VERSION%.*}/libxslt-${LIBXSLT_VERSION}.tar.xz" \
    /build/libxslt
# libxslt ${LIBXSLT_VERSION} ships both an autotools ./configure and a CMakeLists.txt
# but no meson.build -- autotools chosen to match this script's existing convention
# for every other non-meson dependency (gmp, nettle, gnutls, icu, sqlite, lcms2,
# libwebp, libtasn1, ...). --without-python: same reasoning as libxml2's
# -Dpython=disabled above -- no Python bindings needed here.
cd /build/libxslt
./configure --prefix="$PREFIX" --with-libxml-prefix="$PREFIX" --without-python
make -j"$JOBS"
make install
cd /build

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

echo ":: Building libpsl $LIBPSL_VERSION"
fetch_and_extract \
    "https://github.com/rockdaboot/libpsl/releases/download/${LIBPSL_VERSION}/libpsl-${LIBPSL_VERSION}.tar.gz" \
    /build/libpsl
meson setup /build/libpsl/_build /build/libpsl --prefix="$PREFIX"
ninja -C /build/libpsl/_build -j"$JOBS" install

echo ":: Building libgudev $LIBGUDEV_VERSION"
fetch_and_extract \
    "https://download.gnome.org/sources/libgudev/${LIBGUDEV_VERSION%%.*}/libgudev-${LIBGUDEV_VERSION}.tar.xz" \
    /build/libgudev
meson setup /build/libgudev/_build /build/libgudev --prefix="$PREFIX"
ninja -C /build/libgudev/_build -j"$JOBS" install

echo ":: Building libsecret $LIBSECRET_VERSION"
fetch_and_extract \
    "https://download.gnome.org/sources/libsecret/${LIBSECRET_VERSION%.*}/libsecret-${LIBSECRET_VERSION}.tar.xz" \
    /build/libsecret
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

echo ":: Building libsoup $LIBSOUP_VERSION"
fetch_and_extract \
    "https://download.gnome.org/sources/libsoup/${LIBSOUP_VERSION%.*}/libsoup-${LIBSOUP_VERSION}.tar.xz" \
    /build/libsoup
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
fetch_and_extract \
    "https://download.gnome.org/sources/gtk/${GTK4_VERSION%.*}/gtk-${GTK4_VERSION}.tar.xz" \
    /build/gtk4
# Brief's original flag was "-Ddemos=false" -- gtk4's meson.options actually names
# this option "build-demos" (there is no bare "demos" option; "-Ddemos=false" fails
# meson setup immediately with "Unknown option: demos"). Confirmed against the real
# gtk-4.18.6 tarball's meson.options before fixing.
meson setup /build/gtk4/_build /build/gtk4 --prefix="$PREFIX" \
    -Dmedia-gstreamer=disabled -Dvulkan=disabled -Dbuild-tests=false -Dbuild-demos=false -Dbuild-examples=false
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
fetch_and_extract "https://gmplib.org/download/gmp/gmp-${GMP_VERSION}.tar.xz" /build/gmp
cd /build/gmp && ./configure --prefix="$PREFIX" && make -j"$JOBS" && make install && cd /build

echo ":: Building nettle $NETTLE_VERSION"
fetch_and_extract "https://ftp.gnu.org/gnu/nettle/nettle-${NETTLE_VERSION}.tar.gz" /build/nettle
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
fetch_and_extract \
    "https://github.com/p11-glue/p11-kit/releases/download/${P11KIT_VERSION}/p11-kit-${P11KIT_VERSION}.tar.xz" \
    /build/p11-kit
meson setup /build/p11-kit/_build /build/p11-kit --prefix="$PREFIX"
ninja -C /build/p11-kit/_build -j"$JOBS" install

echo ":: Building gnutls $GNUTLS_VERSION"
fetch_and_extract \
    "https://www.gnupg.org/ftp/gcrypt/gnutls/v${GNUTLS_VERSION%.*}/gnutls-${GNUTLS_VERSION}.tar.xz" \
    /build/gnutls
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
fetch_and_extract \
    "https://download.gnome.org/sources/glib-networking/${GLIB_NETWORKING_VERSION%.*}/glib-networking-${GLIB_NETWORKING_VERSION}.tar.xz" \
    /build/glib-networking
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

echo ":: Building mesa $MESA_VERSION"
fetch_and_extract "https://archive.mesa3d.org/mesa-${MESA_VERSION}.tar.xz" /build/mesa
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
# Needs python3-mako + python3-yaml (see Containerfile) -- mesa's own configure-time
# python check hard-requires both regardless of driver selection.
meson setup /build/mesa/_build /build/mesa --prefix="$PREFIX" \
    -Dplatforms=x11,wayland -Dgallium-drivers=softpipe -Dvulkan-drivers=[]
ninja -C /build/mesa/_build -j"$JOBS" install

echo ":: Building libwpe $LIBWPE_VERSION"
fetch_and_extract \
    "https://wpewebkit.org/releases/libwpe-${LIBWPE_VERSION}.tar.xz" /build/libwpe
# libwpe (WPE's core windowing-abstraction library, providing the wpe-1.0 pkg-config
# module) is a real, hard dependency of wpebackend-fdo below (its meson.build does
# dependency('wpe-1.0', ..., fallback: ['libwpe', 'libwpe_dep']) -- not in the
# original plan brief/versions.env, discovered while actually building this task
# rather than guessed at during planning; see the comment on LIBWPE_VERSION in
# versions.env.
meson setup /build/libwpe/_build /build/libwpe --prefix="$PREFIX"
ninja -C /build/libwpe/_build -j"$JOBS" install

echo ":: Building wpebackend-fdo $WPEBACKEND_FDO_VERSION"
fetch_and_extract \
    "https://wpewebkit.org/releases/wpebackend-fdo-${WPEBACKEND_FDO_VERSION}.tar.xz" \
    /build/wpebackend-fdo
meson setup /build/wpebackend-fdo/_build /build/wpebackend-fdo --prefix="$PREFIX"
ninja -C /build/wpebackend-fdo/_build -j"$JOBS" install

# --- GStreamer (native Linux) -----------------------------------------------------
# This is a *native Linux* GStreamer build for WebKitGTK's <video>/<audio> support --
# unrelated to and independent from the existing ProtonSource/gstreamer submodule,
# which cross-compiles GStreamer for the Windows target for use *inside* the Wine
# prefix by Windows apps. Same project, completely different build/purpose/toolchain.
#
# Unlike GTK-4/Mesa/libsecret earlier in this script, all three components here
# configured cleanly with a bare `meson setup --prefix=$PREFIX` -- no flag corrections
# needed. Verified directly: a real build against this same persistent prefix
# completed with exit code 0 and `pkg-config --modversion` printing real version
# strings for all three modules before this was appended here.
#
# Known real gap, intentionally out of this task's scope (brief only covers
# gstreamer + gst-plugins-base + gst-plugins-bad, matching the plan's Global
# Constraints dependency list): the Containerfile has no codec libraries for ogg,
# vorbis, theora, or alsa, so gst-plugins-base's corresponding 'auto' features
# silently resolved to disabled rather than failing the build -- confirmed by
# inspecting the installed plugin set afterward (no libgstogg/libgstvorbis/
# libgsttheora/libgstalsa present in $PREFIX/lib/x86_64-linux-gnu/gstreamer-1.0/).
# gst-plugins-bad's build produced GL-backed plugins (opengl, waylandsink,
# ximagesink) using Mesa/Wayland/X11 from Task 5, and gstreamer-gl-*.pc modules are
# present, so the accelerated video path has real plugin coverage -- but actual
# audio/video *codec* decoding for common web formats (Vorbis/Theora audio-video,
# ALSA output) has no plugin backing it yet. gst-plugins-good/gst-plugins-ugly/
# gst-libav (none of which are in this plan's dependency list) are the usual source
# of that codec coverage upstream; flagged here rather than silently building them,
# since it's outside this task's brief the same way Task 4 flagged the TLS gap
# before a later task filled it in.
echo ":: Building gstreamer $GSTREAMER_VERSION"
fetch_and_extract \
    "https://gstreamer.freedesktop.org/src/gstreamer/gstreamer-${GSTREAMER_VERSION}.tar.xz" \
    /build/gstreamer
meson setup /build/gstreamer/_build /build/gstreamer --prefix="$PREFIX"
ninja -C /build/gstreamer/_build -j"$JOBS" install

echo ":: Building gst-plugins-base $GSTREAMER_VERSION"
fetch_and_extract \
    "https://gstreamer.freedesktop.org/src/gst-plugins-base/gst-plugins-base-${GSTREAMER_VERSION}.tar.xz" \
    /build/gst-plugins-base
meson setup /build/gst-plugins-base/_build /build/gst-plugins-base --prefix="$PREFIX"
ninja -C /build/gst-plugins-base/_build -j"$JOBS" install

echo ":: Building gst-plugins-bad $GSTREAMER_VERSION"
fetch_and_extract \
    "https://gstreamer.freedesktop.org/src/gst-plugins-bad/gst-plugins-bad-${GSTREAMER_VERSION}.tar.xz" \
    /build/gst-plugins-bad
meson setup /build/gst-plugins-bad/_build /build/gst-plugins-bad --prefix="$PREFIX"
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
# this bundle deliberately does NOT vendor (freetype, fontconfig, X11, zlib, libpng,
# libjpeg, libxml2, etc.) -- $PREFIX simply contains no alternate copies of those, so
# find_package still falls through to the system paths for them exactly as before.
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
fetch_and_extract \
    "https://webkitgtk.org/releases/webkitgtk-${WEBKITGTK_VERSION}.tar.xz" \
    /build/webkitgtk

# --- Task 8 finding: WEBKIT_EXEC_PATH is gated behind ENABLE_DEVELOPER_MODE upstream,
# and this build does not (and should not) set -DDEVELOPER_MODE=ON --------------------
# Task 8 (packaging) set out to verify -- not just trust -- the claim that
# WEBKIT_EXEC_PATH overrides the compiled-in PKGLIBEXECDIR path at runtime (needed
# because the extracted tarball's real install location isn't known until TuxBlox's own
# build runs, and can't match whatever $PREFIX this container build used). Verification
# method: `strings lib/libwebkitgtk-6.0.so.4.16.9 | grep WEBKIT_EXEC_PATH` on the
# as-built (pre-patch) library returned NOTHING -- the getenv() call was compiled out
# entirely. Root cause, confirmed by reading the real upstream source
# (Source/WebKit/Shared/glib/ProcessExecutablePathGLib.cpp): the whole WEBKIT_EXEC_PATH
# check lives inside `#if ENABLE(DEVELOPER_MODE)`, and this build's cmake invocation
# below never sets `-DDEVELOPER_MODE=ON` (deliberately -- DEVELOPER_MODE also flips
# DEVELOPER_MODE_FATAL_WARNINGS ON by default, changes ENABLE_THUNDER/USE_CAPSTONE
# defaults, and pulls in other dev-only behavior with a much broader blast radius than
# this one path-resolution fix needs).
#
# Fix: patch ONLY the WEBKIT_EXEC_PATH check to run unconditionally, leaving every
# other DEVELOPER_MODE-gated branch in the same function (the getExecutablePath()
# dev-convenience fallback) untouched. This is a narrow, TuxBlox-specific patch to
# upstream WebKit source, applied here (not via a vendored .patch file, to keep the
# whole change visible in one place) before the normal configure/build/install below.
WEBKIT_EXEC_PATH_FIX_TARGET=/build/webkitgtk/Source/WebKit/Shared/glib/ProcessExecutablePathGLib.cpp
python3 - "$WEBKIT_EXEC_PATH_FIX_TARGET" <<'PYEOF'
import sys
path = sys.argv[1]
with open(path) as f:
    src = f.read()

old = '''static String findWebKitProcess(const char* processName)
{
#if ENABLE(DEVELOPER_MODE)
    static const char* execDirectory = g_getenv("WEBKIT_EXEC_PATH");
    if (execDirectory) {
        String processPath = FileSystem::pathByAppendingComponent(FileSystem::stringFromFileSystemRepresentation(execDirectory), StringView::fromLatin1(processName));
        if (FileSystem::fileExists(processPath))
            return processPath;
    }

    static String executablePath = getExecutablePath();
    if (!executablePath.isNull()) {
        String processPath = FileSystem::pathByAppendingComponent(executablePath, StringView::fromLatin1(processName));
        if (FileSystem::fileExists(processPath))
            return processPath;
    }
#endif

    return FileSystem::pathByAppendingComponent(FileSystem::stringFromFileSystemRepresentation(PKGLIBEXECDIR), StringView::fromLatin1(processName));
}'''

new = '''static String findWebKitProcess(const char* processName)
{
    // TuxBlox patch: WEBKIT_EXEC_PATH must work in release (non-DEVELOPER_MODE)
    // builds too -- this is the only available runtime override for the
    // PKGLIBEXECDIR path, which is a compile-time absolute-path constant that
    // patchelf/RPATH rewriting cannot touch (see webkitgtk-bundle/README.md).
    // Upstream gates this check behind ENABLE(DEVELOPER_MODE); ungated here.
    // Every other DEVELOPER_MODE-gated behavior in this function (the
    // getExecutablePath() dev-convenience fallback) is left untouched.
    static const char* execDirectory = g_getenv("WEBKIT_EXEC_PATH");
    if (execDirectory) {
        String processPath = FileSystem::pathByAppendingComponent(FileSystem::stringFromFileSystemRepresentation(execDirectory), StringView::fromLatin1(processName));
        if (FileSystem::fileExists(processPath))
            return processPath;
    }

#if ENABLE(DEVELOPER_MODE)
    static String executablePath = getExecutablePath();
    if (!executablePath.isNull()) {
        String processPath = FileSystem::pathByAppendingComponent(executablePath, StringView::fromLatin1(processName));
        if (FileSystem::fileExists(processPath))
            return processPath;
    }
#endif

    return FileSystem::pathByAppendingComponent(FileSystem::stringFromFileSystemRepresentation(PKGLIBEXECDIR), StringView::fromLatin1(processName));
}'''

if old not in src:
    sys.exit("ERROR: expected original ProcessExecutablePathGLib.cpp text not found -- "
             "upstream source may have changed shape; re-check this patch against the "
             "new version before continuing")

with open(path, 'w') as f:
    f.write(src.replace(old, new))
print(":: WEBKIT_EXEC_PATH patch applied to ProcessExecutablePathGLib.cpp")
PYEOF
# --- end Task 8 WEBKIT_EXEC_PATH fix ---------------------------------------------------

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
cmake -B /build/webkitgtk/_build -S /build/webkitgtk -GNinja \
    -DPORT=GTK -DUSE_GTK4=ON -DCMAKE_INSTALL_PREFIX="$PREFIX" \
    -DCMAKE_BUILD_TYPE=Release -DENABLE_INTROSPECTION=OFF -DENABLE_DOCUMENTATION=OFF \
    -DENABLE_MINIBROWSER=OFF \
    -DUSE_AVIF=OFF -DUSE_JPEGXL=OFF -DENABLE_SPEECH_SYNTHESIS=OFF -DUSE_LIBBACKTRACE=OFF \
    -DUSE_SYSTEM_SYSPROF_CAPTURE=OFF \
    -DENABLE_SPELLCHECK=OFF -DENABLE_GAMEPAD=OFF -DUSE_LIBHYPHEN=OFF \
    -DCMAKE_PREFIX_PATH="$PREFIX" -DICU_ROOT="$PREFIX" \
    -DCMAKE_C_COMPILER_LAUNCHER=ccache -DCMAKE_CXX_COMPILER_LAUNCHER=ccache \
    -DENABLE_BUBBLEWRAP_SANDBOX=OFF
cmake --build /build/webkitgtk/_build -j"$JOBS"
cmake --install /build/webkitgtk/_build

# Guard against a future upstream version silently changing shape in a way the Task 8
# patch above no longer actually applies to (the python patch step already fails loudly
# if its exact expected source text isn't found, but this double-checks the *compiled
# result* too, catching e.g. a differently-shaped fix upstream ends up applying itself).
if ! strings "$PREFIX"/lib/libwebkitgtk-6.0.so.* 2>/dev/null | grep -q '^WEBKIT_EXEC_PATH$'; then
    echo "ERROR: WEBKIT_EXEC_PATH string not found in installed libwebkitgtk-6.0.so -- the Task 8 patch did not take effect" >&2
    exit 1
fi
