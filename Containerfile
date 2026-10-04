FROM ubuntu:20.04

ENV DEBIAN_FRONTEND=noninteractive

RUN apt-get update && apt-get install -y --no-install-recommends \
    build-essential cmake ninja-build autoconf automake libtool pkg-config git curl ca-certificates \
    libsdl2-dev libxdamage-dev libcurl4-openssl-dev libarchive-dev libssl-dev \
    librsvg2-bin libgl1-mesa-dev \
    libxcb-cursor0 libxcb-icccm4 libxcb-image0 libxcb-keysyms1 \
    libxcb-randr0 libxcb-render-util0 libxcb-shape0 libxcb-xkb1 \
    libxcb-util1 libxcb-shm0 libxcb-render0 \
    libxkbcommon-x11-0 libxkbcommon0 \
    python3 python3-dev python3-pip patchelf \
    && rm -rf /var/lib/apt/lists/*

# libarchive needs cmake 3.17 or newer and this baseline ships 3.16, so a current cmake goes in
# here deliberately OFF the default PATH -- the launcher, installer, bootstrapper and Studio MCP
# builds keep using /usr/bin/cmake exactly as before, and only shared/tools/libs-build.sh reaches for this one.
RUN mkdir -p /opt/cmake \
    && curl -fsSL https://github.com/Kitware/CMake/releases/download/v3.31.12/cmake-3.31.12-linux-x86_64.tar.gz \
    | tar -xz -C /opt/cmake --strip-components=1 \
    && /opt/cmake/bin/cmake --version
