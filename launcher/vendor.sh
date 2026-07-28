#!/bin/bash
set -eo pipefail
cd "$(dirname "$0")"

# Every dependency fetched here is compiled into the shipped launcher binary, so each fetch is pinned to an immutable-as-possible ref and has NO fallback:
# if a pinned fetch fails we want the build to stop loudly rather than silently substitute an unreviewed newer revision
# (set -e makes an unguarded failure exit the script).

mkdir -p third_party

if [ ! -f third_party/imgui/imgui.h ]; then
    echo ":: Vendoring Dear ImGui (pinned v1.91.0)"
    rm -rf third_party/imgui
    git clone --branch v1.91.0 --depth 1 https://github.com/ocornut/imgui.git third_party/imgui
fi

if [ ! -f third_party/json.hpp ]; then
    echo ":: Vendoring nlohmann/json (pinned v3.11.3)"
    curl -fsSL -o third_party/json.hpp \
        https://raw.githubusercontent.com/nlohmann/json/v3.11.3/single_include/nlohmann/json.hpp
fi

if [ ! -f third_party/stb_image.h ]; then
    # stb publishes no release tags, so pin an exact commit SHA rather than floating on master.
    echo ":: Vendoring stb_image.h (pinned 31c1ad3)"
    curl -fsSL -o third_party/stb_image.h \
        https://raw.githubusercontent.com/nothings/stb/31c1ad37456438565541f4919958214b6e762fb4/stb_image.h
fi

echo ":: Vendoring complete"
