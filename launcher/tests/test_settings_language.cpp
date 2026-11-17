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
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <string>

namespace fs = std::filesystem;

int main() {
    using namespace tuxblox;

    const std::string dir = (fs::temp_directory_path() / "tuxblox_test_settings_language").string();
    fs::remove_all(dir);

    // An install that predates the field reads as "system".
    {
        fs::create_directories(dir);
        std::ofstream out(dir + "/settings.json", std::ios::binary);
        out << R"({"channel":"stable","send_crash_reports":false})";
        out.close();
        Settings loaded = loadSettings(dir);
        assert(loaded.language == "system");
        assert(loaded.channel == "stable");
    }

    // Round-trip both shipped languages.
    for (const std::string language : {"pt_BR", "es"}) {
        Settings s;
        s.language = language;
        saveSettings(dir, s);
        assert(loadSettings(dir).language == language);
    }

    // Hand-edited aliases normalize instead of falling back to system.
    for (const auto &[stored, expected] : {std::pair{"pt-BR", "pt_BR"},
                                           std::pair{"pt", "pt_BR"},
                                           std::pair{"es_MX", "es"},
                                           std::pair{"en_US", "en"}}) {
        std::ofstream out(dir + "/settings.json", std::ios::binary);
        out << "{\"language\":\"" << stored << "\",\"channel\":\"stable\",\"send_crash_reports\":false}";
        out.close();
        assert(loadSettings(dir).language == expected);
    }

    // An unknown value does not prevent every other setting from loading.
    {
        std::ofstream out(dir + "/settings.json", std::ios::binary);
        out << R"({"language":"nonsense","channel":"canary","send_crash_reports":false})";
        out.close();
        Settings loaded = loadSettings(dir);
        assert(loaded.language == "system");
        assert(loaded.channel == "canary");
    }

    fs::remove_all(dir);
    printf("settings_language: all tests passed\n");
    return 0;
}
