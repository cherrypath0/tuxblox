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

#pragma once
#include <cstdint>
#include <map>
#include <optional>
#include <string>

namespace tuxblox {

// TuxBlox's own feature flags: switches shipped off and turned on later, without a
// release. A flag does something only because compiled code asks for it by name, so a
// name nothing reads is inert -- the server can choose between paths that were built in,
// never create one.

enum class FlagType { Bool, String, Int };

struct FlagValue {
    FlagType type = FlagType::Bool;
    bool boolean = false;
    int64_t integer = 0;
    std::string text;
};

using FlagTable = std::map<std::string, FlagValue>;

// The type a name declares: tbxFlag is a boolean, tbxString a string, tbxInt an integer.
// Nothing for a name that declares none, which is how an unknown name is refused rather
// than guessed at.
std::optional<FlagType> flagTypeForName(const std::string& name);

// True for a flag the server may not set. tbxFlagDisableUpdater is the one that matters:
// served wrongly it stops every install updating, and updating is how that would be
// corrected. The list is compiled in and the client filters the server's answer against
// it, because the server is not trusted to respect its own rule.
bool isLocalOnlyFlag(const std::string& name);

// The flags in a JSON object, dropping any whose value contradicts its name, any name
// with no known prefix, and -- when `fromServer` -- any local-only name. Never throws:
// anything that is not a flat object of usable values reads as an empty table, which
// leaves the sources below it answering.
FlagTable parseFlagJson(const std::string& json, bool fromServer);

// Applies `higher` over `base`, per key. A key `higher` does not set is left alone, and a
// key only `higher` has is added -- which is how a flag is introduced ahead of the
// release that reads it.
void mergeFlagsOver(FlagTable& base, const FlagTable& higher);

} // namespace tuxblox
