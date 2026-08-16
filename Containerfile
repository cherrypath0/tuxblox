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

# build-container/Containerfile
# Old-glibc-baseline builder image shared by installer/build.sh, launcher/build.sh,
# and build.sh's Nuitka proton-compile step. Ubuntu 20.04 "focal" ships glibc 2.31,
# old enough to cover Ubuntu 20.04+/Debian 11+/Mint 20+ and everything newer --
# deliberately older than webkitgtk-bundle/Containerfile's debian:12 (glibc 2.36),
# since 2.36 is actually newer than Ubuntu 22.04/Mint 21's glibc 2.35 and would repeat
# the exact GLIBC-floor bug this image exists to fix (see project memory
# glibc_floor_installer_bug.md). Bundles both the installer/launcher CMake toolchain
# and the proton step's Python/Nuitka toolchain in one image since a from-scratch
# `podman build` is shared/cached across all three build scripts anyway.
#
# Ubuntu 20.04 was chosen over Debian 11 "bullseye" (same glibc 2.31 floor) because
# this image is rebuilt from scratch on every fresh checkout/cache-clear, and bullseye
# will eventually roll off deb.debian.org/security.debian.org onto archive.debian.org,
# 404ing the `apt-get update` layer below. Ubuntu 20.04 has ESM support through 2030
# with archives actively served from the regular mirrors, which is a real durability
# concern for a build-time-only pinned-old-baseline image like this one -- unlike
# webkitgtk-bundle/Containerfile's debian:12, which wasn't picked for a decade-long
# floor and didn't need to weigh this. This image never ships to end users (it only
# produces binaries copied out at build time), so the base image being
# old/EOL-adjacent is an acceptable tradeoff here.
FROM ubuntu:20.04

ENV DEBIAN_FRONTEND=noninteractive

RUN apt-get update && apt-get install -y --no-install-recommends \
    build-essential cmake pkg-config git curl ca-certificates \
    libsdl2-dev libcurl4-openssl-dev libarchive-dev libssl-dev \
    librsvg2-bin libgl1-mesa-dev \
    libxcb-cursor0 libxcb-icccm4 libxcb-image0 libxcb-keysyms1 \
    libxcb-randr0 libxcb-render-util0 libxcb-shape0 libxcb-xkb1 \
    libxcb-util1 libxcb-shm0 libxcb-render0 \
    libxkbcommon-x11-0 libxkbcommon0 \
    python3 python3-dev python3-pip patchelf \
    && rm -rf /var/lib/apt/lists/*
# libgl1-mesa-dev: absent from both existing installer/build.sh and launcher/build.sh
# host package lists, but both CMakeLists.txt files have an unconditional
# find_package(OpenGL REQUIRED) -- satisfied implicitly on the maintainer's Arch dev
# host by some other already-installed package, but must be explicit on a clean
# Ubuntu 20.04 image.
#
# The libxcb-*/libxkbcommon-x11 block is NOT needed to compile anything -- nothing
# here links against them. They exist purely so launcher/bundle-qt.sh has real files
# to copy: Qt's xcb platform plugin (plugins/platforms/libqxcb.so, dlopen()'d by
# QGuiApplication at startup) links against this xcb-util-family set, Qt does not
# ship it, and it is NOT part of a default Ubuntu 20.04 / Debian 11 desktop install
# -- Qt 6.5 made libxcb-cursor a hard requirement of the xcb QPA plugin, which is
# the single most common "Qt6 app won't start on Ubuntu" report upstream. Without
# these the launcher dies at startup with
# 'Could not load the Qt platform plugin "xcb"' on exactly the old-glibc targets
# this whole image exists to serve. Ordinary ABI-stable X11/xcb libraries, so they
# are safe to redistribute alongside the app the same way Qt's own libraries are.
#
# libxcb-util1/libxcb-shm0/libxcb-render0 are listed explicitly even though apt
# would pull them in anyway as Depends of libxcb-image0/libxcb-cursor0/
# libxcb-render-util0: bundle-qt.sh copies them by name, so they are build inputs
# in their own right rather than incidental. libxcb-util1 in particular is
# invisible to `ldd libqxcb.so` on an image that lacks these packages (ldd does
# not recurse into a library it could not find in the first place), so it is the
# one dep a hand-written list silently drops.
#
# libxkbcommon0 likewise, and for a sharper reason: libxkbcommon-x11 reaches into
# libxkbcommon's internal structures rather than only its public ABI (upstream
# builds both from one source tree), so the two must be bundled as a version-
# matched pair. Shipping only the -x11 half against a newer system libxkbcommon
# SIGSEGVs inside xkb_x11_keymap_new_from_device during Qt's xcb connection
# setup -- reproduced, not theoretical.

# Nuitka baked into the image (not a per-run venv) so build.sh's proton-compile step
# doesn't pay Python venv + pip install cost on every invocation.
# python3-dev provides Python.h (Nuitka compiles generated C into CPython extension
# modules and refuses to run without it); patchelf is required by Nuitka's
# --standalone mode on Linux to rewrite RPATHs in the produced binaries.
# Pinned to the version verified against this image's toolchain/glibc floor --
# bump deliberately, not implicitly via an unpinned `pip3 install nuitka`.
RUN pip3 install nuitka==4.1.3

# Qt6 for the launcher's Qt Widgets UI. Ubuntu 20.04's own apt archive has no
# Qt6 packages at all (Qt6 didn't reach Ubuntu's archives until 22.10), so
# fetch the official prebuilt Qt6 desktop binaries via aqtinstall instead --
# same "pinned fetch, no silent fallback" convention launcher/vendor.sh uses
# for imgui/json.hpp/stb_image.h. Installed once into the image at
# /opt/qt6 so every build.sh invocation reuses it instead of re-downloading
# ~500MB of Qt per run.
RUN pip3 install aqtinstall==3.1.* && \
    python3 -m aqt install-qt linux desktop 6.6.3 gcc_64 -O /opt/qt6 \
        -m qtnetworkauth
ENV CMAKE_PREFIX_PATH="/opt/qt6/6.6.3/gcc_64:${CMAKE_PREFIX_PATH}"
