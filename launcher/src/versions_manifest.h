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
    // True once at least one version for this app type exists in the prefix
    // (i.e. the one-time RobloxPlayerInstaller.exe/RobloxStudioInstaller.exe
    // bootstrap, see watch_launch.cpp, has produced something). Derived from
    // the prefix by reconcileWithPrefix(), not trusted from versions.json.
    bool bootstrapped = false;
};

struct VersionsManifest {
    AppVersions player;
    AppVersions studio;
};

// Absolute path of the prefix directory that holds one subdirectory per
// installed Roblox version (the "version-<hash>" dirs the official
// installer creates). This directory -- not versions.json -- is the source
// of truth for what is actually installed.
std::string prefixVersionsDir(const std::string& installDir);

// Hashes of the version directories in the prefix that actually contain
// `target`'s own exe. Player and Studio share one Versions/ directory, so
// exe presence is what separates them. Empty (never throws) if the prefix
// or the directory doesn't exist yet.
std::vector<std::string> scanPrefixVersions(const std::string& installDir, LaunchTarget target);

// Makes `manifest` agree with what's actually in the prefix: adds version
// directories it didn't know about, drops entries whose directory is gone,
// and re-pins `activeHash` (to the newest version directory) whenever the
// pinned one no longer exists. Only the metadata that can't be recovered
// from disk -- channel, installedAt -- is preserved from the manifest.
// Never throws; leaves `manifest` untouched if the prefix can't be read.
void reconcileWithPrefix(const std::string& installDir, VersionsManifest& manifest);

// loadVersionsManifest() + reconcileWithPrefix(): what is really installed,
// with versions.json contributing only the extra metadata. Use this rather
// than loadVersionsManifest() anywhere the question is "is Roblox
// installed / which exe do we launch", so a deleted or stale versions.json
// can't make an installed Roblox look missing.
VersionsManifest loadInstalledVersions(const std::string& installDir);

// installDir + "/versions.json". Never throws -- same defensive contract as
// settings.h's loadSettings: a missing file, unreadable file, parse error,
// or malformed/missing field falls back to a default-constructed
// VersionsManifest{} wholesale, never a crash.
VersionsManifest loadVersionsManifest(const std::string& installDir);
void saveVersionsManifest(const std::string& installDir, const VersionsManifest& manifest);

AppVersions& appVersionsFor(VersionsManifest& manifest, LaunchTarget target);
const AppVersions& appVersionsFor(const VersionsManifest& manifest, LaunchTarget target);

// Persists what the official RobloxPlayerInstaller.exe/RobloxStudioInstaller.exe
// bootstrap just installed: reconciles versions.json against the prefix (see
// reconcileWithPrefix) and marks this app type bootstrapped. A no-op (never
// throws) if the installer run produced no version directory. This only
// records metadata -- detection itself no longer depends on it running.
void registerBootstrappedVersion(const std::string& installDir, LaunchTarget target);

} // namespace tuxblox
