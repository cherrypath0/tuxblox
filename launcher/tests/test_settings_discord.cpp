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

    // Both are off until asked for: presence goes to an outside service, and the
    // place name is the part that would publish an unreleased game.
    const tuxblox::Settings fresh = tuxblox::loadSettings(dir);
    assert(!fresh.discordRpc);
    assert(!fresh.discordRpcPlaceName);

    tuxblox::Settings enabled = fresh;
    enabled.discordRpc = true;
    enabled.discordRpcPlaceName = true;
    tuxblox::saveSettings(dir, enabled);

    const tuxblox::Settings reloaded = tuxblox::loadSettings(dir);
    assert(reloaded.discordRpc);
    assert(reloaded.discordRpcPlaceName);

    // A settings file written before these keys existed must pick up the
    // defaults without resetting everything else it already holds.
    {
        std::ofstream out(dir + "/settings.json");
        out << "{\"send_crash_reports\":true,\"channel\":\"canary\",\"gpu\":\"card0\"}";
    }
    const tuxblox::Settings older = tuxblox::loadSettings(dir);
    assert(!older.discordRpc);
    assert(!older.discordRpcPlaceName);
    assert(older.channel == "canary");
    assert(older.gpu == "card0");
    assert(older.sendCrashReports);

    std::filesystem::remove_all(dir);
    return 0;
}
