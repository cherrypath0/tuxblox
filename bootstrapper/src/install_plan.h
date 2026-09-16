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
#include "config.h"
#include "roblox_deploy.h"
#include <cstdint>
#include <string>
#include <vector>

namespace tuxblox {

// Which mirrors to try, in order. Both known mirrors serve identical
// content, so an unpinned run falls back to the other one; a run that named
// a server gets only that server.
std::vector<std::string> mirrorsFor(const Config& config);

// <installDir>/<hash>, with any trailing slash on installDir ignored.
std::string versionDir(const std::string& installDir, const std::string& hash);

// Where one package's contents belong inside a version directory. An empty
// subdir means the version's own root.
std::string packageDestDir(const std::string& versionDir, const std::string& subdir);

// One package to fetch and unpack.
struct PackageJob {
    std::string name;
    std::string md5;
    uint64_t packedSize = 0;
    std::string archivePath;
    std::string destDir;
};

// Turns a parsed manifest into the work to do, dropping any package whose
// destination is unknown rather than guessing where its files belong.
std::vector<PackageJob> buildJobs(const std::vector<PackageEntry>& packages, RobloxApp app,
                                   const std::string& stagingDir);

// Total bytes to download, which is what the progress bar measures.
uint64_t totalPackedBytes(const std::vector<PackageJob>& jobs);

// The executable a finished version must contain.
const char* versionExecutable(RobloxApp app);

// True when `versionDir` holds a usable install. A directory without the
// executable is an interrupted or incomplete install, not an installed
// version -- the same rule the launcher's version list applies.
bool versionIsInstalled(const std::string& versionDir, RobloxApp app);

// True when the directory holds either app's executable. Used when an exact
// version was named: the hash alone decides whether it is Player or Studio,
// so the configured app must not make an installed version look missing.
bool versionIsInstalledForAnyApp(const std::string& versionDir);

// The file the official Roblox installer drops into every version directory.
// Roblox's client reads it to find its content folder and will not start
// without it.
std::string appSettingsXml();

} // namespace tuxblox
