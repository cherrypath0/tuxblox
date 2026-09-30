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

# Runs the built launcher on a machine with no GTK at all, laid out the way an install lays it out, and proves the interface libraries beside it bring everything it needs.
set -euo pipefail

BinaryPath="${1:-build/TuxBloxLauncher}"
StackPath="${2:-}"
Image="${TUXBLOX_SMOKE_IMAGE:-debian:11}"

if [[ ! -f "$BinaryPath" ]]; then
    printf 'No launcher at %s. Build it with launcher/build.sh first.\n' "$BinaryPath" >&2
    exit 1
fi
if [[ -z "$StackPath" || ! -f "$StackPath" ]]; then
    printf 'usage: %s <launcher> <ui-stack.tar.zst>\n' "$0" >&2
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
# libcurl and libarchive are the launcher's own dynamic dependencies. The X11 client libraries are hard requirements of GTK's X11 backend and are deliberately not bundled.
apt-get update -qq
apt-get install -y -qq --no-install-recommends xvfb x11-utils x11-xserver-utils procps ca-certificates zstd libcurl4 libarchive13 libxcursor1 libxcb-render0 >/dev/null
# The update check must fail at once rather than reach the real server, which could hand off to an installer mid-test
printf '127.0.0.1 setup.tuxblox.net\n' >>/etc/hosts

# Proof the machine has no toolkit of its own, so anything that resolves must have come from the stack.
if ldconfig -p | grep -qE 'libgtk-4|libadwaita|libglib-2.0|libpango-1|libcairo\.so|libQt6'; then
    ldconfig -p | grep -E 'libgtk-4|libadwaita|libglib-2.0|libpango-1|libcairo\.so|libQt6'
    fail 'the base image has a toolkit installed, so this test proves nothing. Fix the image, do not delete this check.'
fi
printf 'OK: the base image has no GTK, libadwaita, GLib, Pango, Cairo or Qt\n'

# TuxBlox refuses to run as root, so everything from here runs as an ordinary user.
useradd -m tester
home=/home/tester
root=/opt/tuxblox-smoke
install -d -o tester "$root"
install -o tester -m 0755 /TuxBloxLauncher "$root/TuxBloxLauncher"
install -d -o tester "$root/ui"
tar --zstd -xf /ui-stack.tar.zst -C "$root/ui"
chown -R tester "$root/ui"
run() { runuser -u tester -- env HOME="$home" "$@"; }

# Case A: without its libraries the launcher must fail to load, and say why.
mv "$root/ui" "$root/ui.gone"
status=0
run "$root/TuxBloxLauncher" --version >/tmp/missing.txt 2>&1 || status=$?
mv "$root/ui.gone" "$root/ui"
test "$status" -ne 0 || fail 'it ran without its interface libraries'
grep -q 'error while loading shared libraries' /tmp/missing.txt || { cat /tmp/missing.txt; fail 'the failure does not name the loader'; }
printf 'OK: a missing ui folder fails to load (exit %s)\n' "$status"

# Case B: with them, the version is the x.y.z-channel string every component is compared by.
version="$(run "$root/TuxBloxLauncher" --version)"
[[ "$version" =~ ^[0-9]+\.[0-9]+\.[0-9]+-(stable|canary|experimental)$ ]] || fail "unexpected --version output: $version"
printf 'OK: --version prints %s\n' "$version"

# Case C: no display. The window cannot open, and the launcher must say so and exit 1 rather than let GTK exit for it.
status=0
run env -u DISPLAY -u WAYLAND_DISPLAY GDK_BACKEND=x11 timeout 60 "$root/TuxBloxLauncher" >/tmp/nodisplay.txt 2>&1 || status=$?
test "$status" -eq 1 || { cat /tmp/nodisplay.txt; fail "the launcher with no display exited $status, not 1"; }
grep -q 'no display to open the launcher window on' /tmp/nodisplay.txt || { cat /tmp/nodisplay.txt; fail 'the no-display failure was not reported'; }
printf 'OK: with no display the launcher reports it and exits 1\n'

# Case D: a real window, on a display.
rm -rf "$home/.cache"
Xvfb :99 -screen 0 1280x800x24 >/tmp/xvfb.log 2>&1 &
sleep 2
export DISPLAY=:99
run env DISPLAY=:99 "$root/TuxBloxLauncher" >/tmp/launcher.log 2>&1 &

pid=""
window=""
for _ in $(seq 1 30); do
    pid="$(pgrep -u tester -f "^$root/TuxBloxLauncher" | head -n 1 || true)"
    window="$(xwininfo -root -tree 2>/dev/null | grep -E '"TuxBlox".*[0-9]+x[0-9]+\+' | head -n 1 | awk '{print $1}' || true)"
    if [[ -n "$pid" && -n "$window" ]]; then break; fi
    sleep 1
done
test -n "$pid" || { cat /tmp/launcher.log; fail 'the launcher is not running'; }
test -n "$window" || { cat /tmp/launcher.log; fail 'the launcher window is not on the display'; }
printf 'OK: the window is on the display (pid %s)\n' "$pid"

# The .desktop entry declares StartupWMClass=tuxblox-launcher, and a pinned taskbar entry only matches the window through it
wmClass="$(xprop -id "$window" WM_CLASS)"
grep -q '"tuxblox-launcher"' <<<"$wmClass" || fail "the window's WM_CLASS is $wmClass, not tuxblox-launcher"
printf 'OK: WM_CLASS is tuxblox-launcher\n'

ldd "$root/TuxBloxLauncher" | tee /tmp/deps.txt
if grep -q 'not found' /tmp/deps.txt; then fail 'unresolved libraries'; fi

# Every resolved library must sit in the stack or be a known host library: libcurl and libarchive and the X11 client libraries with what they pull in, and the C and C++ runtimes.
hostOk="$(cd /usr/lib/x86_64-linux-gnu && ldd libcurl.so.4 libarchive.so.13 libX11.so.6 libXcursor.so.1 libXi.so.6 libxcb-render.so.0 | awk '/=>/ {print $1}' | sort -u)"
hostPattern='^(libc\.so\.6|libm\.so\.6|libdl\.so\.2|libpthread\.so\.0|librt\.so\.1|libresolv\.so\.2|libstdc\+\+\.so\.6|libgcc_s\.so\.1|libcurl\.so\.4|libarchive\.so\.13|libX[a-zA-Z0-9]*\.so\.[0-9]+|libxcb[a-z-]*\.so\.[0-9]+|libz\.so\.1|libexpat\.so\.1|libpng16\.so\.16|libpcre2-8\.so\.0)$'
stray=""
while read -r name arrow path _; do
    [[ "$arrow" == "=>" ]] || continue
    [[ "$path" == "$root/ui/"* ]] && continue
    if [[ "$name" =~ $hostPattern ]] || grep -qxF "$name" <<<"$hostOk"; then continue; fi
    stray+="$name => $path"$'\n'
done < /tmp/deps.txt
if [[ -n "$stray" ]]; then
    printf 'these libraries resolved outside the stack and are not known host libraries:\n%s' "$stray"
    fail 'stray libraries'
fi
for lib in libgtk-4 libadwaita libglib-2.0 libgobject-2.0 libgio-2.0 libpango-1.0 libcairo.so libgdk_pixbuf libharfbuzz libfontconfig libfreetype; do
    grep -q "$lib.*=> $root/ui/" /tmp/deps.txt || fail "$lib is not resolved from the ui folder"
done
printf 'OK: every library outside the host allow-list resolves from the ui folder\n'

# Missing schemas show up as GLib-GIO critical warnings, and mean settings such as the colour scheme silently do nothing.
if grep -q 'GLib-GIO-CRITICAL' /tmp/launcher.log; then
    cat /tmp/launcher.log
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

pkill -u tester -f "^$root/TuxBloxLauncher" || true
printf 'SMOKE OK\n'
INNER

podman run --rm --init -v "$BinaryPath:/TuxBloxLauncher:ro" -v "$StackPath:/ui-stack.tar.zst:ro" "$Image" bash -c "$InnerScript"
