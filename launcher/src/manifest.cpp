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

#include "manifest.h"
#include "downloader.h"
#include "json.hpp"
#include <curl/curl.h>
#include <stdexcept>
#include <string>

namespace tuxblox {

namespace {

size_t curlWriteToString(char* ptr, size_t size, size_t nmemb, void* userdata) {
    auto* out = static_cast<std::string*>(userdata);
    out->append(ptr, size * nmemb);
    return size * nmemb;
}

int curlCancelCallback(void* clientp, curl_off_t /*dltotal*/, curl_off_t /*dlnow*/,
                        curl_off_t /*ultotal*/, curl_off_t /*ulnow*/) {
    const auto* cancel = static_cast<const std::atomic<bool>*>(clientp);
    return shouldAbortTransfer(cancel) ? 1 : 0;
}

// A manifest artifact's "url" is always a path relative to the setup
// origin ("/v1/canary/0.2.0/launcher"), never a full URL -- resolving it
// here (once, at parse time) means every other artifact.url use site just
// gets something curl can fetch directly, same as before this schema
// change.
std::string resolveUrl(const std::string& baseUrl, const std::string& maybeRelative) {
    if (maybeRelative.rfind("http://", 0) == 0 || maybeRelative.rfind("https://", 0) == 0 ||
        maybeRelative.rfind("file://", 0) == 0) {
        return maybeRelative; // already absolute -- defensive, the server never emits this today
    }
    return baseUrl + maybeRelative;
}

Artifact parseArtifact(const nlohmann::json& artifacts, const char* name, const std::string& baseUrl) {
    if (!artifacts.contains(name)) {
        throw std::runtime_error(std::string("manifest missing artifact: ") + name);
    }
    try {
        const auto& a = artifacts.at(name);
        Artifact artifact;
        artifact.url = resolveUrl(baseUrl, a.at("url").get<std::string>());
        artifact.sha256 = a.at("sha256").get<std::string>();
        artifact.sizeBytes = a.at("size").get<uint64_t>();
        return artifact;
    } catch (const nlohmann::json::exception& e) {
        throw std::runtime_error(std::string("manifest artifact '") + name + "' field error: " + e.what());
    }
}

// The manifest.json schema version this launcher understands. Bump this
// (and re-verify every field access below still matches) when the schema
// changes -- kept as a single named constant rather than a bare literal so
// it's the one obvious place to change, not a magic number buried in a
// comparison.
constexpr int kSupportedManifestVersion = 2;

} // namespace

Manifest parseManifest(const std::string& jsonText, const std::string& baseUrl) {
    nlohmann::json j;
    try {
        j = nlohmann::json::parse(jsonText);
    } catch (const nlohmann::json::parse_error& e) {
        throw std::runtime_error(std::string("manifest is not valid JSON: ") + e.what());
    }

    try {
        if (!j.contains("artifacts")) {
            throw std::runtime_error("manifest missing 'artifacts' object");
        }

        Manifest m;
        m.manifestVersion = j.at("manifest_version").get<int>();
        if (m.manifestVersion != kSupportedManifestVersion) {
            throw std::runtime_error("unsupported manifest_version: " +
                                     std::to_string(m.manifestVersion) +
                                     " (this launcher supports version " +
                                     std::to_string(kSupportedManifestVersion) + ")");
        }
        m.channel = j.at("channel").get<std::string>();
        m.proton = parseArtifact(j.at("artifacts"), "proton", baseUrl);
        m.launcher = parseArtifact(j.at("artifacts"), "launcher", baseUrl);
        m.installer = parseArtifact(j.at("artifacts"), "installer", baseUrl);
        return m;
    } catch (const std::runtime_error&) {
        throw;
    } catch (const nlohmann::json::exception& e) {
        throw std::runtime_error(std::string("manifest field error: ") + e.what());
    }
}

std::string fetchManifestJson(const std::string& url, const std::atomic<bool>* cancel) {
    CURL* curl = curl_easy_init();
    if (!curl) {
        throw std::runtime_error("fetchManifestJson: curl_easy_init failed");
    }

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
    curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 0L);
    curl_easy_setopt(curl, CURLOPT_XFERINFOFUNCTION, curlCancelCallback);
    curl_easy_setopt(curl, CURLOPT_XFERINFODATA, const_cast<std::atomic<bool>*>(cancel));

    CURLcode res = curl_easy_perform(curl);
    long httpCode = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &httpCode);
    curl_easy_cleanup(curl);

    if (res == CURLE_ABORTED_BY_CALLBACK) {
        throw std::runtime_error("fetchManifestJson: cancelled");
    }
    if (res != CURLE_OK) {
        throw std::runtime_error(std::string("fetchManifestJson: ") +
                                  (errBuf[0] ? errBuf : curl_easy_strerror(res)));
    }
    // For file:// URLs, httpCode is 0 (not an HTTP request), so skip the check
    if (httpCode != 0 && (httpCode < 200 || httpCode >= 300)) {
        throw std::runtime_error("fetchManifestJson: HTTP " + std::to_string(httpCode));
    }
    return body;
}

std::optional<std::string> fetchLatestVersion(const std::string& baseUrl, const std::string& channel,
                                               const std::atomic<bool>* cancel) {
    std::string json = fetchManifestJson(baseUrl + "/v2/latest.json", cancel);

    nlohmann::json j;
    try {
        j = nlohmann::json::parse(json);
        const std::string version = j.at("channels").at(channel).get<std::string>();
        return version.empty() ? std::nullopt : std::make_optional(version);
    } catch (const nlohmann::json::exception& e) {
        throw std::runtime_error(std::string("latest.json field error: ") + e.what());
    }
}

} // namespace tuxblox
