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

#include "ui_cache.h"
#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string>

namespace fs = std::filesystem;

int main() {
    using namespace tuxblox;

    fs::path work = fs::temp_directory_path() / "tuxblox_test_ui_cache";
    fs::remove_all(work);
    fs::create_directories(work);

    // XDG_CACHE_HOME wins when it is absolute.
    setenv("XDG_CACHE_HOME", (work / "xdg").c_str(), 1);
    assert(uiCacheRoot() == (work / "xdg" / "tuxblox").string());
    assert(uiCacheDir("2.9.0", std::string(64, 'a')) == (work / "xdg" / "tuxblox" / "ui-2.9.0-aaaaaaaaaaaa").string());
    assert(uiCacheDir("2.9.0", std::string(64, 'a')) != uiCacheDir("2.9.0", std::string(64, 'b')));

    // An empty XDG_CACHE_HOME is the same as it being unset, per the XDG spec.
    setenv("XDG_CACHE_HOME", "", 1);
    setenv("HOME", (work / "home").c_str(), 1);
    assert(uiCacheRoot() == (work / "home" / ".cache" / "tuxblox").string());

    // A relative XDG_CACHE_HOME is rejected rather than joined onto the current directory, matching TUXBLOX_ROOT.
    setenv("XDG_CACHE_HOME", "relative/path", 1);
    bool threw = false;
    try { uiCacheRoot(); } catch (const std::runtime_error&) { threw = true; }
    assert(threw);

    // Nowhere to put a cache at all is an error worth reporting, not a silent fallback to the working directory.
    unsetenv("XDG_CACHE_HOME");
    unsetenv("HOME");
    threw = false;
    try { uiCacheRoot(); } catch (const std::runtime_error&) { threw = true; }
    assert(threw);

    setenv("XDG_CACHE_HOME", (work / "xdg").c_str(), 1);

    // Completion is recorded by digest, so a payload that changed invalidates an extraction that did not.
    const std::string digest(64, 'a');
    const std::string dir = uiCacheDir("2.9.0", digest);
    fs::create_directories(dir);
    assert(!uiCacheIsComplete(dir, digest));            // no marker yet
    uiCacheMarkComplete(dir, digest);
    assert(uiCacheIsComplete(dir, digest));
    assert(!uiCacheIsComplete(dir, std::string(64, 'b'))); // different payload
    { std::ofstream out(fs::path(dir) / ".complete"); out << "garbage"; }
    assert(!uiCacheIsComplete(dir, digest));            // unreadable marker is not complete

    // Pruning keeps the current extraction, removes other ui-* ones, and leaves anything else alone.
    fs::create_directories(fs::path(uiCacheRoot()) / "ui-2.8.0");
    fs::create_directories(fs::path(uiCacheRoot()) / "ui-9.9.9");
    fs::create_directories(fs::path(uiCacheRoot()) / "something-else");
    uiCachePruneOthers(dir);
    assert(fs::exists(dir));
    assert(!fs::exists(fs::path(uiCacheRoot()) / "ui-2.8.0"));
    // A higher-sorting version from another build is still not this build's, so it goes too.
    assert(!fs::exists(fs::path(uiCacheRoot()) / "ui-9.9.9"));
    assert(fs::exists(fs::path(uiCacheRoot()) / "something-else"));

    // keepDir passed WITH a trailing slash: the kept directory still exists afterwards, and a sibling ui-* is still removed.
    fs::create_directories(fs::path(uiCacheRoot()) / "ui-2.8.0");
    uiCachePruneOthers(dir + "/");
    assert(fs::exists(dir));
    assert(!fs::exists(fs::path(uiCacheRoot()) / "ui-2.8.0"));

    // prune with a keepDir whose parent contains a non-"ui-" directory: that directory survives.
    fs::create_directories(fs::path(uiCacheRoot()) / "ui-3.0.0");
    fs::create_directories(fs::path(uiCacheRoot()) / "other-data");
    uiCachePruneOthers(dir);
    assert(fs::exists(dir));
    assert(!fs::exists(fs::path(uiCacheRoot()) / "ui-3.0.0"));
    assert(fs::exists(fs::path(uiCacheRoot()) / "other-data"));

    fs::remove_all(work);
    printf("ui_cache: all tests passed\n");
    return 0;
}
