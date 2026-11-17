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

#include "roblox_config.h"
#include "roblox_deploy.h"
#include "release_file.h"
#include <cstdio>
#include <cstdlib>
#include <stdexcept>

namespace tuxblox {

namespace {

const char* const kConfigHost = "https://config.tuxblox.net";

} // namespace

std::string configBaseUrlFor(const char* host) {
    if (host == nullptr || host[0] == '\0') return kConfigHost;
    const std::string name = host;
    if (!isTuxbloxHost(name)) return kConfigHost;
    return "https://" + name;
}

const std::string& configBaseUrl() {
    static const std::string resolved = [] {
        const char* host = std::getenv("TUXBLOX_CONFIG_HOST");
        const std::string chosen = configBaseUrlFor(host);
        if (chosen == kConfigHost) {
            // Say why, but only when something was actually asked for and ignored.
            if (host != nullptr && host[0] != '\0') {
                fprintf(stderr, "TuxBlox: ignoring TUXBLOX_CONFIG_HOST=\"%s\" -- not a "
                                 "tuxblox.net address; using %s\n", host, kConfigHost);
            }
            return std::string(kConfigHost);
        }
        if (!hostResolves(host)) {
            fprintf(stderr, "TuxBlox: TUXBLOX_CONFIG_HOST=\"%s\" does not resolve; using %s\n",
                     host, kConfigHost);
            return std::string(kConfigHost);
        }
        fprintf(stderr, "TuxBlox: reading the version history from %s (TUXBLOX_CONFIG_HOST)\n",
                 chosen.c_str());
        return chosen;
    }();
    return resolved;
}

std::string configPreviousUrl(const std::string& channel, const std::string& binaryType) {
    return configBaseUrl() + "/v1/roblox/client-version/" + normalizeChannel(channel) + "/" +
           binaryType + "/previous";
}

std::optional<std::string> parsePreviousHash(const std::string& json) {
    try {
        // Roblox's own response shape, so this is the parser the launcher already has.
        return parseClientVersionHash(json);
    } catch (const std::exception&) {
        return std::nullopt;
    }
}

} // namespace tuxblox
