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
    // Studio is open with no place loaded: its start screen, and what closing a place returns to.
    Home,
    Editing,
    PlayTesting,
};

struct PresenceActivity {
    PresenceKind kind = PresenceKind::None;
    // Empty unless a place is open and its name is known. Shown only when the user has opted in.
    std::string placeName;
    // Set instead of the name when Studio identifies the place by number, which is the usual case for a published place.
    std::string placeId;
    // The place's own icon, once Roblox has been asked for it. Shown in place of the Studio mark.
    std::string placeIconUrl;
};

inline bool operator==(const PresenceActivity& a, const PresenceActivity& b) {
    return a.kind == b.kind && a.placeName == b.placeName && a.placeId == b.placeId &&
           a.placeIconUrl == b.placeIconUrl;
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
