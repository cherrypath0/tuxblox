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
#include "release_file.h"
#include "update_changes.h"
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

// One component of an install, and the version it reports.
struct ComponentVersion {
    std::string component;  // the release file's key for it, e.g. "compat"
    std::string name;       // as a user would read it, e.g. "compatibility layer"
    std::string path;       // the binary asked; empty for the one that is read from a file instead
    std::string version;    // its bare "x.y.z", empty if it is not installed
};

// Asks every TuxBlox component in `installDir` what version it is. Four of them: the launcher is
// the one asking and knows its own answer, so the caller supplies that. The interface stack has no
// binary to ask and is read from the file its tarball carries.
//
// A component that is present but cannot be asked -- not executable, exits nonzero, prints nothing
// -- reports an EMPTY version, the same as one that is absent. An install cannot be repaired
// towards a version nobody can read, and a non-empty placeholder would differ from every recorded
// version and force a repair on every launch forever.
std::vector<ComponentVersion> readComponentVersions(const std::string& installDir);

struct UpdateResult {
    bool needsHandoff = false;  // true if the installer should be exec'd to apply the update
    // The release this was checked against, valid iff needsHandoff.
    ReleaseFile release;
    // Its bytes exactly as published. The installer is handed these rather than the release's
    // number, because two independent reads can straddle a pointer move and the install's own
    // record -- written last, from these bytes -- would then name something nobody decided.
    std::string releaseJson;
    // True iff the update bypasses the Auto-Update setting: the installed components do not match
    // the release this install records, which is broken rather than merely out of date.
    bool forced = false;
    // True iff there is no compatibility layer at all, as opposed to an outdated one -- nothing can
    // be launched yet, which App::updateCheckThreadMain() treats differently from an ordinary update.
    bool compatMissing = false;
    // What the update changes, for the dialog that offers it. Worked out here because this is where
    // both release files are already open.
    std::vector<ComponentChange> changes;
};

struct EnsureInstallerResult {
    bool ok = false;
    std::string installerPath;  // valid iff ok
    std::string errorMessage;   // valid iff !ok
};

// Ensures a verified copy of the installer binary is present at <dir>/TuxBloxInstaller, fetching a
// fresh one first if it's missing or its checksum no longer matches the one `release` names. Shared
// by runUpdateCheck (below) and the uninstall path (App::requestUninstall()) -- both need a working
// TuxBloxInstaller to hand off to and neither wants to duplicate this fetch-verify-install dance.
EnsureInstallerResult ensureInstallerBinary(const ReleaseFile& release, const std::string& baseUrl,
                                             const std::string& dir,
                                             const std::atomic<bool>* cancel,
                                             const UpdateProgressFn& onProgress);

// Writes the release bytes the installer is handed, at <installDir>/.release-incoming.json.
//
// Returns an empty string on success, or a sentence to show the user. It cannot be a bool: the
// caller has already reported PreparingUpdater, which the window reads as "an update is being
// applied" and which hides the Launch buttons, so a failure that says nothing leaves the user
// with no way to start Roblox and nothing on screen explaining why.
std::string writeReleaseHandoff(const std::string& installDir, const std::string& releaseJson);

// Exposed for unit testing: computes the download-progress fraction
// (0.0-1.0), falling back to `manifestSize` when curl doesn't report a
// total (`total == 0`, e.g. a chunked response with no Content-Length).
// Returns 0.0 if neither `total` nor `manifestSize` is known.
double downloadProgressFraction(uint64_t now, uint64_t total, uint64_t manifestSize);

// Decides whether this install should be replaced, and whether the user gets a say.
//
// Two fetches, neither of which grows with the number of releases: the channel's pointer (already
// resolved by the caller into `namedRelease`) and that release's own file. Then two independent
// questions -- see update_decision.h: are the installed components the set this install RECORDS
// (incoherent means an update died midway, which is repaired without asking), and does the recorded
// release differ from the named one (an ordinary update, which waits for the Auto-Update setting).
//
// Changes nothing on disk. Preparing the installer binary is the caller's step, once it knows it is
// going to hand off: a check that replaced the installer here left the install's pieces disagreeing
// whenever the handoff then did not happen, and the next check reads that as broken and repairs it
// without asking.
//
// Never downloads the layer or a replacement launcher itself -- that is the installer's job once
// handed off to, run in its upgrade mode against an existing install directory.
UpdateResult runUpdateCheck(const std::string& baseUrl,
                             const std::string& channel,
                             const std::string& namedRelease,
                             const UpdateProgressFn& onProgress,
                             const std::atomic<bool>* cancel,
                             const std::string& installDirOverride = "");

} // namespace tuxblox
