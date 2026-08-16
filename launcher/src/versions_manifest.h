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
#include <string>
#include <vector>
#include "lnk_resolver.h"

namespace tuxblox {

struct InstalledVersion {
    std::string hash;         // e.g. "version-abc123def456"
    std::string channel;      // e.g. "live", or empty for a manually-entered hash
    std::string installedAt;  // ISO-8601 UTC, e.g. "2026-08-16T00:00:00Z"
};

struct AppVersions {
    std::vector<InstalledVersion> installed;
    std::string activeHash;   // empty if nothing pinned active yet
    // True once the one-time RobloxPlayerInstaller.exe/RobloxStudioInstaller.exe
    // bootstrap (see watch_launch.cpp) has run for this app type.
    bool bootstrapped = false;
};

struct VersionsManifest {
    AppVersions player;
    AppVersions studio;
};

// installDir + "/versions.json". Never throws -- same defensive contract as
// settings.h's loadSettings: a missing file, unreadable file, parse error,
// or malformed/missing field falls back to a default-constructed
// VersionsManifest{} wholesale, never a crash.
VersionsManifest loadVersionsManifest(const std::string& installDir);
void saveVersionsManifest(const std::string& installDir, const VersionsManifest& manifest);

AppVersions& appVersionsFor(VersionsManifest& manifest, LaunchTarget target);
const AppVersions& appVersionsFor(const VersionsManifest& manifest, LaunchTarget target);

// Scans <installDir>/runtime/pfx/drive_c/users/user/AppData/Local/Roblox/Versions
// (see the per-user-path cross-plan note in the launcher-versions-tab plan)
// for a version directory not already present in versions.json, and -- if
// exactly one new one is found -- records it as installed, marks this app
// type bootstrapped, and pins it active if nothing was active yet. A no-op
// (never throws) if the versions directory is missing, empty, or has zero
// new entries; if more than one new entry is found (unexpected -- a single
// official-installer run should only ever produce one), registers all of
// them as installed but does not guess which should be active.
void registerBootstrappedVersion(const std::string& installDir, LaunchTarget target);

} // namespace tuxblox
