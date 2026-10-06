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

int main() {
    const std::time_t t = 1700000000;

    // The Player's own phrasing, with no game involved.
    tuxblox::PresenceActivity playing;
    playing.kind = tuxblox::PresenceKind::Playing;
    playing.placeName = "Doors";
    playing.placeId = "6516141723";
    nlohmann::json out = nlohmann::json::parse(tuxblox::activityJson(playing, t));
    assert(out["details"].get<std::string>() == "Playing Doors");
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

    // The Player must not wear the Studio mark. With no place icon known there is no image at all, so Discord falls back to the application's own.
    out = nlohmann::json::parse(tuxblox::activityJson(playing, t));
    assert(!out.contains("assets") || !out["assets"].contains("large_image"));

    // With the place's icon known, that is what is shown, and still no Studio badge beside it.
    tuxblox::PresenceActivity playingIcon = playing;
    playingIcon.placeIconUrl = "https://tr.rbxcdn.com/abc/512/512/Image/Png";
    out = nlohmann::json::parse(tuxblox::activityJson(playingIcon, t));
    assert(out["assets"]["large_image"].get<std::string>() == playingIcon.placeIconUrl);
    assert(out["assets"]["large_text"].get<std::string>() == "Doors");
    assert(!out["assets"].contains("small_image"));

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
    assert(out["details"].get<std::string>() == "Playing Doors");

    // A time the game set is what is shown.
    tuxblox::PresenceActivity timed = playing;
    timed.rpc.timeStart = 1699999999;
    out = nlohmann::json::parse(tuxblox::activityJson(timed, t));
    assert(out["timestamps"]["start"].get<std::time_t>() == 1699999999);

    // An asset the game named becomes a Roblox thumbnail, never a URL of its own.
    tuxblox::PresenceActivity art = playing;
    art.rpc.largeImage.assetId.state = tuxblox::RpcField::State::Set;
    art.rpc.largeImage.assetId.value = "1234";
    art.rpc.largeImage.hoverText.state = tuxblox::RpcField::State::Set;
    art.rpc.largeImage.hoverText.value = "The Hotel";
    out = nlohmann::json::parse(tuxblox::activityJson(art, t));
    assert(out["assets"]["large_image"].get<std::string>() ==
           "https://thumbnails.roblox.com/v1/assets?assetIds=1234&size=420x420&format=Png");
    assert(out["assets"]["large_text"].get<std::string>() == "The Hotel");

    // Clearing it drops the image rather than leaving the last one showing.
    tuxblox::PresenceActivity cleared = art;
    cleared.rpc.largeImage.clear = true;
    out = nlohmann::json::parse(tuxblox::activityJson(cleared, t));
    assert(!out["assets"].contains("large_image"));

    // Review Focus 3: a game writing every frame cannot make us send more than once a second.
    assert(tuxblox::rpcAllowsSend(0, t));
    assert(!tuxblox::rpcAllowsSend(t, t));
    assert(tuxblox::rpcAllowsSend(t, t + 1));

    printf("presence_activity_json: all tests passed\n");
    return 0;
}
