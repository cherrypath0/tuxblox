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

#include "fastflag_import.h"
#include "json.hpp"

namespace tuxblox {

namespace {

FastFlagImport refused(const std::string &reason) {
    FastFlagImport result;
    result.error = reason;
    return result;
}

// The library's messages start with a code in brackets that means nothing to the person reading this
std::string withoutCode(const std::string &message) {
    const size_t end = message.find("] ");
    return end == std::string::npos ? message : message.substr(end + 2);
}

} // namespace

FastFlagImport parseFastFlagJson(const std::string &text) {
    if (text.find_first_not_of(" \t\r\n") == std::string::npos) return refused("Paste the flags as JSON.");

    nlohmann::ordered_json parsed;
    try {
        parsed = nlohmann::ordered_json::parse(text);
    } catch (const nlohmann::json::exception &problem) {
        return refused("This is not valid JSON: " + withoutCode(problem.what()));
    }

    if (!parsed.is_object()) return refused("The flags need to be in curly brackets, like {\"FlagName\": \"true\"}.");
    if (parsed.empty()) return refused("There are no flags in this JSON.");

    FastFlagImport result;
    for (const auto &entry : parsed.items()) {
        const std::string &name = entry.key();
        const auto &value = entry.value();
        if (name.empty()) return refused("A flag has no name.");

        if (value.is_string()) {
            result.flags.push_back({name, value.get<std::string>()});
        } else if (value.is_boolean() || value.is_number()) {
            result.flags.push_back({name, value.dump()});
        } else {
            return refused("The value of " + name + " must be text, a number, true or false.");
        }
    }
    result.ok = true;
    return result;
}

std::vector<FastFlag> mergeFastFlags(const std::vector<FastFlag> &existing, const std::vector<FastFlag> &imported) {
    std::vector<FastFlag> merged = existing;
    for (const FastFlag &flag : imported) {
        bool replaced = false;
        for (FastFlag &current : merged) {
            if (current.name != flag.name) continue;
            current.value = flag.value;
            replaced = true;
        }
        if (!replaced) merged.push_back(flag);
    }
    return merged;
}

} // namespace tuxblox
