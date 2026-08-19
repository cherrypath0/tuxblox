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

# webkitgtk-bundle/package.sh
# Runs INSIDE the container after build-in-container.sh has populated the
# webkitgtk-prefix volume (build.sh runs both in sequence; build-in-container.sh
# always rebuilds WebKitGTK itself, including the WEBKIT_EXEC_PATH source patch, so
# there is no separate "rebuild WebKitGTK only" step to run first). Rewrites every
# ELF file's RPATH to a relative, self-contained path so the extracted tarball works
# regardless of where TuxBlox's build later unpacks it, then tars up only the
# runtime-relevant subset.
#
# Several corrections vs. the original plan draft, all found by inspecting the real
# populated prefix (and, for the strip/gdk-pixbuf items, by testing the packaged
# result OUTSIDE this build container) rather than trusting assumptions -- see
# README.md's "Runtime relocatability" and "Bundle contents and what was
# deliberately left out" sections for the user-facing consequences:
#
# 1. RPATH/RUNPATH rewriting must cover every ELF file in the shipped tree, not just
#    lib/*.so*. WebKitWebProcess/WebKitNetworkProcess/WebKitGPUProcess/jsc (under
#    libexec/webkitgtk-6.0), gst-plugin-scanner (libexec/gstreamer-1.0), and p11-kit's
#    helper binaries (libexec/p11-kit) are real ELF *executables*, not .so files, and
#    they carry the identical absolute-prefix RPATH problem
#    (`readelf -d .../WebKitWebProcess` showed
#    `RPATH: [/opt/tuxblox-webview/lib:/opt/tuxblox-webview/lib/x86_64-linux-gnu]`
#    before this script runs). patchelf handles RPATH/RUNPATH on executables exactly
#    like it does on shared libraries -- this is a real ELF dynamic-section entry, NOT
#    one of the compile-time string constants covered by the env-var mechanism in
#    README.md. Rather than hardcode which subdirectories contain binaries, this scans
#    every regular file in the packaged tree and checks its ELF magic directly.
#
# 2. The tarball must include libexec/ (WebKit's own process binaries live there, not
#    under lib/) and etc/ + share/ (Mesa's drirc lookups, fontconfig config) -- the
#    original plan draft's `tar ... include lib` would have silently shipped a bundle
#    whose WebKitWebProcess/WebKitNetworkProcess/WebKitGPUProcess binaries simply
#    aren't in the archive at all. libexec/installed-tests and share/installed-tests
#    (~22 MB of gdk-pixbuf/GLib "make check" test fixtures -- images, tiny throwaway
#    test binaries) are excluded: real build output, but never consumed by anything
#    that loads this bundle at runtime. bin/ and sbin/ (build-time CLI tools --
#    glib-compile-resources, icu's genccode, etc.) are excluded for the same reason:
#    nothing a WebView2-replacement consumer dlopen()s this bundle for needs them.
set -euo pipefail
source "$(dirname "$0")/versions.env"

PREFIX=/opt/tuxblox-webview
cd "$PREFIX"

# Task 8 post-review correction (2026-08-10), I-2: gdk-pixbuf's loaders.cache (the
# module registry gdk_pixbuf_get_formats()/loader lookup reads at runtime) has each
# loader's absolute .so path baked into the cache FILE ITSELF at build time -- a
# problem RPATH/env-vars can't touch, since it's data, not an ELF dynamic-section
# entry or a getenv() call. Regenerating it needs the real final install path, which
# isn't known until TuxBlox's own Proton build runs (see ProtonSource/Makefile.in's
# extraction rule, which now runs gdk-pixbuf-query-loaders again post-extraction to
# rebuild the cache with correct paths) -- but that means gdk-pixbuf-query-loaders
# itself has to actually be IN the shipped tarball, even though bin/ as a whole is
# deliberately excluded below (build-time CLI tools nothing at runtime needs). Copied
# into libexec/ specifically (not left in bin/, which is never tarred) so it's swept
# up by the same find/RPATH-rewrite loop as everything else without special-casing
# the tar command below for one extra top-level directory.
mkdir -p libexec/gdk-pixbuf-tools
cp -a bin/gdk-pixbuf-query-loaders libexec/gdk-pixbuf-tools/

# webview2loader-host (plan 2026-08-14, Task 2): build-in-container.sh compiles this
# straight into $PREFIX/bin (there's no from-source dependency chain here, just a
# plain gcc invocation -- see that script's own comment), but bin/ as a whole is
# deliberately excluded from the shipped tarball below (build-time CLI tools nothing
# at runtime needs, per this file's own top-of-file comment #2). This is the one
# bin/ binary that IS a real runtime artifact -- Task 3's unixlib.c execs it -- so,
# same reasoning and same mechanism as gdk-pixbuf-query-loaders just above, it's
# copied into libexec/ so the find/RPATH-rewrite loop and the tar command below pick
# it up automatically without special-casing either one for a second top-level
# bin/-derived file.
cp -a bin/webview2loader-host libexec/

# Task 7 (2026-08-14 webview2loader-host plan): relocate this bundle's own
# libEGL.so*/libGL.so*/libGLESv1_CM.so*/libGLESv2.so* out of
# lib/x86_64-linux-gnu/ into a lib/x86_64-linux-gnu/gl-fallback/ subdirectory,
# BEFORE the RPATH-rewrite loop below runs -- so that loop's own depth-based
# $ORIGIN computation picks these files up "for free" (they get an extra
# "../" to walk back up to the shared lib/ dirs, same as any other file one
# level deeper than lib/x86_64-linux-gnu/ itself) and, just as importantly, so
# no OTHER file's RPATH is ever computed relative to this new subdirectory --
# confirmed by the earlier, since-reverted attempt at this exact packaging
# change (see .superpowers/sdd/2026-08-13-webview2-window-docking-messaging/
# lag-glvnd-report.md) via a full `readelf -d` sweep of the whole repackaged
# tree.
#
# Why: unixlib.c's spawn_helper() decides, per-launch, whether
# webview2loader-host should resolve libEGL.so.1/libGL.so.1 against the
# HOST's own real glvnd install (letting a real GPU driver, e.g. NVIDIA, do
# hardware-accelerated rendering) or fall back to this bundle's own Mesa
# llvmpipe build (an LLVM-JIT-compiled software rasterizer -- see
# build-in-container.sh's Mesa comment for the softpipe-vs-llvmpipe history) --
# but only if this bundle's own copies are moved somewhere the normal RPATH
# search never reaches on its own, so the relocation mechanism itself works
# the same way regardless of which of those two branches spawn_helper()
# actually takes.
#
# 2026-08-15 update: as of the software-only-llvmpipe plan, spawn_helper()
# ALWAYS takes the bundle-fallback branch (see its own WV2L_ALWAYS_USE_BUNDLE_GL
# comment in unixlib.c for why -- a confirmed upstream WebKitGTK/NVIDIA driver
# crash, not a portability concern) -- the "host's copies win by default"
# framing above describes the mechanism's ORIGINAL design, not current
# behavior. This relocation step itself is unchanged either way: gl-fallback/
# still needs to exist and be reachable only via LD_LIBRARY_PATH, now simply
# because it's unconditionally selected rather than conditionally selected.
# Left in place, unmoved: libwayland-egl.so.1
# (confirmed via readelf -d to need only libc.so.6 -- not part of the
# EGL/glvnd dispatch decision at all) and libgallium-25.1.8.so/gbm/dri_gbm.so
# (needed unconditionally via GBM_BACKENDS_PATH, regardless of which EGL
# vendor ends up loaded).
#
# Idempotent against re-runs on the persisted webkitgtk-prefix podman volume
# (a second run finds these files already moved and mkdir -p / mv -f/-n
# handle that without erroring or double-nesting).
echo ":: Relocating bundle's own libEGL/libGL/libGLESv1_CM/libGLESv2 into gl-fallback/"
mkdir -p lib/x86_64-linux-gnu/gl-fallback
for pattern in 'libEGL.so*' 'libGL.so*' 'libGLESv1_CM.so*' 'libGLESv2.so*'; do
    for f in lib/x86_64-linux-gnu/$pattern; do
        [ -e "$f" ] || continue
        mv -f "$f" lib/x86_64-linux-gnu/gl-fallback/
    done
done

# fontconfig's installed fonts.conf hardcodes the build prefix as its first
# cache directory ($PREFIX/var/cache/fontconfig). That path exists only inside
# the build container, so on a real install fontconfig tries it, fails, and
# warns -- for nothing, since the very next entry is the xdg one
# (~/.cache/fontconfig) which is where the cache actually belongs anyway.
# Dropped here rather than patched at build time so it stays a packaging
# concern; the relative <include>conf.d</include> next to it is already
# relocatable and is deliberately left alone. unixlib.c's
# set_webkit_relocation_env() is what points FONTCONFIG_PATH at this directory
# in the first place -- without that, none of this file is read at all.
echo ":: Dropping the build-prefix cachedir from etc/fonts/fonts.conf"
if [ -f etc/fonts/fonts.conf ]; then
    sed -i "\|<cachedir>$PREFIX/var/cache/fontconfig</cachedir>|d" etc/fonts/fonts.conf
    if grep -q "$PREFIX" etc/fonts/fonts.conf; then
        echo "ERROR: etc/fonts/fonts.conf still references the build prefix $PREFIX" >&2
        exit 1
    fi
fi

echo ":: Rewriting RPATH/RUNPATH on every ELF file in the packaged tree"
find include lib libexec etc share -type f \
    -not -path 'libexec/installed-tests/*' -not -path 'share/installed-tests/*' \
    -print0 2>/dev/null | while IFS= read -r -d '' f; do
    # Portable ELF magic check (first 4 bytes: 0x7f 'E' 'L' 'F') rather than depending
    # on file(1) being present -- cheaply skips the vast majority of files (headers,
    # fontconfig XML, .pc files, docs) without needing to know their extensions.
    magic=$(head -c4 "$f" 2>/dev/null | od -An -tx1 | tr -d ' \n')
    [ "$magic" = "7f454c46" ] || continue

    # patchelf errors out on ELF files with no dynamic section (static archives'
    # member objects, if any ever end up loose in this tree, or other non-dynamically-
    # linked ELF); --print-rpath is a cheap way to probe for that without aborting the
    # whole packaging run over one unexpected file.
    patchelf --print-rpath "$f" >/dev/null 2>&1 || continue

    # A small number of libraries (found: libsqlite3.so, linked into
    # libwebkitgtk-6.0.so/libjavascriptcoregtk-6.0.so/libsoup-3.0.so/the injected
    # bundle/all three WebKit process binaries) were linked with a full ABSOLUTE path
    # baked into their own DT_NEEDED entry (`readelf -d` shows
    # `NEEDED: [/opt/tuxblox-webview/lib/libsqlite3.so]` instead of a bare
    # `NEEDED: [libsqlite3.so]`) -- almost certainly because CMake found it via an
    # imported target with an absolute IMPORTED_LOCATION rather than through
    # pkg-config the way most of this tree's other dependencies were found. RPATH is
    # NEVER consulted for an absolute DT_NEEDED path -- ld.so uses it literally -- so
    # no amount of --set-rpath above fixes this; confirmed for real via `ldd` against
    # a relocated tarball extracted to /tmp/webkitgtk-verify, which reported
    # "/opt/tuxblox-webview/lib/libsqlite3.so => not found" verbatim on 6 different
    # files even after the RPATH fix above was already correct for everything else.
    # Fix: rewrite any absolute DT_NEEDED entry pointing inside the build's own
    # $PREFIX down to a bare soname, which the RPATH fix above then resolves
    # normally (libsqlite3.so genuinely is in lib/, just referenced the wrong way).
    while IFS= read -r needed; do
        case "$needed" in
            "$PREFIX"/*)
                patchelf --replace-needed "$needed" "$(basename "$needed")" "$f"
                ;;
        esac
    done < <(patchelf --print-needed "$f" 2>/dev/null)

    dir=$(dirname "$f")
    # Depth = number of path segments from the packaged tree's root down to this
    # file's own directory (NOT "relative to lib/" -- that formula only works for
    # files actually nested under lib/, and silently miscounts for libexec/etc/share
    # paths, which sit as siblings of lib/ rather than descendants of it). Counting
    # segments from the root and always appending a literal "lib" at the end handles
    # both cases uniformly and correctly: files directly in lib/ resolve
    # $ORIGIN/../lib right back to their own directory (harmless redundancy with
    # $ORIGIN itself); files under libexec/webkitgtk-6.0/ resolve $ORIGIN/../../lib to
    # the real shared top-level lib/.
    depth=$(python3 -c "import sys; d=sys.argv[1]; print(0 if d=='.' else len(d.split('/')))" "$dir")
    up=""
    for ((i = 0; i < depth; i++)); do up="../$up"; done
    # Both lib/ AND lib/x86_64-linux-gnu/ are needed, not just lib/ -- found by
    # actually testing this (see README.md's "Verification performed" section).
    # WebKitGTK's own CMake build installs to plain lib/, but every meson-built
    # dependency underneath it (glib, gtk4, mesa, gio's TLS module, ...) installs to
    # Debian's multiarch lib/x86_64-linux-gnu/ convention instead (the same
    # discrepancy build-in-container.sh's own top-of-file comment already documents
    # for LDFLAGS at build time -- this is that exact same fact mattering again, one
    # more time, at package time). A single-path RPATH back to just lib/ silently
    # left libwebkitgtk-6.0.so resolving libglib-2.0.so.0 from the *build
    # container's own system copy* instead of the bundle's -- caught by `ldd` against
    # a tarball extracted somewhere with no relationship to /opt/tuxblox-webview, per
    # the brief's Step 3 verification command.
    # --force-rpath: writes DT_RPATH instead of DT_RUNPATH. DT_RPATH is
    # consulted by ld.so BEFORE LD_LIBRARY_PATH (DT_RUNPATH is consulted
    # AFTER), and is inherited transitively by libraries this one loads --
    # required because Proton's own `proton` script prepends its own
    # lib/x86_64-linux-gnu to LD_LIBRARY_PATH for every wine process, which
    # would otherwise win the soname race against this bundle's own,
    # different-versioned GStreamer/graphene builds. See
    # "LD_LIBRARY_PATH beats this bundle's RUNPATH" in README.md.
    patchelf --force-rpath --set-rpath "\$ORIGIN:\$ORIGIN/${up}lib:\$ORIGIN/${up}lib/x86_64-linux-gnu" "$f"

    # Task 8 post-review correction (2026-08-10), C-2: strip debug symbols. The
    # committed tarball came in at 160.9 MiB -- over GitHub's 100 MiB hard per-file
    # limit, which would have made `git push` reject the commit outright. Root cause:
    # the meson-built half of this dependency chain doesn't set a release buildtype
    # (unlike WebKitGTK's own -DCMAKE_BUILD_TYPE=Release), so most of these .so's
    # carry full debug symbol tables. --strip-unneeded removes symbols not needed for
    # relocation/dynamic linking (debug info, local symbols) while preserving the
    # dynamic symbol table every shared library needs to actually be loaded and
    # linked against -- safe to run unconditionally on every ELF file this loop
    # already confirmed has a dynamic section (the --print-rpath probe above), same
    # class of file `strip` is meant for. Run after patchelf, not before: patchelf's
    # RPATH/DT_NEEDED edits are dynamic-section changes, unaffected by stripping
    # symbol/debug sections afterward, so the order only matters for not having to
    # re-probe the ELF-magic/dynamic-section checks twice.
    strip --strip-unneeded "$f"
done

mkdir -p /out
# The trailing "S" flag on --transform is load-bearing, not decoration: GNU tar's
# --transform, by default, rewrites BOTH a member's own archived path AND (for
# symlinks) the literal target text the symlink points to. Nearly every shared
# library in this tree is a chain of relative, same-directory symlinks
# (libfoo.so -> libfoo.so.1 -> libfoo.so.1.2.3), and prepending "webkitgtk/" to
# those target strings the same way as the member path corrupts every single one of
# them (libfoo.so.1 ends up pointing at the non-existent "webkitgtk/libfoo.so.1.2.3"
# relative to its own directory, instead of the real, adjacent "libfoo.so.1.2.3") --
# reproduced directly with a 3-symlink test case before finding "S" as the fix, and
# re-confirmed against the real packaged tree afterward (ldd showed dozens of
# "not found" dependencies -- including libwebkitgtk-6.0.so.4 itself -- purely from
# broken symlinks, RPATH already being completely correct). The "S" flag restricts
# --transform to member archive paths only, leaving symlink target text untouched.
#
# --exclude='*.a' (final-review finding I-5): several dependencies install a static
# archive alongside their shared library (libnettle.a and libhogweed.a at ~18 MB
# each, libpcre2-{8,16,32}.a, libsqlite3.a, libgmp.a, libwebp*.a, liblcms2.a,
# libopenjp2.a, libtasn1.a -- ~50 MB uncompressed, 5.1 MiB of the compressed
# tarball, measured). A static archive is link-time-only input: nothing that
# dlopen()s this bundle at runtime can consume one, and this bundle is never linked
# against at build time (README.md's "Note on pkg-config against a relocated bundle"
# -- the .pc files don't even work relocated). They're also invisible to the RPATH/
# strip loop above, which skips them on the ELF-magic check ("!<arch>", not \x7fELF).
# Dropping them matters beyond tidiness: this tarball is committed to git, and the
# 100 MiB GitHub per-file hard limit was already hit once during Task 8 (C-2, fixed
# by stripping) -- WebKitGTK grows with every release, so headroom for the next
# version bump is worth keeping.
tar -cJf "/out/webkitgtk-${WEBKITGTK_VERSION}-x86_64.tar.xz" \
    --exclude='libexec/installed-tests' --exclude='share/installed-tests' \
    --exclude='*.a' \
    --transform 's,^,webkitgtk/,S' \
    include lib libexec etc share

echo ":: Wrote /out/webkitgtk-${WEBKITGTK_VERSION}-x86_64.tar.xz"
