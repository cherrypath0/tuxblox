#!/usr/bin/env bash
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
#
# Reversible experiment: render Roblox Studio's UI in Selawik instead of
# Wine's own hand-drawn Tahoma.
#
# WHY TAHOMA AND NOT SEGOE UI. Traced with WINEDEBUG=+font on 2026-08-19:
# Studio calls NtGdiHfontCreate for L"Tahoma" 1857 times in a single startup
# and for L"Segoe UI" ZERO times. Every "Segoe" string in that trace is a
# failed SystemLink fallback lookup ("Unable to find file L\"SEGUISYM.TTF\""),
# not a request. Studio's Qt UI resolves through MS Shell Dlg, which this
# prefix maps to Tahoma:
#     trace:font:dump_gdi_font_subst L"MS Shell Dlg"   -> L"Tahoma"
#     trace:font:dump_gdi_font_subst L"MS Shell Dlg 2" -> L"Tahoma"
# So substituting Segoe UI changes nothing at all. Tahoma is the live key.
#
# WHY THIS IS ARGUABLY MORE CORRECT, NOT LESS. On real Windows lfMessageFont
# is Segoe UI 9pt, so Studio's UI is laid out against Segoe UI metrics.
# Selawik is metric-compatible with Segoe UI; Wine's Tahoma is not. Pointing
# the UI font at Selawik gives Studio the metrics it was designed for.
#
# WHY SUBSTITUTION AND NOT RENAMING. Selawik is SIL OFL 1.1 "with Reserved
# Font Name Selawik" (third_party_licenses/selawik-font/LICENSE.txt).
# Renaming its internal family to "Tahoma" or "Segoe UI" would breach that
# clause. FontSubstitutes maps a REQUEST onto the unmodified family name, so
# the shipped files stay byte-identical to Microsoft's release.
#
# Fully reversible: `undo` restores the exact prior registry values (recorded
# at apply time, including "this value did not exist") and removes the font
# files this script installed. Note ./build.sh wipes build/ entirely, so a
# rebuild reverts this on its own -- re-run `apply` afterwards if wanted.

set -uo pipefail

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
FONT_SRC="$REPO_ROOT/fonts/selawik"
SUBST_KEY='HKLM\Software\Microsoft\Windows NT\CurrentVersion\FontSubstitutes'

# The three request names that actually reach the font mapper. "Tahoma" is
# the one doing the work (see the trace note above); the two MS Shell Dlg
# aliases are included so anything resolving the generic UI font lands on
# Selawik too rather than splitting between two faces.
SUBST_NAMES=("Tahoma" "MS Shell Dlg" "MS Shell Dlg 2")
SUBST_TO="Selawik"

usage() {
    cat <<EOF
Usage: $(basename "$0") {apply|undo|status} [--prefix DIR]

  apply    Install Selawik into the prefix and point the UI font at it.
  undo     Restore the previous font substitutions and remove the fonts.
  status   Show what is currently in effect.

  --prefix DIR   Wine prefix to act on. Defaults to the first that exists:
                 \$TUXBLOX_PREFIX/pfx, $REPO_ROOT/build/runtime/pfx,
                 \$HOME/.tuxblox/runtime/pfx

Restart Studio after apply or undo -- Wine enumerates fonts at process start.
EOF
}

die() { printf 'font-experiment: %s\n' "$1" >&2; exit 1; }

resolve_prefix() {
    if [ -n "${OPT_PREFIX:-}" ]; then printf '%s\n' "$OPT_PREFIX"; return; fi
    for candidate in \
        "${TUXBLOX_PREFIX:-}/pfx" \
        "$REPO_ROOT/build/runtime/pfx" \
        "$HOME/.tuxblox/runtime/pfx"
    do
        case "$candidate" in /pfx) continue;; esac
        [ -d "$candidate/drive_c" ] && { printf '%s\n' "$candidate"; return; }
    done
    die "no Wine prefix found -- pass --prefix DIR"
}

# proton/main lives next to the prefix's install root, not inside the prefix.
resolve_proton() {
    for candidate in \
        "$REPO_ROOT/build/proton/main" \
        "$HOME/.tuxblox/proton/main"
    do
        [ -x "$candidate" ] && { printf '%s\n' "$candidate"; return; }
    done
    die "proton/main not found -- build first, or run this from the repo root"
}

# Registry writes go through the LIVE wineserver via reg.exe rather than by
# editing system.reg as text. Editing the file underneath a running wineserver
# loses the change: the server owns the registry in memory and flushes over
# whatever the file says. runinprefix (not run) is deliberate -- see
# include/mcp.sh: the "run" verb's _wait_for_prefix_drain() would block until
# every Roblox process in the prefix exits, and can escalate to wineserver -k.
reg_exec() {
    TUXBLOX_PREFIX="$PREFIX_ROOT_PARENT" PROTON_LOG=0 \
        "$PROTON_BIN" runinprefix reg.exe "$@" 2>/dev/null
}

# Prints the value's data, or the literal token @@ABSENT@@ if unset. Wine's
# reg.exe prints "NAME    REG_SZ    DATA" with whitespace separators.
reg_get() {
    local name="$1" out
    out="$(reg_exec query "$SUBST_KEY" /v "$name" 2>/dev/null | tr -d '\r')"
    local line
    line="$(printf '%s\n' "$out" | grep -F "$name" | grep -F "REG_SZ" | head -1)"
    if [ -z "$line" ]; then printf '@@ABSENT@@\n'; return; fi
    printf '%s\n' "$line" | sed -E 's/.*REG_SZ[[:space:]]+//'
}

cmd_apply() {
    [ -d "$FONT_SRC" ] || die "font source missing: $FONT_SRC"
    ls "$FONT_SRC"/*.ttf >/dev/null 2>&1 || die "no .ttf files in $FONT_SRC"

    local fonts_dir="$PREFIX/drive_c/windows/Fonts"
    mkdir -p "$fonts_dir" || die "cannot write $fonts_dir"

    if [ -f "$BACKUP" ]; then
        printf 'font-experiment: already applied (backup exists at %s)\n' "$BACKUP"
        printf 'font-experiment: run `undo` first if you want to re-apply.\n'
        exit 1
    fi

    # Record the prior state BEFORE touching anything, so undo is exact.
    : > "$BACKUP"
    local name prior
    for name in "${SUBST_NAMES[@]}"; do
        prior="$(reg_get "$name")"
        printf '%s\t%s\n' "$name" "$prior" >> "$BACKUP"
        printf '  was: %-14s = %s\n' "$name" "$prior"
    done

    local f
    for f in "$FONT_SRC"/*.ttf; do
        cp -f "$f" "$fonts_dir/" || die "failed to copy $(basename "$f")"
        printf '  installed %s\n' "$(basename "$f")"
    done

    for name in "${SUBST_NAMES[@]}"; do
        reg_exec add "$SUBST_KEY" /v "$name" /d "$SUBST_TO" /f >/dev/null \
            || die "reg add failed for $name"
        printf '  now: %-14s = %s\n' "$name" "$SUBST_TO"
    done

    printf '\nfont-experiment: applied. Restart Studio to see it.\n'
    printf 'font-experiment: undo with `%s undo`.\n' "$(basename "$0")"
}

cmd_undo() {
    [ -f "$BACKUP" ] || die "nothing to undo (no backup at $BACKUP)"

    local name prior
    while IFS=$'\t' read -r name prior; do
        [ -n "$name" ] || continue
        if [ "$prior" = "@@ABSENT@@" ]; then
            reg_exec delete "$SUBST_KEY" /v "$name" /f >/dev/null
            printf '  removed %s (was unset before)\n' "$name"
        else
            reg_exec add "$SUBST_KEY" /v "$name" /d "$prior" /f >/dev/null
            printf '  restored %-14s = %s\n' "$name" "$prior"
        fi
    done < "$BACKUP"

    local fonts_dir="$PREFIX/drive_c/windows/Fonts" f
    for f in "$FONT_SRC"/*.ttf; do
        rm -f "$fonts_dir/$(basename "$f")" && printf '  removed %s\n' "$(basename "$f")"
    done

    rm -f "$BACKUP"
    printf '\nfont-experiment: reverted. Restart Studio.\n'
}

cmd_status() {
    printf 'prefix:  %s\n' "$PREFIX"
    printf 'applied: %s\n\n' "$([ -f "$BACKUP" ] && echo yes || echo no)"
    local name
    for name in "${SUBST_NAMES[@]}"; do
        printf '  %-14s = %s\n' "$name" "$(reg_get "$name")"
    done
    printf '\ninstalled Selawik files in prefix:\n'
    ls -1 "$PREFIX/drive_c/windows/Fonts"/selaw*.ttf 2>/dev/null | sed 's/^/  /' \
        || printf '  (none)\n'
}

ACTION="${1:-}"; shift || true
OPT_PREFIX=""
while [ $# -gt 0 ]; do
    case "$1" in
        --prefix) OPT_PREFIX="${2:-}"; shift 2 || die "--prefix needs a directory";;
        -h|--help) usage; exit 0;;
        *) die "unknown argument: $1";;
    esac
done

case "$ACTION" in
    apply|undo|status) ;;
    ""|-h|--help) usage; exit 0;;
    *) usage; exit 1;;
esac

PREFIX="$(resolve_prefix)" || exit 1
PROTON_BIN="$(resolve_proton)" || exit 1
# TUXBLOX_PREFIX is the directory CONTAINING pfx/, not pfx/ itself --
# proton.py builds prefix_dir as TUXBLOX_PREFIX + "/pfx/".
PREFIX_ROOT_PARENT="$(dirname "$PREFIX")"
BACKUP="$PREFIX/.tuxblox-font-experiment.bak"

case "$ACTION" in
    apply)  cmd_apply ;;
    undo)   cmd_undo ;;
    status) cmd_status ;;
esac
