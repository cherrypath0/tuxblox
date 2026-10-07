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

#include "server_region.h"

#include <cassert>
#include <cstdio>
#include <string>

int main() {
    // Real addresses from captured sessions: Roblox's own network, and one of theirs on AWS.
    assert(tuxblox::serverAddressForLookup("128.116.31.33") == "128.116.31.33");
    assert(tuxblox::serverAddressForLookup("75.45.133.91") == "75.45.133.91");

    // Review Focus 5: anything that is not a routable IPv4 address is never looked up.
    assert(tuxblox::serverAddressForLookup("127.0.0.1").empty());
    assert(tuxblox::serverAddressForLookup("10.34.6.31").empty());
    assert(tuxblox::serverAddressForLookup("192.168.1.10").empty());
    assert(tuxblox::serverAddressForLookup("172.16.0.1").empty());
    assert(tuxblox::serverAddressForLookup("172.32.0.1") == "172.32.0.1");
    assert(tuxblox::serverAddressForLookup("169.254.1.1").empty());
    assert(tuxblox::serverAddressForLookup("0.0.0.0").empty());
    assert(tuxblox::serverAddressForLookup("239.0.0.1").empty());
    assert(tuxblox::serverAddressForLookup("2606:4700::1111").empty());
    assert(tuxblox::serverAddressForLookup("roblox.com").empty());
    assert(tuxblox::serverAddressForLookup("128.116.31").empty());
    assert(tuxblox::serverAddressForLookup("128.116.31.999").empty());
    assert(tuxblox::serverAddressForLookup("128.116.31.33.5").empty());
    assert(tuxblox::serverAddressForLookup("").empty());

    assert(tuxblox::geoLookupUrl("128.116.31.33") == "https://geo.tuxblox.net/128.116.31.33");
    assert(tuxblox::geoLookupUrl("roblox.com").empty());

    // The live shape, measured 2026-10-06.
    tuxblox::ServerRegion region;
    assert(tuxblox::serverRegionFromJson(
        "{\"ASN\":\"AS22697\",\"owner\":\"Roblox\",\"location\":\"London, England\",\"country\":\"United Kingdom\"}",
        region));
    assert(region.asn == "AS22697");
    assert(region.owner == "Roblox");
    assert(tuxblox::regionNotificationTitle(region) ==
           "Connected to a server in London, England, United Kingdom");
    // No network line: the name of the place is the whole of what anybody wants from this.

    tuxblox::ServerRegion aws;
    assert(tuxblox::serverRegionFromJson(
        "{\"ASN\":\"AS16509\",\"owner\":\"Amazon.com, Inc.\",\"location\":\"Athens, Attica\",\"country\":\"Greece\"}",
        aws));
    assert(tuxblox::regionNotificationTitle(aws) == "Connected to a server in Athens, Attica, Greece");

    // A country with no city still reads properly.
    tuxblox::ServerRegion bare;
    assert(tuxblox::serverRegionFromJson("{\"country\":\"Germany\"}", bare));
    assert(tuxblox::regionNotificationTitle(bare) == "Connected to a server in Germany");

    // Anything that is not an answer is not one, and never throws in the presence loop.
    tuxblox::ServerRegion bad;
    assert(!tuxblox::serverRegionFromJson("", bad));
    assert(!tuxblox::serverRegionFromJson("not json", bad));
    assert(!tuxblox::serverRegionFromJson("{}", bad));
    assert(!tuxblox::serverRegionFromJson("[1,2,3]", bad));
    assert(!tuxblox::serverRegionFromJson("{\"ASN\":5}", bad));

    const std::string answer =
        "{\"ASN\":\"AS22697\",\"owner\":\"Roblox\",\"location\":\"London, England\",\"country\":\"United Kingdom\"}";

    // The session cache: an address already known is never looked up again.
    int lookups = 0;
    int notices = 0;
    tuxblox::ServerRegionWatcher watcher(
        true,
        [&lookups, &answer](const std::string&) { lookups++; return answer; },
        [&notices](const std::string&) { notices++; });
    watcher.consumeServerAddress("128.116.31.33");
    watcher.consumeServerAddress("128.116.31.33");
    assert(lookups == 1);
    assert(notices == 2);
    watcher.consumeServerAddress("75.45.133.91");
    assert(lookups == 2);

    // A failure is remembered too, so it is not retried for the rest of the session.
    int failed = 0;
    tuxblox::ServerRegionWatcher failing(
        true,
        [&failed](const std::string&) { failed++; return std::string(); },
        [](const std::string&) { assert(false); });
    failing.consumeServerAddress("128.116.31.33");
    failing.consumeServerAddress("128.116.31.33");
    assert(failed == 1);

    // Off means nothing leaves the machine at all.
    int offLookups = 0;
    tuxblox::ServerRegionWatcher off(
        false,
        [&offLookups](const std::string&) { offLookups++; return std::string(); },
        [](const std::string&) {});
    off.consumeServerAddress("128.116.31.33");
    assert(offLookups == 0);

    // A local playtest is never asked about, even with the setting on.
    int localLookups = 0;
    tuxblox::ServerRegionWatcher onLocal(
        true,
        [&localLookups](const std::string&) { localLookups++; return std::string(); },
        [](const std::string&) {});
    onLocal.consumeServerAddress("127.0.0.1");
    assert(localLookups == 0);

    printf("server_region: all tests passed\n");
    return 0;
}
