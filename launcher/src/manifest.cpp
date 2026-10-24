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
#include <cstdio>
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

std::string fetchManifestJson(const std::string& url, const std::atomic<bool>* cancel,
                               long* httpStatus) {
    CURL* curl = curl_easy_init();
    if (!curl) {
        throw std::runtime_error("could not start the download");
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

    if (httpStatus != nullptr) *httpStatus = httpCode;

    if (res == CURLE_ABORTED_BY_CALLBACK) {
        throw std::runtime_error("the download was cancelled");
    }
    if (res != CURLE_OK) {
        throw std::runtime_error(std::string("could not reach tuxblox.net: ") +
                                  (errBuf[0] ? errBuf : curl_easy_strerror(res)));
    }
    // For file:// URLs, httpCode is 0 (not an HTTP request), so skip the check
    if (httpCode != 0 && (httpCode < 200 || httpCode >= 300)) {
        throw std::runtime_error("tuxblox.net answered " + std::to_string(httpCode));
    }
    return body;
}

std::optional<std::string> fetchLatestRelease(const std::string& baseUrl, const std::string& channel,
                                               const std::atomic<bool>* cancel) {
    // A channel that has never been published has no pointer at all. In the old layout that was
    // an empty string in latest.json and read as "nothing to update to"; here the file simply is
    // not there, and a 404 means exactly the same thing. Treating it as a failure turned a normal
    // state into a red error on the Home screen.
    std::string body;
    long httpStatus = 0;
    // Only the default edge has somewhere to fall back to; see releasesEdgeUrl().
    const bool mayFallBack = baseUrl == releasesEdgeUrl();
    try {
        body = fetchManifestJson(latestPointerUrl(baseUrl, channel), cancel, &httpStatus);
    } catch (const std::exception&) {
        // A 404 means "this channel has nothing published" only when the ORIGIN says it. From the
        // edge it is indistinguishable from a misrouted pull zone, and taking it at face value
        // would report "no updates" forever while the origin had them all along.
        if (httpStatus == 404 && !mayFallBack) return std::nullopt;
        // The edge could not answer. The origin is behind it and still can, so an edge that is
        // down delays an update rather than making one impossible -- and this is the request that
        // decides whether there IS an update, so failing it quietly stops them altogether.
        if (!mayFallBack) throw;
        fprintf(stderr, "TuxBlox: %s did not serve the release pointer; asking %s instead\n",
                baseUrl.c_str(), releasesOriginUrl().c_str());
        httpStatus = 0;
        try {
            body = fetchManifestJson(latestPointerUrl(releasesOriginUrl(), channel), cancel, &httpStatus);
        } catch (const std::exception&) {
            // Its own try, because fetchManifestJson THROWS on a 404 -- a bare check after the call
            // never runs, and a channel with genuinely nothing published reported a raw HTTP error
            // instead of saying so. Now the origin has spoken, so a 404 means what it says.
            if (httpStatus == 404) return std::nullopt;
            throw;
        }
    }

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
