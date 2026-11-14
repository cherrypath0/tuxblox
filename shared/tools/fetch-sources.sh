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

set -uo pipefail

MANIFEST_DIR=""
SOURCES_DIR=""
ENV_FILE=""
LABEL=""
MODE=fill

usage() {
    echo "Usage: $0 --manifest <dir> [--sources-dir <dir>] [--env <file>] [--label <name>] [--verify|--relock]" >&2
    exit 2
}

while [ "$#" -gt 0 ]; do
    case "$1" in
        --manifest) MANIFEST_DIR="${2:-}"; shift 2 || usage ;;
        --sources-dir) SOURCES_DIR="${2:-}"; shift 2 || usage ;;
        --env) ENV_FILE="${2:-}"; shift 2 || usage ;;
        --label) LABEL="${2:-}"; shift 2 || usage ;;
        --verify) MODE=verify; shift ;;
        --relock) MODE=relock; shift ;;
        *) usage ;;
    esac
done

[ -n "$MANIFEST_DIR" ] || usage
[ -d "$MANIFEST_DIR" ] || { echo "ERROR: no such manifest directory: $MANIFEST_DIR" >&2; exit 1; }
MANIFEST_DIR="$(cd "$MANIFEST_DIR" && pwd)"

: "${LABEL:=$(basename "$MANIFEST_DIR")}"
: "${SOURCES_DIR:=${TUXBLOX_SOURCES_DIR:-$MANIFEST_DIR/sources}}"
: "${ENV_FILE:=$MANIFEST_DIR/versions.env}"

SUMS_FILE="$MANIFEST_DIR/sha256sums"
LIST_FILE="$MANIFEST_DIR/sources.list"

[ -f "$LIST_FILE" ] || { echo "ERROR: missing $LIST_FILE" >&2; exit 1; }

if [ -f "$ENV_FILE" ]; then
    set -a; . "$ENV_FILE"; set +a
fi

mkdir -p "$SOURCES_DIR"

expand() { eval "printf '%s' \"$1\""; }

recorded_sum() {
    [ -f "$SUMS_FILE" ] || return 1
    awk -v f="$1" '$2 == f || $2 == "*" f { print $1; found = 1; exit } END { exit !found }' "$SUMS_FILE"
}

declare -a FILES=() MISSING=() BAD=() FETCHED=()
NEW_SUMS="$(mktemp)"
trap 'rm -f "$NEW_SUMS"' EXIT

while IFS= read -r raw; do
    [ -n "$raw" ] || continue
    case "$raw" in \#*) continue ;; esac

    fname="$(expand "${raw%%|*}")"
    urls_raw="${raw#*|}"
    read -r -a urls <<< "$(expand "$urls_raw")"
    target="$SOURCES_DIR/$fname"
    FILES+=("$fname")

    if [ ! -f "$target" ]; then
        if [ "$MODE" != fill ]; then
            echo "MISSING: $fname"
            MISSING+=("$fname")
            continue
        fi
        echo ":: Fetching $fname"
        got=0
        for u in "${urls[@]}"; do
            if curl -fL --proto '=https' --proto-redir '=https' \
                    --connect-timeout 30 --retry 3 --retry-all-errors \
                    "$u" -o "$target.part"; then
                mv -f "$target.part" "$target"
                got=1
                FETCHED+=("$fname")
                break
            fi
            echo "   download failed: $u -- trying next source" >&2
            rm -f "$target.part"
        done
        if [ "$got" -ne 1 ]; then
            echo "ERROR: every source failed for $fname" >&2
            MISSING+=("$fname")
            continue
        fi
    fi

    actual="$(sha256sum "$target" | awk '{print $1}')"
    printf '%s  %s\n' "$actual" "$fname" >> "$NEW_SUMS"

    if [ "$MODE" = relock ]; then
        continue
    fi
    if expected="$(recorded_sum "$fname")"; then
        if [ "$actual" != "$expected" ]; then
            echo "!!! CHECKSUM MISMATCH: $fname" >&2
            echo "       recorded: $expected" >&2
            echo "       on disk:  $actual" >&2
            BAD+=("$fname")
        fi
    else
        echo "   no recorded checksum for $fname -- run --relock to record it"
        BAD+=("$fname")
    fi
done < "$LIST_FILE"

if [ "$MODE" = relock ]; then
    sort -k2 "$NEW_SUMS" > "$SUMS_FILE"
    echo
    echo "Recorded $(wc -l < "$SUMS_FILE") of ${#FILES[@]} checksums in $SUMS_FILE"
    if [ "${#MISSING[@]}" -gt 0 ]; then
        echo "WARNING: ${#MISSING[@]} not on disk, so nothing was recorded for them. Run without --relock first." >&2
        exit 1
    fi
    exit 0
fi

if [ -d "$SOURCES_DIR" ]; then
    while IFS= read -r stale; do
        [ -n "$stale" ] || continue
        printf '%s\n' "${FILES[@]}" | grep -qxF "$stale" || echo "   stale (not in sources.list): $stale"
    done < <(cd "$SOURCES_DIR" && ls -1 2>/dev/null)
fi

echo
echo "############################################################"
echo "## Summary: $LABEL"
echo "############################################################"
echo "in sources.list: ${#FILES[@]}"
echo "downloaded now:  ${#FETCHED[@]}"
echo "missing:         ${#MISSING[@]}"
echo "failing check:   ${#BAD[@]}"

if [ "${#MISSING[@]}" -gt 0 ] || [ "${#BAD[@]}" -gt 0 ]; then
    echo
    echo "RESULT: FAIL -- the build will refuse to start until this is clean."
    exit 1
fi
echo
echo "RESULT: PASS -- every archive is present and matches its recorded checksum."
