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

#include "roblox_universe.h"

#include <cassert>
#include <cstdio>
#include <string>

int main() {
    assert(tuxblox::universeLookupUrl("6516141723") ==
           "https://apis.roblox.com/universes/v1/places/6516141723/universe");
    // A log line must never be able to steer the request somewhere else.
    assert(tuxblox::universeLookupUrl("6516141723/../evil").empty());
    assert(tuxblox::universeLookupUrl("").empty());

    assert(tuxblox::universeIdFromJson("{\"universeId\":2440500124}") == "2440500124");
    assert(tuxblox::universeIdFromJson("{}").empty());
    assert(tuxblox::universeIdFromJson("not json").empty());
    assert(tuxblox::universeIdFromJson("{\"universeId\":\"x\"}").empty());

    assert(tuxblox::universeDetailsUrl("2440500124") ==
           "https://games.roblox.com/v1/games?universeIds=2440500124");
    assert(tuxblox::universeDetailsUrl("x").empty());

    // The live shape, measured 2026-10-07.
    const std::string body =
        "{\"data\":[{\"name\":\"DOORS \xf0\x9f\x9a\xa8\",\"creator\":{\"name\":\"LSPLASH\",\"hasVerifiedBadge\":true}}]}";
    tuxblox::UniverseInfo info;
    assert(tuxblox::universeInfoFromJson(body, info));
    assert(info.name == "DOORS \xf0\x9f\x9a\xa8");
    assert(info.creatorName == "LSPLASH");
    assert(info.creatorVerified);
    assert(tuxblox::creatorLine(info) == "by LSPLASH \xe2\x98\x91\xef\xb8\x8f");

    tuxblox::UniverseInfo plain;
    assert(tuxblox::universeInfoFromJson(
        "{\"data\":[{\"name\":\"Some Game\",\"creator\":{\"name\":\"Someone\",\"hasVerifiedBadge\":false}}]}",
        plain));
    assert(tuxblox::creatorLine(plain) == "by Someone");

    // Nothing that is not an answer becomes one, and nothing throws in the presence loop.
    tuxblox::UniverseInfo bad;
    assert(!tuxblox::universeInfoFromJson("", bad));
    assert(!tuxblox::universeInfoFromJson("{\"data\":[]}", bad));
    assert(!tuxblox::universeInfoFromJson("{\"data\":{}}", bad));
    assert(!tuxblox::universeInfoFromJson("[1,2]", bad));
    // Roblox answers a restricted universe with placeholders, which is not a name worth showing.
    assert(!tuxblox::universeInfoFromJson(
        "{\"data\":[{\"name\":\"[TITLE UNAVAILABLE]\",\"creator\":{\"name\":\"[UNKNOWN]\",\"hasVerifiedBadge\":false}}]}",
        bad));

    // A universe with no creator still has a usable name, and then there is no line to show.
    tuxblox::UniverseInfo nameOnly;
    assert(tuxblox::universeInfoFromJson("{\"data\":[{\"name\":\"Just A Name\"}]}", nameOnly));
    assert(nameOnly.name == "Just A Name");
    assert(tuxblox::creatorLine(nameOnly).empty());

    printf("roblox_universe: all tests passed\n");
    return 0;
}
