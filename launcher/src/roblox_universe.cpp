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

#include "roblox_universe.h"

#include "json.hpp"
#include "roblox_http.h"

#include <string>

namespace tuxblox {
namespace {

// Roblox answers with these instead of refusing when it will not describe an experience, and showing them to somebody would be worse than showing nothing.
const char UnavailableName[] = "[TITLE UNAVAILABLE]";
const std::size_t MaxNameLength = 128;

bool allDigits(const std::string& text) {
    if (text.empty()) {
        return false;
    }
    for (const char c : text) {
        if (c < '0' || c > '9') {
            return false;
        }
    }
    return true;
}

} // namespace

std::string universeLookupUrl(const std::string& placeId) {
    if (!allDigits(placeId)) {
        return std::string();
    }
    return "https://apis.roblox.com/universes/v1/places/" + placeId + "/universe";
}

std::string universeIdFromJson(const std::string& body) {
    const nlohmann::json parsed = nlohmann::json::parse(body, nullptr, false);
    if (parsed.is_discarded() || !parsed.is_object()) {
        return std::string();
    }
    const auto id = parsed.find("universeId");
    if (id == parsed.end() || !id->is_number_unsigned()) {
        return std::string();
    }
    return std::to_string(id->get<std::uint64_t>());
}

std::string universeDetailsUrl(const std::string& universeId) {
    if (!allDigits(universeId)) {
        return std::string();
    }
    return "https://games.roblox.com/v1/games?universeIds=" + universeId;
}

bool universeInfoFromJson(const std::string& body, UniverseInfo& into) {
    const nlohmann::json parsed = nlohmann::json::parse(body, nullptr, false);
    if (parsed.is_discarded() || !parsed.is_object()) {
        return false;
    }
    const auto data = parsed.find("data");
    if (data == parsed.end() || !data->is_array() || data->empty()) {
        return false;
    }
    const nlohmann::json& first = data->front();
    if (!first.is_object()) {
        return false;
    }

    UniverseInfo info;
    const auto name = first.find("name");
    if (name == first.end() || !name->is_string()) {
        return false;
    }
    info.name = name->get<std::string>();
    if (info.name.empty() || info.name == UnavailableName) {
        return false;
    }
    if (info.name.size() > MaxNameLength) {
        info.name.resize(MaxNameLength);
    }

    const auto rootPlace = first.find("rootPlaceId");
    if (rootPlace != first.end() && rootPlace->is_number_unsigned() &&
        rootPlace->get<std::uint64_t>() != 0) {
        info.rootPlaceId = std::to_string(rootPlace->get<std::uint64_t>());
    }

    const auto creator = first.find("creator");
    if (creator != first.end() && creator->is_object()) {
        const auto creatorName = creator->find("name");
        if (creatorName != creator->end() && creatorName->is_string()) {
            info.creatorName = creatorName->get<std::string>();
            if (info.creatorName.size() > MaxNameLength) {
                info.creatorName.resize(MaxNameLength);
            }
        }
        const auto verified = creator->find("hasVerifiedBadge");
        if (verified != creator->end() && verified->is_boolean()) {
            info.creatorVerified = verified->get<bool>();
        }
    }
    // Roblox names an experience it will not describe this way too.
    if (info.creatorName == "[UNKNOWN]") {
        return false;
    }

    into = info;
    return true;
}

std::string creatorLine(const UniverseInfo& info) {
    if (info.creatorName.empty()) {
        return std::string();
    }
    return "by " + info.creatorName + (info.creatorVerified ? " \xe2\x98\x91\xef\xb8\x8f" : "");
}

bool fetchUniverseInfoById(const std::string& universeId, UniverseInfo& into) {
    return universeInfoFromJson(httpGet(universeDetailsUrl(universeId)), into);
}

bool fetchUniverseInfo(const std::string& placeId, UniverseInfo& into) {
    const std::string universeId = universeIdFromJson(httpGet(universeLookupUrl(placeId)));
    if (universeId.empty()) {
        return false;
    }
    return universeInfoFromJson(httpGet(universeDetailsUrl(universeId)), into);
}

} // namespace tuxblox
