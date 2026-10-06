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

#include "roblox_place_info.h"

#include <cassert>
#include <string>

int main() {
    // The shape Roblox's asset details endpoint returns for a place.
    const std::string body =
        "{\"TargetId\":95206881,\"ProductType\":null,\"AssetId\":95206881,"
        "\"Name\":\"Crossroads\",\"Description\":\"The classic\",\"AssetTypeId\":9,"
        "\"Creator\":{\"Id\":1,\"Name\":\"Roblox\"},\"IsPublicDomain\":true}";
    assert(tuxblox::placeNameFromJson(body) == "Crossroads");

    // Anything that is not the expected shape must yield no name rather than
    // throw: this runs inside the presence loop and a failure there is not
    // worth losing a session over.
    assert(tuxblox::placeNameFromJson("").empty());
    assert(tuxblox::placeNameFromJson("not json at all").empty());
    assert(tuxblox::placeNameFromJson("{}").empty());
    assert(tuxblox::placeNameFromJson("[1,2,3]").empty());
    assert(tuxblox::placeNameFromJson("{\"Name\":null}").empty());
    assert(tuxblox::placeNameFromJson("{\"Name\":12345}").empty());
    assert(tuxblox::placeNameFromJson("{\"errors\":[{\"code\":404}]}").empty());

    // A name that is only whitespace is not a name.
    assert(tuxblox::placeNameFromJson("{\"Name\":\"   \"}").empty());

    // A very long name is cut rather than published whole: Discord truncates
    // its own way and a wall of text helps nobody.
    std::string longName(400, 'x');
    const std::string cut = tuxblox::placeNameFromJson("{\"Name\":\"" + longName + "\"}");
    assert(!cut.empty());
    assert(cut.size() <= 128);

    // The endpoint is built from the id and nothing else.
    assert(tuxblox::placeDetailsUrl("95206881").find("95206881") != std::string::npos);
    assert(tuxblox::placeDetailsUrl("95206881").rfind("https://", 0) == 0);
    // An id that is not a plain number must never reach a URL.
    assert(tuxblox::placeDetailsUrl("../../evil").empty());
    assert(tuxblox::placeDetailsUrl("").empty());

    // The thumbnail answer carries the image behind a "data" array, and only
    // a completed one is usable.
    const std::string icons =
        "{\"data\":[{\"targetId\":121959868194179,\"state\":\"Completed\","
        "\"imageUrl\":\"https://t0.rbxcdn.com/180DAY-a158cfee\",\"version\":\"TN3\"}]}";
    assert(tuxblox::placeIconUrlFromJson(icons) == "https://t0.rbxcdn.com/180DAY-a158cfee");

    // A pending or blocked thumbnail has no image worth showing.
    assert(tuxblox::placeIconUrlFromJson(
        "{\"data\":[{\"state\":\"Pending\",\"imageUrl\":null}]}").empty());
    assert(tuxblox::placeIconUrlFromJson(
        "{\"data\":[{\"state\":\"Blocked\",\"imageUrl\":\"https://x/y\"}]}").empty());
    assert(tuxblox::placeIconUrlFromJson("{\"data\":[]}").empty());
    assert(tuxblox::placeIconUrlFromJson("{}").empty());
    assert(tuxblox::placeIconUrlFromJson("garbage").empty());

    // Only an https image is ever handed to Discord.
    assert(tuxblox::placeIconUrlFromJson(
        "{\"data\":[{\"state\":\"Completed\",\"imageUrl\":\"http://insecure/x\"}]}").empty());

    assert(tuxblox::placeIconsUrl("95206881").find("95206881") != std::string::npos);
    assert(tuxblox::placeIconsUrl("../evil").empty());

    // The page a viewer lands on, built from the id and nothing else.
    assert(tuxblox::placeGameUrl("121959868194179") ==
           "https://www.roblox.com/games/121959868194179");
    assert(tuxblox::placeGameUrl("../../evil").empty());
    assert(tuxblox::placeGameUrl("").empty());

    // The join link somebody clicks to land in the same server.
    assert(tuxblox::placeJoinUrl("6516141723", "5ccfb412-0b58-47d2-b91e-321903eceb59", "") ==
           "https://www.roblox.com/games/start?placeId=6516141723&gameInstanceId=5ccfb412-0b58-47d2-b91e-321903eceb59");
    assert(tuxblox::placeJoinUrl("6516141723", "5ccfb412-0b58-47d2-b91e-321903eceb59", "room=7") ==
           "https://www.roblox.com/games/start?placeId=6516141723&gameInstanceId=5ccfb412-0b58-47d2-b91e-321903eceb59&launchData=room%3D7");
    // What a game sets is carried, but it cannot add parameters of its own.
    assert(tuxblox::placeJoinUrl("1", "5ccfb412-0b58-47d2-b91e-321903eceb59", "a b&c=d") ==
           "https://www.roblox.com/games/start?placeId=1&gameInstanceId=5ccfb412-0b58-47d2-b91e-321903eceb59&launchData=a%20b%26c%3Dd");
    // Without a place or a server there is nothing to join.
    assert(tuxblox::placeJoinUrl("", "5ccfb412-0b58-47d2-b91e-321903eceb59", "").empty());
    assert(tuxblox::placeJoinUrl("6516141723", "", "").empty());
    assert(tuxblox::placeJoinUrl("not-a-place", "5ccfb412-0b58-47d2-b91e-321903eceb59", "").empty());

    return 0;
}
