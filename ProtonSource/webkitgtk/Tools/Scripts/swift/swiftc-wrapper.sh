#!/bin/bash
# cmake accumulates CFLAGS from pkg-config, and then passes them to swiftc.
# This script filters out the arguments that swiftc cannot accommodate.

set -e

REAL_SWIFTC=swiftc
args=()

for arg in "$@"; do
    case "$arg" in
        "-mfpmath=sse") ;;
        "-msse") ;;
        "-msse2") ;;
        "-pthread") ;;
        "-Wl,"*)
            ldarg="${arg#-Wl,}"
            args+=("-Xlinker" "${ldarg//,/=}")
            ;;
        "--original-swift-compiler="*)
            REAL_SWIFTC="${arg#--original-swift-compiler=}"
            ;;
        *)
            args+=("$arg")
            ;;
    esac
done

exec "$REAL_SWIFTC" "${args[@]}"
