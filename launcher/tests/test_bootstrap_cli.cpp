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

// "--update" is a prefix of "--update-roblox", so a plain find() cannot tell a retired
// spelling from a current one. Counting both and comparing can.
static size_t occurrences(const std::string& haystack, const std::string& needle) {
    size_t count = 0;
    for (size_t at = haystack.find(needle); at != std::string::npos;
         at = haystack.find(needle, at + 1)) {
        ++count;
    }
    return count;
}

// Every mention of a mode in `text` uses the current spelling.
static void assertNoRetiredSpellings(const std::string& text) {
    assert(occurrences(text, "--update") == occurrences(text, "--update-roblox"));
    assert(occurrences(text, "--install") == occurrences(text, "--install-roblox"));
    assert(occurrences(text, "--preview") == occurrences(text, "--preview-rbxinstaller"));
}

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

    // Two modes at once is a usage error, not a silent last-one-wins -- and what it
    // tells the user to do instead must name flags that still exist.
    {
        CliOptions o = parse({"--install-roblox", "--update-roblox"});
        assert(!o.error.empty());
        assertNoRetiredSpellings(o.error);
    }

    // Same for the two other pieces of text a mistake can put on screen.
    assertNoRetiredSpellings(parse({}).error);
    assertNoRetiredSpellings(usageText());

    // Asking for help or the version must do that and nothing else. The old separate
    // program checked both before it built anything; passing --help beside a real mode
    // must not start a multi-gigabyte download.
    assert(commandAction(parse({"--update-roblox"})) == CommandAction::Run);
    assert(commandAction(parse({"--install-roblox"})) == CommandAction::Run);
    assert(commandAction(parse({"--help"})) == CommandAction::Usage);
    assert(commandAction(parse({"--update-roblox", "--help"})) == CommandAction::Usage);
    assert(commandAction(parse({"--install-roblox", "--help"})) == CommandAction::Usage);
    assert(commandAction(parse({"--version"})) == CommandAction::Version);
    assert(commandAction(parse({"--preview-rbxinstaller", "--version"})) == CommandAction::Version);
    // Version is answered ahead of help, as the old program answered it.
    assert(commandAction(parse({"--version", "--help"})) == CommandAction::Version);
    assert(commandAction(parse({"--install-roblox", "--update-roblox"})) == CommandAction::Error);
    assert(commandAction(parse({})) == CommandAction::Error);

    // Unknown flags and stray positionals are reported, never ignored.
    assert(!parse({"--instal-roblox"}).error.empty());
    assert(!parse({"version-abc123"}).error.empty());
    assert(!parse({"--preview-rbxinstaller", "--nonsense"}).error.empty());

    assert(usageText() != nullptr);

    printf("cli: all tests passed\n");
    return 0;
}
