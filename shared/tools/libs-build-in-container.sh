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

# Builds the libraries TuxBlox ships inside its own programs, as static archives.

set -euo pipefail

Prefix=/out
SrcRoot=/src
Markers="$Prefix/.markers"
JOBS="${JOBS:-$(nproc 2>/dev/null || echo 1)}"

mkdir -p "$Prefix/lib" "$Prefix/include" "$Markers" /build
export PKG_CONFIG_PATH="$Prefix/lib/pkgconfig"

# Every full build wipes build/ and so rebuilds all six from scratch; the cache is mounted from
# outside it, which turns an unchanged pin into a few seconds instead of several minutes.
if command -v ccache >/dev/null 2>&1 && [ -n "${CCACHE_DIR:-}" ]; then
    export CC="ccache gcc"
    export CXX="ccache g++"
    CMakeLauncher="-DCMAKE_C_COMPILER_LAUNCHER=ccache -DCMAKE_CXX_COMPILER_LAUNCHER=ccache"
else
    CMakeLauncher=""
fi

# The marker carries the submodule commit, which the host resolves and passes in because the source is mounted read-only
# The recipe is part of what decides the archive, so the marker carries a hash of this file too.
RecipeId="$(sha256sum "${BASH_SOURCE[0]}" | cut -c1-12)"

runStep() {
    local name="$1" commitVar="$2" func="$3" commit marker
    commit="${!commitVar:-nocommit}"
    marker="$Markers/$name-$commit-$RecipeId"
    if [ -e "$marker" ]; then
        printf ':: Skipping %s %s (already built)\n' "$name" "$commit"
        return 0
    fi
    printf ':: Building %s %s\n' "$name" "$commit"
    "$func"
    touch "$marker"
}

# The source is mounted read-only, so every build happens on a copy
copySource() {
    local from="$1" to="$2"
    rm -rf "$to"
    cp -a "$SrcRoot/$from" "$to"
}

buildZlib() {
    copySource shared/fs/zlib /build/zlib
    cd /build/zlib
    ./configure --prefix="$Prefix" --static
    make -j"$JOBS"
    make install
    cd /build
}

buildZstd() {
    copySource shared/fs/zstd /build/zstd
    # Library only: no command line tool, and no legacy format support
    make -C /build/zstd/lib -j"$JOBS" libzstd.a ZSTD_LEGACY_SUPPORT=0
    make -C /build/zstd/lib install-static install-includes install-pc \
        PREFIX="$Prefix" ZSTD_LEGACY_SUPPORT=0
    cd /build
}

buildNghttp2() {
    copySource shared/network/nghttp2 /build/nghttp2
    cd /build/nghttp2
    autoreconf -i
    ./configure --prefix="$Prefix" --enable-lib-only --enable-static --disable-shared
    make -j"$JOBS"
    make install
    cd /build
}

buildOpenssl() {
    copySource shared/crypto/openssl /build/openssl
    cd /build/openssl
    # Keeps the TLS 1.2/1.3 client, X.509 for the layer's signature checks, and SHA-256 for checksums
    ./Configure linux-x86_64 --prefix="$Prefix" --openssldir="$Prefix/ssl" --libdir=lib \
        no-shared no-dso no-engine no-tests no-apps no-docs \
        no-legacy no-md2 no-md4 no-rc2 no-rc4 no-rc5 no-idea no-seed no-camellia \
        no-weak-ssl-ciphers no-ssl3 no-comp -O2
    make -j"$JOBS"
    make install_dev
    cd /build
}

buildCurl() {
    copySource shared/network/libcurl /build/curl
    cd /build/curl
    autoreconf -fi
    # Everything nothing in TuxBlox uses is off, so the list of libraries cannot grow back
    ./configure --prefix="$Prefix" --disable-shared --enable-static \
        --with-openssl="$Prefix" --with-nghttp2="$Prefix" --without-zlib \
        --without-libssh2 --without-libssh --without-gssapi --without-libidn2 \
        --without-libpsl --without-brotli --without-zstd --without-nghttp3 \
        --without-ca-bundle --without-ca-path \
        --without-ngtcp2 --without-librtmp --without-ldap-lib --without-libgsasl \
        --disable-ldap --disable-ldaps --disable-rtsp --disable-dict --disable-telnet \
        --disable-tftp --disable-pop3 --disable-imap --disable-smtp --disable-gopher \
        --disable-smb --disable-mqtt --disable-manual --disable-ntlm
    make -j"$JOBS"
    make install
    cd /build
}

buildLibarchive() {
    copySource shared/fs/libarchive /build/libarchive
    cd /build/libarchive
    # libarchive wants a newer cmake than this baseline's, so the one installed beside it is used
    local cmakeBin=/opt/cmake/bin/cmake
    # Exactly the formats and filters the three tar_extract.cpp files ask for, and nothing else
    # shellcheck disable=SC2086
    "$cmakeBin" -B _b -S . -G Ninja -DCMAKE_BUILD_TYPE=Release $CMakeLauncher \
        -DCMAKE_PREFIX_PATH="$Prefix" -DCMAKE_INSTALL_PREFIX="$Prefix" \
        -DBUILD_SHARED_LIBS=OFF -DENABLE_TEST=OFF -DENABLE_INSTALL=ON \
        -DENABLE_ZLIB=ON -DENABLE_ZSTD=ON \
        -DENABLE_BZip2=OFF -DENABLE_LZMA=OFF -DENABLE_LZ4=OFF -DENABLE_LZO=OFF \
        -DENABLE_OPENSSL=OFF -DENABLE_LIBXML2=OFF -DENABLE_EXPAT=OFF \
        -DENABLE_ICONV=OFF -DENABLE_ACL=OFF -DENABLE_XATTR=OFF \
        -DENABLE_CNG=OFF -DENABLE_PCREPOSIX=OFF -DENABLE_PCRE2POSIX=OFF \
        -DENABLE_CAT=OFF -DENABLE_TAR=OFF -DENABLE_CPIO=OFF -DENABLE_UNZIP=OFF
    "$cmakeBin" --build _b -j"$JOBS"
    "$cmakeBin" --install _b
    cd /build
}

# curl must carry no certificate list of its own. With one baked in, a download that forgot to ask
# for the list would quietly work on this builder's distribution and fail on everyone else's.
checkCurlHasNoBakedCertificates() {
    local found
    found="$(strings "$Prefix/lib/libcurl.a" | grep -cE '^/etc/(ssl|pki)' || true)"
    if [ "$found" -ne 0 ]; then
        printf 'ERROR: libcurl.a has %s certificate paths compiled into it; pass --without-ca-bundle --without-ca-path\n' "$found" >&2
        strings "$Prefix/lib/libcurl.a" | grep -E '^/etc/(ssl|pki)' | sort -u >&2
        exit 1
    fi
    printf ':: curl carries no certificate list of its own\n'
}

runStep zlib       ZLIB_COMMIT       buildZlib
runStep zstd       ZSTD_COMMIT       buildZstd
runStep nghttp2    NGHTTP2_COMMIT    buildNghttp2
runStep openssl    OPENSSL_COMMIT    buildOpenssl
runStep curl       CURL_COMMIT       buildCurl
runStep libarchive LIBARCHIVE_COMMIT buildLibarchive

# The old formats were dropped on purpose, so a build that silently kept them is a build that lied
checkZstdHasNoLegacyFormats() {
    local found
    found="$(nm "$Prefix/lib/libzstd.a" 2>/dev/null | grep -cE 'ZSTDv0[0-9]_' || true)"
    if [ "$found" -ne 0 ]; then
        printf 'ERROR: libzstd.a still carries %s symbols for the old formats\n' "$found" >&2
        exit 1
    fi
    printf ':: zstd carries no old-format decoders\n'
}

checkZstdHasNoLegacyFormats
checkCurlHasNoBakedCertificates

printf ':: Done. Static archives in %s/lib\n' "$Prefix"
