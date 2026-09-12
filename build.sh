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

# Builds everything, in order: installer, launcher, proton. All output lands in build/

set -eo pipefail
cd "$(dirname "$0")"

export ROOT="$(pwd)"
export BUILD_LOG="$ROOT/build.log"

debug=1
force_deps=0
log_enabled=0
stage_only=0
for arg in "$@"; do
    case "$arg" in
        --nodebug) debug=0 ;;
        --reinstall-deps) force_deps=1 ;;
        --log) log_enabled=1 ;;
        # Re-publish what is already in build/ without rebuilding it. Useful
        # after the release folder is deleted or its metadata needs redoing:
        # everything under releases/ is derived from build/, so it can always
        # be regenerated as long as build/ is intact.
        --stage-only) stage_only=1 ;;
    esac
done

# The one version file for the whole repo: line 1 the version, line 2 the
# channel. The launcher, the installer and the compatibility layer all take
# their version from here (via TUXBLOX_BUILD_VERSION, which their own build.sh
# forwards into the build container), so one build cannot stamp three different
# numbers into the three halves of one release.
#
# It is also rewritten at the end of this block with whatever this build used,
# so it stays current on its own and never needs editing by hand.
version_file="$ROOT/VERSION"
file_version=""
file_channel=""
if [[ -r "$version_file" ]]; then
    file_version="$(sed -n '1p' "$version_file" | tr -d '[:space:]')"
    file_channel="$(sed -n '2p' "$version_file" | tr -d '[:space:]')"
fi

# An explicit TUXBLOX_BUILD_VERSION means "build exactly this" and is never
# second-guessed. Otherwise the prompt offers what VERSION already holds, so
# bumping is a matter of typing the new number rather than editing a file, and
# pressing Enter rebuilds the same version. -t 0 keeps a non-interactive run
# (CI, a pipe, nohup) on the file's value instead of blocking on a prompt it
# cannot answer.
# A --stage-only run publishes what is ALREADY in build/, so the version has
# to come from the artifact itself: the compiled layer reports the number that
# was baked into it, while VERSION may have been bumped since that build.
# Publishing binaries under a version they do not themselves report is exactly
# the mismatch this avoids -- and it is why this run neither prompts nor writes
# VERSION back. An explicit TUXBLOX_BUILD_VERSION still wins, for the case
# where you really do mean to publish under a different number.
if [[ $stage_only -eq 1 && -z "$TUXBLOX_BUILD_VERSION" && -x "$ROOT/build/compat/main" ]]; then
    built_version="$("$ROOT/build/compat/main" --version 2>/dev/null | head -1 | tr -d '[:space:]')"
    if [[ -n "$built_version" ]]; then
        if [[ -n "$file_version" && "$file_version" != "$built_version" ]]; then
            echo "!! VERSION says $file_version, but build/ was built as $built_version." >&2
            echo "!! Staging as $built_version, which is what these binaries report." >&2
        fi
        TUXBLOX_BUILD_VERSION="$built_version"
    fi
fi

if [[ -z "$TUXBLOX_BUILD_VERSION" ]]; then
    if [[ -t 0 && $stage_only -eq 0 ]]; then
        read -rp "Enter version for this build${file_version:+ ($file_version)}: " TUXBLOX_BUILD_VERSION
        TUXBLOX_BUILD_VERSION="${TUXBLOX_BUILD_VERSION:-$file_version}"
        while [[ -z "$TUXBLOX_BUILD_VERSION" ]]; do
            read -rp "Version cannot be empty. Enter version for this build: " TUXBLOX_BUILD_VERSION
        done
    else
        TUXBLOX_BUILD_VERSION="$file_version"
    fi
fi

if [[ -z "$TUXBLOX_BUILD_VERSION" ]]; then
    echo "!! No version to build: set TUXBLOX_BUILD_VERSION or put one on line 1 of VERSION." >&2
    exit 1
fi

# Same three-way resolution as the version above. An empty answer takes
# whatever VERSION holds, falling back to stable, since the channel picks which
# releases/ folder this build is published under.
if [[ -z "$TUXBLOX_CHANNEL" ]]; then
    default_channel="${file_channel:-stable}"
    if [[ -t 0 && $stage_only -eq 0 ]]; then
        read -rp "Enter channel for this build [stable/canary/dev] ($default_channel): " TUXBLOX_CHANNEL
        TUXBLOX_CHANNEL="${TUXBLOX_CHANNEL:-$default_channel}"
        while [[ ! "$TUXBLOX_CHANNEL" =~ ^(stable|canary|dev)$ ]]; do
            read -rp "Channel must be stable, canary or dev: " TUXBLOX_CHANNEL
            TUXBLOX_CHANNEL="${TUXBLOX_CHANNEL:-$default_channel}"
        done
    else
        TUXBLOX_CHANNEL="$default_channel"
    fi
fi

case "$TUXBLOX_CHANNEL" in
    stable|canary|dev) ;;
    *)
        echo "!! Unknown channel \"$TUXBLOX_CHANNEL\" -- expected stable, canary or dev" >&2
        exit 1
        ;;
esac

# Written back before anything is built, not after: a build that fails halfway
# still leaves VERSION agreeing with what was attempted, and a re-run then
# offers that same number as its default rather than silently reverting to the
# last one that happened to succeed.
#
# Not done for --stage-only: that run publishes an existing build rather than
# deciding what to build, so it has no business changing what the next build
# will be.
if [[ $stage_only -eq 0 ]] &&
   [[ "$file_version" != "$TUXBLOX_BUILD_VERSION" || "$file_channel" != "$TUXBLOX_CHANNEL" ]]; then
    printf '%s\n%s\n' "$TUXBLOX_BUILD_VERSION" "$TUXBLOX_CHANNEL" > "$version_file"
    echo ":: Updated VERSION to $TUXBLOX_BUILD_VERSION $TUXBLOX_CHANNEL"
fi

# And committed, so the number a build was published under is recorded rather
# than left as a stray modification for someone to notice later. Only VERSION
# is committed -- "git commit -- <path>" takes that path alone and ignores
# whatever else is staged -- and nothing is ever pushed. Checked against HEAD
# rather than against the write above, so a VERSION that was edited by hand
# before the build gets committed too. Set TUXBLOX_NO_VERSION_COMMIT=1 to skip.
if [[ $stage_only -eq 0 && -z "${TUXBLOX_NO_VERSION_COMMIT:-}" ]] &&
   git -C "$ROOT" rev-parse --is-inside-work-tree >/dev/null 2>&1 &&
   ! git -C "$ROOT" diff --quiet HEAD -- "$version_file" 2>/dev/null; then
    if git -C "$ROOT" commit -q -m "Change version to $TUXBLOX_BUILD_VERSION" \
            -- "$version_file" 2>/dev/null; then
        echo ":: Committed VERSION as $TUXBLOX_BUILD_VERSION $TUXBLOX_CHANNEL"
    else
        echo "!! Could not commit VERSION -- left it modified for you to commit." >&2
    fi
fi

if [[ $stage_only -eq 1 ]]; then
    # Checked before anything is announced, so a run with nothing to publish
    # says so instead of first claiming it is staging from a build/ that is
    # not there.
    if [[ ! -d build ]]; then
        echo "!! --stage-only publishes what is in build/, and there is no build/ to publish." >&2
        echo "!! Run ./build.sh without --stage-only first." >&2
        exit 1
    fi
    echo ":: Staging TuxBlox $TUXBLOX_BUILD_VERSION ($TUXBLOX_CHANNEL) from build/"
else
    echo ":: Building TuxBlox $TUXBLOX_BUILD_VERSION ($TUXBLOX_CHANNEL)"
fi
export TUXBLOX_BUILD_VERSION TUXBLOX_CHANNEL

packages=(
    python3
    podman
    curl
    gcc
    uidmap
    git
    zstd
)

JOBS="${TUXBLOX_MAKE_JOBS:-$(nproc 2>/dev/null || echo 1)}"

ORANGE='\e[38;5;208m'
BLUE='\e[34m'
RESET='\e[0m'

step() {
    echo -e "${BLUE}:: $1${RESET}"
}

run_step() {
    local label="$1"
    local allow_failure="$2"
    shift 2
    local start_time end_time elapsed minutes seconds status

    start_time=$(date +%s)

    set +e
    if [[ $debug -eq 1 ]]; then
        "$@"
    else
        "$@" >/dev/null
    fi
    status=$?
    set -e

    end_time=$(date +%s)
    elapsed=$((end_time - start_time))
    minutes=$((elapsed / 60))
    seconds=$((elapsed % 60))

    printf "Task completed in: %02d minutes, %02d seconds\n" "$minutes" "$seconds"
    sleep 1

    if [[ $status -ne 0 ]]; then
        if [[ "$allow_failure" == "allow-fail" ]]; then
            echo "!! Step '$label' failed as expected (exit $status) — continuing to next step." >&2
        else
            echo "" >&2
            echo "!! Step '$label' failed (exit $status)" >&2
            report_failure_cause
            exit "$status"
        fi
    fi
}

report_failure_cause() {
    if [[ ! -f "$BUILD_LOG" ]]; then
        echo "!! No build.log to inspect (re-run with --log to capture one)." >&2
        return
    fi

    local pattern='error:|undefined reference|fatal error|No such file|cannot find|ld returned|segmentation fault|core dumped|killed|signal 1[0-9]|Error [0-9]+$'
    local total_lines last_match_from_end last_match_line

    total_lines=$(wc -l < "$BUILD_LOG")
    last_match_from_end=$(tac "$BUILD_LOG" | grep -n -E -i -m1 "$pattern" | cut -d: -f1)

    if [[ -z "$last_match_from_end" ]]; then
        echo "!! Could not isolate a specific error pattern. Last 60 lines of build.log:" >&2
        tail -n 60 "$BUILD_LOG" >&2
        return
    fi

    last_match_line=$((total_lines - last_match_from_end + 1))

    echo "!! Likely root cause (build.log line $last_match_line), with context:" >&2
    echo "----------------------------------------------------------------------" >&2
    sed -n "$((last_match_line > 5 ? last_match_line - 5 : 1)),$((last_match_line + 15))p" "$BUILD_LOG" >&2
    echo "----------------------------------------------------------------------" >&2
    echo "!! Full log at build.log if you need more context." >&2
}

logged() {
    if [[ $log_enabled -eq 1 ]]; then
        "$@" 2>&1 | tee -a "$BUILD_LOG"
    else
        "$@"
    fi
}

apply_patches() {
    echo ":: Reloading submodules to their recorded commit"
    git submodule update --init --force

    local patches_dir="compat/patches"

    if [[ ! -d "$patches_dir" ]]; then
        return 0
    fi

    # The patches themselves are applied by the Proton build, onto its own copy
    # of each submodule under build/.artifacts/proton/src-<package>, right after
    # it syncs that copy -- never onto the submodule checkout, which therefore
    # stays clean and never has to be committed into. See
    # compat/make/rules-source.mk.
    #
    # What is left to do here is check that every patch directory names a
    # package the build actually has, because one that does not is silently
    # never applied, and a patch that quietly does nothing is worse than a
    # build that stops.
    local known
    known="$(sed -n 's/.*rules-source,\([^,]*\),.*/\1/p' compat/Makefile.in | sort -u)"

    shopt -s nullglob
    local patch_dir name count=0 bad=0
    for patch_dir in "$patches_dir"/*/; do
        name="$(basename "$patch_dir")"

        if [[ "$name" == "wine" ]]; then
            echo "!! patches/wine is not supported: wine is a maintained fork, patched directly in compat/wine" >&2
            bad=1
            continue
        fi

        if ! grep -qx "$name" <<<"$known"; then
            echo "!! patches/$name matches no package in the build -- it would never be applied." >&2
            echo "!! Patch directories are named after the build's package (the src-<name> folder)," >&2
            echo "!! which is not always the submodule folder name. Known packages:" >&2
            echo "$known" | sed 's/^/!!   /' >&2
            bad=1
            continue
        fi

        count=$(( count + $(find "$patch_dir" -type f | wc -l) ))
        echo ":: patches/$name will be overlaid onto src-$name ($(find "$patch_dir" -type f | wc -l) file(s))"
    done
    shopt -u nullglob

    if [[ $bad -ne 0 ]]; then
        return 1
    fi

    echo ":: $count patch file(s) queued, applied by the Proton build into build/.artifacts/"
}

# Publishes what this build produced into releases/<channel>/<version>/, in the
# shape setup.tuxblox.net serves it, so the server can sync straight from this
# folder. It is TWO sync paths, not one: releases/<channel>/ mirrors
# /v1/<channel>/, while releases/latest.json is published at /v2/latest.json.
#
# build/.artifacts/ (build scratch) and build/runtime/ (the virtual drive, which
# can hold a logged-in Roblox session) are deliberately never copied here.
stage_release() {
    local version="$TUXBLOX_BUILD_VERSION"
    local channel="$TUXBLOX_CHANNEL"
    # Dots are legal in a URL path, but every published artifact has used the
    # dashed form since the first release -- keep it, so an existing mirror
    # does not end up holding both spellings of the same file.
    local slug="${version//./-}"
    local release_dir="$ROOT/releases/$channel/$version"
    local url_prefix="/v1/$channel/$version"

    # Absent means the build predates this file; recorded as unknown-and-dirty
    # so deploy.sh refuses it rather than publishing an unattributable build.
    local source_commit="" source_dirty="true"
    if [[ -r "$ROOT/build/.provenance" ]]; then
        { read -r source_commit || true; read -r source_dirty || true; } < "$ROOT/build/.provenance"
    fi

    # The launcher's Qt6 bundle is three entries, not one directory that
    # happens to exist -- an empty libtuxblox/ would tar up fine and fail at
    # the user's machine instead.
    local required=(compat/main libtuxblox/lib libtuxblox/plugins libtuxblox/qt.conf
                    TuxBloxLauncher TuxBloxInstaller mcp.sh)
    local entry
    for entry in "${required[@]}"; do
        if [[ ! -e "$ROOT/build/$entry" ]]; then
            echo "!! build/$entry is missing, so this build cannot be published." >&2
            return 1
        fi
    done

    # Rebuilding a version replaces its folder outright. Merging into it would
    # leave the previous run's tarball sitting next to a manifest that no
    # longer describes it, which the installer would reject as a checksum
    # mismatch only after the user had downloaded the whole thing.
    rm -rf "$release_dir"
    mkdir -p "$release_dir"

    # Both archives are packed with their contents at the ROOT, no wrapper
    # directory: the installer extracts an archive artifact into
    # installDir/<path>/<filename>, so a wrapper would nest the payload one
    # level too deep (compat/compat/main).
    echo ":: Packing the compatibility layer"
    tar --zstd -cf "$release_dir/compat-$slug.tar.zst" -C "$ROOT/build/compat" .
    echo ":: Packing the TuxBlox libraries"
    tar --zstd -cf "$release_dir/libtuxblox-$slug.tar.zst" -C "$ROOT/build/libtuxblox" .

    # The launcher, installer and MCP helper ship unpacked. They are copied
    # under the basename the manifest's url gives them, which is how the
    # server serves them and therefore what a plain rsync of this folder has
    # to find on disk.
    cp "$ROOT/build/TuxBloxLauncher" "$release_dir/launcher"
    cp "$ROOT/build/TuxBloxInstaller" "$release_dir/installer"
    cp "$ROOT/build/mcp.sh" "$release_dir/mcp.sh"

    echo ":: Writing manifest.json and latest.json"
    # Written by python rather than assembled from shell heredocs: it hashes
    # and stats the files it is describing, so a size or checksum cannot drift
    # from the artifact it belongs to, and latest.json is a read-modify-write
    # of a file the other channels also have entries in.
    python3 - "$release_dir" "$ROOT/releases/latest.json" "$channel" "$url_prefix" "$slug" \
             "$source_commit" "$source_dirty" <<'PY'
import hashlib, json, os, sys
from datetime import datetime, timezone

release_dir, latest_path, channel, url_prefix, slug = sys.argv[1:6]
source_commit, source_dirty = sys.argv[6:8]

# key -> (file on disk, displayname, filename the installer installs it as).
# "filename" is extension-less by convention: for an archive it names the
# directory the archive is extracted into, for a flat file it is the name the
# download is saved under.
artifacts = [
    ("launcher",   "launcher",                    "Launcher",             "TuxBloxLauncher"),
    ("installer",  "installer",                   "Updater",              "TuxBloxInstaller"),
    # Published as "proton" until the layer was renamed. The launcher reads
    # either spelling, so emitting the new one is safe for older installs.
    ("compat",     f"compat-{slug}.tar.zst",      "Compatibility layer",  "compat"),
    ("libtuxblox", f"libtuxblox-{slug}.tar.zst",  "Libraries",            "libtuxblox"),
    ("mcp",        "mcp.sh",                      "Studio MCP",           "mcp.sh"),
]

def digest(path):
    h = hashlib.sha256()
    with open(path, "rb") as f:
        for chunk in iter(lambda: f.read(1 << 20), b""):
            h.update(chunk)
    return h.hexdigest()

stamp = datetime.now(timezone.utc).strftime("%m/%d/%y %H:%M:%S")

entries = {}
for key, filename, displayname, install_as in artifacts:
    path = os.path.join(release_dir, filename)
    # The size and hash are of the file that is actually downloaded, which for
    # the two archives means the compressed tarball, not the tree inside it.
    entries[key] = {
        "size": os.path.getsize(path),
        "sha256": digest(path),
        "url": f"{url_prefix}/{filename}",
        "displayname": displayname,
        "filename": install_as,
        "path": "/",
    }

manifest = {
    "channel": channel,
    "uploadDate": stamp,
    "data": {"hasPlayer": False, "hasStudio": True, "isLatest": True},
    "manifest_version": 2,
    # The commit this build was made from. Unknown keys are ignored by the
    # launcher's parser, so this needs no manifest_version bump.
    "source_commit": source_commit or None,
    "source_dirty": source_dirty == "true",
    "artifacts": entries,
}

with open(os.path.join(release_dir, "manifest.json"), "w") as f:
    json.dump(manifest, f, indent=2)
    f.write("\n")

# Only this build's channel moves. Building canary must not disturb which
# version stable points at, so the other entries are read back and kept.
latest = {"channels": {"stable": "", "canary": "", "dev": ""}}
try:
    with open(latest_path) as f:
        existing = json.load(f)
    if isinstance(existing.get("channels"), dict):
        latest["channels"].update(existing["channels"])
except (FileNotFoundError, ValueError):
    pass

latest["channels"][channel] = os.path.basename(release_dir)
latest["lastUpdate"] = stamp

with open(latest_path, "w") as f:
    json.dump(latest, f, indent=2)
    f.write("\n")
PY

    echo ":: Published to releases/$channel/$version/"
    du -h "$release_dir"/* | sed 's/^/   /'
}

# Records the commit this build is made from. stage_release() stamps it into
# manifest.json, which is what lets deploy.sh refuse to publish a binary whose
# source is not public -- an LGPL obligation, and it also tells anyone holding
# a download exactly which source built it.
#
# Written at build time, not stage time: --stage-only republishes an older
# build/, so reading HEAD then would describe a commit that build never saw.
record_provenance() {
    local commit dirty
    commit="$(git -C "$ROOT" rev-parse HEAD 2>/dev/null || true)"
    if [[ -n "$(git -C "$ROOT" status --porcelain 2>/dev/null)" ]]; then
        dirty=true
    else
        dirty=false
    fi
    printf '%s\n%s\n' "$commit" "$dirty" > "$ROOT/build/.provenance"
}

# Copies include/ over build/, the last thing to land in a build. Shared, so a
# --stage-only run ships exactly the files a full build would.
copy_include() {
    if [[ -d "$ROOT/include" ]]; then
        cp -a "$ROOT/include/." "$ROOT/build/"
    fi
}

# Everything below this point builds, and the first thing it does is wipe
# build/ -- which is precisely what a --stage-only run must not do, since
# build/ is the input it publishes from. So that run ends here, before any of
# it, having touched nothing but releases/.
if [[ $stage_only -eq 1 ]]; then
    # include/ holds shipped files that are copied, never compiled, so
    # refreshing them is not a rebuild -- and without this a --stage-only run
    # would republish whatever copy the last full build happened to leave in
    # build/, silently ignoring anything edited in include/ since.
    echo ":: Copying include/ into build/"
    copy_include
    stage_release
    echo -e "Staged TuxBlox $TUXBLOX_BUILD_VERSION ($TUXBLOX_CHANNEL) into releases/"
    exit 0
fi

step "Cleaning up previous build logs"
rm -f "$BUILD_LOG"

step "Cleaning up old build output"
rm -rf build
mkdir -p build/.artifacts build/runtime
record_provenance

step "Updating dependencies"

declare -A override_apt=()

declare -A override_dnf=(
    [uidmap]="shadow-utils"
)
declare -A override_pacman=(
    [uidmap]="shadow"
)
declare -A override_brew=(
    [python3]="python@3"
    [uidmap]="podman"
)
declare -A override_apk=()

detect_pm() {
    if command -v apt-get &>/dev/null; then echo "apt"
    elif command -v dnf &>/dev/null; then echo "dnf"
    elif command -v pacman &>/dev/null; then echo "pacman"
    elif command -v brew &>/dev/null; then echo "brew"
    elif command -v apk &>/dev/null; then echo "apk"
    else echo "unknown"
    fi
}

pkg_name() {
    local pm="$1" pkg="$2"
    local -n override_map="override_${pm}"
    echo "${override_map[$pkg]:-$pkg}"
}

is_installed() {
    local pm="$1" pkg="$2"
    case "$pm" in
        apt)    dpkg -s "$pkg" &>/dev/null ;;
        dnf)    rpm -q "$pkg" &>/dev/null ;;
        pacman) pacman -Qi "$pkg" &>/dev/null ;;
        brew)   brew list "$pkg" &>/dev/null ;;
        apk)    apk info -e "$pkg" &>/dev/null ;;
    esac
}

pm=$(detect_pm)
echo ":: Detected package manager: $pm"
to_install=()
for pkg in "${packages[@]}"; do
    resolved_pkg="$(pkg_name "$pm" "$pkg")"
    if is_installed "$pm" "$resolved_pkg"; then
        echo ":: $resolved_pkg already installed "
    else
        to_install+=("$resolved_pkg")
    fi
done

if [[ ${#to_install[@]} -eq 0 && $force_deps -eq 0 && -t 0 ]]; then
    read -rp ":: All dependencies already installed. Reinstall/check for updates anyway? [y/N] " reinstall_choice || true
    [[ "$reinstall_choice" =~ ^[Yy]$ ]] && force_deps=1
fi

if [[ ${#to_install[@]} -gt 0 || $force_deps -eq 1 ]]; then
    [[ $force_deps -eq 1 ]] && to_install=("${packages[@]}")
    case "$pm" in
        apt)
            sudo apt-get update
            sudo apt-get install -y "${to_install[@]}"
            ;;
        dnf)
            sudo dnf install -y "${to_install[@]}"
            ;;
        pacman)
            sudo pacman -Sy --needed --noconfirm "${to_install[@]}"
            ;;
        brew)
            brew update
            brew install "${to_install[@]}"
            ;;
        apk)
            sudo apk update
            sudo apk add "${to_install[@]}"
            ;;
        *)
            echo "!! Unsupported or undetected package manager. Install manually: ${packages[*]}"
            exit 1
            ;;
    esac
else
    echo ":: All dependencies satisfied, skipping package manager."
fi

step "Checking rootless podman and warming the old-glibc builder image"
run_step "check_podman" strict bash -c 'podman info >/dev/null && podman build -t tuxblox-old-glibc-builder -f Containerfile .'

step "Reloading submodules and applying patches"
run_step "apply_patches" strict apply_patches

step "Building TuxBlox Installer (podman, old-glibc baseline)"
run_step "build_installer" strict logged env TUXBLOX_SKIP_DEPS=1 ./installer/build.sh

step "Staging installer output into build/"

rm -f build/TuxBloxInstaller
mv installer/build build/.artifacts/installer
mv build/.artifacts/installer/TuxBloxInstaller build/TuxBloxInstaller

step "Building TuxBlox Launcher (podman, old-glibc baseline)"
run_step "build_launcher" strict logged env TUXBLOX_SKIP_DEPS=1 ./launcher/build.sh

step "Staging launcher output into build/"

rm -f build/TuxBloxLauncher
rm -rf build/libtuxblox
mv launcher/build build/.artifacts/launcher
mv build/.artifacts/launcher/TuxBloxLauncher build/TuxBloxLauncher

mv build/.artifacts/launcher/libtuxblox build/libtuxblox

PROTON_BUILD_DIR="$ROOT/build/.artifacts/proton"
mkdir -p "$PROTON_BUILD_DIR"
cd "$PROTON_BUILD_DIR"

step "Configuring Proton (ccache enabled for faster rebuilds)"
run_step "configure_proton" strict logged "$ROOT/compat/configure.sh" --enable-ccache

step "First-pass build (1/4) (using $JOBS parallel jobs)"
run_step "first_pass_build" allow-fail logged make -j"$JOBS"

step "Fetching external sources (2/4)"
run_step "fetch_external_sources" strict bash -c 'cd src-glslang && rm -rf External/spirv-tools External/googletest && python3 update_glslang_sources.py'

step "Initializing nested submodules (3/4)"
run_step "init_submodules" strict bash -c 'cd "$ROOT/compat/submodules/dxvk-nvapi" && git submodule update --init --recursive'

step "Ensuring wine x86_64 is configured"
run_step "configure_wine_x86_64" strict logged make wine-x86_64-configure

step "Ensuring x86_64 NLS data is built"
run_step "build_x86_64_nls" strict bash -c 'cd obj-wine-x86_64 && make nls/locale.nls'

step "Resuming build (4/4) (using $JOBS parallel jobs)"
run_step "resume_build" strict logged make -j"$JOBS"

step "Compiling proton launcher"
run_step "compile_proton_native" strict bash -c '
    set -e
    podman build -t tuxblox-old-glibc-builder -f "$ROOT/Containerfile" "$ROOT"

    workdir="$(mktemp -d)"
    mkdir -p "$workdir/src" "$workdir/out"
    cp -a "$ROOT"/compat/tuxblox/src/. "$workdir/src/"
    "$ROOT/compat/tuxblox/tools/embed-data.py" "$ROOT/compat/tuxblox/data" "$workdir/src/embedded_data.h"

    # Run through sh so the *.cpp glob is expanded inside the container.
    podman run --rm --userns=keep-id -v "$workdir:/work:Z" -w /work/src \
        -e TUXBLOX_BUILD_VERSION -e TUXBLOX_CHANNEL \
        tuxblox-old-glibc-builder sh -c \
        '"'"'g++ -std=c++17 -O2 -Wall -Wextra -I. \
            -DTUXBLOX_VERSION="\"$TUXBLOX_BUILD_VERSION\"" \
            -DTUXBLOX_CHANNEL="\"$TUXBLOX_CHANNEL\"" \
            -o /work/out/main $(find . -name "*.cpp")'"'"'

    install -m 755 "$workdir/out/main" dist/main
    rm -rf "$workdir"
'

step "Clearing up unnecessary junk"
shopt -s nullglob
if [ -z "$TUXBLOX_KEEP_OBJ" ]; then
    rm -rf obj-* dst-*
fi
shopt -u nullglob

cd "$ROOT"

step "Staging Proton"
mv "$PROTON_BUILD_DIR/dist" build/compat

step "Copying licenses into Proton"
cp -a LICENSE build/compat/LICENSE
rm -rf build/compat/third_party_licenses
cp -a third_party_licenses build/compat/third_party_licenses

step "Copying include/ into build/"
copy_include

# Last, so it only ever describes a build that got all the way here. build/ is
# left exactly as it is either way -- this publishes a copy, it does not move
# anything out.
step "Publishing to releases/$TUXBLOX_CHANNEL/$TUXBLOX_BUILD_VERSION/"
run_step "stage_release" strict stage_release

echo -e "Successfully built TuxBlox!"
