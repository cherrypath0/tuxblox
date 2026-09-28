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

#include "studio_presence.h"

#include <cstddef>

namespace tuxblox {

namespace {

// Studio names its own state in these two lines and nowhere else, so matching the marker alongside the state avoids catching the same word printed as output.
const char StateMarker[] = "Setting StudioGameStateType to StudioGameStateType_";
const char FallbackMarker[] = "Setting fallback DataModel StudioGameStateType_";
const char OpenPlaceMarker[] = "[FLog::StudioKeyEvents] open place (identifier = ";

bool contains(const std::string& line, const char *pNeedle) {
    return line.find(pNeedle) != std::string::npos;
}

// The place as a person would name it: the file's own name, without the folders around it or the extension after it.
std::string placeNameFromIdentifier(const std::string& identifier) {
    const size_t slash = identifier.find_last_of("/\\");
    std::string leaf = slash == std::string::npos ? identifier : identifier.substr(slash + 1);
    const size_t dot = leaf.find_last_of('.');
    if (dot != std::string::npos && dot > 0) {
        leaf = leaf.substr(0, dot);
    }
    return leaf;
}

} // namespace

void StudioPresenceReader::consumeLine(const std::string& line) {
    if (contains(line, "[FLog::StudioKeyEvents] exit") ||
        contains(line, "[FLog::StudioKeyEvents] close")) {
        activity_ = PresenceActivity{};
        return;
    }

    const size_t openPlace = line.find(OpenPlaceMarker);
    if (openPlace != std::string::npos) {
        const size_t start = openPlace + sizeof(OpenPlaceMarker) - 1;
        const size_t end = line.rfind(") [start]");
        if (end != std::string::npos && end > start) {
            activity_.placeName = placeNameFromIdentifier(line.substr(start, end - start));
        }
        return;
    }

    if (contains(line, "[FLog::StudioKeyEvents] team create connect")) {
        activity_.kind = PresenceKind::TeamCreate;
        return;
    }

    if (contains(line, "[FLog::StudioKeyEvents] end play test")) {
        activity_.kind = PresenceKind::Editing;
        return;
    }

    const bool stateLine = contains(line, StateMarker) || contains(line, FallbackMarker);
    if (!stateLine) {
        return;
    }

    if (contains(line, "StudioGameStateType_PlayClient") ||
        contains(line, "StudioGameStateType_PlayServer")) {
        activity_.kind = PresenceKind::PlayTesting;
    } else if (contains(line, "StudioGameStateType_Edit")) {
        activity_.kind = PresenceKind::Editing;
    } else if (contains(line, "StudioGameStateType_Standalone")) {
        activity_.kind = PresenceKind::InStudio;
    }
}

PresenceActivity StudioPresenceReader::activity() const {
    return activity_;
}

} // namespace tuxblox
