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
#include <functional>
#include <map>
#include <optional>
#include <string>

namespace tuxblox {

struct ServerRegion {
    std::string asn;
    std::string owner;
    std::string location;
    std::string country;
};

// The address, if it is one worth asking about. A loopback or private address is a local test rather than a server, and anything malformed is refused so no request is ever built out of a log line we did not understand.
std::string serverAddressForLookup(const std::string& address);

// Where the region is looked up. Empty unless the address passed the check above.
std::string geoLookupUrl(const std::string& ip);

// The region out of that answer. False for anything that is not one. Never throws: this runs in the presence loop.
bool serverRegionFromJson(const std::string& body, ServerRegion& into);

std::string regionNotificationTitle(const ServerRegion& region);
std::string regionNotificationBody(const ServerRegion& region);

// Tells the user which region a server is in. The lookup and the notification are passed in so this is testable without a network or a desktop.
class ServerRegionWatcher {
public:
    using LookupFn = std::function<std::string(const std::string& url)>;
    using NotifyFn = std::function<void(const std::string& title, const std::string& body)>;

    ServerRegionWatcher(bool enabled, LookupFn lookup, NotifyFn notify);

    void consumeServerAddress(const std::string& address);

private:
    bool enabled_;
    LookupFn lookup_;
    NotifyFn notify_;
    // Held for the session only, failures included, so an address already known is never asked about again.
    std::map<std::string, std::optional<ServerRegion>> seen_;
};

// The watcher the launcher uses: a real request with a short timeout, and a desktop notification.
ServerRegionWatcher makeServerRegionWatcher(bool enabled);

} // namespace tuxblox
