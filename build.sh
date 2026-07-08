#!/bin/bash
set -e
cd "$(dirname "$0")"

rm -rf ProtonBuild
mkdir ProtonBuild
cd ProtonBuild

./../ProtonSource/configure.sh
make 2>&1 | tee ../build.log