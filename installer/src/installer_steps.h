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
#include <functional>
#include <string>
#include "manifest.h"
#include "progress.h"

namespace tuxblox {

struct InstallOutcome {
    bool ok = false;
    bool cancelled = false;
    std::string errorMessage; // populated when !ok && !cancelled
};

// Called on every progress update: (currentStep, overallPercent).
using StepProgressFn = std::function<void(Step, double)>;

// Runs the full install pipeline against an already-fetched manifest:
// create directories, download+verify+extract Proton, download+verify+move
// the Launcher and the Installer itself (persisted so the launcher can
// invoke it again later for updates). Reports progress via `onProgress`.
// Aborts early if `*cancel` becomes true, returning
// InstallOutcome::cancelled = true.
//
// `isUpgrade`, when true, means an existing install was found at the
// target directory: the pipeline replaces only ProtonBuild/ (wiped, but
// only after the replacement tarball is downloaded and checksum-verified)
// and the Launcher/Installer binaries, leaving `runtime/` and everything
// else untouched -- never re-wipes the whole directory the way a fresh
// install populates it. `onProgress`'s step labels (see
// `stepLabel(Step, bool)`) reflect this with "Upgrading ..." wording.
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
