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

#include "bridge.h"

namespace tuxblox {
namespace bridge {

namespace {

const char* const kRelease = "3.12.15.2";
const char* const kSha256 = "dbd0a7f2e8616fb216eb2093a53c9151f01b8a0761d629a494bfa097e00872ea";

// The same two, in the same order, as every other TuxBlox download.
const char* const kHosts[] = {"https://setup-bunny.tuxblox.net", "https://setup.tuxblox.net"};

} // namespace

const char* pinnedRelease() {
    return kRelease;
}

const char* installerSha256() {
    return kSha256;
}

std::vector<std::string> installerUrls() {
    std::vector<std::string> urls;
    for (const char* host : kHosts) {
        urls.push_back(std::string(host) + "/client/releases/" + kRelease + "/x86_64/installer");
    }
    return urls;
}

bool answersItself(const std::vector<std::string>& argv) {
    for (const std::string& arg : argv) {
        if (arg == "--version" || arg == "--help" || arg == "-h") return true;
    }
    return false;
}

bool wantsVersion(const std::vector<std::string>& argv) {
    for (const std::string& arg : argv) {
        if (arg == "--version") return true;
    }
    return false;
}

} // namespace bridge
} // namespace tuxblox
