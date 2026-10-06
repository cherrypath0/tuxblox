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

#include "json.hpp"

#include <cassert>
#include <cstdio>
#include <string>
#include <vector>

int main() {
    const std::time_t t = 1700000000;

    // The Player's own phrasing, with no game involved.
    tuxblox::PresenceActivity playing;
    playing.kind = tuxblox::PresenceKind::Playing;
    playing.placeName = "Doors";
    playing.placeId = "6516141723";
    // The experience's own name, bare, which is what a player recognises.
    nlohmann::json out = nlohmann::json::parse(tuxblox::activityJson(playing, t));
    assert(out["details"].get<std::string>() == "Doors");
    assert(out["timestamps"]["start"].get<std::time_t>() == t);

    tuxblox::PresenceActivity joining;
    joining.kind = tuxblox::PresenceKind::Joining;
    out = nlohmann::json::parse(tuxblox::activityJson(joining, t));
    assert(out["details"].get<std::string>() == "Joining a game");

    tuxblox::PresenceActivity teleporting;
    teleporting.kind = tuxblox::PresenceKind::Teleporting;
    out = nlohmann::json::parse(tuxblox::activityJson(teleporting, t));
    assert(out["details"].get<std::string>() == "Moving to another server");

    tuxblox::PresenceActivity inApp;
    inApp.kind = tuxblox::PresenceKind::InApp;
    out = nlohmann::json::parse(tuxblox::activityJson(inApp, t));
    assert(out["details"].get<std::string>() == "In the Roblox app");

    // The Player wears its own mark, never Studio's, and shows it when no place icon is known.
    out = nlohmann::json::parse(tuxblox::activityJson(playing, t));
    assert(out["assets"]["large_image"].get<std::string>() == "robloxplayer");
    assert(out["assets"]["large_text"].get<std::string>() == "Roblox");

    // With the place's icon known, that is what is shown, with the Player's mark badged onto it.
    tuxblox::PresenceActivity playingIcon = playing;
    playingIcon.placeIconUrl = "https://tr.rbxcdn.com/abc/512/512/Image/Png";
    out = nlohmann::json::parse(tuxblox::activityJson(playingIcon, t));
    assert(out["assets"]["large_image"].get<std::string>() == playingIcon.placeIconUrl);
    assert(out["assets"]["large_text"].get<std::string>() == "Doors");
    assert(out["assets"]["small_image"].get<std::string>() == "robloxplayer");
    assert(out["assets"]["small_text"].get<std::string>() == "Roblox");

    // Studio's own art is untouched.
    tuxblox::PresenceActivity editing;
    editing.kind = tuxblox::PresenceKind::Editing;
    editing.placeName = "DOORS GAME";
    out = nlohmann::json::parse(tuxblox::activityJson(editing, t));
    assert(out["details"].get<std::string>() == "Editing DOORS GAME");
    assert(out["assets"]["large_image"].get<std::string>() == "studio");

    tuxblox::PresenceActivity editingIcon = editing;
    editingIcon.placeIconUrl = "https://tr.rbxcdn.com/abc/512/512/Image/Png";
    out = nlohmann::json::parse(tuxblox::activityJson(editingIcon, t));
    assert(out["assets"]["large_image"].get<std::string>() == editingIcon.placeIconUrl);
    assert(out["assets"]["small_image"].get<std::string>() == "studio");

    // The Player's presence reads the way players already know it: the experience's own name, its
    // author beneath, its icon, and the two buttons.
    tuxblox::PresenceActivity full;
    full.kind = tuxblox::PresenceKind::Playing;
    full.placeName = "DOORS";
    full.placeId = "6516141723";
    full.jobId = "5ccfb412-0b58-47d2-b91e-321903eceb59";
    full.creatorLine = "by LSPLASH";
    full.placeIconUrl = "https://tr.rbxcdn.com/abc/512/512/Image/Png";
    out = nlohmann::json::parse(tuxblox::activityJson(full, t));
    assert(out["details"].get<std::string>() == "DOORS");
    assert(out["state"].get<std::string>() == "by LSPLASH");
    assert(out["assets"]["large_image"].get<std::string>() == full.placeIconUrl);
    assert(out["assets"]["large_text"].get<std::string>() == "DOORS");
    assert(out["assets"]["small_image"].get<std::string>() == "robloxplayer");
    assert(out["assets"]["small_text"].get<std::string>() == "Roblox");
    assert(out["buttons"].size() == 2);
    assert(out["buttons"][0]["label"].get<std::string>() == "Join server");
    assert(out["buttons"][0]["url"].get<std::string>() ==
           "https://www.roblox.com/games/start?placeId=6516141723&gameInstanceId=5ccfb412-0b58-47d2-b91e-321903eceb59");
    assert(out["buttons"][1]["label"].get<std::string>() == "See game page");
    assert(out["buttons"][1]["url"].get<std::string>() == "https://www.roblox.com/games/6516141723");

    // What the game wants carried into that link rides along on it.
    tuxblox::PresenceActivity withLaunch = full;
    withLaunch.rpc.launchData = "room=7";
    out = nlohmann::json::parse(tuxblox::activityJson(withLaunch, t));
    assert(out["buttons"][0]["url"].get<std::string>().find("&launchData=room%3D7") != std::string::npos);

    // Without a server there is nothing to join, so only the game page is offered.
    tuxblox::PresenceActivity noServer = full;
    noServer.jobId.clear();
    out = nlohmann::json::parse(tuxblox::activityJson(noServer, t));
    assert(out["buttons"].size() == 1);
    assert(out["buttons"][0]["label"].get<std::string>() == "See game page");

    // With no icon known the Player's own mark stands in, and nothing is badged onto it.
    tuxblox::PresenceActivity noIcon = full;
    noIcon.placeIconUrl.clear();
    out = nlohmann::json::parse(tuxblox::activityJson(noIcon, t));
    assert(out["assets"]["large_image"].get<std::string>() == "robloxplayer");
    assert(out["assets"]["large_text"].get<std::string>() == "Roblox");
    assert(!out["assets"].contains("small_image"));
    // The name still reaches the details line even with no art to hang it on.
    assert(out["details"].get<std::string>() == "DOORS");

    // Studio's own presence is untouched by any of it, buttons included.
    tuxblox::PresenceActivity studioButtons;
    studioButtons.kind = tuxblox::PresenceKind::Editing;
    studioButtons.placeName = "DOORS GAME";
    studioButtons.placeId = "6516141723";
    studioButtons.jobId = "5ccfb412-0b58-47d2-b91e-321903eceb59";
    out = nlohmann::json::parse(tuxblox::activityJson(studioButtons, t));
    assert(out["details"].get<std::string>() == "Editing DOORS GAME");
    assert(out["buttons"].size() == 1);
    assert(out["buttons"][0]["label"].get<std::string>() == "Open Game Link");
    assert(!out.contains("state"));

    // What the game asked for wins over what TuxBlox would have shown.
    tuxblox::PresenceActivity custom = playing;
    custom.rpc.details.state = tuxblox::RpcField::State::Set;
    custom.rpc.details.value = "Floor 3";
    custom.rpc.state.state = tuxblox::RpcField::State::Set;
    custom.rpc.state.value = "Hiding from the entity";
    out = nlohmann::json::parse(tuxblox::activityJson(custom, t));
    assert(out["details"].get<std::string>() == "Floor 3");
    assert(out["state"].get<std::string>() == "Hiding from the entity");

    // "<reset>" puts TuxBlox's own text back.
    tuxblox::PresenceActivity reset = custom;
    reset.rpc.details.state = tuxblox::RpcField::State::Reset;
    out = nlohmann::json::parse(tuxblox::activityJson(reset, t));
    assert(out["details"].get<std::string>() == "Doors");

    // A time the game set is what is shown.
    tuxblox::PresenceActivity timed = playing;
    timed.rpc.timeStart = 1699999999;
    out = nlohmann::json::parse(tuxblox::activityJson(timed, t));
    assert(out["timestamps"]["start"].get<std::time_t>() == 1699999999);

    // An asset the game named is shown only once it has been looked up, and only ever as the picture Roblox serves for it.
    tuxblox::PresenceActivity art = playing;
    art.rpc.largeImage.assetId.state = tuxblox::RpcField::State::Set;
    art.rpc.largeImage.assetId.value = "1234";
    art.rpc.largeImage.hoverText.state = tuxblox::RpcField::State::Set;
    art.rpc.largeImage.hoverText.value = "The Hotel";
    out = nlohmann::json::parse(tuxblox::activityJson(art, t));
    // Until it has been looked up, TuxBlox's own mark stays rather than an address Discord cannot load.
    assert(out["assets"]["large_image"].get<std::string>() == "robloxplayer");
    assert(out["assets"]["large_text"].get<std::string>() == "The Hotel");

    art.rpc.largeImage.resolvedUrl = "https://tr.rbxcdn.com/1234/512/512/Image/Png";
    out = nlohmann::json::parse(tuxblox::activityJson(art, t));
    assert(out["assets"]["large_image"].get<std::string>() == art.rpc.largeImage.resolvedUrl);

    // Clearing it drops the image rather than leaving the last one showing.
    tuxblox::PresenceActivity cleared = art;
    cleared.rpc.largeImage.assetId.state = tuxblox::RpcField::State::Clear;
    out = nlohmann::json::parse(tuxblox::activityJson(cleared, t));
    assert(!out["assets"].contains("large_image"));

    // A blank string from the game drops the field rather than showing an empty one.
    tuxblox::PresenceActivity blanked = playing;
    blanked.rpc.details.state = tuxblox::RpcField::State::Clear;
    out = nlohmann::json::parse(tuxblox::activityJson(blanked, t));
    assert(!out.contains("details"));

    // Review Focus 3: a game writing every frame cannot push more at Discord than it accepts, which
    // is five updates in any twenty seconds -- not the one a second the game itself is limited to.
    std::vector<std::time_t> sends;
    for (int i = 0; i < 5; i++) {
        assert(tuxblox::rpcAllowsSend(sends, t));
        sends.push_back(t);
    }
    assert(!tuxblox::rpcAllowsSend(sends, t));
    assert(!tuxblox::rpcAllowsSend(sends, t + 19));
    // Once the window has moved on, the oldest no longer counts.
    assert(tuxblox::rpcAllowsSend(sends, t + 20));
    assert(sends.size() == 0);

    printf("presence_activity_json: all tests passed\n");
    return 0;
}
