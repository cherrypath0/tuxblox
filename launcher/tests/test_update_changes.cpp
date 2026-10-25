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

// What the dialog offering an update says it would change. There is no changelog to read, so this
// is the difference between the two release files -- the same file the download is driven by, which
// is what stops it describing one update and performing another.

#include "update_changes.h"

#include <cassert>
#include <cstdio>
#include <map>
#include <string>

using namespace tuxblox;

namespace {

const std::map<std::string, std::string> kInstalled = {
    {"launcher", "3.5.0"}, {"installer", "3.1.0"}, {"studio-mcp", "2.0.1"},
    {"compat", "3.3.0"}, {"libtuxblox", "3.1.0"},
};

std::map<std::string, std::string> with(std::map<std::string, std::string> base,
                                         const std::string& name, const std::string& version) {
    base[name] = version;
    return base;
}

void test_only_the_components_that_move_are_listed() {
    const auto changes = updateChanges(kInstalled, with(kInstalled, "launcher", "3.6.0"));
    assert(changes.size() == 1);
    assert(changes[0].display == "Launcher");
    assert(changes[0].from == "3.5.0");
    assert(changes[0].to == "3.6.0");
    printf("  only the components that move are listed\n");
}

// The order is the layout's, so the dialog reads the same way every time.
void test_several_moving_at_once_keep_the_layouts_order() {
    auto named = with(kInstalled, "compat", "3.4.0");
    named = with(named, "launcher", "3.6.0");
    const auto changes = updateChanges(kInstalled, named);
    assert(changes.size() == 2);
    assert(changes[0].display == "Launcher");
    assert(changes[1].display == "Runtime");
    printf("  several moving at once keep the layout's order\n");
}

// A rebuild publishes the same component versions under a new release number. Nothing moved, so the
// dialog has a release to name and no lines under it.
void test_a_release_in_which_nothing_moved_lists_nothing() {
    assert(updateChanges(kInstalled, kInstalled).empty());
    printf("  a release in which nothing moved lists nothing\n");
}

// An install that records no release has nothing to compare against, so every component reads as
// new rather than as unchanged -- which is what the dialog should say, since all of them arrive.
void test_an_install_with_no_record_lists_everything_the_release_names() {
    const auto changes = updateChanges({}, kInstalled);
    assert(changes.size() == 5);
    for (const ComponentChange& change : changes) assert(change.from.empty());
    printf("  an install with no record lists everything the release names\n");
}

// Releases gain and lose components. One this build has never heard of cannot be described, and one
// the new release drops cannot be called removed without knowing what that would mean.
void test_a_component_the_new_release_does_not_name_is_left_out() {
    auto named = kInstalled;
    named.erase("studio-mcp");
    const auto changes = updateChanges(kInstalled, named);
    assert(changes.empty());

    const auto unknown = updateChanges(kInstalled, with(kInstalled, "something-new", "1.0.0"));
    assert(unknown.empty());
    printf("  a component the new release does not name is left out\n");
}

void test_the_line_names_the_component_on_both_sides() {
    assert(changeLine({"Launcher", "3.5.0", "3.6.0"}) == "Launcher 3.5.0 \xE2\x86\x92 Launcher 3.6.0");
    printf("  the line names the component on both sides\n");
}

void test_a_component_with_nothing_to_come_from_reads_as_one_version() {
    assert(changeLine({"Runtime", "", "3.4.0"}) == "Runtime 3.4.0");
    printf("  a component with nothing to come from reads as one version\n");
}

} // namespace

int main() {
    test_only_the_components_that_move_are_listed();
    test_several_moving_at_once_keep_the_layouts_order();
    test_a_release_in_which_nothing_moved_lists_nothing();
    test_an_install_with_no_record_lists_everything_the_release_names();
    test_a_component_the_new_release_does_not_name_is_left_out();
    test_the_line_names_the_component_on_both_sides();
    test_a_component_with_nothing_to_come_from_reads_as_one_version();
    printf("update_changes: all tests passed\n");
    return 0;
}
