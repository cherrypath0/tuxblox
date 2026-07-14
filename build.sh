#!/bin/bash
set -eo pipefail
cd "$(dirname "$0")"

echo ":: Cleaning up previous build logs"
rm build.log

echo ":: Cleaning up old prefix directory"
rm -rf prefix
mkdir -p prefix

echo ":: Cleaning up old proton build"
rm -rf ProtonBuild
mkdir ProtonBuild
cd ProtonBuild

echo ":: Configuring Proton"
./../ProtonSource/configure.sh

echo ":: First-pass build"
make 2>&1 | tee ../build.log || true

echo ":: Fetching external sources"
(cd src-glslang && rm -rf External/spirv-tools External/googletest && python3 update_glslang_sources.py)

echo ":: Initializing nested submodules"
(cd src-dxvk-nvapi && git submodule update --init --recursive)

echo ":: Resuming build"
make 2>&1 | tee -a ../build.log