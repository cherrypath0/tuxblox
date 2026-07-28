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

Artifact parseArtifact(const nlohmann::json& artifacts, const char* name) {
    if (!artifacts.contains(name)) {
        throw std::runtime_error(std::string("manifest missing artifact: ") + name);
    }
    try {
        const auto& a = artifacts.at(name);
        Artifact artifact;
        artifact.url = a.at("url").get<std::string>();
        artifact.sha256 = a.at("sha256").get<std::string>();
        artifact.sizeBytes = a.at("size_bytes").get<uint64_t>();
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
constexpr int kSupportedManifestVersion = 1;

} // namespace

Manifest parseManifest(const std::string& jsonText) {
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
        m.tuxbloxVersion = j.at("tuxblox_version").get<std::string>();
        m.protonVersion = j.at("proton_version").get<std::string>();
        m.protonbuild = parseArtifact(j.at("artifacts"), "protonbuild");
        m.launcher = parseArtifact(j.at("artifacts"), "launcher");
        m.installer = parseArtifact(j.at("artifacts"), "installer");
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

} // namespace tuxblox
