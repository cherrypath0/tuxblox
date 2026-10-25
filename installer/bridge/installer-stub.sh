#!/bin/sh
set -eu

BRIDGE_VERSION="3.5.0"
RELEASE="3.5.0.1"
INSTALLER_VERSION="3.1.0"
INSTALLER_SHA256="42194cc410bf6a954dfbd9381ea5072daae3f04b0afa16155e55ab49fdab0f4f"
ARTIFACT="client/installer/$INSTALLER_VERSION/x86_64/TuxBloxInstaller"

uninstall=0
dir=""
allow_root=0
while [ $# -gt 0 ]; do
    case "$1" in
        --version) echo "$BRIDGE_VERSION"; exit 0 ;;
        --help|-h) echo "TuxBlox bridge installer: fetches and runs the installer for $RELEASE."; exit 0 ;;
        --uninstall) uninstall=1 ;;
        --allow-root) allow_root=1 ;;
        --dir) shift; [ $# -gt 0 ] || { echo "bridge: --dir needs a value" >&2; exit 2; }; dir="$1" ;;
        *) ;;
    esac
    shift
done

fetch() {
    for host in setup-bunny.tuxblox.net setup.tuxblox.net; do
        if command -v curl >/dev/null 2>&1; then
            curl -sSLf --retry 3 --retry-delay 2 --connect-timeout 15 --max-time 600 \
                 -o "$1" "https://$host/$ARTIFACT" && return 0
        elif command -v wget >/dev/null 2>&1; then
            wget -q -O "$1" "https://$host/$ARTIFACT" && return 0
        else
            echo "bridge: neither curl nor wget is available" >&2
            return 1
        fi
        echo "bridge: could not download from $host, trying the next one" >&2
    done
    return 1
}

digest() {
    if command -v sha256sum >/dev/null 2>&1; then sha256sum "$1" | cut -d' ' -f1
    elif command -v shasum >/dev/null 2>&1; then shasum -a 256 "$1" | cut -d' ' -f1
    elif command -v openssl >/dev/null 2>&1; then openssl dgst -sha256 "$1" | awk '{print $NF}'
    else return 1
    fi
}

if [ -z "$dir" ]; then
    self="$(cd "$(dirname "$0")" 2>/dev/null && pwd)" || self=""
    if [ -n "$self" ] && { [ -e "$self/TuxBloxLauncher" ] || [ -d "$self/proton" ] || [ -d "$self/compat" ]; }; then
        dir="$self"
    fi
fi

tmpdir="$(mktemp -d)"
trap 'rm -rf "$tmpdir"' EXIT
real="$tmpdir/TuxBloxInstaller"

echo ">>> TuxBlox is moving to its new update layout. Fetching the installer for $RELEASE."
if ! fetch "$real"; then
    echo "bridge: could not download the installer. Nothing has been changed; try again later." >&2
    exit 1
fi

if ! got="$(digest "$real")"; then
    echo "bridge: no sha256sum, shasum or openssl here, so the installer cannot be verified." >&2
    echo "bridge: refusing to run it unverified. Nothing has been changed -- install from https://tuxblox.net instead." >&2
    exit 1
fi
if [ "$got" != "$INSTALLER_SHA256" ]; then
    echo "bridge: the downloaded installer does not match its expected checksum. Nothing has been changed." >&2
    exit 1
fi

chmod +x "$real"

set --
[ "$uninstall" -eq 1 ] && set -- "$@" --uninstall
[ "$uninstall" -eq 0 ] && set -- "$@" --channel stable --release "$RELEASE"
[ -n "$dir" ] && set -- "$@" --dir "$dir"
[ "$allow_root" -eq 1 ] && set -- "$@" --allow-root

exec "$real" "$@"
