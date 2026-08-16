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

#include "roblox_deploy.h"
#include "downloader.h"
#include "json.hpp"
#include <curl/curl.h>
#include <map>
#include <sstream>
#include <stdexcept>

namespace tuxblox {

namespace {

size_t curlWriteToString(char* ptr, size_t size, size_t nmemb, void* userdata) {
    auto* out = static_cast<std::string*>(userdata);
    out->append(ptr, size * nmemb);
    return size * nmemb;
}

// Same shape as manifest.cpp's fetchManifestJson -- duplicated rather than
// shared because manifest.cpp's version is specific to TuxBlox's own setup
// origin naming ("fetchManifestJson"), while this fetches arbitrary Roblox
// CDN/API URLs; a future cleanup could factor a single generic "fetchText"
// helper both call, out of scope for this plan.
std::string httpGet(const std::string& url, const std::atomic<bool>* cancel) {
    CURL* curl = curl_easy_init();
    if (!curl) throw std::runtime_error("httpGet: curl_easy_init failed");

    std::string body;
    char errBuf[CURL_ERROR_SIZE] = {0};

    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_USERAGENT, "TuxBlox-Client/1.0");
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, curlWriteToString);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &body);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_ERRORBUFFER, errBuf);
    curl_easy_setopt(curl, CURLOPT_FAILONERROR, 1L);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 30L);
    if (cancel) {
        curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 0L);
        curl_easy_setopt(curl, CURLOPT_XFERINFOFUNCTION,
            +[](void* clientp, curl_off_t, curl_off_t, curl_off_t, curl_off_t) -> int {
                return shouldAbortTransfer(static_cast<const std::atomic<bool>*>(clientp)) ? 1 : 0;
            });
        curl_easy_setopt(curl, CURLOPT_XFERINFODATA, const_cast<std::atomic<bool>*>(cancel));
    }

    CURLcode res = curl_easy_perform(curl);
    long httpCode = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &httpCode);
    curl_easy_cleanup(curl);

    if (res == CURLE_ABORTED_BY_CALLBACK) throw std::runtime_error("httpGet: cancelled");
    if (res != CURLE_OK) {
        throw std::runtime_error(std::string("httpGet: ") + (errBuf[0] ? errBuf : curl_easy_strerror(res)));
    }
    if (httpCode < 200 || httpCode >= 300) {
        throw std::runtime_error("httpGet: HTTP " + std::to_string(httpCode) + " for " + url);
    }
    return body;
}

// Ported from the publicly documented RDD/Bloxstrap package->folder
// mapping. NOT independently re-verified against a live Roblox deployment
// -- covers the packages this table's author was confident about. Anything
// else returns nullopt (see packageInstallSubdir's header doc).
const std::map<std::string, std::string>& knownPackageSubdirs() {
    static const std::map<std::string, std::string> kMap = {
        {"RobloxApp.zip", ""},
        {"redist.zip", ""},
        {"shaders.zip", "shaders/"},
        {"ssl.zip", "ssl/"},
        {"WebView2.zip", "WebView2/"},
        {"content-avatar.zip", "content/avatar/"},
        {"content-configs.zip", "content/configs/"},
        {"content-fonts.zip", "content/fonts/"},
        {"content-sky.zip", "content/sky/"},
        {"content-sounds.zip", "content/sounds/"},
        {"content-textures2.zip", "content/textures/"},
        {"content-models.zip", "content/models/"},
        {"content-textures3.zip", "PlatformContent/pc/textures/"},
        {"content-terrain.zip", "PlatformContent/pc/terrain/"},
        {"content-platform-fonts.zip", "PlatformContent/pc/fonts/"},
        {"extracontent-luapackages.zip", "ExtraContent/LuaPackages/"},
        {"extracontent-translations.zip", "ExtraContent/translations/"},
        {"extracontent-models.zip", "ExtraContent/models/"},
        {"extracontent-textures.zip", "ExtraContent/textures/"},
        {"extracontent-places.zip", "ExtraContent/places/"},
        // Studio-only packages.
        {"BuiltInPlugins.zip", "BuiltInPlugins/"},
        {"ApplicationConfig.zip", "ApplicationConfig/"},
        {"Plugins.zip", "Plugins/"},
        {"StudioFonts.zip", "StudioFonts/"},
        {"Qml.zip", "Qml/"},
        {"Bin.zip", ""},
    };
    return kMap;
}

} // namespace

const char* robloxBinaryType(LaunchTarget target) {
    return target == LaunchTarget::Player ? "WindowsPlayer" : "WindowsStudio64";
}

std::string fetchClientVersionJson(const std::string& binaryType, const std::string& channel,
                                    const std::atomic<bool>* cancel) {
    std::string url = "https://clientsettings.roblox.com/v2/client-version/" + binaryType;
    if (channel != "live" && !channel.empty()) {
        url += "/channel/" + channel;
    }
    return httpGet(url, cancel);
}

std::string parseClientVersionHash(const std::string& json) {
    try {
        nlohmann::json j = nlohmann::json::parse(json);
        // "clientVersionUpload" is the field name documented publicly by
        // RDD/Bloxstrap for this endpoint's response -- not independently
        // re-verified against a live response in this repo. If this ever
        // turns out wrong, this is the one line to fix.
        return j.at("clientVersionUpload").get<std::string>();
    } catch (const nlohmann::json::exception& e) {
        throw std::runtime_error(std::string("parseClientVersionHash: ") + e.what());
    }
}

std::string fetchDeployHistory(const std::atomic<bool>* cancel) {
    return httpGet("https://setup.rbxcdn.com/DeployHistory.txt", cancel);
}

std::string fetchText(const std::string& url, const std::atomic<bool>* cancel) {
    return httpGet(url, cancel);
}

std::optional<std::string> previousVersionFromDeployHistory(const std::string& deployHistoryText,
                                                              const std::string& binaryType,
                                                              const std::string& currentHash) {
    // Publicly documented line format: "New <BinaryType> version-<hash> at
    // <date>" -- NOT independently re-verified against a live fetch, see
    // this module's header doc.
    std::vector<std::string> hashesForType;
    std::istringstream stream(deployHistoryText);
    std::string line;
    const std::string prefix = "New " + binaryType + " ";
    while (std::getline(stream, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (line.compare(0, prefix.size(), prefix) != 0) continue;
        std::string rest = line.substr(prefix.size());
        size_t spacePos = rest.find(' ');
        if (spacePos == std::string::npos) continue;
        hashesForType.push_back(rest.substr(0, spacePos));
    }

    for (size_t i = 0; i < hashesForType.size(); ++i) {
        if (hashesForType[i] == currentHash) {
            if (i == 0) return std::nullopt; // oldest known entry for this type -- no predecessor
            return hashesForType[i - 1];
        }
    }
    return std::nullopt; // currentHash not present in this log at all
}

std::vector<PackageEntry> parsePackageManifest(const std::string& manifestText) {
    std::vector<std::string> lines;
    std::istringstream stream(manifestText);
    std::string line;
    while (std::getline(stream, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (!line.empty()) lines.push_back(line);
    }
    if (lines.empty()) throw std::runtime_error("parsePackageManifest: empty manifest");

    // First line is a version marker ("v0"/"v1") -- not otherwise used.
    std::vector<std::string> entryLines(lines.begin() + 1, lines.end());
    if (entryLines.size() % 4 != 0) {
        throw std::runtime_error("parsePackageManifest: entry line count not a multiple of 4");
    }

    std::vector<PackageEntry> packages;
    for (size_t i = 0; i + 3 < entryLines.size(); i += 4) {
        PackageEntry pkg;
        pkg.name = entryLines[i];
        pkg.md5 = entryLines[i + 1];
        try {
            pkg.packedSize = std::stoull(entryLines[i + 2]);
            pkg.unpackedSize = std::stoull(entryLines[i + 3]);
        } catch (const std::exception& e) {
            throw std::runtime_error(std::string("parsePackageManifest: bad size field: ") + e.what());
        }
        packages.push_back(pkg);
    }
    return packages;
}

std::optional<std::string> packageInstallSubdir(const std::string& packageName, LaunchTarget /*target*/) {
    // `target` is accepted for forward-compatibility (some packages could
    // theoretically map differently per app type) but every entry in
    // knownPackageSubdirs() today is target-independent.
    auto it = knownPackageSubdirs().find(packageName);
    if (it == knownPackageSubdirs().end()) return std::nullopt;
    return it->second;
}

std::string setupCdnUrl(const std::string& mirrorHost, const std::string& hash, const std::string& filename) {
    return "https://" + mirrorHost + "/" + hash + "-" + filename;
}

} // namespace tuxblox
