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
#include <string>

namespace tuxblox {

enum class PresenceKind {
    None,
    InStudio,
    Editing,
    PlayTesting,
    TeamCreate,
};

struct PresenceActivity {
    PresenceKind kind = PresenceKind::None;
    // Empty unless a place is open and its name is known. Shown only when the user has opted in.
    std::string placeName;
    // Whether the session joined Team Create, which outlives any one play test.
    bool teamCreate = false;
};

inline bool operator==(const PresenceActivity& a, const PresenceActivity& b) {
    return a.kind == b.kind && a.placeName == b.placeName && a.teamCreate == b.teamCreate;
}

inline bool operator!=(const PresenceActivity& a, const PresenceActivity& b) {
    return !(a == b);
}

// One log line at a time, because that is all any Roblox target gives us. The Player gets its own implementation when it runs.
class PresenceReader {
public:
    virtual ~PresenceReader() = default;
    virtual void consumeLine(const std::string& line) = 0;
    virtual PresenceActivity activity() const = 0;
};

} // namespace tuxblox
