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
    const std::string dir = "/tmp/tuxblox-settings-discord-test";
    std::filesystem::remove_all(dir);
    std::filesystem::create_directories(dir);

    // Off until asked for: presence goes to an outside service and names the
    // place you have open, including one you have not released.
    const tuxblox::Settings fresh = tuxblox::loadSettings(dir);
    assert(!fresh.discordRpc);

    tuxblox::Settings enabled = fresh;
    enabled.discordRpc = true;
    tuxblox::saveSettings(dir, enabled);

    const tuxblox::Settings reloaded = tuxblox::loadSettings(dir);
    assert(reloaded.discordRpc);

    // A settings file written before these keys existed must pick up the
    // defaults without resetting everything else it already holds.
    {
        std::ofstream out(dir + "/settings.json");
        out << "{\"send_crash_reports\":true,\"channel\":\"canary\",\"gpu\":\"card0\"}";
    }
    const tuxblox::Settings older = tuxblox::loadSettings(dir);
    assert(!older.discordRpc);
    assert(older.channel == "canary");
    assert(older.gpu == "card0");
    assert(older.sendCrashReports);

    std::filesystem::remove_all(dir);
    return 0;
}
