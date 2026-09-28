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

#include <cassert>
#include <string>

int main() {
    tuxblox::StudioPresenceReader reader;
    assert(reader.activity().kind == tuxblox::PresenceKind::None);

    reader.consumeLine("2026-09-28T15:37:41.313Z,2.313522,0428,6,Info [FLog::RenderSchedulerState] "
                       "Setting fallback DataModel StudioGameStateType_Standalone");
    assert(reader.activity().kind == tuxblox::PresenceKind::InStudio);

    reader.consumeLine("2026-09-28T15:37:43.811Z,4.811502,0428,6,Info [FLog::StudioKeyEvents] "
                       "open place (identifier = C:/users/cherry/files/Workspace/place 6839171747 "
                       "DOORS GAME(9).rbxl) [start]");
    reader.consumeLine("2026-09-28T15:37:44.913Z,5.913712,0428,6,Info "
                       "[FLog::AssetDataModelManager] Setting StudioGameStateType to "
                       "StudioGameStateType_Edit");
    assert(reader.activity().kind == tuxblox::PresenceKind::Editing);
    assert(reader.activity().placeName == "place 6839171747 DOORS GAME(9)");

    reader.consumeLine("[FLog::AssetDataModelManager] Setting StudioGameStateType to "
                       "StudioGameStateType_PlayClient");
    assert(reader.activity().kind == tuxblox::PresenceKind::PlayTesting);
    // The place survives a play test, so it is still there when editing resumes.
    assert(reader.activity().placeName == "place 6839171747 DOORS GAME(9)");

    reader.consumeLine("[FLog::StudioKeyEvents] end play test");
    assert(reader.activity().kind == tuxblox::PresenceKind::Editing);

    reader.consumeLine("[FLog::StudioKeyEvents] team create connect");
    assert(reader.activity().kind == tuxblox::PresenceKind::TeamCreate);

    reader.consumeLine("[FLog::StudioKeyEvents] exit");
    assert(reader.activity().kind == tuxblox::PresenceKind::None);
    assert(reader.activity().placeName.empty());

    // A Windows path is the normal form, and its separators must not survive into the name.
    tuxblox::StudioPresenceReader windowsPath;
    windowsPath.consumeLine("[FLog::StudioKeyEvents] open place (identifier = "
                            "C:\\users\\cherry\\files\\My \"Best\" Game.rbxlx) [start]");
    assert(windowsPath.activity().placeName == "My \"Best\" Game");

    // An unparseable identifier must leave the name empty rather than produce rubbish.
    tuxblox::StudioPresenceReader malformed;
    malformed.consumeLine("[FLog::StudioKeyEvents] open place (identifier = ) [start]");
    assert(malformed.activity().placeName.empty());

    // Lines Studio produces in their thousands must not be mistaken for state changes.
    tuxblox::StudioPresenceReader noise;
    noise.consumeLine("[FLog::Output] StudioGameStateType_Edit is a string in a print statement");
    assert(noise.activity().kind == tuxblox::PresenceKind::None);

    return 0;
}
