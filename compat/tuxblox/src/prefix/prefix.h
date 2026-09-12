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
//
// Portions derived from Proton's proton.py:
// Copyright (c) 2018-2022, Valve Corporation. All rights reserved.
// Licensed under the 3-clause BSD license; see
// third_party_licenses/proton/LICENSE.proton for the full text.

#pragma once
#include <filesystem>
#include <string>

#include "support/filelock.h"
#include "launch/proton.h"
#include "launch/session.h"

namespace tuxblox {

// The Wine prefix Roblox runs in. Created from the template prefix the build
// ships, then kept in step with the installed TuxBlox on every launch.
class Prefix {
public:
    Prefix(Proton& proton, std::filesystem::path baseDir, std::string prefixVersion);

    // Brings the prefix up to date, creating it if it is missing. Reads the
    // session's graphics settings to decide which Direct3D files to install,
    // and adds the DLL overrides they need.
    void setup(Session& session);

    // What a removeTrackedFiles() pass is for.
    enum class Removal {
        // Everything this launcher put in the prefix goes, registry included.
        All,
        // An upgrade: the files carrying state the prefix accumulated stay,
        // since they are no longer only ours to replace.
        KeepAccumulatedState,
    };

    // Deletes everything this launcher put in the prefix, leaving whatever
    // the user or Roblox added.
    void removeTrackedFiles(Removal removal = Removal::All);

    std::filesystem::path prefixDir;

private:
    std::filesystem::path path(const std::string& relative) const;

    std::string readVersion() const;
    void writeVersion() const;

    void copyTemplatePrefix();
    void copyTemplateEntry(const std::filesystem::path& src,
                           const std::filesystem::path& dst, bool dllCopy);
    void updateBuiltinLibs(const std::string& copyPatterns);
    void createFontSymlinks();
    void installGraphicsFiles(Session& session);
    void migrateUserPaths();
    void syncHostTheme();
    void syncHaptics();

    Proton& proton;
    std::filesystem::path baseDir;
    std::string prefixVersion;
    std::filesystem::path versionFile;
    std::filesystem::path configInfoFile;
    std::filesystem::path trackedFilesFile;
    std::filesystem::path creationGuard;
    FileLock prefixLock;
};

// Reads the host desktop's light/dark preference. Returns an empty string when
// the host states no preference.
std::string detectHostColorScheme();

} // namespace tuxblox
