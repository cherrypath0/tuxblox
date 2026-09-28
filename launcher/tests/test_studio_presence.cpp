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
#include <filesystem>
#include <fstream>
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

    // A growing file must be read incrementally, and a line that arrives in
    // two pieces must not reach the state machine until it is whole.
    {
        const std::string path = "/tmp/tuxblox-presence-tail.log";
        std::filesystem::remove(path);
        {
            std::ofstream out(path);
            out << "[FLog::AssetDataModelManager] Setting StudioGameStateType to "
                   "StudioGameStateType_Standalone\n";
        }

        tuxblox::StudioPresenceReader tailReader;
        tuxblox::SessionLogTail tail(path);
        assert(tail.open());
        tail.pump(tailReader);
        assert(tailReader.activity().kind == tuxblox::PresenceKind::InStudio);

        // Nothing new: the state must not change and nothing must be re-read.
        tail.pump(tailReader);
        assert(tailReader.activity().kind == tuxblox::PresenceKind::InStudio);

        // Write half a line. It must be held back, not acted on.
        {
            std::ofstream out(path, std::ios::app);
            out << "[FLog::AssetDataModelManager] Setting StudioGameStateType to Studio";
        }
        tail.pump(tailReader);
        assert(tailReader.activity().kind == tuxblox::PresenceKind::InStudio);

        // Complete it; now it counts.
        {
            std::ofstream out(path, std::ios::app);
            out << "GameStateType_Edit\n";
        }
        tail.pump(tailReader);
        assert(tailReader.activity().kind == tuxblox::PresenceKind::Editing);

        std::filesystem::remove(path);
    }

    // A log that is not there yet is not an error; Studio writes it a moment after it starts.
    {
        tuxblox::SessionLogTail missing("/tmp/tuxblox-presence-does-not-exist.log");
        assert(!missing.open());
        tuxblox::StudioPresenceReader missingReader;
        missing.pump(missingReader);
        assert(missingReader.activity().kind == tuxblox::PresenceKind::None);
    }

    // The heading comes from the Application ID; these two fields are all the
    // program controls. "via TuxBlox" is in the one the place name shares, so
    // no setting can show the place without naming the layer.
    {
        tuxblox::PresenceActivity editing;
        editing.kind = tuxblox::PresenceKind::Editing;
        editing.placeName = "DOORS GAME";

        const std::string hidden = tuxblox::activityJson(editing, 1700000000, false);
        assert(hidden.find("\"details\":\"Editing\"") != std::string::npos);
        assert(hidden.find("\"state\":\"via TuxBlox\"") != std::string::npos);
        // The whole point of the opt-in: the name must not appear anywhere.
        assert(hidden.find("DOORS GAME") == std::string::npos);
        assert(hidden.find("\"start\":1700000000") != std::string::npos);

        const std::string shown = tuxblox::activityJson(editing, 1700000000, true);
        assert(shown.find("DOORS GAME") != std::string::npos);
        assert(shown.find("via TuxBlox") != std::string::npos);

        // A Windows path's backslashes and any quotes must be escaped, not emitted raw.
        tuxblox::PresenceActivity awkward;
        awkward.kind = tuxblox::PresenceKind::Editing;
        awkward.placeName = "My \"Best\" \\ Game";
        const std::string escaped = tuxblox::activityJson(awkward, 1, true);
        assert(escaped.find("\\\"Best\\\"") != std::string::npos);
        assert(escaped.find("\\\\") != std::string::npos);

        tuxblox::PresenceActivity playing;
        playing.kind = tuxblox::PresenceKind::PlayTesting;
        assert(tuxblox::activityJson(playing, 1, false).find("\"details\":\"Play testing\"") !=
               std::string::npos);

        tuxblox::PresenceActivity team;
        team.kind = tuxblox::PresenceKind::TeamCreate;
        assert(tuxblox::activityJson(team, 1, false).find("\"details\":\"In Team Create\"") !=
               std::string::npos);
    }

    return 0;
}
