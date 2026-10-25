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

// Which number names an install. Home and About both show one, and showing different numbers on two
// screens of the same window is the failure this exists to prevent.

#include "install_identity.h"

#include <cassert>
#include <cstdio>

using namespace tuxblox;

namespace {

void test_a_recorded_release_is_what_names_the_install() {
    const InstallIdentity id = installIdentity("3.6.0.0", "stable", "3.5.0");
    assert(id.isRelease);
    assert(id.version == "3.6.0.0");
    assert(id.channel == "stable");
    printf("  a recorded release is what names the install\n");
}

// Ordinary, not broken: an install made before the release file existed kept no record of one.
void test_an_install_with_no_record_falls_back_to_this_program() {
    const InstallIdentity id = installIdentity("", "", "3.5.0");
    assert(!id.isRelease);
    assert(id.version == "3.5.0");
    printf("  an install with no record falls back to this program\n");
}

// The channel is never compiled in, so there is nothing to fall back to and nothing to invent: one
// copy of each component serves all three channels, and a release belongs to exactly one. A screen
// showing a release with no channel beside it would be stating a thing it does not know.
void test_a_channel_is_never_reported_without_a_release() {
    assert(installIdentity("", "", "3.5.0").channel.empty());
    assert(installIdentity("", "canary", "3.5.0").channel.empty());
    printf("  a channel is never reported without a release\n");
}

// A record naming a release but no channel cannot be trusted to name the release either, so it
// reads the same way as no record at all rather than showing half of one.
void test_a_release_without_a_channel_reads_as_no_record() {
    const InstallIdentity id = installIdentity("3.6.0.0", "", "3.5.0");
    assert(!id.isRelease);
    assert(id.version == "3.5.0");
    printf("  a release without a channel reads as no record\n");
}

// The release and the launcher's own version are different numbers with different shapes, and the
// whole point of the change is that the screens stopped showing the second one.
void test_the_release_is_not_the_launcher_version() {
    const InstallIdentity id = installIdentity("3.6.1.0", "experimental", "3.5.0");
    assert(id.version != "3.5.0");
    assert(id.channel == "experimental");
    printf("  the release is not the launcher version\n");
}

} // namespace

int main() {
    test_a_recorded_release_is_what_names_the_install();
    test_an_install_with_no_record_falls_back_to_this_program();
    test_a_channel_is_never_reported_without_a_release();
    test_a_release_without_a_channel_reads_as_no_record();
    test_the_release_is_not_the_launcher_version();
    printf("install_identity: all tests passed\n");
    return 0;
}
