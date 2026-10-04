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

#include "roblox_place_info.h"
#include "json.hpp"
#include "ca_bundle.h"

#include <algorithm>
#include <cctype>

#include <curl/curl.h>

namespace tuxblox {

namespace {

// Long enough for a slow connection, short enough that presence is not held up when Roblox does not answer.
const long RequestTimeoutSeconds = 6;

// Discord truncates in its own way, and a wall of text in a status line helps nobody.
const size_t MaxNameLength = 128;

size_t writeToString(char *pData, size_t size, size_t count, void *pUser) {
    static_cast<std::string *>(pUser)->append(pData, size * count);
    return size * count;
}

std::string trimmed(const std::string& text) {
    const auto notSpace = [](unsigned char c) { return std::isspace(c) == 0; };
    const auto begin = std::find_if(text.begin(), text.end(), notSpace);
    const auto end = std::find_if(text.rbegin(), text.rend(), notSpace).base();
    return begin < end ? std::string(begin, end) : std::string();
}

} // namespace

std::string placeDetailsUrl(const std::string& placeId) {
    const bool isNumber = !placeId.empty() && std::all_of(placeId.begin(), placeId.end(),
                                                          [](unsigned char c) {
                                                              return std::isdigit(c) != 0;
                                                          });
    if (!isNumber) {
        return std::string();
    }
    return "https://economy.roblox.com/v2/assets/" + placeId + "/details";
}

std::string placeGameUrl(const std::string& placeId) {
    if (placeDetailsUrl(placeId).empty()) {
        return std::string();
    }
    return "https://www.roblox.com/games/" + placeId;
}

std::string placeIconsUrl(const std::string& placeId) {
    if (placeDetailsUrl(placeId).empty()) {
        return std::string();
    }
    return "https://thumbnails.roblox.com/v1/places/gameicons?placeIds=" + placeId +
           "&size=512x512&format=Png&isCircular=false";
}

std::string placeIconUrlFromJson(const std::string& body) {
    try {
        const nlohmann::json parsed = nlohmann::json::parse(body);
        const auto data = parsed.find("data");
        if (data == parsed.end() || !data->is_array() || data->empty()) {
            return std::string();
        }
        const nlohmann::json& first = data->front();
        const auto state = first.find("state");
        if (state == first.end() || !state->is_string() || state->get<std::string>() != "Completed") {
            return std::string();
        }
        const auto image = first.find("imageUrl");
        if (image == first.end() || !image->is_string()) {
            return std::string();
        }
        const std::string url = image->get<std::string>();
        // Only https reaches Discord, which fetches whatever it is handed.
        return url.rfind("https://", 0) == 0 ? url : std::string();
    } catch (...) {
        return std::string();
    }
}

std::string placeNameFromJson(const std::string& body) {
    try {
        const nlohmann::json parsed = nlohmann::json::parse(body);
        if (!parsed.is_object()) {
            return std::string();
        }
        const auto name = parsed.find("Name");
        if (name == parsed.end() || !name->is_string()) {
            return std::string();
        }
        std::string text = trimmed(name->get<std::string>());
        if (text.size() > MaxNameLength) {
            text.resize(MaxNameLength);
        }
        return text;
    } catch (...) {
        return std::string();
    }
}

namespace {

std::string httpGet(const std::string& url) {
    if (url.empty()) {
        return std::string();
    }

    CURL *pCurl = curl_easy_init();
    if (pCurl == nullptr) {
        return std::string();
    }
    tuxblox::applyCaBundle(pCurl);

    std::string body;
    curl_easy_setopt(pCurl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(pCurl, CURLOPT_USERAGENT, "TuxBlox-Client/1.0");
    curl_easy_setopt(pCurl, CURLOPT_WRITEFUNCTION, writeToString);
    curl_easy_setopt(pCurl, CURLOPT_WRITEDATA, &body);
    curl_easy_setopt(pCurl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(pCurl, CURLOPT_TIMEOUT, RequestTimeoutSeconds);
    curl_easy_setopt(pCurl, CURLOPT_CONNECTTIMEOUT, RequestTimeoutSeconds);
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

} // namespace

std::string fetchPlaceName(const std::string& placeId) {
    return placeNameFromJson(httpGet(placeDetailsUrl(placeId)));
}

std::string fetchPlaceIconUrl(const std::string& placeId) {
    return placeIconUrlFromJson(httpGet(placeIconsUrl(placeId)));
}

} // namespace tuxblox
