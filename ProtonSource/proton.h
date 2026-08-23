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
#include <functional>
#include <string>
#include <vector>

#include "filelock.h"
#include "util.h"

namespace tuxblox {

// Runs a command and returns its exit code. Session provides the real one;
// keeping it a callback stops Proton from having to know about Session.
using ProcessRunner =
    std::function<int(const std::vector<std::string>& command, const Environment& env)>;

// The installed TuxBlox distribution: where its binaries, libraries and the
// template prefix live. Everything here is derived from the install directory.
class Proton {
public:
    explicit Proton(const std::filesystem::path& baseDir);

    std::filesystem::path path(const std::string& relative) const;

    // Directory holding a component's PE files for either the host
    // architecture or the 32-bit one Wine uses for WoW64 processes.
    std::filesystem::path archPeDir(const std::string& component, bool wow64) const;

    // Removes the dist directory older releases installed alongside this one.
    void cleanupLegacyDist();

    bool missingDefaultPrefix() const;

    // Creates the template prefix that new prefixes are copied from. Does
    // nothing when it already exists.
    void makeDefaultPrefix(const Environment& sessionEnv, const ProcessRunner& runProc);

    std::filesystem::path baseDir;
    std::filesystem::path distDir;
    std::filesystem::path binDir;
    std::filesystem::path libDir;
    std::filesystem::path fontsDir;
    std::filesystem::path mediaDir;
    std::filesystem::path wineFontsDir;
    std::filesystem::path wineInf;
    std::filesystem::path defaultPfxDir;
    std::filesystem::path userSettingsFile;
    std::filesystem::path wineBin;
    std::filesystem::path wineserverBin;

    std::string hostPeArch = "x86_64-windows";
    std::string wow64PeArch = "i386-windows";

    FileLock distLock;
};

} // namespace tuxblox
