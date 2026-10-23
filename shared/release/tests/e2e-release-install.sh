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

# The release model end to end: a real TuxBloxInstaller, a real HTTP server, a published tree laid
# out the way the CDN serves one.
#
# Everything else in this work is tested a function at a time. This is the only thing that proves
# the parts meet: that the installer resolves a release over HTTP, downloads what the release
# names, records what it installed, and -- on a second run against a release where one component
# moved -- fetches THAT COMPONENT ONLY. The last one is the whole point of the layout and the one
# claim no unit test can make, because it is about what crosses the network.
#
# A hand-built tree rather than `./build.sh --stage-only`: staging currently refuses, because the
# interface stack tarball predates per-component versions and carries no VERSION. Rebuilding it
# takes over an hour and proves nothing this does not.
#
# The launcher's half -- deciding coherent/stale and handing off -- is NOT here: it has no headless
# entry point, so driving it needs a display. test_updater.cpp covers that decision against a real
# file:// tree, fetches included.

set -u
cd "$(dirname "$0")/../../.."
Root=$PWD
Work=$(mktemp -d /tmp/tuxblox-e2e-XXXXXX)
ServerPid=""
cleanup() {
    [ -n "$ServerPid" ] && kill "$ServerPid" 2>/dev/null
    rm -rf "$Work"
}
trap cleanup EXIT

fails=0
fail() { printf '  FAIL: %s\n' "$1"; fails=$((fails + 1)); }
pass() { printf '  ok: %s\n' "$1"; }

Build=$Work/build
Cdn=$Work/cdn
Install=$Work/install

echo ":: Building the installer with the test seam"
# TUXBLOX_TEST_SETUP_URL is what lets a build be pointed at a local server. A shipped build never
# sets it -- see setupBaseUrl().
cmake -S "$Root/installer" -B "$Build" -DCMAKE_BUILD_TYPE=Release \
    -DTUXBLOX_TEST_SETUP_URL=ON \
    -DTUXBLOX_UI_STACK_DEV="$Root/shared/ui/dist/dev" > "$Work/cmake.log" 2>&1 \
    || { fail "cmake failed: $(tail -3 "$Work/cmake.log")"; printf ':: %d failure(s)\n' "$fails"; exit 1; }
cmake --build "$Build" -j"$(nproc)" --target TuxBloxInstaller > "$Work/build.log" 2>&1 \
    || { fail "build failed: $(tail -5 "$Work/build.log")"; printf ':: %d failure(s)\n' "$fails"; exit 1; }
Installer=$Build/TuxBloxInstaller
[ -x "$Installer" ] || { fail "no installer at $Installer"; printf ':: %d failure(s)\n' "$fails"; exit 1; }

# --- the published tree ---------------------------------------------------
# client/components/<name>/<version>/x86_64/<published name>, exactly as the server lays it out.
publish_flat() {   # name version content
    local dir="$Cdn/client/components/$1/$2/x86_64"
    mkdir -p "$dir"
    printf '#!/bin/sh\necho %s\n' "$2" > "$dir/$3"
    chmod 0755 "$dir/$3"
}
publish_archive() { # name version rootfile content
    local dir="$Cdn/client/components/$1/$2/x86_64" stage="$Work/stage-$1-$2"
    mkdir -p "$dir" "$stage/$(dirname "$3")"
    printf '%s' "$4" > "$stage/$3"
    [ "$3" = "main" ] && chmod 0755 "$stage/$3"
    tar --zstd -cf "$dir/$1.tar.zst" -C "$stage" .
}
sha_of() { sha256sum "$1" | cut -d' ' -f1; }
size_of() { stat -c %s "$1"; }

entry() {  # name version
    local file
    file=$(find "$Cdn/client/components/$1/$2/x86_64" -type f | head -1)
    printf '"%s": {"version": "%s", "arch": {"x86_64": {"sha256": "%s", "size": %s}, "arm64": null}}' \
        "$1" "$2" "$(sha_of "$file")" "$(size_of "$file")"
}

write_release() {  # release launcherVersion
    local dir="$Cdn/client/releases/stable"
    mkdir -p "$dir"
    {
        printf '{\n  "release": "%s",\n  "channel": "stable",\n  "changelog": "components",\n' "$1"
        printf '  "source_commit": "e2e", "source_dirty": false,\n  "components": {\n    '
        entry launcher "$2"; printf ',\n    '
        entry installer 3.1.0; printf ',\n    '
        entry studio-mcp 2.0.1; printf ',\n    '
        entry compat 3.3.0; printf ',\n    '
        entry libtuxblox 3.1.0
        printf '\n  }\n}\n'
    } > "$dir/$1.json"
    printf '%s\n' "$1" > "$dir/latest"
}

echo ":: Publishing release 3.6.0.0"
publish_flat launcher 3.5.0 TuxBloxLauncher
publish_flat installer 3.1.0 TuxBloxInstaller
publish_flat studio-mcp 2.0.1 studio-mcp
publish_archive compat 3.3.0 main '#!/bin/sh
echo 3.3.0
'
publish_archive libtuxblox 3.1.0 VERSION '3.1.0
'
write_release 3.6.0.0 3.5.0

echo ":: Serving it on 18099"
if ss -ltn 2>/dev/null | grep -q ':18099 '; then
    fail "port 18099 is already in use"
    printf ':: %d failure(s)\n' "$fails"; exit 1
fi
(cd "$Cdn" && python3 -m http.server 18099 --bind 127.0.0.1 > "$Work/server.log" 2>&1) &
ServerPid=$!
for _ in $(seq 1 50); do
    curl -fs -o /dev/null "http://127.0.0.1:18099/client/releases/stable/latest" && break
    sleep 0.1
done
export TUXBLOX_SETUP_URL=http://127.0.0.1:18099

# The pre-warm fetches Roblox's own installers from the real CDN and is explicitly allowed to fail,
# so an offline run still exercises everything this is about.
run_install() {   # $1 names the log only; it is not passed to the installer
    local label=$1; shift
    "$Installer" --headless --nolaunch --dir "$Install" --channel stable "$@" \
        > "$Work/install-$label.log" 2>&1
}

echo ":: A fresh install"
run_install first
if [ -f "$Install/TuxBloxLauncher" ] && [ -f "$Install/TuxBloxInstaller" ] \
   && [ -f "$Install/studio-mcp" ] && [ -f "$Install/compat/main" ] \
   && [ -f "$Install/libtuxblox/VERSION" ]; then
    pass "every component the release names landed"
else
    fail "a component is missing: $(ls -A "$Install" 2>/dev/null | tr '\n' ' ')"
fi

# The record is the install's identity and its "update completed" marker, byte for byte what the
# server served -- not a re-serialisation, so the launcher's coherence check compares against
# exactly what the release said.
if cmp -s "$Install/release.json" "$Cdn/client/releases/stable/3.6.0.0.json"; then
    pass "the install records the release, byte for byte"
else
    fail "release.json is absent or differs from what was published"
fi

echo ":: A release in which only the launcher moved"
: > "$Work/server.log"
publish_flat launcher 3.5.1 TuxBloxLauncher
write_release 3.6.0.1 3.5.1
run_install second

fetched=$(grep -ao 'GET /client/components/[^/]*' "$Work/server.log" | sed 's|.*/||' | sort -u | tr '\n' ' ')
if [ "$fetched" = "launcher " ]; then
    pass "one component changed, one component was fetched (${fetched% })"
else
    fail "expected only the launcher to be fetched, got: ${fetched:-nothing}"
fi

if [ "$("$Install/TuxBloxLauncher" --version 2>/dev/null)" = "3.5.1" ]; then
    pass "the launcher was replaced"
else
    fail "the launcher still reports $("$Install/TuxBloxLauncher" --version 2>/dev/null)"
fi
if cmp -s "$Install/release.json" "$Cdn/client/releases/stable/3.6.0.1.json"; then
    pass "the record moved to the new release"
else
    fail "the record was not updated to 3.6.0.1"
fi

echo ":: A release naming a component whose bytes are not served"
publish_flat launcher 9.9.9 TuxBloxLauncher
write_release 3.6.0.2 9.9.9
rm -rf "$Cdn/client/components/launcher/9.9.9"
if run_install missing; then
    fail "a release naming an unpublished component installed anyway"
else
    pass "a release naming an unpublished component is refused"
fi
if cmp -s "$Install/release.json" "$Cdn/client/releases/stable/3.6.0.1.json"; then
    pass "a refused install leaves the previous record alone"
else
    fail "a refused install changed what the install says it is"
fi

printf ':: %d failure(s)\n' "$fails"
[ "$fails" -eq 0 ]
