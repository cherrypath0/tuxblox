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

    // Studio's start screen: no place loaded. This is the only place Standalone appears.
    reader.consumeLine("[FLog::RenderSchedulerState] Setting fallback DataModel Standalone");
    assert(reader.activity().kind == tuxblox::PresenceKind::Home);

    reader.consumeLine("[FLog::StudioKeyEvents] open place (identifier = "
                       "C:/users/cherry/files/DOORS GAME.rbxl) [start]");
    reader.consumeLine("[FLog::AssetDataModelManager] Setting StudioGameStateType to "
                       "StudioGameStateType_Edit");
    assert(reader.activity().kind == tuxblox::PresenceKind::Editing);
    assert(reader.activity().placeName == "DOORS GAME");

    reader.consumeLine("[FLog::AssetDataModelManager] Setting StudioGameStateType to "
                       "StudioGameStateType_PlayClient");
    assert(reader.activity().kind == tuxblox::PresenceKind::PlayTesting);
    assert(reader.activity().placeName == "DOORS GAME");

    reader.consumeLine("[FLog::StudioKeyEvents] end play test");
    assert(reader.activity().kind == tuxblox::PresenceKind::Editing);

    // Closing the place leaves no datamodel, which is the home page again --
    // and the place that was open must not linger in the name.
    reader.consumeLine("[FLog::AssetDataModelManager] Setting StudioGameStateType to "
                       "StudioGameStateType_Null");
    assert(reader.activity().kind == tuxblox::PresenceKind::Home);
    assert(reader.activity().placeName.empty());

    // Team Create is not a state of its own: editing is editing either way, so
    // leaving one cannot leave the presence stuck.
    tuxblox::StudioPresenceReader tc;
    tc.consumeLine("[FLog::StudioKeyEvents] open place (identifier = 121959868194179) [start]");
    tc.consumeLine("[FLog::StudioKeyEvents] team create connect (matchmaker start)");
    tc.consumeLine("[FLog::AssetDataModelManager] Setting StudioGameStateType to "
                   "StudioGameStateType_Edit");
    assert(tc.activity().kind == tuxblox::PresenceKind::Editing);
    tc.consumeLine("[FLog::AssetDataModelManager] Setting StudioGameStateType to "
                   "StudioGameStateType_PlayClient");
    tc.consumeLine("[FLog::StudioKeyEvents] end play test");
    assert(tc.activity().kind == tuxblox::PresenceKind::Editing);
    tc.consumeLine("[FLog::StudioKeyEvents] Handling team create disconnection event 0");
    tc.consumeLine("[FLog::AssetDataModelManager] Setting StudioGameStateType to "
                   "StudioGameStateType_Null");
    assert(tc.activity().kind == tuxblox::PresenceKind::Home);

    reader.consumeLine("[FLog::StudioKeyEvents] exit LifecycleManager UserSession scope (standalone shutdown)");
    assert(reader.activity().kind == tuxblox::PresenceKind::None);

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

    // Closing a document tab is not the end of the session -- real logs carry
    // on for thousands of lines after one.
    tuxblox::StudioPresenceReader tabs;
    tabs.consumeLine("[FLog::StudioKeyEvents] open place (identifier = "
                     "C:/users/cherry/files/Baseplate.rbxl) [start]");
    tabs.consumeLine("[FLog::AssetDataModelManager] Setting StudioGameStateType to "
                     "StudioGameStateType_Edit");
    tabs.consumeLine("[FLog::StudioKeyEvents] close IDE doc");
    assert(tabs.activity().kind == tuxblox::PresenceKind::Editing);
    assert(tabs.activity().placeName == "Baseplate");

    // The identifier is a bare place id far more often than a filename, and a
    // number on a profile tells a viewer nothing, so it is not published.
    tuxblox::StudioPresenceReader byId;
    byId.consumeLine("[FLog::StudioKeyEvents] open place (identifier = 95206881) [start]");
    assert(byId.activity().placeName.empty());

    // A growing file must be read incrementally, and a line that arrives in
    // two pieces must not reach the state machine until it is whole.
    {
        const std::string path = "/tmp/tuxblox-presence-tail.log";
        std::filesystem::remove(path);
        {
            std::ofstream out(path);
            out << "[FLog::AssetDataModelManager] Setting StudioGameStateType to "
                   "StudioGameStateType_Null\n";
        }

        tuxblox::StudioPresenceReader tailReader;
        tuxblox::SessionLogTail tail(path);
        assert(tail.open());
        tail.pump(tailReader);
        assert(tailReader.activity().kind == tuxblox::PresenceKind::Home);

        // Nothing new: the state must not change and nothing must be re-read.
        tail.pump(tailReader);
        assert(tailReader.activity().kind == tuxblox::PresenceKind::Home);

        // Write half a line. It must be held back, not acted on.
        {
            std::ofstream out(path, std::ios::app);
            out << "[FLog::AssetDataModelManager] Setting StudioGameStateType to Studio";
        }
        tail.pump(tailReader);
        assert(tailReader.activity().kind == tuxblox::PresenceKind::Home);

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

        // One phrase, with the place named whenever Studio gave a name.
        const std::string shown = tuxblox::activityJson(editing, 1700000000);
        assert(shown.find("\"details\":\"Editing DOORS GAME\"") != std::string::npos);
        assert(shown.find("\"start\":1700000000") != std::string::npos);
        assert(shown.find("TuxBlox") == std::string::npos);

        // A place Studio never named still reads as a place.
        tuxblox::PresenceActivity unnamed;
        unnamed.kind = tuxblox::PresenceKind::Editing;
        assert(tuxblox::activityJson(unnamed, 1).find("\"details\":\"Editing a place\"") !=
               std::string::npos);

        // With an icon the place leads and Studio becomes the badge; without
        // one there is just the Studio mark and no badge to duplicate it.
        tuxblox::PresenceActivity withIcon = editing;
        withIcon.placeIconUrl = "https://t0.rbxcdn.com/180DAY-abc";
        const std::string art = tuxblox::activityJson(withIcon, 1);
        assert(art.find("\"large_image\":\"https://t0.rbxcdn.com/180DAY-abc\"") != std::string::npos);
        assert(art.find("\"large_text\":\"DOORS GAME\"") != std::string::npos);
        assert(art.find("\"small_image\":\"studio\"") != std::string::npos);

        const std::string noArt = tuxblox::activityJson(editing, 1);
        assert(noArt.find("\"large_image\":\"studio\"") != std::string::npos);
        assert(noArt.find("small_image") == std::string::npos);

        // A published place gets a button to its page; a local file has no
        // page to link to, so there is no button at all.
        tuxblox::PresenceActivity published = editing;
        published.placeId = "121959868194179";
        const std::string linked = tuxblox::activityJson(published, 1);
        assert(linked.find("\"label\":\"Open Game Link\"") != std::string::npos);
        assert(linked.find("\"url\":\"https://www.roblox.com/games/121959868194179\"") !=
               std::string::npos);
        assert(tuxblox::activityJson(editing, 1).find("buttons") == std::string::npos);

        tuxblox::PresenceActivity home;
        home.kind = tuxblox::PresenceKind::Home;
        assert(tuxblox::activityJson(home, 1).find("\"details\":\"In the home page\"") !=
               std::string::npos);

        tuxblox::PresenceActivity playing;
        playing.kind = tuxblox::PresenceKind::PlayTesting;
        assert(tuxblox::activityJson(playing, 1).find("\"details\":\"Playtesting a place\"") !=
               std::string::npos);
        playing.placeName = "DOORS GAME";
        assert(tuxblox::activityJson(playing, 1).find("\"details\":\"Playtesting DOORS GAME\"") !=
               std::string::npos);

        // A Windows path's backslashes and any quotes must be escaped, not emitted raw.
        tuxblox::PresenceActivity awkward;
        awkward.kind = tuxblox::PresenceKind::Editing;
        awkward.placeName = "My \"Best\" \\ Game";
        const std::string escaped = tuxblox::activityJson(awkward, 1);
        assert(escaped.find("\\\"Best\\\"") != std::string::npos);

        // A name Roblox wrote in something other than UTF-8 must not throw:
        // the watcher dying here loses the session log and the crash dialog.
        tuxblox::PresenceActivity latin1;
        latin1.kind = tuxblox::PresenceKind::Editing;
        latin1.placeName = "Caf\xE9 Game";
        assert(!tuxblox::activityJson(latin1, 1).empty());
    }

    // The identifier is a bare place id far more often than a filename, and a
    // number on a profile tells a viewer nothing, so it is not published.
    {
        tuxblox::StudioPresenceReader byId;
        byId.consumeLine("[FLog::StudioKeyEvents] open place (identifier = 95206881) [start]");
        assert(byId.activity().placeName.empty());

        tuxblox::StudioPresenceReader byPath;
        byPath.consumeLine("[FLog::StudioKeyEvents] open place (identifier = "
                           "C:/users/cherry/files/Baseplate.rbxl) [start]");
        assert(byPath.activity().placeName == "Baseplate");
    }

    // Closing a document tab is not the end of the session -- real logs carry
    // over a hundred Edit states after one -- so presence must survive it.
    {
        tuxblox::StudioPresenceReader tabs;
        tabs.consumeLine("[FLog::StudioKeyEvents] open place (identifier = "
                         "C:/users/cherry/files/Baseplate.rbxl) [start]");
        tabs.consumeLine("[FLog::AssetDataModelManager] Setting StudioGameStateType to "
                         "StudioGameStateType_Edit");
        tabs.consumeLine("[FLog::StudioKeyEvents] close IDE doc");
        assert(tabs.activity().kind == tuxblox::PresenceKind::Editing);
        assert(tabs.activity().placeName == "Baseplate");
    }

    // Studio's own wording for "no place loaded", which is what the start
    // screen shows. The state name is not in this line.
    {
        tuxblox::StudioPresenceReader home;
        home.consumeLine("[FLog::RenderSchedulerState] Setting fallback DataModel Standalone");
        assert(home.activity().kind == tuxblox::PresenceKind::Home);
    }

    return 0;
}
