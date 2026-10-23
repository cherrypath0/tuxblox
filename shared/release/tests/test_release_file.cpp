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

#include "release_file.h"

#include <cassert>
#include <chrono>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace fs = std::filesystem;
using namespace tuxblox;

namespace {

// A release as shared/tools/stage_tree.py writes it.
const char* kRelease = R"({
  "release": "3.6.0.0", "channel": "stable", "changelog": "components",
  "source_commit": "abc123", "source_dirty": false,
  "components": {
    "launcher":   {"version": "3.5.0", "arch": {"x86_64": {"sha256": "aa", "size": 12}, "arm64": null}},
    "compat":     {"version": "3.3.0", "arch": {"x86_64": {"sha256": "bb", "size": 34}, "arm64": null}}
  }})";

class TempDir {
public:
    TempDir() : path_(fs::temp_directory_path() / ("tuxblox-release-test-" + std::to_string(++counter_))) {
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

void writeFile(const std::string& path, const std::string& body) {
    std::ofstream(path) << body;
}

void makeDir(const std::string& path) {
    fs::create_directories(path);
}

std::string readFile(const std::string& path) {
    std::ifstream in(path);
    std::ostringstream body;
    body << in.rdbuf();
    return body.str();
}

void test_parses_a_release() {
    ReleaseFile r = parseReleaseFile(kRelease);
    assert(r.release == "3.6.0.0");
    assert(r.channel == "stable");
    assert(r.changelog == "components");
    assert(r.sourceCommit == "abc123");
    assert(r.sourceDirty == false);
    assert(r.components.size() == 2);
    assert(r.components.at("launcher").version == "3.5.0");
    assert(r.components.at("launcher").x86_64->sha256 == "aa");
    assert(r.components.at("launcher").x86_64->size == 12);
    // Review Focus 1: null is absence, never a placeholder.
    assert(!r.components.at("launcher").arm64.has_value());
    printf("  parses a release\n");
}

void test_refuses_a_structurally_wrong_release() {
    // Each pair: the body, and a word the message must contain so the refusal names the problem.
    const std::pair<const char*, const char*> cases[] = {
        {R"({})", "release"},
        {R"({"release": "3.6.0", "channel": "stable", "components": {"launcher": {}}})", "3.6.0"},
        {R"({"release": "3.6.0.0", "components": {"launcher": {}}})", "channel"},
        {R"({"release": "3.6.0.0", "channel": "stable"})", "components"},
        {R"({"release": "3.6.0.0", "channel": "stable", "components": {}})", "components"},
        {R"({"release": "3.6.0.0", "channel": "stable", "components": {"launcher": {"arch": {"x86_64": {"sha256": "a", "size": 1}}}}})", "launcher"},
        {R"({"release": "3.6.0.0", "channel": "stable", "components": {"launcher": {"version": "3.5", "arch": {"x86_64": {"sha256": "a", "size": 1}}}}})", "launcher"},
        {R"({"release": "3.6.0.0", "channel": "stable", "components": {"launcher": {"version": "3.5.0"}}})", "launcher"},
        // A component this build does not know is carried (see below), but the shape still has to
        // be a release's shape -- a malformed one is refused by name whether we recognise it or not.
        {R"({"release": "3.6.0.0", "channel": "stable", "components": {"nonesuch": {"arch": {"x86_64": {"sha256": "a", "size": 1}}}}})", "nonesuch"},
        {R"({"release": "3.6.0.0", "channel": "stable", "components": {"nonesuch": {"version": "1.0.0", "arch": {"x86_64": null, "arm64": null}}}})", "nonesuch"},
        {"not json", "parse"},
    };
    for (const auto& [body, word] : cases) {
        bool threw = false;
        try {
            parseReleaseFile(body);
        } catch (const std::runtime_error& e) {
            threw = true;
            assert(std::string(e.what()).find(word) != std::string::npos);
        }
        assert(threw);
    }
    printf("  refuses a structurally wrong release, naming the problem\n");
}

// Review Focus 1: a component published for no architecture at all.
void test_refuses_a_component_published_nowhere() {
    const char* body = R"({"release": "3.6.0.0", "channel": "stable", "components":
        {"launcher": {"version": "3.5.0", "arch": {"x86_64": null, "arm64": null}}}})";
    bool threw = false;
    try {
        parseReleaseFile(body);
    } catch (const std::runtime_error& e) {
        threw = true;
        assert(std::string(e.what()).find("launcher") != std::string::npos);
    }
    assert(threw);
    printf("  refuses a component published for no architecture\n");
}

void test_the_layout_table() {
    assert(componentNames().size() == 5);
    assert(componentLayout("compat")->publishedAs == "compat.tar.zst");
    assert(componentLayout("compat")->filename == "compat");
    assert(componentLayout("compat")->isArchive == true);
    assert(componentLayout("launcher")->displayName == "Launcher");
    assert(componentLayout("installer")->displayName == "Updater");
    assert(componentLayout("studio-mcp")->filename == "studio-mcp");
    assert(componentLayout("libtuxblox")->isArchive == true);
    assert(!componentLayout("bootstrapper").has_value());   // folded into the launcher in 3.4.0
    assert(!componentLayout("constructor").has_value());
    printf("  the layout table\n");
}

void test_urls() {
    assert(componentUrl("https://setup.tuxblox.net", "compat", "3.3.0", "x86_64") ==
           "https://setup.tuxblox.net/client/components/compat/3.3.0/x86_64/compat.tar.zst");
    assert(latestPointerUrl("https://setup.tuxblox.net", "canary") ==
           "https://setup.tuxblox.net/client/releases/canary/latest");
    assert(releaseFileUrl("https://setup.tuxblox.net", "stable", "3.6.0.0") ==
           "https://setup.tuxblox.net/client/releases/stable/3.6.0.0.json");
    printf("  urls\n");
}

// Review Focus 4: an install whose record is absent or damaged is an ordinary case, not an error.
void test_a_damaged_record_reads_as_no_record() {
    TempDir d;
    assert(readRecordedRelease(d.path()).present == false);          // nothing written yet
    for (const char* body : {"", "   ", "{", R"({"release": "3.6.0.0"})", "not json"}) {
        writeFile(d.path() + "/release.json", body);
        assert(readRecordedRelease(d.path()).present == false);
    }
    printf("  a damaged record reads as no record\n");
}

// The stack ships its version as a file inside its tarball; it is read, never executed.
void test_the_stack_version_is_read_from_its_file() {
    TempDir d;
    assert(!readStackVersion(d.path()).has_value());              // no libtuxblox at all
    makeDir(d.path() + "/libtuxblox");
    assert(!readStackVersion(d.path()).has_value());              // no VERSION in it
    writeFile(d.path() + "/libtuxblox/VERSION", "");
    assert(!readStackVersion(d.path()).has_value());              // empty is not a version
    writeFile(d.path() + "/libtuxblox/VERSION", "  3.1.0 \n");
    assert(*readStackVersion(d.path()) == "3.1.0");               // trimmed
    printf("  the stack version is read from its file\n");
}

void test_a_written_record_reads_back() {
    TempDir d;
    assert(writeRecordedRelease(d.path(), kRelease));
    RecordedRelease r = readRecordedRelease(d.path());
    assert(r.present);
    assert(r.file.release == "3.6.0.0");
    assert(r.file.components.at("compat").version == "3.3.0");
    // The published bytes, verbatim: the record is what the server served, not a re-serialisation
    // of it, so the coherence check compares against exactly what the release said.
    assert(readFile(recordedReleasePath(d.path())) == kRelease);
    // Nothing is left behind by the rename-into-place.
    assert(!fs::exists(recordedReleasePath(d.path()) + ".incoming"));
    printf("  a written record reads back\n");
}

// --------------------------------------------- asking a component its version
// One rule, shared: the launcher deciding "this install is coherent" and the installer deciding
// "this one is already current" must not be able to reach different answers about one install.

void writeStub(const std::string& path, const std::string& body) {
    fs::create_directories(fs::path(path).parent_path());
    writeFile(path, body);
    fs::permissions(path, fs::perms::owner_all);
}

void test_a_component_that_cannot_be_asked_has_no_version() {
    TempDir d;
    assert(!readComponentVersion(d.path(), "studio-mcp").has_value());        // absent
    writeFile(d.path() + "/studio-mcp", "not an executable");                 // present, not runnable
    assert(!readComponentVersion(d.path(), "studio-mcp").has_value());
    writeStub(d.path() + "/studio-mcp", "#!/bin/sh\nexit 1\n");               // runs, fails
    assert(!readComponentVersion(d.path(), "studio-mcp").has_value());
    writeStub(d.path() + "/studio-mcp", "#!/bin/sh\n");                       // runs, says nothing
    assert(!readComponentVersion(d.path(), "studio-mcp").has_value());
    writeStub(d.path() + "/studio-mcp", "#!/bin/sh\necho 2.0.1\n");
    assert(*readComponentVersion(d.path(), "studio-mcp") == "2.0.1");
    printf("  a component that cannot be asked has no version\n");
}

// Leading whitespace too, not only trailing: a version read one way here and another way elsewhere
// is an install that reads as incoherent forever and is repaired by nothing.
void test_a_version_is_trimmed_at_both_ends() {
    TempDir d;
    writeStub(d.path() + "/studio-mcp", "#!/bin/sh\nprintf '  2.0.1 \\n'\n");
    assert(*readComponentVersion(d.path(), "studio-mcp") == "2.0.1");
    printf("  a version is trimmed at both ends\n");
}

// An install made before the layer's folder was renamed still has it under the old name.
void test_the_layer_is_found_under_either_name() {
    TempDir d;
    writeStub(d.path() + "/proton/main", "#!/bin/sh\necho 3.3.0\n");
    assert(*readComponentVersion(d.path(), "compat") == "3.3.0");
    writeStub(d.path() + "/compat/main", "#!/bin/sh\necho 3.4.0\n");
    assert(*readComponentVersion(d.path(), "compat") == "3.4.0");   // the new name wins
    printf("  the layer is found under either name\n");
}

// A component present but unusable must not hang the program that asks it. The installer execs the
// LAUNCHER here, a GTK binary whose loader runs before main(), and the launcher's update check has
// no cancel on this path.
void test_a_component_that_never_exits_is_given_up_on() {
    TempDir d;
    writeStub(d.path() + "/studio-mcp", "#!/bin/sh\nsleep 60\n");
    const auto start = std::chrono::steady_clock::now();
    assert(!readComponentVersion(d.path(), "studio-mcp").has_value());
    const auto elapsed = std::chrono::steady_clock::now() - start;
    assert(std::chrono::duration_cast<std::chrono::seconds>(elapsed).count() < 15);
    printf("  a component that never exits is given up on\n");
}

// A release naming a component this build has never heard of must not wall the client off: the day
// a sixth component ships, every deployed launcher would otherwise refuse the release file outright
// and never update again, with no way back but a manual reinstall.
void test_an_unknown_component_is_carried_not_refused() {
    const char* body = R"({"release": "3.7.0.0", "channel": "stable", "components": {
        "launcher": {"version": "3.5.0", "arch": {"x86_64": {"sha256": "aa", "size": 1}, "arm64": null}},
        "something-new": {"version": "1.0.0", "arch": {"x86_64": {"sha256": "bb", "size": 2}, "arm64": null}}
    }})";
    ReleaseFile r = parseReleaseFile(body);
    assert(r.components.count("launcher") == 1);
    assert(r.components.count("something-new") == 1);
    assert(!componentLayout("something-new").has_value());   // still not one this build can place
    printf("  an unknown component is carried, not refused\n");
}

} // namespace

int main() {
    test_parses_a_release();
    test_refuses_a_structurally_wrong_release();
    test_refuses_a_component_published_nowhere();
    test_the_layout_table();
    test_urls();
    test_a_damaged_record_reads_as_no_record();
    test_the_stack_version_is_read_from_its_file();
    test_a_written_record_reads_back();
    test_a_component_that_cannot_be_asked_has_no_version();
    test_a_version_is_trimmed_at_both_ends();
    test_the_layer_is_found_under_either_name();
    test_a_component_that_never_exits_is_given_up_on();
    test_an_unknown_component_is_carried_not_refused();
    printf("release_file: all tests passed\n");
    return 0;
}
