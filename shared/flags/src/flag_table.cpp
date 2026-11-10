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

#include "flag_table.h"
#include "json.hpp"
#include <array>

namespace tuxblox {

namespace {

constexpr std::array<std::pair<const char*, FlagType>, 3> kPrefixes = {{
    {"tbxFlag", FlagType::Bool},
    {"tbxString", FlagType::String},
    {"tbxInt", FlagType::Int},
}};

// Flags the server may not set, enforced here rather than trusted to the server. Adding
// to this list takes a release, which is the point.
constexpr std::array<const char*, 1> kLocalOnly = {{
    "tbxFlagDisableUpdater",
}};

bool startsWith(const std::string& name, const char* prefix) {
    const std::string p = prefix;
    return name.size() > p.size() && name.compare(0, p.size(), p) == 0;
}

} // namespace

std::optional<FlagType> flagTypeForName(const std::string& name) {
    for (const auto& entry : kPrefixes) {
        if (startsWith(name, entry.first)) return entry.second;
    }
    return std::nullopt;
}

bool isLocalOnlyFlag(const std::string& name) {
    for (const char* local : kLocalOnly) {
        if (name == local) return true;
    }
    return false;
}

FlagTable parseFlagJson(const std::string& json, bool fromServer) {
    FlagTable table;
    nlohmann::json parsed;
    try {
        parsed = nlohmann::json::parse(json);
    } catch (const nlohmann::json::exception&) {
        return table;
    }
    if (!parsed.is_object()) return table;

    for (auto it = parsed.begin(); it != parsed.end(); ++it) {
        const std::optional<FlagType> type = flagTypeForName(it.key());
        if (!type.has_value()) continue;
        if (fromServer && isLocalOnlyFlag(it.key())) continue;

        FlagValue value;
        value.type = *type;
        if (*type == FlagType::Bool) {
            if (!it->is_boolean()) continue;
            value.boolean = it->get<bool>();
        } else if (*type == FlagType::String) {
            if (!it->is_string()) continue;
            value.text = it->get<std::string>();
        } else {
            // is_number_integer() is false for a boolean and for 4.5, which is what keeps
            // "true" from arriving as 1 and a fraction from being truncated.
            if (!it->is_number_integer() || it->is_boolean()) continue;
            value.integer = it->get<int64_t>();
        }
        table[it.key()] = value;
    }
    return table;
}

void mergeFlagsOver(FlagTable& base, const FlagTable& higher) {
    for (const auto& entry : higher) base[entry.first] = entry.second;
}

} // namespace tuxblox
