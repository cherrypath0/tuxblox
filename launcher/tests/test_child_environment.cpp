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

#include "adw_env.h"
#include "app.h"
#include "open_url.h"

#include <cassert>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <sys/stat.h>
#include <thread>
#include <unistd.h>

namespace fs = std::filesystem;

namespace {

void makeStack(const fs::path &root) {
    fs::create_directories(root / "fonts");
    fs::create_directories(root / "share/glib-2.0/schemas");
    std::ofstream(root / "fonts/fonts.conf") << "<fontconfig/>";
    std::ofstream(root / "share/glib-2.0/schemas/gschemas.compiled") << "x";
}

// Stands in for the program being started: records its arguments, then its environment, where the test can read them
void writeRecorder(const fs::path &path) {
    std::ofstream(path) << "#!/bin/sh\n"
                           "printf '%s\\n' \"$@\" > \"$RECORD_OUT.args\"\n"
                           "env > \"$RECORD_OUT.tmp\"\n"
                           "mv \"$RECORD_OUT.tmp\" \"$RECORD_OUT\"\n";
    chmod(path.c_str(), 0755);
}

std::string waitFor(const fs::path &path) {
    for (int attempt = 0; attempt < 100 && !fs::exists(path); ++attempt) {
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
    assert(fs::exists(path));
    std::ifstream in(path);
    std::stringstream text;
    text << in.rdbuf();
    return text.str();
}

bool hasLine(const std::string &text, const std::string &line) {
    std::istringstream lines(text);
    std::string current;
    while (std::getline(lines, current)) {
        if (current == line) return true;
    }
    return false;
}

bool setsVariable(const std::string &text, const std::string &name) {
    std::istringstream lines(text);
    std::string current;
    while (std::getline(lines, current)) {
        if (current.compare(0, name.size() + 1, name + "=") == 0) return true;
    }
    return false;
}

// The launcher re-runs itself to start Roblox, and nothing it hands on may point at the bundled fonts
void watcherDoesNotInheritTheBundledEnvironment(const fs::path &work) {
    const fs::path recorder = work / "fake-launcher";
    writeRecorder(recorder);
    const fs::path record = work / "watcher.env";
    setenv("RECORD_OUT", record.c_str(), 1);
    fs::create_directories(work / "install");

    tuxblox::App app((work / "install").string(), "0.0.0-test", recorder.string());
    tuxblox::useBundledEnvironment((work / "libtuxblox").string());
    app.requestLaunch(tuxblox::LaunchTarget::Studio);
    const std::string environment = waitFor(record);
    const std::string arguments = waitFor(record.string() + ".args");
    tuxblox::restoreBundledEnvironment();

    assert(app.shouldQuit());
    assert(hasLine(environment, "FONTCONFIG_FILE=/host/fonts.conf"));
    assert(!setsVariable(environment, "GSETTINGS_SCHEMA_DIR"));
    assert(hasLine(arguments, "--watch-launch"));
    assert(hasLine(arguments, "studio"));
}

// The browser opened from the About page must use the desktop's own fonts
void browserDoesNotInheritTheBundledEnvironment(const fs::path &work) {
    const fs::path bin = work / "bin";
    fs::create_directories(bin);
    writeRecorder(bin / "xdg-open");
    const fs::path record = work / "browser.env";
    setenv("RECORD_OUT", record.c_str(), 1);
    const std::string oldPath = getenv("PATH") ? getenv("PATH") : "";
    setenv("PATH", (bin.string() + ":" + oldPath).c_str(), 1);

    tuxblox::useBundledEnvironment((work / "libtuxblox").string());
    tuxblox::openUrl("https://tuxblox.net/docs");
    const std::string environment = waitFor(record);
    const std::string arguments = waitFor(record.string() + ".args");
    tuxblox::restoreBundledEnvironment();
    setenv("PATH", oldPath.c_str(), 1);

    assert(hasLine(environment, "FONTCONFIG_FILE=/host/fonts.conf"));
    assert(!setsVariable(environment, "GSETTINGS_SCHEMA_DIR"));
    assert(hasLine(arguments, "https://tuxblox.net/docs"));
}

} // namespace

int main() {
    const fs::path work = fs::temp_directory_path() / ("child_environment_test_" + std::to_string(getpid()));
    fs::remove_all(work);
    fs::create_directories(work);
    makeStack(work / "libtuxblox");
    setenv("FONTCONFIG_FILE", "/host/fonts.conf", 1);
    unsetenv("GSETTINGS_SCHEMA_DIR");

    watcherDoesNotInheritTheBundledEnvironment(work);
    browserDoesNotInheritTheBundledEnvironment(work);

    fs::remove_all(work);
    printf("child_environment: all tests passed\n");
    return 0;
}
