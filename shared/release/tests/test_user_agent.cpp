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

#include <cassert>
#include <cstdio>
#include <string>

using namespace tuxblox;

int main() {
    const std::string arch = hostArch();

    // Before anything sets it: a name, and nothing it would have to invent.
    assert(userAgent() == "TuxBloxClient");

    setUserAgent(UserAgentProduct::Client, "3.5.0", "stable");
    assert(userAgent() == "TuxBloxClient/3.5.0 (" + arch + "; stable)");

    setUserAgent(UserAgentProduct::Installer, "3.1.0", "canary");
    assert(userAgent() == "TuxBloxInstaller/3.1.0 (" + arch + "; canary)");

    setUserAgent(UserAgentProduct::Client, "3.5.0", "experimental");
    assert(userAgent() == "TuxBloxClient/3.5.0 (" + arch + "; experimental)");

    // An unknown or absent channel is omitted, never reported as "stable". The firewall rule this
    // agent is exempted from, and anything that later reads these logs, key on the channel -- a
    // wrong one is worse than a missing one. The architecture is always known, so it stays.
    for (const char* channel : {"", "dev", "nightly", "STABLE", " stable"}) {
        setUserAgent(UserAgentProduct::Client, "3.5.0", channel);
        assert(userAgent() == "TuxBloxClient/3.5.0 (" + arch + ")");
    }

    // No version to report is still a usable identity.
    setUserAgent(UserAgentProduct::Installer, "", "stable");
    assert(userAgent() == "TuxBloxInstaller (" + arch + "; stable)");

    // Nothing of the old form survives: the firewall exemption and the server's logs are both
    // written against the new one.
    setUserAgent(UserAgentProduct::Client, "3.5.0", "stable");
    assert(userAgent().find("TuxBlox-Client") == std::string::npos);
    assert(userAgent().find("1.0") == std::string::npos);

    printf("user_agent: all tests passed\n");
    return 0;
}
