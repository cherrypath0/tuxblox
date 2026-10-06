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

#include "ca_bundle.h"
#include "desktop_notify.h"
#include "json.hpp"

#include <curl/curl.h>

#include <array>
#include <string>
#include <utility>

namespace tuxblox {
namespace {

const long RequestTimeoutSeconds = 3;

std::size_t writeToString(char *pData, std::size_t size, std::size_t count, void *pUser) {
    static_cast<std::string *>(pUser)->append(pData, size * count);
    return size * count;
}

bool parseOctets(const std::string& address, std::array<int, 4>& into) {
    std::size_t at = 0;
    for (int part = 0; part < 4; part++) {
        std::size_t digits = 0;
        int value = 0;
        while (at < address.size() && address[at] >= '0' && address[at] <= '9') {
            value = value * 10 + (address[at] - '0');
            at++;
            digits++;
            if (digits > 3) {
                return false;
            }
        }
        if (digits == 0 || value > 255) {
            return false;
        }
        into[static_cast<std::size_t>(part)] = value;
        if (part < 3) {
            if (at >= address.size() || address[at] != '.') {
                return false;
            }
            at++;
        }
    }
    return at == address.size();
}

std::string textField(const nlohmann::json& body, const char *pName) {
    if (!body.contains(pName) || !body[pName].is_string()) {
        return std::string();
    }
    return body[pName].get<std::string>();
}

std::string httpGet(const std::string& url) {
    CURL *pCurl = curl_easy_init();
    if (pCurl == nullptr) {
        return std::string();
    }
    applyCaBundle(pCurl);

    std::string body;
    curl_easy_setopt(pCurl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(pCurl, CURLOPT_USERAGENT, "TuxBlox-Client/1.0");
    curl_easy_setopt(pCurl, CURLOPT_WRITEFUNCTION, writeToString);
    curl_easy_setopt(pCurl, CURLOPT_WRITEDATA, &body);
    curl_easy_setopt(pCurl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(pCurl, CURLOPT_TIMEOUT, RequestTimeoutSeconds);
    curl_easy_setopt(pCurl, CURLOPT_CONNECTTIMEOUT, RequestTimeoutSeconds);
    curl_easy_setopt(pCurl, CURLOPT_NOSIGNAL, 1L);

    const CURLcode code = curl_easy_perform(pCurl);
    long status = 0;
    curl_easy_getinfo(pCurl, CURLINFO_RESPONSE_CODE, &status);
    curl_easy_cleanup(pCurl);

    if (code != CURLE_OK || status != 200) {
        return std::string();
    }
    return body;
}

} // namespace

std::string serverAddressForLookup(const std::string& address) {
    std::array<int, 4> octet = {0, 0, 0, 0};
    if (!parseOctets(address, octet)) {
        return std::string();
    }
    // A local playtest, a link-local address, a multicast address or anything inside a private range is not a server somebody connected to.
    if (octet[0] == 0 || octet[0] == 127 || octet[0] == 10 || octet[0] >= 224) {
        return std::string();
    }
    if (octet[0] == 192 && octet[1] == 168) {
        return std::string();
    }
    if (octet[0] == 172 && octet[1] >= 16 && octet[1] <= 31) {
        return std::string();
    }
    if (octet[0] == 169 && octet[1] == 254) {
        return std::string();
    }
    return address;
}

std::string geoLookupUrl(const std::string& ip) {
    const std::string checked = serverAddressForLookup(ip);
    if (checked.empty()) {
        return std::string();
    }
    return "https://geo.tuxblox.net/" + checked;
}

bool serverRegionFromJson(const std::string& body, ServerRegion& into) {
    const nlohmann::json parsed = nlohmann::json::parse(body, nullptr, false);
    if (parsed.is_discarded() || !parsed.is_object()) {
        return false;
    }
    ServerRegion region;
    region.asn = textField(parsed, "ASN");
    region.owner = textField(parsed, "owner");
    region.location = textField(parsed, "location");
    region.country = textField(parsed, "country");
    // Without somewhere to name there is nothing worth telling anybody.
    if (region.location.empty() && region.country.empty()) {
        return false;
    }
    into = region;
    return true;
}

std::string regionNotificationTitle(const ServerRegion& region) {
    std::string where = region.location;
    if (!where.empty() && !region.country.empty()) {
        where += ", ";
    }
    where += region.country;
    return "Connected to a server in " + where;
}

std::string regionNotificationBody(const ServerRegion& region) {
    if (region.owner.empty()) {
        return region.asn;
    }
    if (region.asn.empty()) {
        return region.owner;
    }
    return region.owner + " (" + region.asn + ")";
}

ServerRegionWatcher::ServerRegionWatcher(bool enabled, LookupFn lookup, NotifyFn notify)
    : enabled_(enabled), lookup_(std::move(lookup)), notify_(std::move(notify)) {}

void ServerRegionWatcher::consumeServerAddress(const std::string& address) {
    if (!enabled_) {
        return;
    }
    const std::string url = geoLookupUrl(address);
    if (url.empty()) {
        return;
    }

    auto known = seen_.find(address);
    if (known == seen_.end()) {
        std::optional<ServerRegion> answer;
        ServerRegion region;
        if (serverRegionFromJson(lookup_(url), region)) {
            answer = region;
        }
        known = seen_.emplace(address, answer).first;
    }
    if (known->second.has_value()) {
        notify_(regionNotificationTitle(*known->second), regionNotificationBody(*known->second));
    }
}

ServerRegionWatcher makeServerRegionWatcher(bool enabled) {
    return ServerRegionWatcher(
        enabled,
        [](const std::string& url) { return httpGet(url); },
        [](const std::string& title, const std::string& body) {
            showDesktopNotification(title, body, "network-server");
        });
}

} // namespace tuxblox
