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

#include "bootstrap/cli.h"
#include <cassert>
#include <cstdio>
#include <string>
#include <vector>

namespace {
tuxblox::bootstrap::CliOptions parse(const std::vector<std::string>& args) {
    std::vector<const char*> argv;
    argv.push_back("TuxBloxLauncher");
    for (const auto& a : args) argv.push_back(a.c_str());
    return tuxblox::bootstrap::parseArgs(static_cast<int>(argv.size()), argv.data());
}
} // namespace

int main() {
    using namespace tuxblox;
    using namespace tuxblox::bootstrap;

    // No arguments does nothing on purpose: a double-click must not start a
    // multi-gigabyte download.
    {
        CliOptions o = parse({});
        assert(o.mode == Mode::None);
        assert(!o.error.empty());
    }

    // The three real modes.
    assert(parse({"--preview-rbxinstaller"}).mode == Mode::Preview);
    assert(parse({"--install-roblox"}).mode == Mode::Install);
    assert(parse({"--update-roblox"}).mode == Mode::Update);
    assert(parse({"--preview-rbxinstaller"}).error.empty());

    // The old names are not aliases: a caller still passing one is told so
    // rather than quietly getting the mode it used to mean.
    assert(!parse({"--preview"}).error.empty());
    assert(!parse({"--install"}).error.empty());
    assert(!parse({"--update"}).error.empty());

    // Help and version short-circuit, and neither is a mode.
    {
        CliOptions o = parse({"--help"});
        assert(o.help && o.mode == Mode::None && o.error.empty());
    }
    {
        CliOptions o = parse({"--version"});
        assert(o.version && o.error.empty());
    }

    // Two modes at once is a usage error, not a silent last-one-wins.
    {
        CliOptions o = parse({"--install-roblox", "--update-roblox"});
        assert(!o.error.empty());
    }

    // Unknown flags and stray positionals are reported, never ignored.
    assert(!parse({"--instal-roblox"}).error.empty());
    assert(!parse({"version-abc123"}).error.empty());
    assert(!parse({"--preview-rbxinstaller", "--nonsense"}).error.empty());

    assert(usageText() != nullptr);

    printf("cli: all tests passed\n");
    return 0;
}
