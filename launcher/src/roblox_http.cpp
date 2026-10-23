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

#include "roblox_http.h"
#include "user_agent.h"

#include "ca_bundle.h"

#include <curl/curl.h>

#include <string>

namespace tuxblox {
namespace {

std::size_t writeToString(char *pData, std::size_t size, std::size_t count, void *pUser) {
    static_cast<std::string *>(pUser)->append(pData, size * count);
    return size * count;
}

} // namespace

std::string httpGet(const std::string& url, long timeoutSeconds) {
    if (url.empty()) {
        return std::string();
    }

    CURL *pCurl = curl_easy_init();
    if (pCurl == nullptr) {
        return std::string();
    }
    applyCaBundle(pCurl);

    std::string body;
    curl_easy_setopt(pCurl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(pCurl, CURLOPT_USERAGENT, userAgent().c_str());
    curl_easy_setopt(pCurl, CURLOPT_WRITEFUNCTION, writeToString);
    curl_easy_setopt(pCurl, CURLOPT_WRITEDATA, &body);
    curl_easy_setopt(pCurl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(pCurl, CURLOPT_TIMEOUT, timeoutSeconds);
    curl_easy_setopt(pCurl, CURLOPT_CONNECTTIMEOUT, timeoutSeconds);
    curl_easy_setopt(pCurl, CURLOPT_NOSIGNAL, 1L);

    const CURLcode code = curl_easy_perform(pCurl);
    long status = 0;
    curl_easy_getinfo(pCurl, CURLINFO_RESPONSE_CODE, &status);
    curl_easy_cleanup(pCurl);

    if (code != CURLE_OK || status != 200) {
        return std::string();
    }
    return body;
}

} // namespace tuxblox
