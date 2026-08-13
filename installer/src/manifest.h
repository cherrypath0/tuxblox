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
#include <map>
#include <optional>
#include <string>

namespace tuxblox {

struct Artifact {
    std::string url; // always absolute -- resolved against baseUrl at parse time
    std::string sha256;
    uint64_t sizeBytes = 0;
    std::string displayname; // shown in install progress, e.g. "Proton"
    // Extension-less -- the real extension (including multi-part ones like
    // ".tar.zst") lives on the download slug itself (url's own basename),
    // same convention setup.tuxblox.net's Content-Disposition uses.
    std::string filename;
    // Relative to installDir, e.g. "/" (installDir itself) or "/somefolder"
    // (installDir/somefolder). Not necessarily just "/" forever -- this is
    // what lets a future artifact install somewhere other than the top
    // level without installer_steps.cpp needing to know about it by name.
    std::string path;
};

struct Manifest {
    int manifestVersion = 0;
    std::string channel;
    // Keyed by component name ("launcher", "installer", "proton", "mcp",
    // and whatever else a manifest lists) -- deliberately not fixed named
    // fields, so installing "everything in artifacts" (see
    // installer_steps.cpp) needs no hardcoded set of expected keys.
    std::map<std::string, Artifact> artifacts;
};

// `baseUrl` (e.g. "https://setup.tuxblox.net", no trailing slash) resolves
// each artifact's manifest-relative url ("/v1/canary/0.2.0/launcher") to an
// absolute one -- the per-version manifest schema no longer embeds a
// tuxblox_version of its own; the single version this manifest represents
// is implied by which /v1/<channel>/<version>/manifest.json was fetched,
// not repeated inside the JSON body.
//
// Throws std::runtime_error (with a human-readable message) if the JSON is
// malformed or missing required fields.
Manifest parseManifest(const std::string& jsonText, const std::string& baseUrl);

// Fetches manifest.json from `url` over HTTPS and returns the raw body.
// Throws std::runtime_error on network failure or non-2xx HTTP status.
// If `cancel` is non-null and becomes true mid-transfer, the request is
// aborted promptly (rather than blocking until CURLOPT_TIMEOUT) and a
// "cancelled" std::runtime_error is thrown.
std::string fetchManifestJson(const std::string& url,
                               const std::atomic<bool>* cancel = nullptr);

// Fetches baseUrl + "/v2/latest.json" and returns the currently-latest
// version for `channel`, or std::nullopt if that channel has no releases
// published yet (an empty string in latest.json).
std::optional<std::string> fetchLatestVersion(const std::string& baseUrl, const std::string& channel,
                                               const std::atomic<bool>* cancel = nullptr);

} // namespace tuxblox
