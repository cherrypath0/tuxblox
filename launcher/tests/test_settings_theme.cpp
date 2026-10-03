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


#include "settings.h"

#include <cassert>
#include <filesystem>
#include <fstream>
#include <string>

int main() {
    const std::string dir = "/tmp/tuxblox-settings-theme-test";
    std::filesystem::remove_all(dir);
    std::filesystem::create_directories(dir);

    // Follows the desktop until the user picks something else.
    const tuxblox::Settings fresh = tuxblox::loadSettings(dir);
    assert(fresh.theme == "system");

    for (const char *theme : {"dark", "grey", "light", "system"}) {
        tuxblox::Settings chosen = fresh;
        chosen.theme = theme;
        tuxblox::saveSettings(dir, chosen);
        assert(tuxblox::loadSettings(dir).theme == theme);
    }

    // A settings file written before this key existed keeps everything it already holds.
    {
        std::ofstream out(dir + "/settings.json");
        out << "{\"send_crash_reports\":true,\"channel\":\"canary\",\"discord_rpc\":true}";
    }
    const tuxblox::Settings older = tuxblox::loadSettings(dir);
    assert(older.theme == "system");
    assert(older.channel == "canary");
    assert(older.discordRpc);

    // A name the launcher has no colours for falls back rather than leaving the window unstyled.
    {
        std::ofstream out(dir + "/settings.json");
        out << "{\"send_crash_reports\":false,\"theme\":\"neon\"}";
    }
    assert(tuxblox::loadSettings(dir).theme == "system");

    std::filesystem::remove_all(dir);
    return 0;
}
