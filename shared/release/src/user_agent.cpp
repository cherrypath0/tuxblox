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

#include "user_agent.h"

#include "release_file.h"

namespace tuxblox {

namespace {

// Named before anything sets it, so a request made early is still identifiable rather than
// carrying a version or a channel that was guessed.
std::string gAgent = "TuxBloxClient";

bool isKnownChannel(const std::string& channel) {
    return channel == "stable" || channel == "canary" || channel == "experimental";
}

} // namespace

void setUserAgent(UserAgentProduct product, const std::string& version, const std::string& channel) {
    std::string agent = product == UserAgentProduct::Installer ? "TuxBloxInstaller" : "TuxBloxClient";
    if (!version.empty()) agent += "/" + version;
    agent += std::string(" (") + hostArch();
    // An unknown channel is left out rather than reported as "stable". The firewall exempts this
    // agent and anything reading these logs keys on the channel, so a wrong one is worse than a
    // missing one.
    if (isKnownChannel(channel)) agent += "; " + channel;
    agent += ")";
    gAgent = agent;
}

const std::string& userAgent() { return gAgent; }

} // namespace tuxblox
