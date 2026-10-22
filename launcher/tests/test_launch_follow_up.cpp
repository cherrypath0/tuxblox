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

#include "app.h"
#include "settings.h"

#include <cassert>
#include <cstdio>
#include <filesystem>
#include <fstream>

namespace fs = std::filesystem;
using namespace tuxblox;

namespace {

void test_minimize_to_background_defaults_to_off() {
    assert(!Settings{}.minimizeToBackground);
}

// The flipped default reaches fresh installs only. Nothing migrates a settings file, so somebody
// who already has TuxBlox keeps what their file says until they change it themselves.
void test_an_existing_file_keeps_its_explicit_value(const fs::path& dir) {
    const fs::path install = dir / "explicit";
    fs::create_directories(install);
    std::ofstream(install / "settings.json")
        << R"({"send_crash_reports": true, "minimize_to_background": true})";
    assert(loadSettings(install.string()).minimizeToBackground);
}

void test_a_missing_key_reads_the_new_default(const fs::path& dir) {
    const fs::path install = dir / "missing";
    fs::create_directories(install);
    std::ofstream(install / "settings.json") << R"({"send_crash_reports": true})";
    assert(!loadSettings(install.string()).minimizeToBackground);
}

void test_with_minimize_off_a_launch_shows_sessions() {
    assert(launchFollowUp(false, true) == LaunchFollowUp::ShowSessions);
}

void test_with_minimize_on_a_launch_closes_the_window() {
    assert(launchFollowUp(true, true) == LaunchFollowUp::CloseWindow);
}

// A roblox:// link raises no window at all, so there is nothing to show a session in.
void test_a_uri_launch_never_shows_sessions() {
    assert(launchFollowUp(false, false) == LaunchFollowUp::CloseWindow);
    assert(launchFollowUp(true, false) == LaunchFollowUp::CloseWindow);
}

} // namespace

int main() {
    const fs::path dir = fs::temp_directory_path() / "tuxblox_launch_follow_up_test";
    fs::remove_all(dir);
    fs::create_directories(dir);

    test_minimize_to_background_defaults_to_off();
    test_an_existing_file_keeps_its_explicit_value(dir);
    test_a_missing_key_reads_the_new_default(dir);
    test_with_minimize_off_a_launch_shows_sessions();
    test_with_minimize_on_a_launch_closes_the_window();
    test_a_uri_launch_never_shows_sessions();

    fs::remove_all(dir);
    std::printf("launch_follow_up: all tests passed\n");
    return 0;
}
