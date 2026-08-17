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

#include "cli.h"
#include <cassert>
#include <cstdio>
#include <initializer_list>
#include <string>
#include <vector>

using tuxblox::CliOptions;
using tuxblox::parseArgs;

namespace {
// Rebuilds a real argv (argv[0] is the program name, as it always is at
// runtime) so the tests exercise parseArgs exactly as main() calls it.
CliOptions parse(std::initializer_list<const char*> args) {
    std::vector<const char*> argv;
    argv.push_back("TuxBloxInstaller");
    for (const char* a : args) argv.push_back(a);
    return parseArgs(static_cast<int>(argv.size()), argv.data());
}
} // namespace

int main() {
    // No arguments -- the double-clicked, fresh-install case.
    {
        CliOptions o = parse({});
        assert(!o.uninstall && !o.headless && !o.noLaunch && !o.help);
        assert(o.channel == "stable");
        assert(o.error.empty());
    }

    // Each flag on its own.
    {
        CliOptions o = parse({"--headless"});
        assert(o.headless && !o.noLaunch && o.error.empty());
    }
    {
        CliOptions o = parse({"--nolaunch"});
        assert(o.noLaunch && !o.headless && o.error.empty());
    }
    {
        CliOptions o = parse({"--uninstall"});
        assert(o.uninstall && o.error.empty());
    }
    {
        CliOptions o = parse({"--help"});
        assert(o.help && o.error.empty());
        CliOptions s = parse({"-h"});
        assert(s.help && s.error.empty());
    }

    // --channel consumes the following argument as its value.
    {
        CliOptions o = parse({"--channel", "beta"});
        assert(o.channel == "beta");
        assert(o.error.empty());
    }

    // All of them together, in an order the launcher would never use.
    {
        CliOptions o = parse({"--nolaunch", "--channel", "beta", "--headless"});
        assert(o.headless && o.noLaunch);
        assert(o.channel == "beta");
        assert(o.error.empty());
    }

    // --uninstall used to be recognized only as argv[1]; it must now be
    // accepted anywhere, without breaking the launcher's argv[1] handoff.
    {
        CliOptions o = parse({"--headless", "--uninstall"});
        assert(o.uninstall && o.headless && o.error.empty());
    }

    // --channel as the trailing argument has no value to consume: an error,
    // not a silent fall back to "stable" (which would quietly install the
    // wrong channel during a launcher upgrade handoff).
    {
        CliOptions o = parse({"--channel"});
        assert(!o.error.empty());
    }

    // Unknown flags and stray positional arguments are rejected rather than
    // ignored -- a typo'd --nolanch must not silently launch the launcher.
    {
        CliOptions o = parse({"--nolanch"});
        assert(!o.error.empty());
        assert(o.error.find("--nolanch") != std::string::npos);
    }
    {
        CliOptions o = parse({"install"});
        assert(!o.error.empty());
    }

    // Usage text exists and mentions every supported flag -- main() prints
    // it for both --help and a usage error.
    {
        const std::string usage = tuxblox::usageText();
        assert(usage.find("--headless") != std::string::npos);
        assert(usage.find("--nolaunch") != std::string::npos);
        assert(usage.find("--uninstall") != std::string::npos);
        assert(usage.find("--channel") != std::string::npos);
    }

    printf("cli: all tests passed\n");
    return 0;
}
