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
#include "user_agent.h"
#include "downloader.h"
#include "release_file.h"
#include "json.hpp"
#include "ca_bundle.h"
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

} // namespace

std::string fetchManifestJson(const std::string& url, const std::atomic<bool>* cancel) {
    CURL* curl = curl_easy_init();
    if (!curl) {
        throw std::runtime_error("fetchManifestJson: curl_easy_init failed");
    }
    tuxblox::applyCaBundle(curl);

    std::string body;
    char errBuf[CURL_ERROR_SIZE] = {0};

    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_USERAGENT, userAgent().c_str());
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

std::optional<std::string> fetchLatestRelease(const std::string& baseUrl, const std::string& channel,
                                               const std::atomic<bool>* cancel) {
    std::string body = fetchManifestJson(latestPointerUrl(baseUrl, channel), cancel);

    const size_t first = body.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) return std::nullopt;
    const std::string release = body.substr(first, body.find_last_not_of(" \t\r\n") - first + 1);

    // Four dot-separated numbers and nothing else. The pointer is a few bytes of plain text from
    // the network, and anything that is not a release version is "nothing to update to" rather
    // than something to go and ask for.
    int fields = 1;
    size_t digits = 0;
    for (char c : release) {
        if (c == '.') {
            if (digits == 0) return std::nullopt;
            digits = 0;
            fields++;
        } else if (c >= '0' && c <= '9') {
            digits++;
        } else {
            return std::nullopt;
        }
    }
    if (digits == 0 || fields != 4) return std::nullopt;
    return release;
}

} // namespace tuxblox
