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
#include <string>

namespace tuxblox {

struct Artifact {
    std::string url;
    std::string sha256;
    uint64_t sizeBytes = 0;
};

struct Manifest {
    int manifestVersion = 0;
    std::string tuxbloxVersion;
    std::string protonVersion;
    Artifact protonbuild;
    Artifact launcher;
    Artifact installer;
};

Manifest parseManifest(const std::string& jsonText);
std::string fetchManifestJson(const std::string& url, const std::atomic<bool>* cancel = nullptr);

} // namespace tuxblox
