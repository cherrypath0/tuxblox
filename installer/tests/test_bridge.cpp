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

#include "../bridge/bridge.h"

#include "release_file.h"

#include <cassert>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

int main() {
    using namespace tuxblox::bridge;

    // The whole point of the bridge: the release it fetches from has to be one whose installer can
    // read the current layout, which means newer than the pre-layout installs it rescues.
    assert(tuxblox::compareReleases(pinnedRelease(), "3.9.11.0") > 0);

    // A sha256 is 64 lowercase hex characters, and an empty or short one would mean the bridge ran
    // an unverified binary.
    const std::string sha = installerSha256();
    assert(sha.size() == 64);
    assert(sha.find_first_not_of("0123456789abcdef") == std::string::npos);

    // Both addresses, edge first, and each naming the pinned release rather than a channel: a
    // channel pointer moves, and the bridge has to keep working years after it was published.
    const std::vector<std::string> urls = installerUrls();
    assert(urls.size() == 2);
    assert(urls[0].find("setup-bunny.tuxblox.net") != std::string::npos);
    assert(urls[1].find("setup.tuxblox.net") != std::string::npos);
    for (const std::string &url : urls) {
        assert(url.find(pinnedRelease()) != std::string::npos);
        assert(url.find("/client/releases/") != std::string::npos);
        assert(url.find("/latest") == std::string::npos);
    }
    std::printf("  the bridge points at a pinned post-layout release\n");

    // Asking the bridge its version must never download or install anything.
    assert(answersItself({"--version"}));
    assert(answersItself({"--help"}));
    assert(answersItself({"-h"}));
    assert(answersItself({"--channel", "stable", "--help"}));
    // An ordinary handoff is not a question, however many arguments it carries.
    assert(!answersItself({}));
    assert(!answersItself({"--channel", "stable", "--dir", "/home/x/.tuxblox"}));
    assert(!answersItself({"--release-file", "/home/x/.tuxblox/.release-incoming.json"}));
    std::printf("  only --version/--help answer without installing\n");

    // The launcher reads a component's version by running it with --version and taking the first
    // line, so --version must be told apart from --help and answered with a bare number.
    assert(wantsVersion({"--version"}));
    assert(!wantsVersion({"--help"}));
    assert(!wantsVersion({"-h"}));
    assert(!wantsVersion({"--channel", "stable"}));
    std::printf("  --version is answered apart from --help\n");

    std::printf("bridge: all tests passed\n");
    return 0;
}
