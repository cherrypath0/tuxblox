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
#include <optional>
#include <string>

namespace tuxblox {

struct Artifact {
    std::string url; // always absolute -- resolved against baseUrl at parse time
    std::string sha256;
    uint64_t sizeBytes = 0;
};

struct Manifest {
    int manifestVersion = 0;
    std::string channel;
    Artifact proton;
    Artifact launcher;
    Artifact installer;
};

// `baseUrl` (e.g. "https://setup.tuxblox.net", no trailing slash) resolves
// each artifact's manifest-relative url ("/v1/canary/0.2.0/launcher") to an
// absolute one -- the per-version manifest schema no longer embeds a
// tuxblox_version/proton_version of its own; the single version this
// manifest represents is implied by which /v1/<channel>/<version>/
// manifest.json was fetched, not repeated inside the JSON body.
Manifest parseManifest(const std::string& jsonText, const std::string& baseUrl);
std::string fetchManifestJson(const std::string& url, const std::atomic<bool>* cancel = nullptr);

// Fetches baseUrl + "/v2/latest.json" and returns the currently-latest
// version for `channel`, or std::nullopt if that channel has no releases
// published yet (an empty string in latest.json).
std::optional<std::string> fetchLatestVersion(const std::string& baseUrl, const std::string& channel,
                                               const std::atomic<bool>* cancel = nullptr);

} // namespace tuxblox
