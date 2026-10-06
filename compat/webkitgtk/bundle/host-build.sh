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

# compat/webkitgtk/bundle/host-build.sh
# Builds webview2loader-host inside the builder container, against an already-built
# bundle in $PREFIX. Called by build-in-container.sh as one step of a full build, and
# by build-host.sh on its own when only the host's own sources changed.
set -eo pipefail

PREFIX="${PREFIX:-/opt/tuxblox-webview}"
export PKG_CONFIG_PATH="$PREFIX/lib/pkgconfig:$PREFIX/lib/x86_64-linux-gnu/pkgconfig"

# -ldl is last for Debian 11's binutils, which refuses dlclose from an implicit DSO.
#
# The error page (host/errorpage.html) and its two fonts (host/fonts/*.woff2) are
# compiled into the binary, so nothing has to ship beside it -- the page appears
# when the network is unreachable, which is exactly when a separate file or a
# webfont request would fail. Both generated headers are committed as well, so a
# plain gcc still works; regenerating here is what stops a real build from
# quietly shipping a page that no longer matches the HTML.
#
# Generated into a writable directory rather than in place: /src is mounted
# read-only. It comes first on the include path, so these win over the committed
# copies in /src/host.
# --- webview2loader-host (plan 2026-08-14, Task 2) --------------------------------
# The separate helper process that owns the GTK4/WebKitGTK main loop out-of-process
# from Wine (see the design spec's Lifecycle/IPC Protocol sections and Task 3's
# unixlib.c client, not yet written as of this task). Built as a plain two-file gcc
# invocation, not folded into any of the meson/cmake dependency builds above --
# there's nothing to configure and no reason to give it its own build system. Source
# lives at /src/host (the whole compat/webkitgtk/bundle tree is bind-mounted read-only at
# /src by build.sh's podman run, and build.sh additionally overlays Task 1's shared
# protocol header at /src/host/webview2loader_ipc_protocol.h so this single source
# tree compiles unmodified on both the Wine side and this GTK-process side).
#
# Task 5 addition: geometry.c (X11 window geometry sync/reparenting, ported from
# unixlib.c's sync_window_geometry_on_gtk_thread/get_window_geometry_on_gtk_thread)
# calls raw Xlib entry points (XReparentWindow, XMoveResizeWindow, XSendEvent, ...)
# directly against a real #include <X11/Xlib.h>, unlike the pre-Task-5 source list
# above which only ever touched X11 indirectly through GTK4/GDK's own API. `pkg-config
# --cflags/--libs gtk4` alone does not guarantee -lX11 on its own Libs line (GDK's X11
# backend support is compiled into libgtk-4.so itself, so linking gtk4 doesn't
# necessarily re-export libX11 as a direct link dependency of anything ELSE that calls
# raw Xlib symbols) -- confirmed needed by a real link failure
# (`undefined reference to XReparentWindow` etc.) without it. The old, pre-Task-3
# in-process unixlib.c never needed this: it resolved libX11 via the same dlmopen
# namespace as libgtk-4.so.1's own already-loaded dependency at runtime instead of
# linking against it at build time (see that file's own X11_FUNCS-adjacent comments).
#
# Task 6 addition: navigate.c (navigation/cookies/the real roblox-studio-auth:
# OAuth redirect handoff, ported from unixlib.c's on_load_changed/
# start_get_all_cookies_on_gtk_thread/start_count_cookies_on_gtk_thread/
# start_get_cookies_on_gtk_thread/on_decide_policy) calls into libsoup's own
# SoupCookie API (soup_cookie_get_name/_value/_domain/_path/_expires/etc.)
# directly -- already available for free here: webkitgtk-6.0.pc's own `Requires:`
# line already lists libsoup-3.0, so `pkg-config --cflags/--libs gtk4 webkitgtk-6.0`
# below already pulls in everything navigate.c needs, no separate `pkg-config
# libsoup-3.0` invocation required.
#
# Crash-fix addition (2026-08-15, gdk-assertion-crash-fix): watchdog.c opens a
# second, dedicated Xlib connection (raw <X11/Xlib.h> calls, same situation
# geometry.c's own Task 5 comment above already documents for -lX11) used to
# detect a reparented-into parent X11 window being destroyed out from under a
# webview and react before GDK's own async event processing on its own,
# separate connection can hit a fatal internal-consistency assertion over it
# -- see compat/webkitgtk/bundle/host/watchdog.h's own top-of-file comment for the
# full crash mechanism and rationale.
# -ldl is required, not belt-and-braces: this host dlopen()s libEGL and uses
# dladdr for the GL-provenance log. glibc >= 2.34 folded libdl into libc, so the
# old debian:12 base linked fine without it; sniper is glibc 2.31 where libdl is
# still a separate DSO, and omitting it fails with
# "undefined reference to symbol 'dlclose@@GLIBC_2.2.5' ... DSO missing from
# command line". Harmless on newer glibc, where -ldl resolves to a stub.
# The error page (host/errorpage.html) and its two fonts (host/fonts/*.woff2) are
# compiled into the binary, so nothing has to ship beside it -- the page appears
# when the network is unreachable, which is exactly when a separate file or a
# webfont request would fail. Both generated headers are committed as well, so a
# plain gcc still works; regenerating here is what stops a real build from
# quietly shipping a page that no longer matches the HTML.
#
# Generated into a writable directory rather than in place: /src is mounted
# read-only. It comes first on the include path, so these win over the committed
# copies in /src/host.

echo ":: Generating webview2loader-host error page"
WV2L_GENERATED=/tmp/webview2loader-host-generated
mkdir -p "$WV2L_GENERATED"
python3 /src/host/generate-errorpage-template.py "$WV2L_GENERATED"
python3 /src/host/fonts/generate-fonts-header.py "$WV2L_GENERATED"

echo ":: Building webview2loader-host"
gcc -O2 -Wall -Werror -o "$PREFIX/bin/webview2loader-host" \
    /src/host/main.c /src/host/ipc.c /src/host/webview.c /src/host/geometry.c /src/host/navigate.c \
    /src/host/watchdog.c /src/host/errorpage.c \
    -I"$WV2L_GENERATED" -I/src/host \
    $(pkg-config --cflags gtk4 webkitgtk-6.0) \
    $(pkg-config --libs gtk4 webkitgtk-6.0) \
    -lX11 -ldl
