#!/bin/bash
set -eo pipefail
cd "$(dirname "$0")"

ORANGE='\e[38;5;208m'
BLUE='\e[34m'
RED='\e[31m'
RESET='\e[0m'

step() {
    echo -e "${BLUE}:: $1${RESET}"
}

fail() {
    echo -e "${RED}!! $1${RESET}" >&2
    exit 1
}

[[ -f ProtonBuild/dist/proton ]] || fail "ProtonBuild/dist/proton not found -- run ./build.sh first."
[[ -f ProtonBuild/dist/version ]] || fail "ProtonBuild/dist/version not found -- this build didn't finish recording its version."

command -v zstd >/dev/null 2>&1 || fail "zstd is required to pack a .tar.zst release artifact but was not found on PATH."

build_version=$(awk '{print $2}' ProtonBuild/dist/version)
[[ -n "$build_version" ]] || fail "Could not read a version from ProtonBuild/dist/version."

version_slug="${build_version//./-}"
out="protonbuild-${version_slug}.tar.zst"

step "Packing ProtonBuild/dist/ -> $out (version $build_version)"

dist_size_bytes=$(du -sb ProtonBuild/dist | cut -f1)
dist_size_human=$(du -sh ProtonBuild/dist | cut -f1)

rm -f "$out"
tar --zstd -cf "$out" -C ProtonBuild dist

step "Clearing build scratch from ProtonBuild/ (src-*, obj-*, dst-*, stage markers, compile_commands)"
shopt -s nullglob dotglob
rm -rf ProtonBuild/src-* ProtonBuild/obj-* ProtonBuild/dst-* ProtonBuild/compile_commands ProtonBuild/.*-* compile_commands
shopt -u nullglob dotglob

sha=$(sha256sum "$out" | cut -d' ' -f1)
size=$(stat -c%s "$out")
size_human=$(du -h "$out" | cut -f1)

echo
echo -e "${ORANGE}Done.${RESET} $out"
echo "  Decompressed (ProtonBuild/dist/): $dist_size_human ($dist_size_bytes bytes)"
echo "  Compressed archive:               $size_human ($size bytes)"
echo
echo "manifest.json artifacts.protonbuild entry:"
cat <<EOF
{
  "url": "<CDN URL for $out>",
  "sha256": "$sha",
  "size_bytes": $size
}
EOF
