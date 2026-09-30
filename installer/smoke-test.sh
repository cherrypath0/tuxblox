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
# Runs the built single-file installer on a machine with no GTK at all and proves the unpacked interface brings everything it needs.
set -euo pipefail

BinaryPath="${1:-build/TuxBloxInstaller}"
Image="${TUXBLOX_SMOKE_IMAGE:-debian:11}"

if [[ ! -f "$BinaryPath" ]]; then
    printf 'No installer at %s. Build it with installer/build.sh first.\n' "$BinaryPath" >&2
    exit 1
fi

BinaryPath="$(realpath "$BinaryPath")"

read -r -d '' InnerScript <<'INNER' || true
set -eu
export DEBIAN_FRONTEND=noninteractive
# Debian 11 is end of life, so its packages now live on the archive mirror.
if grep -q '^VERSION_ID="11"' /etc/os-release; then
    printf 'deb http://archive.debian.org/debian bullseye main\n' >/etc/apt/sources.list
    rm -f /etc/apt/sources.list.d/*
    printf 'Acquire::Check-Valid-Until "false";\n' >/etc/apt/apt.conf.d/99archive
fi
# libcurl and libarchive are the installer's own dynamic dependencies. The X11 client libraries (libxcursor1, libxcb-render0) are hard requirements of GTK's X11 backend and are deliberately not bundled, so this test assumes a desktop has them.
apt-get update -qq
apt-get install -y -qq --no-install-recommends xvfb x11-utils procps ca-certificates libcurl4 libarchive13 libxcursor1 libxcb-render0 >/dev/null

# Proof the machine has no toolkit of its own, so anything that resolves must have come from the payload.
if ldconfig -p | grep -qE 'libgtk-4|libadwaita|libglib-2.0|libpango-1|libcairo\.so'; then
    printf 'FAIL: the base image has a toolkit installed, so this test proves nothing. Fix the image, do not delete this check.\n'
    ldconfig -p | grep -E 'libgtk-4|libadwaita|libglib-2.0|libpango-1|libcairo\.so'
    exit 1
fi
printf 'OK: the base image has no GTK, libadwaita, GLib, Pango or Cairo\n'
printf 'CLAIM: no GTK, GLib, Pango or Cairo required; X11 client libraries assumed present\n'

# TuxBlox refuses to run as root, so everything from here runs as an ordinary user.
useradd -m tester
home=/home/tester
Xvfb :99 -screen 0 1280x800x24 >/tmp/xvfb.log 2>&1 &
sleep 2
export DISPLAY=:99

# --nolaunch so it never hands off to the launcher; --dir keeps it out of the default path.
install -d -o tester /opt/tuxblox-smoke
runuser -u tester -- env HOME="$home" DISPLAY=:99 /TuxBloxInstaller --nolaunch --dir /opt/tuxblox-smoke >/tmp/installer.log 2>&1 &

cache=""
for _ in $(seq 1 60); do
    cache="$(ls -d $home/.cache/tuxblox/ui-* 2>/dev/null | head -n 1 || true)"
    if [[ -n "$cache" ]] && pgrep -u tester -f "^$cache/TuxBloxInstaller-ui" >/dev/null; then break; fi
    sleep 1
done
sleep 5

printf 'cache directory: %s\n' "$cache"
test -n "$cache" && test -d "$cache" || { printf 'FAIL: nothing was unpacked\n'; cat /tmp/installer.log; exit 1; }
test -x "$cache/TuxBloxInstaller-ui" || { printf 'FAIL: no executable interface in the cache\n'; exit 1; }

# Anchored to the real path: this script's own shell has the interface's name in its command line, so an unanchored match would always succeed.
pid="$(pgrep -u tester -f "^$cache/TuxBloxInstaller-ui" | head -n 1 || true)"
test -n "$pid" || { printf 'FAIL: the interface binary is not running\n'; cat /tmp/installer.log; exit 1; }
argv0="$(tr '\0' '\n' <"/proc/$pid/cmdline" | head -n 1)"
test "$argv0" = "$cache/TuxBloxInstaller-ui" || { printf 'FAIL: pid %s is %s, not the unpacked interface\n' "$pid" "$argv0"; exit 1; }
printf 'interface running as pid %s (%s)\n' "$pid" "$argv0"

ldd "$cache/TuxBloxInstaller-ui" | tee /tmp/deps.txt
if grep -q 'not found' /tmp/deps.txt; then printf 'FAIL: unresolved libraries\n'; exit 1; fi

# Every resolved library must sit in the payload or be a known host library: the installer's own libcurl and libarchive and the X11 client libraries, with what they pull in, the C and C++ runtimes, and the X11 client libraries.
hostOk="$(cd /usr/lib/x86_64-linux-gnu && ldd libcurl.so.4 libarchive.so.13 libX11.so.6 libXcursor.so.1 libXi.so.6 libxcb-render.so.0 | awk '/=>/ {print $1}' | sort -u)"
hostPattern='^(libc\.so\.6|libm\.so\.6|libdl\.so\.2|libpthread\.so\.0|librt\.so\.1|libresolv\.so\.2|libstdc\+\+\.so\.6|libgcc_s\.so\.1|libcurl\.so\.4|libarchive\.so\.13|libX[a-zA-Z0-9]*\.so\.[0-9]+|libxcb[a-z-]*\.so\.[0-9]+|libz\.so\.1|libexpat\.so\.1|libpng16\.so\.16|libpcre2-8\.so\.0)$'
stray=""
while read -r name arrow path _; do
    [[ "$arrow" == "=>" ]] || continue
    [[ "$path" == "$cache/"* ]] && continue
    if [[ "$name" =~ $hostPattern ]] || grep -qxF "$name" <<<"$hostOk"; then continue; fi
    stray+="$name => $path"$'\n'
done < /tmp/deps.txt
if [[ -n "$stray" ]]; then
    printf 'FAIL: these libraries resolved outside the payload and are not known host libraries:\n%s' "$stray"
    exit 1
fi
inPayload="$(grep -c "=> $cache/" /tmp/deps.txt)"
printf 'all %s payload libraries resolved inside the cache; every other entry is a known host library\n' "$inPayload"
for lib in libgtk-4 libadwaita libglib-2.0 libgobject-2.0 libgio-2.0 libpango-1.0 libcairo.so libgdk_pixbuf libharfbuzz libfontconfig libfreetype; do
    grep -q "$lib.*=> $cache/" /tmp/deps.txt || { printf 'FAIL: %s is not in the payload dependencies\n' "$lib"; exit 1; }
done

# The running process must not have mapped a library from outside the cache that the payload also ships, which covers modules loaded at run time such as pixbuf loaders and pango modules.
# Read as the interface's own user: root in a container is not allowed to look inside another user's process.
maps="$(runuser -u tester -- cat "/proc/$pid/maps")"
mapped="$(grep -c "$cache/" <<<"$maps" || true)"
test "$mapped" -gt 0 || { printf 'FAIL: the running interface maps nothing from the payload\n'; exit 1; }
# Compared by soname, and a library the installer's own host libraries already loaded is exempt: the first copy loaded wins and there is nothing the payload can do about it.
shipped="$(ls "$cache/lib/x86_64-linux-gnu" | sed -E 's/(\.so\.[0-9]+).*/\1/' | sort -u)"
outside=""
while read -r file; do
    soname="$(basename "$file" | sed -E 's/(\.so\.[0-9]+).*/\1/')"
    grep -qxF "$soname" <<<"$hostOk" && continue
    grep -qxF "$soname" <<<"$shipped" && outside+="$file"$'\n'
done < <(awk '{print $6}' <<<"$maps" | grep -E '\.so' | grep -v "^$cache/" | sort -u)
if [[ -n "$outside" ]]; then
    printf 'FAIL: the running interface mapped libraries from outside the payload that the payload also ships:\n%s\n' "$outside"
    exit 1
fi
printf 'running interface maps %s payload files and nothing shadowing the payload\n' "$mapped"

# Missing schemas show up as GLib-GIO critical warnings, and mean settings such as the colour scheme silently do nothing.
if grep -q 'GLib-GIO-CRITICAL' /tmp/installer.log; then
    printf 'FAIL: GLib could not find its settings schemas\n'
    cat /tmp/installer.log
    exit 1
fi
printf 'no GLib-GIO criticals in the interface log\n'

# Fontconfig writes a cache of every directory it scanned, so a cache naming the bundled font proves the bundled configuration was found.
fontFound=""
for _ in $(seq 1 20); do
    if grep -qa 'Adwaita Sans' "$home"/.cache/tuxblox/fontconfig/*.cache-* 2>/dev/null; then fontFound=1; break; fi
    sleep 1
done
if [[ -z "$fontFound" ]]; then
    printf 'FAIL: fontconfig never scanned the bundled font\n'
    ls -la "$home/.cache/tuxblox/fontconfig" || true
    exit 1
fi
printf 'bundled font scanned: Adwaita Sans is in the fontconfig cache\n'

# A titled window on the display proves something was actually drawn.
xwininfo -root -tree | tee /tmp/tree.txt
if ! grep -qE '"TuxBlox Installer".*[0-9]+x[0-9]+\+' /tmp/tree.txt; then
    printf 'FAIL: the installer window is not on the display\n'
    exit 1
fi
printf 'installer log:\n'
cat /tmp/installer.log
printf 'SMOKE OK\n'
INNER

podman run --rm --init -v "$BinaryPath:/TuxBloxInstaller:ro" "$Image" bash -c "$InnerScript"

read -r -d '' NoexecScript <<'INNER' || true
set -eu
export DEBIAN_FRONTEND=noninteractive
if grep -q '^VERSION_ID="11"' /etc/os-release; then
    printf 'deb http://archive.debian.org/debian bullseye main\n' >/etc/apt/sources.list
    rm -f /etc/apt/sources.list.d/*
    printf 'Acquire::Check-Valid-Until "false";\n' >/etc/apt/apt.conf.d/99archive
fi
apt-get update -qq
apt-get install -y -qq --no-install-recommends xvfb ca-certificates libcurl4 libarchive13 libxcursor1 libxcb-render0 >/dev/null

useradd -m tester
install -d -o tester /home/tester/.cache /opt/tuxblox-smoke
if ! mount -t tmpfs -o noexec,uid=1000 tmpfs /home/tester/.cache; then
    printf 'NOEXEC NOT TESTED: the container could not mount a noexec folder\n'
    exit 3
fi
Xvfb :99 >/dev/null 2>&1 &
sleep 2

# A cache folder that forbids running programs must produce the named error, not a crash.
status=0
runuser -u tester -- env HOME=/home/tester DISPLAY=:99 /TuxBloxInstaller --nolaunch --dir /opt/tuxblox-smoke >/tmp/err.txt 2>&1 || status=$?
cat /tmp/err.txt
printf 'exit status: %s\n' "$status"
test "$status" -ne 0 || { printf 'FAIL: it reported success with an unrunnable cache\n'; exit 1; }
grep -q 'does not allow programs to run' /tmp/err.txt || { printf 'FAIL: the error does not name the cause\n'; exit 1; }
grep -q -- '--headless' /tmp/err.txt || { printf 'FAIL: the error does not mention --headless\n'; exit 1; }
printf 'NOEXEC MESSAGE OK\n'
INNER

podman run --rm --init --cap-add SYS_ADMIN -v "$BinaryPath:/TuxBloxInstaller:ro" "$Image" bash -c "$NoexecScript"
