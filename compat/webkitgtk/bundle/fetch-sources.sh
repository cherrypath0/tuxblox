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

# compat/webkitgtk/bundle/fetch-sources.sh
#
# Fills the bundle's sources/ directory from sources.list and records a checksum for
# every archive in sha256sums. This is the only part of the bundle build that touches
# the network, and it is deliberately a separate step the operator runs by hand:
# build-in-container.sh resolves every package out of sources/ by name and downloads
# nothing, so a build either has the bytes it needs on disk or stops.
#
# The point of the split is that the bytes a release was built from are decided once,
# recorded, and then re-used -- rather than re-fetched from whatever a mirror is
# serving the day someone rebuilds. Several of these upstreams are mirror redirectors
# or volunteer hosts, and one of them has already gone missing mid-build once (see
# build-in-container.sh's note on the freetype/Savannah outage). A checksum that was
# recorded before the archive was ever built turns a hijacked or corrupted mirror into
# a failed verification instead of a release nobody can account for.
#
# Usage:
#   fetch-sources.sh              # download whatever is missing, verify everything
#   fetch-sources.sh --verify     # verify what is on disk, download nothing
#   fetch-sources.sh --relock     # re-record sha256sums from what is on disk
#
# --relock is for a deliberate version bump: change versions.env, run it, and commit
# the new sha256sums alongside. It rewrites recorded checksums, so it is never part of
# an ordinary build -- that is the whole protection this file provides.
set -uo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
SOURCES_DIR="${TUXBLOX_SOURCES_DIR:-$SCRIPT_DIR/sources}"
SUMS_FILE="$SCRIPT_DIR/sha256sums"
LIST_FILE="$SCRIPT_DIR/sources.list"

MODE=fill
case "${1:-}" in
    --verify) MODE=verify ;;
    --relock) MODE=relock ;;
    "") ;;
    *) echo "Usage: $0 [--verify|--relock]" >&2; exit 2 ;;
esac

[ -f "$LIST_FILE" ] || { echo "ERROR: missing $LIST_FILE" >&2; exit 1; }

# shellcheck disable=SC1091
set -a; . "$SCRIPT_DIR/versions.env"; set +a

mkdir -p "$SOURCES_DIR"

# sources.list carries real shell parameter expansions (${EXPAT_VERSION//./_},
# ${GLIB_VERSION%.*}) so that one version in versions.env drives both the archive name
# and every URL that serves it. Expanding them needs the shell itself; the file is
# tracked in this repository and carries exactly the same trust as this script.
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
            # --proto/--proto-redir pin the whole exchange, redirects included, to https:
            # several of these are mirror redirectors, and a redirect to plain http would
            # otherwise hand the archive to anyone on the path.
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
    echo "Recorded ${#FILES[@]} checksums in $SUMS_FILE"
    exit 0
fi

# An archive left behind by an older versions.env is not an error, but it is dead
# weight that a reader would reasonably mistake for an input to this build.
if [ -d "$SOURCES_DIR" ]; then
    while IFS= read -r stale; do
        [ -n "$stale" ] || continue
        printf '%s\n' "${FILES[@]}" | grep -qxF "$stale" || echo "   stale (not in sources.list): $stale"
    done < <(cd "$SOURCES_DIR" && ls -1 2>/dev/null)
fi

echo
echo "############################################################"
echo "## Summary"
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
