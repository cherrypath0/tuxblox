// TuxBlox - Linux Compatibility Layer for the Roblox Engine
// Copyright (C) 2026 TuxBlox Developers
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program. If not, see <https://www.gnu.org/licenses/>.

#pragma once
#include <atomic>
#include <cstdint>
#include <functional>
#include <string>
#include "manifest.h"
#include "progress.h"

namespace tuxblox {

struct InstallOutcome {
    bool ok = false;
    bool cancelled = false;
    std::string errorMessage; // populated when !ok && !cancelled
    // The installed launcher *executable's* path, populated iff ok -- the
    // caller should exec this, and it is what the .desktop entries' Exec=
    // lines name. When the "launcher" artifact is a flat file this is simply
    // its installed path (the shape shipped today -- the launcher's Qt6
    // dependencies ride along in their own separate "libtuxblox" archive
    // artifact, which needs no handling here); when it is an archive the
    // artifact's own filename names the extraction *directory*, so this points
    // one level inside it at "TuxBloxLauncher". Artifact placement is otherwise fully
    // generic (see runInstall's comment), so this is the one thing the
    // pipeline still has to hand back by name.
    std::string launcherPath;
};

// Called on every progress update: (currentPhaseLabel, overallPercent).
// The label is already fully resolved -- fresh-install vs "Upgrading ..."
// wording is decided at the point it's generated, not looked up separately.
using StepProgressFn = std::function<void(const std::string&, double)>;

// Exposed for unit testing: computes the download-progress fraction
// (0.0-1.0), falling back to `manifestSize` when curl doesn't report a
// total (`total == 0`, e.g. a chunked response with no Content-Length).
// Returns 0.0 if neither `total` nor `manifestSize` is known.
double downloadProgressFraction(uint64_t now, uint64_t total, uint64_t manifestSize);

// Runs the full install pipeline against an already-fetched manifest:
// create directories, then for EVERY artifact manifest.artifacts lists
// (not a fixed, hardcoded set) -- download it, verify its sha256, and
// either extract it (if its url ends in a recognized archive extension --
// ".tar.zst", ".tar.gz", ".tar.xz", ".tar.bz2") into
// installDir + artifact.path + "/" + artifact.filename/, or move it into
// place at installDir + artifact.path + "/" + artifact.filename directly
// (chmod 0755). A future manifest with a 5th artifact, or one installed
// under a different path, needs no code change here to be picked up.
//
// The one artifact this treats specially is "launcher" (if present): its
// installed executable becomes the thing the caller should exec once the
// pipeline succeeds -- the artifact's own path if it is a flat file, or
// "<extraction dir>/TuxBloxLauncher" if it is an archive (see
// InstallOutcome::launcherPath). This is the sole remaining hardcoded
// assumption -- unavoidable, since something has to be the app's actual
// entry point.
//
// Reports progress via `onProgress`. Aborts early if `*cancel` becomes
// true, returning InstallOutcome::cancelled = true.
//
// `isUpgrade`, when true, means an existing install was found at the
// target directory: an archive artifact's extraction directory is wiped
// (but only after the replacement tarball is downloaded and
// checksum-verified) before extracting the new one; a plain-file artifact
// simply overwrites the old file at the same path either way. Never
// touches anything outside artifacts' own declared paths (runtime/
// etc. are left alone). Progress labels read "Upgrading
// <displayname>" instead of "Downloading <displayname>" in this mode.
//
// Also pre-warms the Roblox Player/Studio installer cache (same cache
// paths the launcher's own lazy-download fallback checks) so the first
// "Launch Player"/"Launch Studio" click doesn't need to download it --
// these are only ever fetched here, never run. This is best-effort: a
// download failure (Roblox's CDN unreachable, etc.) does not fail the
// whole TuxBlox install, since the launcher's fallback covers a missing
// cache entry when the user actually launches. An explicit user *cancel*
// during this step is still respected like any other step, though.
//
// `installDirOverride`, when non-empty, is used instead of installDir()
// (for testing against a temp directory instead of the real $HOME).
// `robloxPlayerInstallerUrl`/`robloxStudioInstallerUrl` default to the
// real Roblox CDN, overridable so tests can point them at local fixtures.
InstallOutcome runInstall(const Manifest& manifest,
                           const StepProgressFn& onProgress,
                           const std::atomic<bool>* cancel,
                           bool isUpgrade,
                           const std::string& installDirOverride = "",
                           const std::string& robloxPlayerInstallerUrl = "https://setup.rbxcdn.com/RobloxPlayerInstaller.exe",
                           const std::string& robloxStudioInstallerUrl = "https://setup.rbxcdn.com/RobloxStudioInstaller.exe");

} // namespace tuxblox
