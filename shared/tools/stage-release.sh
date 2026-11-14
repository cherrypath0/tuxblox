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
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"

StageOnly=0
OutDir=""
while [ "$#" -gt 0 ]; do
    case "$1" in
        --stage-only) StageOnly=1; shift ;;
        --out-dir) OutDir="${2:?--out-dir needs a path}"; shift 2 ;;
        *) echo "Usage: $0 [--stage-only] [--out-dir <dir>]" >&2; exit 2 ;;
    esac
done

# The tree exists only to be packed and uploaded, so it goes somewhere temporary unless asked.
if [ -z "$OutDir" ]; then
    OutDir="$(mktemp -d)"
    echo ":: Staging into $OutDir" >&2
fi

Seed="$(sed -n '1p' "$ROOT/VERSION" | tr -d '[:space:]')"
Build="$(sed -n '2p' "$ROOT/VERSION" | tr -d '[:space:]')"
FileChannel="$(sed -n '3p' "$ROOT/VERSION" | tr -d '[:space:]')"
[[ "$Build" =~ ^[0-9]+$ ]] || Build=0

migrateChannel() {
    if [ "$1" = dev ]; then printf 'experimental'; else printf '%s' "$1"; fi
}

# A plain run is experimental whatever VERSION says, so stable and canary are always deliberate.
# Re-staging keeps the channel the build already went out on rather than moving it.
Channel="${TUXBLOX_CHANNEL:-}"
if [ -z "$Channel" ]; then
    if [ "$StageOnly" -eq 1 ]; then Channel="${FileChannel:-experimental}"; else Channel=experimental; fi
fi
Channel="$(migrateChannel "$Channel")"
case "$Channel" in
    stable|canary|experimental) ;;
    *) echo "unknown channel: $Channel" >&2; exit 1 ;;
esac

Explicit=()
[ -n "${TUXBLOX_BUILD_VERSION:-}" ] && Explicit=(--release "$TUXBLOX_BUILD_VERSION")

Layout=()
if [ "${TUXBLOX_LEGACY_PATHS:-}" = "1" ]; then
    Layout=(--legacy-paths)
    echo ":: Publishing component paths without the release in them (TUXBLOX_LEGACY_PATHS=1)"
fi

Release="$(python3 "$ROOT/shared/tools/stage_release.py" \
    --root "$ROOT" --channel "$Channel" --seed "$Seed" --build "$Build" \
    --out-dir "$OutDir" "${Explicit[@]}" "${Layout[@]}")" || exit 1

if [ "$StageOnly" -eq 0 ]; then
    IFS=. read -r major minor build patch <<< "$Release"
    printf '%s\n%s\n%s\n' "$major.$minor.$patch" "$build" "$Channel" > "$ROOT/VERSION"
    echo ":: Updated VERSION to $Release $Channel"
    if [ -z "${TUXBLOX_NO_VERSION_COMMIT:-}" ] &&
       git -C "$ROOT" rev-parse --is-inside-work-tree >/dev/null 2>&1 &&
       ! git -C "$ROOT" diff --quiet HEAD -- "$ROOT/VERSION" 2>/dev/null; then
        if git -C "$ROOT" commit -q -m "Change version to $Release" -- "$ROOT/VERSION" 2>/dev/null; then
            echo ":: Committed VERSION as $Release $Channel"
        else
            echo "!! Could not commit VERSION -- left it modified for you to commit." >&2
        fi
    fi
fi

echo ":: Staged $Release ($Channel) into $OutDir" >&2
printf '%s\n' "$OutDir"
