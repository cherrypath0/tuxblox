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

#include "bloxstrap_rpc.h"

#include "json.hpp"

#include <cstdint>
#include <string>

namespace tuxblox {
namespace {

const char Marker[] = "[BloxstrapRPC]";
// A game writes these, so the line is bounded before anything parses it.
const std::size_t MaxLineLength = 4 * 1024;
// What Discord accepts in details and state.
const std::size_t MaxTextLength = 128;
const char ResetValue[] = "<reset>";
const char AssetPrefix[] = "rbxassetid://";

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

// Control characters would reach Discord verbatim, and the limit is in characters rather than bytes because that is what Discord counts -- a status written in anything but Latin script would otherwise lose most of itself.
std::string cleanText(const std::string& text) {
    std::string out;
    out.reserve(text.size());
    std::size_t characters = 0;
    for (const unsigned char c : text) {
        const bool starts = (c & 0xc0) != 0x80;
        if (starts && characters == MaxTextLength) {
            break;
        }
        if (starts) {
            characters++;
        }
        out.push_back(c < 0x20 || c == 0x7f ? ' ' : static_cast<char>(c));
    }
    return out;
}

void readField(const nlohmann::json& data, const char *pName, RpcField& into) {
    if (!data.contains(pName) || !data[pName].is_string()) {
        return;
    }
    const std::string text = data[pName].get<std::string>();
    into.value.clear();
    if (text == ResetValue) {
        into.state = RpcField::State::Reset;
        return;
    }
    // The protocol's way of dropping a field, which is not the same as showing an empty one.
    if (text.empty()) {
        into.state = RpcField::State::Clear;
        return;
    }
    into.state = RpcField::State::Set;
    into.value = cleanText(text);
}

void readImage(const nlohmann::json& data, const char *pName, RpcImage& into) {
    if (!data.contains(pName) || !data[pName].is_object()) {
        return;
    }
    const nlohmann::json& image = data[pName];

    if (image.contains("assetId")) {
        std::string id;
        if (image["assetId"].is_number_unsigned()) {
            id = std::to_string(image["assetId"].get<std::uint64_t>());
        } else if (image["assetId"].is_string()) {
            id = image["assetId"].get<std::string>();
            if (id.rfind(AssetPrefix, 0) == 0) {
                id = id.substr(sizeof(AssetPrefix) - 1);
            }
        }
        if (allDigits(id) && id != into.assetId.value) {
            into.assetId.state = RpcField::State::Set;
            into.assetId.value = id;
            into.resolvedUrl.clear();
        } else if (allDigits(id)) {
            into.assetId.state = RpcField::State::Set;
        }
    }
    readField(image, "hoverText", into.hoverText);

    // These two are commands, not state: acting on them once is what lets a game clear its art on
    // leaving a stage and name new art on entering the next, which holding them as flags would not.
    if (image.contains("clear") && image["clear"].is_boolean() && image["clear"].get<bool>()) {
        into.assetId = RpcField{RpcField::State::Clear, std::string()};
        into.hoverText = RpcField{RpcField::State::Clear, std::string()};
        into.resolvedUrl.clear();
    }
    if (image.contains("reset") && image["reset"].is_boolean() && image["reset"].get<bool>()) {
        into.assetId = RpcField{RpcField::State::Reset, std::string()};
        into.hoverText = RpcField{RpcField::State::Reset, std::string()};
        into.resolvedUrl.clear();
    }
}

} // namespace

bool isBloxstrapRpcLine(const std::string& line) {
    return line.find(Marker) != std::string::npos;
}

bool applyBloxstrapRpcLine(const std::string& line, RpcOverride& into) {
    if (line.size() > MaxLineLength) {
        return false;
    }
    const std::size_t at = line.find(Marker);
    if (at == std::string::npos) {
        return false;
    }

    const nlohmann::json message =
        nlohmann::json::parse(line.substr(at + sizeof(Marker) - 1), nullptr, false);
    if (message.is_discarded() || !message.is_object()) {
        return false;
    }
    if (!message.contains("command") || !message["command"].is_string()) {
        return false;
    }
    const std::string command = message["command"].get<std::string>();
    if (command != "SetRichPresence" && command != "SetLaunchData") {
        return false;
    }
    if (!message.contains("data") || !message["data"].is_object()) {
        return false;
    }

    const nlohmann::json& data = message["data"];

    // The other command carries what a join link should hand to the experience, and touches nothing that is on show.
    if (command == "SetLaunchData") {
        if (!data.contains("data") || !data["data"].is_string()) {
            return false;
        }
        into.launchData = data["data"].get<std::string>();
        if (into.launchData.size() > MaxLaunchDataLength) {
            into.launchData.resize(MaxLaunchDataLength);
        }
        return true;
    }
    readField(data, "details", into.details);
    readField(data, "state", into.state);
    if (data.contains("timeStart") && data["timeStart"].is_number()) {
        into.timeStart = data["timeStart"].get<std::time_t>();
    }
    if (data.contains("timeEnd") && data["timeEnd"].is_number()) {
        into.timeEnd = data["timeEnd"].get<std::time_t>();
    }
    readImage(data, "largeImage", into.largeImage);
    readImage(data, "smallImage", into.smallImage);
    return true;
}

std::string rpcAssetLookupUrl(const std::string& assetId) {
    if (!allDigits(assetId)) {
        return std::string();
    }
    return "https://thumbnails.roblox.com/v1/assets?assetIds=" + assetId +
           "&size=512x512&format=Png";
}

} // namespace tuxblox
