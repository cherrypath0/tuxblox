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

// True if `installed` and `required` differ (plain string inequality --
// the manifest is always authoritative, no version ordering).
bool versionNeedsUpdate(const std::string& installed, const std::string& required);

struct UpdateResult {
    bool needsHandoff = false;  // true if the installer should be exec'd to apply the update
    std::string installerPath;  // valid iff needsHandoff -- the binary to exec
};

// Checks manifest.json against the currently-installed launcher/Proton
// versions. If either is out of date, ensures a verified copy of the
// installer binary is present at <installDir>/TuxBloxInstaller (fetching a
// fresh one first if it's missing or its checksum no longer matches the
// manifest's `artifacts.installer` entry) and returns
// {needsHandoff = true, installerPath}: the caller should exec that binary
// and exit, letting it perform the actual update. Never downloads Proton
// or a replacement launcher binary itself -- that's the installer's job
// once handed off to, run in its "upgrade" mode (an existing install
// directory).
UpdateResult runUpdateCheck(const std::string& currentLauncherVersion,
                             const std::string& manifestUrl,
                             const UpdateProgressFn& onProgress,
                             const std::atomic<bool>* cancel,
                             const std::string& installDirOverride = "");

} // namespace tuxblox
