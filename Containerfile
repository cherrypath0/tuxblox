FROM ubuntu:20.04

ENV DEBIAN_FRONTEND=noninteractive

RUN apt-get update && apt-get install -y --no-install-recommends \
    build-essential cmake pkg-config git curl ca-certificates \
    libsdl2-dev libxdamage-dev libcurl4-openssl-dev libarchive-dev libssl-dev \
    librsvg2-bin libgl1-mesa-dev \
    libxcb-cursor0 libxcb-icccm4 libxcb-image0 libxcb-keysyms1 \
    libxcb-randr0 libxcb-render-util0 libxcb-shape0 libxcb-xkb1 \
    libxcb-util1 libxcb-shm0 libxcb-render0 \
    libxkbcommon-x11-0 libxkbcommon0 \
    python3 python3-dev python3-pip patchelf \
    && rm -rf /var/lib/apt/lists/*
