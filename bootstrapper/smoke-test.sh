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
# Runs the built bootstrapper on a machine with no GTK at all, laid out the way an install lays it out, and proves the interface libraries beside it bring everything it needs.
set -euo pipefail

BinaryPath="${1:-build/TuxBloxBootstrapper}"
StackPath="${2:-}"
Image="${TUXBLOX_SMOKE_IMAGE:-debian:11}"

if [[ ! -f "$BinaryPath" ]]; then
    printf 'No bootstrapper at %s. Build it with bootstrapper/build.sh first.\n' "$BinaryPath" >&2
    exit 1
fi
if [[ -z "$StackPath" || ! -f "$StackPath" ]]; then
    printf 'usage: %s <bootstrapper> <ui-stack.tar.zst>\n' "$0" >&2
    exit 2
fi

BinaryPath="$(realpath "$BinaryPath")"
StackPath="$(realpath "$StackPath")"

read -r -d '' InnerScript <<'INNER' || true
set -eu
export DEBIAN_FRONTEND=noninteractive
fail() { printf 'FAIL: %s\n' "$1"; exit 1; }

# Debian 11 is end of life, so its packages now live on the archive mirror.
if grep -q '^VERSION_ID="11"' /etc/os-release; then
    printf 'deb http://archive.debian.org/debian bullseye main\n' >/etc/apt/sources.list
    rm -f /etc/apt/sources.list.d/*
    printf 'Acquire::Check-Valid-Until "false";\n' >/etc/apt/apt.conf.d/99archive
fi
# libcurl and libarchive are the bootstrapper's own dynamic dependencies. The X11 client libraries (libxcursor1, libxcb-render0) are hard requirements of GTK's X11 backend and are deliberately not bundled.
apt-get update -qq
apt-get install -y -qq --no-install-recommends xvfb x11-utils procps ca-certificates zstd libcurl4 libarchive13 libxcursor1 libxcb-render0 >/dev/null

# Proof the machine has no toolkit of its own, so anything that resolves must have come from the stack.
if ldconfig -p | grep -qE 'libgtk-4|libadwaita|libglib-2.0|libpango-1|libcairo\.so'; then
    ldconfig -p | grep -E 'libgtk-4|libadwaita|libglib-2.0|libpango-1|libcairo\.so'
    fail 'the base image has a toolkit installed, so this test proves nothing. Fix the image, do not delete this check.'
fi
printf 'OK: the base image has no GTK, libadwaita, GLib, Pango or Cairo\n'

# TuxBlox refuses to run as root, so everything from here runs as an ordinary user.
useradd -m tester
home=/home/tester
root=/opt/tuxblox-smoke
install -d -o tester "$root"
install -o tester -m 0755 /TuxBloxBootstrapper "$root/TuxBloxBootstrapper"
install -d -o tester "$root/libtuxblox"
tar --zstd -xf /ui-stack.tar.zst -C "$root/libtuxblox"
chown -R tester "$root/libtuxblox"
run() { runuser -u tester -- env HOME="$home" "$@"; }

# Case A: without its libraries the bootstrapper must fail to load, so the launcher reads no version and repairs the install instead of trusting a broken one.
mv "$root/libtuxblox" "$root/libtuxblox.gone"
status=0
run "$root/TuxBloxBootstrapper" --version >/tmp/missing.txt 2>&1 || status=$?
mv "$root/libtuxblox.gone" "$root/libtuxblox"
test "$status" -ne 0 || fail 'it ran without its interface libraries'
grep -q 'error while loading shared libraries' /tmp/missing.txt || { cat /tmp/missing.txt; fail 'the failure does not name the loader'; }
printf 'OK: a missing libtuxblox folder fails to load (exit %s)\n' "$status"

# Case B: with them, the version is the x.y.z-channel string the launcher compares.
version="$(run "$root/TuxBloxBootstrapper" --version)"
[[ "$version" =~ ^[0-9]+\.[0-9]+\.[0-9]+-(stable|canary|experimental)$ ]] || fail "unexpected --version output: $version"
printf 'OK: --version prints %s\n' "$version"

# Case C: no display. The window cannot open, the worker must carry on, and a preview must still finish and exit cleanly.
status=0
run env -u DISPLAY -u WAYLAND_DISPLAY timeout 120 "$root/TuxBloxBootstrapper" --preview >/tmp/nodisplay.txt 2>&1 || status=$?
test "$status" -eq 0 || { cat /tmp/nodisplay.txt; fail "--preview with no display exited $status"; }
printf 'OK: --preview with no display finishes and exits 0\n'

# Case D: no display and a failing install. The error has to reach stderr, because the launcher captures that into the session log.
status=0
run env -u DISPLAY -u WAYLAND_DISPLAY \
    TUXBLOX_BOOTSTRAPPER_DOWNLOAD_SERVER=http://127.0.0.1:9 \
    TUXBLOX_BOOTSTRAPPER_DOWNLOAD_RBXHASH=version-0000000000000000 \
    TUXBLOX_BOOTSTRAPPER_INSTALL_DIR=/tmp/bs-install \
    timeout 120 "$root/TuxBloxBootstrapper" --install >/tmp/err.txt 2>&1 || status=$?
test "$status" -eq 1 || { cat /tmp/err.txt; fail "--install with no display and no server exited $status, not 1"; }
grep -q '^TuxBlox: ' /tmp/err.txt || { cat /tmp/err.txt; fail 'the failure was not reported on stderr'; }
printf 'OK: a failed install with no window reports on stderr and exits 1\n'

# Case E: a real window, on a display.
rm -rf "$home/.cache"
Xvfb :99 -screen 0 1280x800x24 >/tmp/xvfb.log 2>&1 &
sleep 2
export DISPLAY=:99
run env DISPLAY=:99 "$root/TuxBloxBootstrapper" --preview >/tmp/bootstrapper.log 2>&1 &

pid=""
for _ in $(seq 1 30); do
    pid="$(pgrep -u tester -f "^$root/TuxBloxBootstrapper" | head -n 1 || true)"
    if [[ -n "$pid" ]] && xwininfo -root -tree 2>/dev/null | grep -qE '"TuxBlox".*[0-9]+x[0-9]+\+'; then break; fi
    sleep 1
done
test -n "$pid" || { cat /tmp/bootstrapper.log; fail 'the bootstrapper is not running'; }
xwininfo -root -tree | tee /tmp/tree.txt
grep -qE '"TuxBlox".*[0-9]+x[0-9]+\+' /tmp/tree.txt || { cat /tmp/bootstrapper.log; fail 'the bootstrapper window is not on the display'; }
printf 'OK: the window is on the display (pid %s)\n' "$pid"

ldd "$root/TuxBloxBootstrapper" | tee /tmp/deps.txt
if grep -q 'not found' /tmp/deps.txt; then fail 'unresolved libraries'; fi

# Every resolved library must sit in the stack or be a known host library: the bootstrapper's own libcurl and libarchive and the X11 client libraries with what they pull in, and the C and C++ runtimes.
hostOk="$(cd /usr/lib/x86_64-linux-gnu && ldd libcurl.so.4 libarchive.so.13 libX11.so.6 libXcursor.so.1 libXi.so.6 libxcb-render.so.0 | awk '/=>/ {print $1}' | sort -u)"
hostPattern='^(libc\.so\.6|libm\.so\.6|libdl\.so\.2|libpthread\.so\.0|librt\.so\.1|libresolv\.so\.2|libstdc\+\+\.so\.6|libgcc_s\.so\.1|libcurl\.so\.4|libarchive\.so\.13|libX[a-zA-Z0-9]*\.so\.[0-9]+|libxcb[a-z-]*\.so\.[0-9]+|libz\.so\.1|libexpat\.so\.1|libpng16\.so\.16|libpcre2-8\.so\.0)$'
stray=""
while read -r name arrow path _; do
    [[ "$arrow" == "=>" ]] || continue
    [[ "$path" == "$root/libtuxblox/"* ]] && continue
    if [[ "$name" =~ $hostPattern ]] || grep -qxF "$name" <<<"$hostOk"; then continue; fi
    stray+="$name => $path"$'\n'
done < /tmp/deps.txt
if [[ -n "$stray" ]]; then
    printf 'these libraries resolved outside the stack and are not known host libraries:\n%s' "$stray"
    fail 'stray libraries'
fi
for lib in libgtk-4 libadwaita libglib-2.0 libgobject-2.0 libgio-2.0 libpango-1.0 libcairo.so libgdk_pixbuf libharfbuzz libfontconfig libfreetype; do
    grep -q "$lib.*=> $root/libtuxblox/" /tmp/deps.txt || fail "$lib is not resolved from the libtuxblox folder"
done
printf 'OK: every library outside the host allow-list resolves from the libtuxblox folder\n'

# The running process must not have mapped a library from outside the stack that the stack also ships, which covers modules loaded at run time such as pixbuf loaders and pango modules. Read as the same user: root in a container may not look inside another user's process.
maps="$(runuser -u tester -- cat "/proc/$pid/maps")"
mapped="$(grep -c "$root/libtuxblox/" <<<"$maps" || true)"
test "$mapped" -gt 0 || fail 'the running bootstrapper maps nothing from the libtuxblox folder'
shipped="$(ls "$root/libtuxblox/lib/x86_64-linux-gnu" | sed -E 's/(\.so\.[0-9]+).*/\1/' | sort -u)"
outside=""
while read -r file; do
    soname="$(basename "$file" | sed -E 's/(\.so\.[0-9]+).*/\1/')"
    grep -qxF "$soname" <<<"$hostOk" && continue
    grep -qxF "$soname" <<<"$shipped" && outside+="$file"$'\n'
done < <(awk '{print $6}' <<<"$maps" | grep -E '\.so' | grep -v "^$root/libtuxblox/" | sort -u)
if [[ -n "$outside" ]]; then
    printf '%s\n' "$outside"
    fail 'the running bootstrapper mapped libraries from outside the stack that the stack also ships'
fi
printf 'OK: the running bootstrapper maps %s files from ui and nothing shadowing them\n' "$mapped"

# Missing schemas show up as GLib-GIO critical warnings, and mean settings such as the colour scheme silently do nothing.
if grep -q 'GLib-GIO-CRITICAL' /tmp/bootstrapper.log; then
    cat /tmp/bootstrapper.log
    fail 'GLib could not find its settings schemas'
fi
printf 'OK: no GLib-GIO criticals\n'

# Fontconfig writes a cache of every directory it scanned, so a cache naming the bundled font proves the bundled configuration was found.
fontFound=""
for _ in $(seq 1 20); do
    if grep -qa 'Adwaita Sans' "$home"/.cache/tuxblox/fontconfig/*.cache-* 2>/dev/null; then fontFound=1; break; fi
    sleep 1
done
test -n "$fontFound" || fail 'fontconfig never scanned the bundled font'
printf 'OK: Adwaita Sans is in the fontconfig cache\n'

pkill -u tester -f "^$root/TuxBloxBootstrapper" || true
printf 'SMOKE OK\n'
INNER

podman run --rm --init -v "$BinaryPath:/TuxBloxBootstrapper:ro" -v "$StackPath:/ui-stack.tar.zst:ro" "$Image" bash -c "$InnerScript"
