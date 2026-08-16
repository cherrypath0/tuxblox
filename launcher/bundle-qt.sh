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

# launcher/bundle-qt.sh
# Runs INSIDE the tuxblox-old-glibc-builder container, immediately after
# `cmake --build build` has produced build/TuxBloxLauncher (build.sh chains both
# into the same `podman run`). Must run in-container: Qt6 lives at
# /opt/qt6/6.6.3/gcc_64, installed by the Containerfile via aqtinstall because
# Ubuntu 20.04's apt archive has no Qt6 packages at all -- that path exists
# nowhere on the host, so a CMake POST_BUILD step or a host-side script could
# not see it.
#
# Why this exists: the linker bakes /opt/qt6/6.6.3/gcc_64/lib into the binary's
# DT_RUNPATH. On any real deployment target (Ubuntu 20.04 / Debian 11 / Mint 20 --
# the whole reason for the pinned old-glibc baseline) that path does not exist and
# no system Qt6 exists either, so the binary dies at exec with
# "error while loading shared libraries: libQt6Widgets.so.6". On the maintainer's
# Arch host it *appeared* to work only because Arch has an unrelated, different
# system Qt6 that silently substituted (proven by `ldd` resolving libicui18n.so.78
# from /usr/lib while Qt's own libQt6Core.so.6.6.3 declares libicui18n.so.56).
#
# Fix: copy Qt's own libraries into a libtuxblox/ directory beside the binary and
# rewrite RPATH to be $ORIGIN-relative, so the bundle works from any directory it
# is later placed in. Same core technique as webkitgtk-bundle/package.sh, much
# smaller scope.
#
# Layout produced (mirrors the installed ~/.tuxblox/ layout exactly, which is why
# the nesting exists): the executable stays alone at the top level and everything
# it needs lives one level down in libtuxblox/, because the two ship as two
# SEPARATE release artifacts -- "launcher" is a flat file, "libtuxblox" is a
# tarball. The installer wipes an archive artifact's whole target directory
# before re-extracting it on upgrade, so libtuxblox has to own a directory of its
# own; a flat extraction into ~/.tuxblox itself would delete proton/, runtime/
# and the launcher binary on every libtuxblox upgrade.
#
#   build/TuxBloxLauncher                        <- artifact "launcher" (flat file)
#   build/libtuxblox/lib/*.so*                   <- artifact "libtuxblox" (tarball)
#   build/libtuxblox/plugins/platforms/libqxcb.so
#   build/libtuxblox/qt.conf
set -euo pipefail
cd "$(dirname "$0")"

QT_ROOT="${TUXBLOX_QT_ROOT:-/opt/qt6/6.6.3/gcc_64}"
BIN="build/TuxBloxLauncher"
BUNDLE_DIR="build/libtuxblox"
LIB_DIR="$BUNDLE_DIR/lib"
PLUGIN_DIR="$BUNDLE_DIR/plugins/platforms"

# The xcb platform plugin is dlopen()'d by QGuiApplication at startup, so it is
# NOT part of the binary's own ldd closure -- it has to be named explicitly, and
# its own dependency closure walked separately.
#
# Only xcb is bundled, not wayland: with no wayland plugin present Qt falls back
# to xcb through XWayland on a Wayland session, which is a working (if not
# native) path, whereas bundling the wayland stack would pull in
# libwayland-client/libQt6WaylandClient plus three more plugin directories for a
# UI that has no Wayland-specific requirement.
PLUGINS_TO_BUNDLE=("$QT_ROOT/plugins/platforms/libqxcb.so")

# System (non-Qt) libraries libqxcb.so needs that Qt does NOT ship and that are
# NOT part of a default Ubuntu 20.04 / Debian 11 desktop install. Qt 6.5 made
# libxcb-cursor a hard requirement of the xcb QPA plugin specifically, and that
# is the single most common "Qt6 app won't start on Ubuntu" report upstream --
# so without these the launcher still dies with
# 'Could not load the Qt platform plugin "xcb"' on exactly the old-glibc targets
# this build image exists to serve, Qt bundling or not.
#
# These need their own explicit list rather than falling out of the ldd closure
# below: that closure is filtered to $QT_ROOT/lib, and these live in the normal
# multiarch system path (/usr/lib/x86_64-linux-gnu), so it would never match
# them no matter how it were tuned. The Containerfile apt-installs them purely
# so there are real files here to copy -- nothing in the build links against
# them.
#
# ROOTS is hand-listed (it is the documented gap, and there is no rule that
# could derive it -- libqxcb.so's ldd closure is transitively enormous and
# almost entirely made of libraries that must NOT be bundled). But the roots are
# only a starting point: the walk below expands them transitively through
# DT_NEEDED, which is load-bearing rather than defensive. libxcb-image.so.0
# needs libxcb-util.so.1, which is absent from the base image too and which
# `ldd libqxcb.so` never once reported -- ldd does not recurse into a library it
# could not find in the first place, so a hand-written list of exactly the
# sonames ldd flagged silently drops it and ships a bundle that still fails to
# load.
SYSTEM_LIB_ROOTS=(
    libxcb-cursor.so.0
    libxcb-icccm.so.4
    libxcb-image.so.0
    libxcb-keysyms.so.1
    libxcb-randr.so.0
    libxcb-render-util.so.0
    libxcb-shape.so.0
    libxcb-xkb.so.1
    libxkbcommon-x11.so.0
)

# The stop set for that transitive walk: libraries assumed present on any target
# able to run an X11 application at all, deliberately left unbundled for the same
# reason the Qt closure leaves libc/libX11/libGL alone. Each entry is here on
# evidence, not on a hunch -- every one of them is already present in this
# minimal, desktop-less build image, i.e. it arrives with something far more
# fundamental than the xcb-util family being added here:
#   libc/libm/libdl/libpthread/librt/libgcc_s/libstdc++ -- the glibc + toolchain
#     floor this whole pinned-old-baseline image is designed around.
#   libxcb.so.1, libXau.so.6, libXdmcp.so.6, libbsd.so.0 -- libxcb1 is a hard
#     Depends of libX11-6, which any X11 client requires by definition, and the
#     other three are libxcb1's own hard Depends.
# In other words the line is drawn at libxcb.so.1 itself: within the closure of
# SYSTEM_LIB_ROOTS, everything above libxcb.so.1 in the stack is bundled and
# everything at or below it is assumed. Anything NOT listed here that turns up in
# the walk gets bundled, which is the safe direction to fail in: worst case ships
# a redundant, ABI-stable X11 library.
#
# libxcb-shm.so.0 and libxcb-render.so.0 were on this list on the first attempt,
# justified as "same upstream libxcb source package as libxcb.so.1, and hard
# Depends of libcairo2, so present on any desktop". Testing the bundle against a
# deliberately minimal Ubuntu 20.04 target (X11 + GL + fontconfig/glib/dbus, no
# cairo) left libxcb-render.so.0 "not found" -- the cairo argument turns out to be
# a guess about what else the user installed, not a guarantee. They are ~20 KB
# each of auto-generated protocol bindings that only ever call libxcb.so.1's
# frozen public API (xcb_send_request / xcb_wait_for_reply /
# xcb_get_extension_data), so bundling them is close to free and removes the
# guess. Note this is the opposite call from libxkbcommon below, and for the
# opposite reason: these really are pure public-ABI consumers.
#
# libxcb-sync.so.1 and libxcb-xfixes.so.0 look like the same case but are
# deliberately in NEITHER list, so they stay unbundled: they reach libqxcb.so
# through libQt6XcbQpa.so.6 rather than through any root here, which puts them
# outside this closure entirely, and they were never part of the gap this change
# fixes -- they already resolved before it, since libglx-mesa0 lists both as hard
# Depends and Qt requires that GL stack anyway.
#
# libxkbcommon.so.0 is pointedly NOT on this list even though it qualifies on
# every criterion above (universal, present in this bare image, hard Depends of
# GTK3/SDL2). It was excluded on the first attempt and that crashed: bundled
# libxkbcommon-x11.so.0 (0.10.0, focal) loaded against the host's much newer
# system libxkbcommon.so.0 and SIGSEGV'd inside xkb_x11_keymap_new_from_device,
# under QXcbConnection's constructor, before the first window ever appeared.
# libxkbcommon-x11 is not an ordinary consumer of libxkbcommon's public ABI --
# upstream builds both halves from one source tree and the -x11 half reaches
# into libxkbcommon's *internal* structures, so the pair is only ABI-stable when
# versions match. It has to be bundled as a matched set or not at all; the walk
# picks it up automatically via libxkbcommon-x11.so.0's DT_NEEDED now that it is
# off the stop set.
SYSTEM_LIBS_ASSUMED_PRESENT=(
    libc.so.6 libm.so.6 libdl.so.2 libpthread.so.0 librt.so.1
    libgcc_s.so.1 libstdc++.so.6
    libxcb.so.1 libXau.so.6 libXdmcp.so.6 libbsd.so.0
)

[[ -x "$BIN" ]] || { echo "!! $BIN not found -- run the cmake build first" >&2; exit 1; }
[[ -d "$QT_ROOT/lib" ]] || { echo "!! Qt6 not found at $QT_ROOT" >&2; exit 1; }
for p in "${PLUGINS_TO_BUNDLE[@]}"; do
    [[ -f "$p" ]] || { echo "!! Qt plugin not found: $p" >&2; exit 1; }
done

echo ":: Bundling Qt6 from $QT_ROOT"
rm -rf "$BUNDLE_DIR"
mkdir -p "$LIB_DIR" "$PLUGIN_DIR"

# Reset the binary's RPATH back to the container's Qt before anything else.
# Required for incremental rebuilds, where cmake finds TuxBloxLauncher already
# up to date and does NOT relink it -- so the file on disk still carries the
# $ORIGIN/libtuxblox/lib RPATH this script wrote on the previous run, pointing at
# the build/libtuxblox/lib that was just deleted three lines up. The ldd-based
# closure below would then see libQt6Widgets.so.6 as simply "not found", silently
# omit it, and produce a bundle missing the one library only the executable (not
# the xcb plugin) pulls in. Caught for real: a second back-to-back ./build.sh
# dropped the lib dir from 19 files to 17, losing libQt6Widgets.so.6 and its
# symlink.
patchelf --set-rpath "$QT_ROOT/lib" "$BIN"

# Copy a shared library plus every symlink hop in its chain
# (libQt6Core.so.6 -> libQt6Core.so.6.6.3), preserving the links as links rather
# than flattening them into duplicate 7 MB copies. Confirmed by `ls -la` in
# $QT_ROOT/lib that every hop is a bare same-directory relative name, so
# basename() is enough to follow the chain.
copy_lib_chain() {
    local name="$1"
    while [[ -n "$name" ]]; do
        if [[ ! -e "$LIB_DIR/$name" && ! -L "$LIB_DIR/$name" ]]; then
            cp -a "$QT_ROOT/lib/$name" "$LIB_DIR/"
        fi
        if [[ -L "$QT_ROOT/lib/$name" ]]; then
            name="$(basename "$(readlink "$QT_ROOT/lib/$name")")"
        else
            name=""
        fi
    done
}

# Compute the closure by asking ld.so (via ldd) what actually resolves, rather
# than hand-listing libraries -- that picks up Qt's privately bundled ICU and
# anything else Qt pulls in transitively without having to know about it here.
# Everything that resolves OUTSIDE $QT_ROOT/lib (libc, libX11, libxcb, libGL,
# libcurl, glib, ...) is a normal system dependency present on any desktop
# target and is deliberately NOT bundled -- with the one documented exception of
# SYSTEM_LIB_ROOTS above, which is handled by its own separate walk further down
# precisely because this filter can never see it.
#
# realpath -s normalizes the "../../lib" that shows up in the plugin's own
# resolved paths without following symlinks, so the versioned-vs-soname
# filenames stay intact for copy_lib_chain to walk.
#
# QT_ROOT_ESCAPED: grep's pattern below is anchored on QT_ROOT's own value
# (a filesystem path, not a literal grep pattern), and QT_ROOT is a
# documented override point (TUXBLOX_QT_ROOT) -- escaping just the "."
# characters (the only regex metacharacter a path realistically contains)
# keeps `grep` from treating e.g. "6.6.3" as "6-any-char-6-any-char-3".
#
# The trailing `|| true` on the pipeline is load-bearing, not decoration:
# under `set -euo pipefail`, if `grep` matches nothing (a real "empty
# closure" bug) it exits 1, pipefail propagates that through the rest of
# the pipe regardless of xargs/sort's own exit codes, and the `qt_libs=$(...)`
# assignment then trips `set -e` immediately -- killing the script with a
# bare, unlabelled `exit 1` and skipping the explicit, informative check
# below entirely. `|| true` lets the pipeline itself always succeed, so an
# empty `$qt_libs` reaches the real check on the next line intact.
QT_ROOT_ESCAPED="${QT_ROOT//./\\.}"
qt_libs="$(
    { ldd "$BIN"; for p in "${PLUGINS_TO_BUNDLE[@]}"; do ldd "$p"; done; } \
        | sed -n 's/^.* => \(.*\) (0x[0-9a-f]*)$/\1/p' \
        | while IFS= read -r path; do realpath -s "$path"; done \
        | grep "^$QT_ROOT_ESCAPED/lib/" \
        | xargs -r -n1 basename \
        | sort -u \
        || true
)"

[[ -n "$qt_libs" ]] || { echo "!! Computed an empty Qt closure -- refusing to ship an unbundled binary" >&2; exit 1; }

while IFS= read -r lib; do
    copy_lib_chain "$lib"
done <<< "$qt_libs"

# Bundle the xcb-ecosystem system libraries into the SAME lib dir as Qt's own.
# Not a separate directory: libqxcb.so already ships RUNPATH=$ORIGIN/../../lib,
# i.e. exactly $LIB_DIR (two levels up from plugins/platforms/ is the bundle
# root, whose lib/ this is), so one directory means zero extra RPATH entries and --
# more importantly -- means the plugin file itself still needs no rewriting,
# which is the only reason patchelf 0.10 never gets a chance to destroy its
# .note.qt.metadata section (see the long ensure_rpath comment below).
echo ":: Bundling system xcb libraries the xcb platform plugin needs"

declare -A assumed_present=()
for s in "${SYSTEM_LIBS_ASSUMED_PRESENT[@]}"; do assumed_present["$s"]=1; done

# Resolve a soname the way ld.so would, via the cache, instead of hardcoding
# /usr/lib/x86_64-linux-gnu -- that path is Debian/Ubuntu multiarch-specific and
# ldconfig knows the real answer for whatever base image this is. The x86-64
# guard skips a 32-bit entry of the same soname if the image ever gains i386
# multiarch.
#
# The trailing `|| true` is load-bearing, not decoration: awk's `exit` closes
# its stdin the moment it finds the first match, and ldconfig -p (still mid-
# write on its full, much longer library list) gets SIGPIPE on its next write
# -- an intermittent failure, since it depends on exactly how much of
# ldconfig's output awk has already buffered when the match is found. Under
# this script's `set -o pipefail`, that non-zero ldconfig exit status (128+13)
# propagates through the whole pipeline even though awk itself printed the
# right answer, and `set -e` then kills the entire script at the `sys_path=`
# assignment below -- after build/libtuxblox (lib/, plugins/ and qt.conf) was
# already wiped by this script's own cleanup step earlier, but before it is
# repopulated. Reproduced directly: intermittent `exit 141` mid-run, `awk`'s
# own stdout unaffected. `|| true` makes the pipeline's exit status reflect
# only whether resolve_soname found a match, matching the empty-output check
# at this function's every call site (see the `[[ -z "$sys_path" ...`
# check just below).
resolve_soname() {
    ldconfig -p | awk -v s="$1" '$1 == s && /x86-64/ { print $NF; exit }' || true
}

# Breadth-first over DT_NEEDED, indexed rather than shifting the array so an
# emptied queue can never trip `set -u`. patchelf --print-needed is used instead
# of ldd because the closure has to be walked one level at a time to apply the
# stop set -- ldd would flatten it and drag in everything libc pulls.
declare -A sys_bundled=()
queue=("${SYSTEM_LIB_ROOTS[@]}")
qi=0
while [[ $qi -lt ${#queue[@]} ]]; do
    soname="${queue[$qi]}"
    qi=$((qi + 1))
    [[ -n "${sys_bundled[$soname]:-}" ]] && continue
    [[ -n "${assumed_present[$soname]:-}" ]] && continue

    sys_path="$(resolve_soname "$soname")"
    if [[ -z "$sys_path" || ! -e "$sys_path" ]]; then
        echo "!! $soname not installed in this build image -- add its package to the Containerfile" >&2
        exit 1
    fi

    # cp -L, not the cp -a that copy_lib_chain uses for Qt: in the system
    # library path the soname is a symlink to a versioned file
    # (libxcb-cursor.so.0 -> libxcb-cursor.so.0.0.0) whose target lives outside
    # the bundle, so preserving it as a link would leave a dangling symlink in
    # $LIB_DIR. Dereferencing gives one regular file named by the soname, which
    # is the only name any DT_NEEDED entry ever asks for.
    cp -L "$sys_path" "$LIB_DIR/$soname"
    sys_bundled["$soname"]=1

    while IFS= read -r need; do
        [[ -n "$need" ]] && queue+=("$need")
    done < <(patchelf --print-needed "$sys_path")
done

for p in "${PLUGINS_TO_BUNDLE[@]}"; do
    cp -a "$p" "$PLUGIN_DIR/"
done

echo ":: Rewriting RPATHs to \$ORIGIN-relative paths"
# --force-rpath writes DT_RPATH instead of DT_RUNPATH. Two reasons, both
# specific to this bundle rather than copied from webkitgtk-bundle/package.sh
# (whose stated reason -- Proton prepending its own lib dir to LD_LIBRARY_PATH
# for every wine process -- does not apply here; nothing prepends
# LD_LIBRARY_PATH before the launcher):
#   1. ld.so consults DT_RPATH BEFORE LD_LIBRARY_PATH but DT_RUNPATH AFTER it.
#      The bug this whole script fixes is a *silent substitution* of a
#      different, unrelated system Qt6 -- so the executable's own bundle
#      lookup should win that race rather than merely hoping the
#      environment is clean. Note this guarantee covers only the
#      executable's own DT_NEEDED entries: libraries loaded transitively
#      through Qt's own .so files (e.g. libicuuc.so.56 via libQt6Core)
#      resolve via THEIR RUNPATH, which ld.so still consults after
#      LD_LIBRARY_PATH -- an unusual environment could in principle still
#      substitute one of those. Not a concern for this bundle's actual
#      threat model (an unset-by-default env var on a target machine that
#      has no Qt6 to substitute from in the first place), just not the
#      unconditional guarantee this comment used to claim.
#   2. DT_RPATH is inherited by libraries loaded further down the chain,
#      DT_RUNPATH is not -- which matters because Qt's own .so files keep the
#      DT_RUNPATH=$ORIGIN they shipped with (see the ensure_rpath comment
#      below for why they are deliberately left untouched).
#
# --remove-rpath BEFORE the --force-rpath --set-rpath is load-bearing, not
# redundant: this image ships patchelf 0.10, whose --force-rpath only converts
# an existing DT_RUNPATH to DT_RPATH when the file also already has a DT_RPATH
# entry. Every file here has DT_RUNPATH *only*, so patchelf reuses that entry
# in place and silently leaves the tag as DT_RUNPATH -- verified directly:
# `patchelf --force-rpath --set-rpath '$ORIGIN/libtuxblox/lib'` on the freshly linked
# binary still produced `(RUNPATH)` under readelf -d, while removing first and
# then setting produced `(RPATH)`. Removing first makes patchelf add a brand new
# entry, which it tags from --force-rpath correctly.
#
# ensure_rpath is a no-op when the file's existing search path is already
# exactly what the bundle layout needs. That guard is NOT an optimisation, it is
# a correctness requirement, and it is why nothing below rewrites Qt's own
# shipped files:
#
#   patchelf 0.10 DESTROYS Qt plugin metadata. Running it on
#   plugins/platforms/libqxcb.so made Qt refuse the plugin outright --
#   "'libqxcb.so' is not a Qt plugin (metadata not found)", followed by
#   "no Qt platform plugin could be initialized" and SIGABRT, reproduced from a
#   scratch directory with an otherwise complete and correct bundle. Cause found
#   by diffing `readelf -SW` before/after: Qt 6 stores a plugin's metadata in a
#   dedicated `.note.qt.metadata` NOTE section (section [1] in the pristine
#   file), and patchelf 0.10 rebuilds the section header table when it relocates
#   .dynamic, DROPPING that section entirely -- 32 sections before, 32 after,
#   but `.note.qt.metadata` gone and a relocated `.dynamic` appended in its
#   place. RPATH itself came out correct; the plugin was simply no longer
#   loadable.
#
# Since Qt's own .so files already ship RUNPATH=$ORIGIN and libqxcb.so already
# ships RUNPATH=$ORIGIN/../../lib -- exactly the layout produced here (nesting
# everything under libtuxblox/ moved the plugin and lib/ down by the same one
# level, so the path BETWEEN them is unchanged: plugins/platforms -> ../.. is the
# bundle root either way), verified
# with `patchelf --print-rpath` against the pristine aqtinstall tree -- and no
# file in this closure has an absolute DT_NEEDED entry (so unlike
# webkitgtk-bundle/package.sh there is no --replace-needed pass to do either),
# the only file that actually needs rewriting is the executable, which is not a
# plugin and carries no metadata section to lose.
#
# If a future Qt version ships different paths, the calls below will start
# rewriting Qt's files for real; the .note.qt.metadata check in the verification
# pass at the end of this script exists to make that fail the build loudly
# instead of shipping a silently unloadable plugin.
ensure_rpath() {
    local want="$1" file="$2"
    [[ "$(patchelf --print-rpath "$file")" == "$want" ]] && return 0
    patchelf --remove-rpath "$file"
    patchelf --force-rpath --set-rpath "$want" "$file"
}

# The $LIB_DIR loop now also sweeps the system xcb libraries copied in above, and
# for those it is doing real work rather than hitting the no-op guard: they ship
# no RPATH at all (a distro library has no reason to), yet several of them depend
# on each other (libxcb-cursor.so.0 -> libxcb-image.so.0 -> libxcb-util.so.1).
# The plugin's own RUNPATH cannot cover that -- DT_RUNPATH is not inherited by
# dependencies -- so without $ORIGIN here they would resolve against the target's
# /usr/lib, which is exactly the copy assumed not to exist. They carry no Qt
# plugin metadata, so patchelf rewriting them is safe.
#
# Only the executable's own RPATH depends on the libtuxblox/ nesting: $ORIGIN is
# the directory holding the file being rewritten, and the executable's directory
# is one level ABOVE lib/ now instead of being its parent-by-sibling, hence
# $ORIGIN/libtuxblox/lib. The other two are untouched by the nesting -- the
# bundled libraries find each other in their own directory ($ORIGIN), and the
# plugin still reaches lib/ by going two levels up to the bundle root.
ensure_rpath '$ORIGIN/libtuxblox/lib' "$BIN"
while IFS= read -r -d '' f; do ensure_rpath '$ORIGIN' "$f"; done \
    < <(find "$LIB_DIR" -maxdepth 1 -type f -name '*.so*' -print0)
while IFS= read -r -d '' f; do ensure_rpath '$ORIGIN/../../lib' "$f"; done \
    < <(find "$BUNDLE_DIR/plugins" -type f -name '*.so' -print0)

# qt.conf overrides the paths compiled into QLibraryInfo (which otherwise still
# point at /opt/qt6/6.6.3/gcc_64/plugins). Prefix is the one path resolved
# relative to qt.conf's own location; every other entry is resolved relative to
# Prefix. Both are written explicitly rather than relying on the defaults so the
# resulting plugin search path is unambiguous.
#
# It ships INSIDE libtuxblox/ (a sibling of plugins/, so its contents are correct
# for its new location) rather than next to the executable, because the
# executable ships as its own separate flat-file artifact -- see the layout note
# at the top of this file. Note what that means: QLibraryInfo only ever looks for
# qt.conf in QCoreApplication::applicationDirPath(), so from here it is NOT read
# at all, and the file is a documentation/no-op copy rather than the mechanism.
#
# What actually locates the plugins is Qt's relocatable-prefix support: Qt 6
# dladdr()s its own libQt6Core.so and derives the prefix as <that dir>/.., i.e.
# libtuxblox/lib/.. = libtuxblox/, making PluginsPath libtuxblox/plugins with no
# configuration at all. Verified by deleting qt.conf entirely and re-running from
# a scratch directory: QT_DEBUG_PLUGINS still reported
# "<dir>/libtuxblox/plugins/platforms/libqxcb.so loaded library". (The same
# mechanism, not qt.conf, was doing the work in the previous flat layout too.)
cat > "$BUNDLE_DIR/qt.conf" <<'EOF'
[Paths]
Prefix = .
Plugins = plugins
EOF

echo ":: Verifying the bundle resolves without $QT_ROOT"
# Fail the build rather than ship a bundle that only works on a machine that
# happens to have a compatible system Qt6 -- that failure mode is exactly what
# this script exists to eliminate, and it is invisible on the maintainer's own
# Arch host.
fail=0

# The executable must be fully resolvable inside this container: anything still
# pointing into $QT_ROOT means a file was missed, anything "not found" means the
# closure walk dropped something.
leak="$(ldd "$BIN" | grep -E "not found|$QT_ROOT" || true)"
if [[ -n "$leak" ]]; then
    echo "!! $BIN did not resolve cleanly against the bundle:" >&2
    echo "$leak" >&2
    fail=1
fi

# Every bundled system library must resolve its own bundled dependencies out of
# $LIB_DIR, not out of the build image's /usr/lib. This is the check with real
# teeth: DT_RPATH=$ORIGIN is searched ahead of the default directories, so if the
# RPATH rewrite above worked, ldd reports the $LIB_DIR copy, and if it silently
# did not, ldd reports /usr/lib -- which would "work" here and fail on every
# target. The container has these packages installed (that is where the copies
# came from), so an outright "not found" here means the closure walk itself broke.
lib_abs="$(cd "$LIB_DIR" && pwd)"
for soname in "${!sys_bundled[@]}"; do
    out="$(ldd "$LIB_DIR/$soname" || true)"
    miss="$(echo "$out" | grep "not found" || true)"
    if [[ -n "$miss" ]]; then
        echo "!! bundled $soname has unresolved dependencies:" >&2
        echo "$miss" >&2
        fail=1
    fi
    while read -r dep path; do
        [[ -n "${sys_bundled[$dep]:-}" ]] || continue
        if [[ "$path" != "$lib_abs/"* ]]; then
            echo "!! bundled $soname resolves bundled $dep from $path, not from the bundle" >&2
            fail=1
        fi
    done < <(echo "$out" | sed -n 's/^\s*\(\S*\) => \(\S*\) (0x[0-9a-f]*)$/\1 \2/p')
done

# The plugin is checked differently, because a "not found" against it is not
# automatically a bug: libqxcb.so's closure also contains ordinary system
# libraries this script deliberately does not ship (see
# SYSTEM_LIBS_ASSUMED_PRESENT), and a build image is not a desktop, so one of
# those can legitimately be absent here. A "not found" is a real, build-failing
# miss only when the soname is something this script was supposed to have
# bundled, which is now two distinct sources:
#   - it exists under $QT_ROOT/lib, i.e. Qt ships it and the closure walk
#     dropped it;
#   - it is in sys_bundled, i.e. this script copied it out of the system path on
#     purpose. Before those copies existed, all nine of these were logged as
#     informational notes, which was correct then and would be a silent hole now.
# Anything else stays an informational note, plus any leak back into $QT_ROOT.
for p in "${PLUGINS_TO_BUNDLE[@]}"; do
    bundled="$PLUGIN_DIR/$(basename "$p")"

    # Qt 6 keeps a plugin's metadata (IID, className, the Q_PLUGIN_METADATA
    # JSON) in a `.note.qt.metadata` ELF NOTE section, and QPluginLoader rejects
    # any plugin whose metadata it cannot find. patchelf 0.10 silently drops
    # that section when it rewrites the file -- see the ensure_rpath comment
    # above -- and the resulting failure is invisible to ldd, so check the
    # section directly rather than trusting that nothing rewrote the plugin.
    if ! readelf -SW "$bundled" | grep -q '\.note\.qt\.metadata'; then
        echo "!! $bundled lost its .note.qt.metadata section -- Qt will reject it as 'not a Qt plugin'" >&2
        fail=1
    fi

    out="$(ldd "$bundled" || true)"
    leak="$(echo "$out" | grep -F "$QT_ROOT" || true)"
    if [[ -n "$leak" ]]; then
        echo "!! $bundled still resolves against $QT_ROOT:" >&2
        echo "$leak" >&2
        fail=1
    fi
    while IFS= read -r soname; do
        [[ -n "$soname" ]] || continue
        if [[ -e "$QT_ROOT/lib/$soname" ]]; then
            echo "!! $(basename "$p") needs $soname, which Qt ships but was not bundled" >&2
            fail=1
        elif [[ -n "${sys_bundled[$soname]:-}" ]]; then
            echo "!! $(basename "$p") needs $soname, which this script bundles from the system path but which did not resolve" >&2
            fail=1
        else
            echo "   note: $(basename "$p") needs system library $soname (absent from this build image, expected on desktop targets)"
        fi
    done < <(echo "$out" | sed -n 's/^\s*\(\S*\) => not found$/\1/p' | sort -u)
done

[[ "$fail" -eq 0 ]] || exit 1

echo ":: Bundled $(find "$LIB_DIR" "$BUNDLE_DIR/plugins" \( -type f -o -type l \) | wc -l) files"
du -sh "$LIB_DIR" "$BUNDLE_DIR/plugins"
