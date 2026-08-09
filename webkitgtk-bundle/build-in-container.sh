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
# regardless of whether the apt package is installed. Not a loss of real capability:
# `libproxy` (left enabled -- its dependency, libproxy-dev, is a normal apt package)
# already covers proxy auto-detection across desktop environments, including its own
# internal GNOME/gsettings-aware config module; gnome_proxy would only have added a
# second, GNOME-specific, largely-overlapping resolver on top of that.
meson setup /build/glib-networking/_build /build/glib-networking --prefix="$PREFIX" -Dgnome_proxy=disabled
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
