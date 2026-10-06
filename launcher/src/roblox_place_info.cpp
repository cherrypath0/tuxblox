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
#include "roblox_http.h"

#include <algorithm>
#include <cctype>


namespace tuxblox {

namespace {

// Long enough for a slow connection, short enough that presence is not held up when Roblox does not answer.

// Discord truncates in its own way, and a wall of text in a status line helps nobody.
const size_t MaxNameLength = 128;

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

std::string placeJoinUrl(const std::string& placeId, const std::string& jobId,
                         const std::string& launchData) {
    if (placeDetailsUrl(placeId).empty() || jobId.empty()) {
        return std::string();
    }
    std::string url = "https://www.roblox.com/games/start?placeId=" + placeId +
                      "&gameInstanceId=" + jobId;
    if (launchData.empty()) {
        return url;
    }

    url += "&launchData=";
    for (const unsigned char c : launchData) {
        const bool plain = (c >= '0' && c <= '9') || (c >= 'A' && c <= 'Z') ||
                           (c >= 'a' && c <= 'z') || c == '-' || c == '_' || c == '.' || c == '~';
        if (plain) {
            url.push_back(static_cast<char>(c));
            continue;
        }
        static const char hex[] = "0123456789ABCDEF";
        url.push_back('%');
        url.push_back(hex[c >> 4]);
        url.push_back(hex[c & 0x0f]);
    }
    return url;
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


} // namespace

std::string fetchPlaceName(const std::string& placeId) {
    return placeNameFromJson(httpGet(placeDetailsUrl(placeId), 6));
}

std::string fetchPlaceIconUrl(const std::string& placeId) {
    return placeIconUrlFromJson(httpGet(placeIconsUrl(placeId), 6));
}

std::string fetchThumbnailImageUrl(const std::string& lookupUrl) {
    // Every thumbnail Roblox serves is described the same way, so the place icon's reader answers for an asset too.
    return placeIconUrlFromJson(httpGet(lookupUrl, 6));
}

} // namespace tuxblox
