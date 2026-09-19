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
#include "manifest.h"
#include <atomic>
#include <cstdint>
#include <functional>
#include <string>
#include <vector>

namespace tuxblox {

enum class UpdatePhase {
    Idle,
    CheckingManifest,
    // Fetching/verifying the persisted installer binary before handing off
    // to it -- the launcher no longer downloads/applies Proton or its own
    // update itself, it delegates that entirely to TuxBloxInstaller (which
    // shows its own "Upgrading Proton"/"Upgrading TuxBlox" progress).
    PreparingUpdater,
    UpToDate,
    Error
};

struct UpdateProgress {
    UpdatePhase phase = UpdatePhase::Idle;
    double fraction = 0.0;
    std::string errorMessage;
};

using UpdateProgressFn = std::function<void(UpdateProgress)>;

// True if `required` is a HIGHER version number than `installed`, comparing
// dot-separated parts numerically. It answers "is the server ahead", and says
// nothing about the other direction -- versionIsAhead below is that question.
//
// Both sides may be "x.y.z-channel"; the channel is NOT part of the decision.
// atoi() stops at the '-', so "2.6.0-canary" and "2.6.0-stable" compare equal.
bool versionNeedsUpdate(const std::string& installed, const std::string& required);

// Whether the installed version is NEWER than the one the channel publishes,
// compared the same way. A channel is moved back to an earlier release when a
// release turns out to be broken, and the installs that already took the broken
// one are exactly the ones that have to follow it back, so this is treated as
// an ordinary update in the other direction rather than as "up to date".
bool versionIsAhead(const std::string& installed, const std::string& required);

// The channel part of an "x.y.z-channel" build id, or an empty string when
// there is no suffix -- which is what builds before 2.7.0 report, and means
// "cannot tell", not "no channel".
std::string buildChannel(const std::string& buildId);

// One binary that makes up an install, and the build it reports.
struct ComponentVersion {
    std::string name;     // as a user would read it, e.g. "compatibility layer"
    std::string path;     // the binary asked
    std::string buildId;  // its --version output, empty if it is not installed
};

// Asks every TuxBlox binary in `installDir` what build it is. The launcher is
// not among them: it is the one asking, and it knows its own answer.
std::vector<ComponentVersion> readComponentVersions(const std::string& installDir);

// The components that do not agree with `requiredBuildId`: installed but
// reporting something else, or -- when `absentCounts` is true -- not installed
// at all. A release places every one of them together, so once the install
// exists an absent piece is as broken as a wrong-version one. `absentCounts`
// is false only before there is an install to be missing from.
std::vector<ComponentVersion> mismatchedComponents(const std::vector<ComponentVersion>& components,
                                                    const std::string& requiredBuildId,
                                                    bool absentCounts);

struct UpdateResult {
    bool needsHandoff = false;  // true if the installer should be exec'd to apply the update
    std::string installerPath;  // valid iff needsHandoff -- the binary to exec
    // True iff the pieces of this install do not agree on one build -- an
    // update that stopped halfway, or a channel switch that only some of
    // them followed. Such an install is broken rather than merely out of
    // date, so the caller applies the update without asking the Auto-Update
    // setting.
    bool mixedInstall = false;
    // True iff needsHandoff and there is no recorded Proton install at all
    // (as opposed to an outdated one) -- nothing can be launched yet, so
    // the Auto-Update opt-out doesn't apply: App::updateCheckThreadMain()
    // hands off to the installer immediately regardless of that setting.
    bool protonMissing = false;
};

struct EnsureInstallerResult {
    bool ok = false;
    std::string installerPath;  // valid iff ok
    std::string errorMessage;   // valid iff !ok
};

// Ensures a verified copy of the installer binary is present at
// <dir>/TuxBloxInstaller, fetching a fresh one first if it's missing or its
// checksum no longer matches `manifest.installer`. Shared by runUpdateCheck
// (below) and the uninstall path (App::requestUninstall()) -- both need a
// working TuxBloxInstaller to hand off to and neither wants to duplicate
// this fetch-verify-install dance.
EnsureInstallerResult ensureInstallerBinary(const Manifest& manifest, const std::string& dir,
                                             const std::atomic<bool>* cancel,
                                             const UpdateProgressFn& onProgress);

// Exposed for unit testing: computes the download-progress fraction
// (0.0-1.0), falling back to `manifestSize` when curl doesn't report a
// total (`total == 0`, e.g. a chunked response with no Content-Length).
// Returns 0.0 if neither `total` nor `manifestSize` is known.
double downloadProgressFraction(uint64_t now, uint64_t total, uint64_t manifestSize);

// Fetches baseUrl + "/v1/" + channel + "/" + requiredVersion + "/manifest.json"
// and checks it against every installed TuxBlox binary.
//
// Two separate questions, with different comparisons:
//  - "is a newer release out" -- versionNeedsUpdate, which orders version
//    numbers and ignores the channel suffix entirely.
//  - "do the pieces of this install agree" -- exact string equality on the
//    full "x.y.z-channel" build id, so a half-applied update is caught rather
//    than mistaken for being up to date.
// If anything is out of date, ensures a verified copy of the
// installer binary is present at <installDir>/TuxBloxInstaller (fetching a
// fresh one first if it's missing or its checksum no longer matches the
// manifest's `artifacts.installer` entry) and returns
// {needsHandoff = true, installerPath}: the caller should exec that binary
// and exit, letting it perform the actual update. Never downloads Proton
// or a replacement launcher binary itself -- that's the installer's job
// once handed off to, run in its "upgrade" mode (an existing install
// directory).
UpdateResult runUpdateCheck(const std::string& currentLauncherBuildId,
                             const std::string& baseUrl,
                             const std::string& channel,
                             const std::string& requiredVersion,
                             const UpdateProgressFn& onProgress,
                             const std::atomic<bool>* cancel,
                             const std::string& installDirOverride = "");

} // namespace tuxblox
