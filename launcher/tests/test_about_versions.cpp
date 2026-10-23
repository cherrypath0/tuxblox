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

#include "about_versions.h"
#include "release_file.h"

#include <algorithm>
#include <cassert>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <string>
#include <utility>

namespace fs = std::filesystem;
using namespace tuxblox;

namespace {

const char* kRelease = R"({
  "release": "3.6.0.0", "channel": "stable", "changelog": "components",
  "source_commit": "abc123", "source_dirty": false,
  "components": {
    "launcher":   {"version": "3.5.0", "arch": {"x86_64": {"sha256": "aa", "size": 1}, "arm64": null}},
    "installer":  {"version": "3.1.0", "arch": {"x86_64": {"sha256": "bb", "size": 1}, "arm64": null}},
    "studio-mcp": {"version": "2.0.1", "arch": {"x86_64": {"sha256": "cc", "size": 1}, "arm64": null}},
    "compat":     {"version": "3.3.0", "arch": {"x86_64": {"sha256": "dd", "size": 1}, "arm64": null}},
    "libtuxblox": {"version": "3.1.0", "arch": {"x86_64": {"sha256": "ee", "size": 1}, "arm64": null}}
  }})";

class TempDir {
public:
    TempDir() : path_(fs::temp_directory_path() / ("tuxblox-about-test-" + std::to_string(++counter_))) {
        fs::remove_all(path_);
        fs::create_directories(path_);
    }
    ~TempDir() { fs::remove_all(path_); }
    std::string path() const { return path_.string(); }

private:
    static int counter_;
    fs::path path_;
};
int TempDir::counter_ = 0;

void installFakeComponent(const TempDir& d, const std::string& relativePath,
                           const std::string& version) {
    const fs::path file = fs::path(d.path()) / relativePath;
    fs::create_directories(file.parent_path());
    { std::ofstream out(file); out << "#!/bin/sh\necho " << version << "\n"; }
    fs::permissions(file, fs::perms::owner_all);
}

bool contains(const std::vector<std::pair<std::string, std::string>>& rows,
               const std::pair<std::string, std::string>& row) {
    return std::find(rows.begin(), rows.end(), row) != rows.end();
}

void test_the_release_and_its_components() {
    TempDir d;
    writeRecordedRelease(d.path(), kRelease);
    installFakeComponent(d, "compat/main", "3.3.0");
    AboutVersions a = aboutVersions(d.path(), "3.5.0");
    assert(a.release == "3.6.0.0");
    assert(a.channel == "stable");
    // The launcher first, and its version is the one the running binary knows rather than one read
    // off disk -- it is the thing doing the asking.
    assert(a.components.front() == std::make_pair(std::string("Launcher"), std::string("3.5.0")));
    assert(contains(a.components, {"Compatibility layer", "3.3.0"}));
    printf("  the release and its components\n");
}

// An install with no record still has components, and the screen still has to say something.
void test_an_install_with_no_record_says_so_rather_than_showing_nothing() {
    TempDir d;
    AboutVersions a = aboutVersions(d.path(), "3.5.0");
    assert(a.release.empty());
    assert(!a.components.empty());
    assert(a.components.front() == std::make_pair(std::string("Launcher"), std::string("3.5.0")));
    printf("  an install with no record still says what it can\n");
}

void test_a_component_that_is_not_installed_is_named_as_such() {
    TempDir d;
    writeRecordedRelease(d.path(), kRelease);
    AboutVersions a = aboutVersions(d.path(), "3.5.0");
    // Named rather than omitted: the list is always the same five rows, so an absence is something
    // a person can see instead of something they have to notice is missing.
    assert(contains(a.components, {"Studio MCP", "not installed"}));
    assert(a.components.size() == componentNames().size());
    printf("  a component that is not installed is named as such\n");
}

// The order is the layout's, so two bug reports list the same things in the same order.
void test_the_rows_are_always_the_same_five_in_the_same_order() {
    TempDir d;
    AboutVersions a = aboutVersions(d.path(), "3.5.0");
    std::vector<std::string> names;
    for (const auto& [name, version] : a.components) names.push_back(name);
    assert((names == std::vector<std::string>{"Launcher", "Updater", "Studio MCP",
                                               "Compatibility layer", "Libraries"}));
    printf("  the rows are always the same five in the same order\n");
}

} // namespace

int main() {
    test_the_release_and_its_components();
    test_an_install_with_no_record_says_so_rather_than_showing_nothing();
    test_a_component_that_is_not_installed_is_named_as_such();
    test_the_rows_are_always_the_same_five_in_the_same_order();
    printf("about_versions: all tests passed\n");
    return 0;
}
