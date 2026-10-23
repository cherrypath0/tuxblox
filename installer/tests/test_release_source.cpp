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

// Which release an installer run installs, and where it learns it from.

#include "cli.h"

#include <cassert>
#include <cstdio>
#include <initializer_list>
#include <string>
#include <vector>

using namespace tuxblox;

namespace {

CliOptions parse(std::initializer_list<const char*> args) {
    std::vector<const char*> argv;
    argv.push_back("TuxBloxInstaller");
    for (const char* a : args) argv.push_back(a);
    return parseArgs(static_cast<int>(argv.size()), argv.data());
}

void test_latest_is_the_default_now() {
    // A bare installer someone downloaded installs the newest release on its channel. It used to
    // install the version it was built for, which is now a COMPONENT version and not a release
    // at all -- it would 404 and report "TuxBlox 3.1.0 is not published".
    CliOptions o = parse({});
    assert(o.error.empty());
    assert(releaseSourceFor(o).kind == ReleaseSource::Kind::Latest);
    printf("  latest is the default now\n");
}

void test_a_named_release_for_support() {
    CliOptions o = parse({"--release", "3.6.0.1"});
    assert(o.error.empty());
    ReleaseSource s = releaseSourceFor(o);
    assert(s.kind == ReleaseSource::Kind::Named);
    assert(s.release == "3.6.0.1");
    printf("  a named release for support\n");
}

void test_a_release_that_is_not_a_release_version_is_refused() {
    // Refused here rather than 404ing later, so the message names the mistake instead of the HTTP
    // status. A three-field version is the likely slip: that is a COMPONENT version now.
    for (const char* bad : {"3.6.0", "latest", "", "../3.6.0.0", "3.6.0.0 ", "v3.6.0.0", "3.6.0.0.0"}) {
        CliOptions o = parse({"--release", bad});
        assert(!o.error.empty());
    }
    assert(!parse({"--release"}).error.empty());          // the flag with no value at all
    printf("  a release that is not a release version is refused\n");
}

void test_the_handoff_file_wins_over_everything() {
    CliOptions o = parse({"--release-file", "/tmp/x.json", "--release", "3.6.0.1"});
    assert(o.error.empty());
    assert(releaseSourceFor(o).kind == ReleaseSource::Kind::File);
    assert(releaseSourceFor(o).path == "/tmp/x.json");
    printf("  the handoff file wins over everything\n");
}

void test_a_release_file_needs_a_value() {
    assert(!parse({"--release-file"}).error.empty());
    printf("  a release file needs a value\n");
}

void test_latest_is_still_accepted_and_does_nothing() {
    // An older launcher's handoff passes --latest. Accepting it keeps that handoff working
    // through the transition; it names what is now the default.
    CliOptions o = parse({"--latest"});
    assert(o.error.empty());
    assert(releaseSourceFor(o).kind == ReleaseSource::Kind::Latest);
    printf("  latest is still accepted and does nothing\n");
}

void test_the_usage_text_names_the_new_flags() {
    std::string u = usageText();
    assert(u.find("--release") != std::string::npos);
    assert(u.find("--release-file") != std::string::npos);
    printf("  the usage text names the new flags\n");
}

} // namespace

int main() {
    test_latest_is_the_default_now();
    test_a_named_release_for_support();
    test_a_release_that_is_not_a_release_version_is_refused();
    test_the_handoff_file_wins_over_everything();
    test_a_release_file_needs_a_value();
    test_latest_is_still_accepted_and_does_nothing();
    test_the_usage_text_names_the_new_flags();
    printf("release_source: all tests passed\n");
    return 0;
}
