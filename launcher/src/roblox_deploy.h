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
#include <vector>
#include "lnk_resolver.h"

namespace tuxblox {

// "WindowsPlayer" / "WindowsStudio64" -- Roblox's own binaryType strings,
// used both in the clientsettings API path and in DeployHistory.txt lines.
const char* robloxBinaryType(LaunchTarget target);

// Fetches https://clientsettings.roblox.com/v2/client-version/{binaryType}
// (channel segment "/channel/{channel}" appended unless channel == "live",
// matching Roblox's default-channel convention). Raw curl GET, same
// pattern as manifest.cpp's fetchManifestJson. Throws std::runtime_error on
// any transport/HTTP error.
std::string fetchClientVersionJson(const std::string& binaryType, const std::string& channel,
                                    const std::atomic<bool>* cancel = nullptr);

// Extracts the version hash from a clientsettings v2 response body (field
// "clientVersionUpload" -- see this module's .cpp for the verification
// caveat). Throws std::runtime_error if the field is missing or the body
// isn't valid JSON.
std::string parseClientVersionHash(const std::string& json);

// Fetches the raw https://setup.rbxcdn.com/DeployHistory.txt body.
std::string fetchDeployHistory(const std::atomic<bool>* cancel = nullptr);

// Generic HTTP GET returning the response body as text -- exposed publicly
// (not just used internally by the two functions above) because Task 6
// (App's install thread) also uses this directly to fetch
// {hash}-rbxPkgManifest.txt, which has no dedicated wrapper function of its
// own since its URL varies per-hash/per-mirror in a way the two fetchers
// above don't need to handle. Throws std::runtime_error on any
// transport/HTTP error.
std::string fetchText(const std::string& url, const std::atomic<bool>* cancel = nullptr);

// Scans `deployHistoryText` for lines matching binaryType, finds the entry
// for `currentHash`, and returns the hash of the entry immediately before
// it in the log (same binary type only). Returns std::nullopt if
// currentHash isn't found, or has no earlier entry -- both expected
// outcomes (a purged/unknown hash, or the very first deploy), not errors.
std::optional<std::string> previousVersionFromDeployHistory(const std::string& deployHistoryText,
                                                              const std::string& binaryType,
                                                              const std::string& currentHash);

struct PackageEntry {
    std::string name;        // e.g. "RobloxApp.zip"
    std::string md5;         // hex, from the manifest
    uint64_t packedSize = 0;
    uint64_t unpackedSize = 0;
};

// Parses a raw {hash}-rbxPkgManifest.txt body: a version-marker first line,
// then groups of 4 lines (filename, MD5, packed size, unpacked size) until
// EOF. Throws std::runtime_error if the line count after the marker isn't
// a multiple of 4, or a size field isn't a valid integer.
std::vector<PackageEntry> parsePackageManifest(const std::string& manifestText);

// Well-known package -> relative-install-subdir mapping (empty string
// means "extract to the version's root directory"). Ported from the
// publicly documented RDD/Bloxstrap package-mapping convention -- NOT
// independently re-verified against a live Roblox deployment in this repo.
// Returns std::nullopt for anything not in this table -- callers must skip
// unknown packages with a visible warning rather than guessing a
// destination, since extracting to the wrong place could scatter files
// across the versions/ directory tree.
std::optional<std::string> packageInstallSubdir(const std::string& packageName, LaunchTarget target);

// "https://" + mirrorHost + "/" + hash + "-" + filename -- both known
// mirrors (setup.rbxcdn.com, setup-aws.rbxcdn.com) serve identical content
// per the spec; callers retry with the alternate mirror on failure.
std::string setupCdnUrl(const std::string& mirrorHost, const std::string& hash, const std::string& filename);

} // namespace tuxblox
