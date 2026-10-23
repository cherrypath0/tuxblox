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

#include "updater.h"
#include "manifest.h"
#include "release_file.h"

#include <cassert>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <map>
#include <sstream>
#include <string>
#include <vector>

namespace fs = std::filesystem;
using namespace tuxblox;

namespace {

fs::path gWork;

void writeFile(const fs::path& path, const std::string& body) {
    fs::create_directories(path.parent_path());
    std::ofstream out(path, std::ios::binary);
    out << body;
}

// A TuxBlox binary answers `--version` with its own bare "x.y.z" -- no channel suffix any more,
// because one component build serves every channel.
void writeVersionStub(const fs::path& path, const std::string& version) {
    writeFile(path, "#!/bin/sh\necho " + version + "\n");
    fs::permissions(path, fs::perms::owner_all);
}

void makeExecutable(const fs::path& path, const std::string& body) {
    writeFile(path, body);
    fs::permissions(path, fs::perms::owner_all);
}

std::vector<std::string> namesOf(const std::vector<ComponentVersion>& components) {
    std::vector<std::string> names;
    for (const ComponentVersion& c : components) names.push_back(c.component);
    return names;
}

std::string versionOf(const std::vector<ComponentVersion>& components, const std::string& name) {
    for (const ComponentVersion& c : components) {
        if (c.component == name) return c.version;
    }
    return "<absent from the list entirely>";
}

// Every path under `dir` with its size, so a check that must touch nothing can be proved to have
// touched nothing.
std::map<std::string, uintmax_t> snapshotDir(const fs::path& dir) {
    std::map<std::string, uintmax_t> seen;
    if (!fs::exists(dir)) return seen;
    for (const auto& entry : fs::recursive_directory_iterator(dir)) {
        seen[entry.path().string()] = entry.is_regular_file() ? fs::file_size(entry.path()) : 0;
    }
    return seen;
}

const std::map<std::string, std::string> kFullSet = {
    {"launcher", "3.5.0"}, {"installer", "3.1.0"}, {"studio-mcp", "2.0.1"},
    {"compat", "3.3.0"}, {"libtuxblox", "3.1.0"},
};

std::string releaseBody(const std::string& release, const std::string& channel,
                         const std::map<std::string, std::string>& versions) {
    std::ostringstream body;
    body << "{\n  \"release\": \"" << release << "\",\n  \"channel\": \"" << channel
         << "\",\n  \"changelog\": \"components\",\n  \"source_commit\": \"abc123\",\n"
            "  \"source_dirty\": false,\n  \"components\": {\n";
    bool first = true;
    for (const auto& [name, version] : versions) {
        if (!first) body << ",\n";
        first = false;
        body << "    \"" << name << "\": {\"version\": \"" << version
             << "\", \"arch\": {\"x86_64\": {\"sha256\": \"aa\", \"size\": 1}, \"arm64\": null}}";
    }
    body << "\n  }\n}\n";
    return body.str();
}

// A published channel: the release file, then the pointer at it.
void publish(const std::string& channel, const std::string& release,
             const std::map<std::string, std::string>& versions) {
    writeFile(gWork / "client" / "releases" / channel / (release + ".json"),
              releaseBody(release, channel, versions));
    writeFile(gWork / "client" / "releases" / channel / "latest", release + "\n");
}

// An install: the four components that can be asked, plus the record of which release it is.
// `installed` and `recorded` are separate on purpose -- an install whose components do not match
// the release it claims to be is the whole incoherent case, and a fixture that writes the record
// from the installed versions can never produce one.
void writeInstall(const fs::path& dir, const std::map<std::string, std::string>& installed,
                  const std::string& recordedRelease,
                  const std::map<std::string, std::string>& recorded) {
    writeVersionStub(dir / "compat" / "main", installed.at("compat"));
    writeVersionStub(dir / "TuxBloxInstaller", installed.at("installer"));
    writeVersionStub(dir / "studio-mcp", installed.at("studio-mcp"));
    writeFile(dir / "libtuxblox" / "VERSION", installed.at("libtuxblox") + "\n");
    // Deliberately a leftover: nothing ships TuxBloxBootstrapper since 3.4.0, so it must never
    // read as part of the install -- counting it would hand off to a repair that cannot fix it.
    writeVersionStub(dir / "TuxBloxBootstrapper", "0.0.1");
    if (!recordedRelease.empty()) {
        writeFile(dir / "release.json", releaseBody(recordedRelease, "stable", recorded));
    }
}

const std::string& baseUrl() {
    static const std::string url = "file://" + gWork.string();
    return url;
}

void test_download_progress_fraction() {
    assert(downloadProgressFraction(50, 100, 999) == 0.5);   // total known -- ignores the fallback
    assert(downloadProgressFraction(50, 0, 200) == 0.25);    // total unknown -- falls back
    assert(downloadProgressFraction(0, 0, 0) == 0.0);        // neither known -- not a divide by zero
    assert(downloadProgressFraction(0, 100, 999) == 0.0);
    printf("  download progress fraction\n");
}

void test_reads_the_four_components_it_can_ask() {
    // libtuxblox is new here, and it is read from its VERSION file rather than run. The launcher
    // is still absent: it is the one asking. A bare "x.y.z" now, with no channel suffix.
    const fs::path dir = gWork / "install_reads_four";
    writeInstall(dir, kFullSet, "3.6.0.0", kFullSet);
    auto v = readComponentVersions(dir.string());
    assert(v.size() == 4);
    assert((namesOf(v) == std::vector<std::string>{"compat", "installer", "studio-mcp", "libtuxblox"}));
    assert(versionOf(v, "compat") == "3.3.0");
    assert(versionOf(v, "libtuxblox") == "3.1.0");
    for (const ComponentVersion& c : v) assert(c.component != "bootstrapper");
    printf("  reads the four components it can ask\n");
}

// Review Focus 3: a binary that exists but cannot be asked is ABSENT, not a version of "".
// An empty string would compare unequal to every recorded version and force a repair forever.
void test_a_component_that_cannot_be_asked_counts_as_absent() {
    const fs::path dir = gWork / "install_unaskable";
    writeFile(dir / "studio-mcp", "not an executable");            // present, not executable
    assert(versionOf(readComponentVersions(dir.string()), "studio-mcp").empty());
    makeExecutable(dir / "studio-mcp", "#!/bin/sh\nexit 1\n");     // runs, exits nonzero
    assert(versionOf(readComponentVersions(dir.string()), "studio-mcp").empty());
    makeExecutable(dir / "studio-mcp", "#!/bin/sh\n");             // runs, prints nothing
    assert(versionOf(readComponentVersions(dir.string()), "studio-mcp").empty());
    printf("  a component that cannot be asked counts as absent\n");
}

// Review Focus 2: a channel with nothing published, and a pointer that is not a release version.
void test_an_unpublished_or_malformed_pointer_is_nothing_to_update_to() {
    for (const char* body : {"", "   ", "\n", "not-a-version", "3.6.0", "../../etc/passwd",
                              "3.6.0.0 extra", "3.6.0.0.0"}) {
        writeFile(gWork / "client" / "releases" / "canary" / "latest", body);
        assert(!fetchLatestRelease(baseUrl(), "canary").has_value());
    }
    printf("  an unpublished or malformed pointer is nothing to update to\n");
}

void test_a_pointer_is_trimmed() {
    writeFile(gWork / "client" / "releases" / "experimental" / "latest", "3.6.0.0\n");
    assert(*fetchLatestRelease(baseUrl(), "experimental") == "3.6.0.0");
    printf("  a pointer is trimmed\n");
}

void test_a_coherent_install_on_the_named_release_needs_no_handoff() {
    publish("stable", "3.6.0.0", kFullSet);
    const fs::path dir = gWork / "install_uptodate";
    writeInstall(dir, kFullSet, "3.6.0.0", kFullSet);
    auto r = runUpdateCheck(baseUrl(), "stable", "3.6.0.0", nullptr, nullptr, dir.string());
    assert(!r.needsHandoff);
    assert(!r.forced);
    printf("  a coherent install on the named release needs no handoff\n");
}

void test_an_incoherent_install_is_forced_and_carries_the_release_bytes() {
    publish("stable", "3.6.0.0", kFullSet);
    auto broken = kFullSet;
    broken["compat"] = "3.2.0";                 // an update that died midway
    const fs::path dir = gWork / "install_mixed";
    // Records 3.6.0.0 (compat 3.3.0) but HAS compat 3.2.0: the pieces do not match the release
    // this install says it is.
    writeInstall(dir, broken, "3.6.0.0", kFullSet);
    auto r = runUpdateCheck(baseUrl(), "stable", "3.6.0.0", nullptr, nullptr, dir.string());
    assert(r.needsHandoff);
    assert(r.forced);
    // The bytes as published, so what the installer is handed is what the launcher read --
    // not a second fetch that could straddle a pointer move.
    assert(parseReleaseFile(r.releaseJson).release == r.release.release);
    assert(r.release.release == "3.6.0.0");
    printf("  an incoherent install is forced and carries the release bytes\n");
}

void test_a_stale_install_hands_off_without_being_forced() {
    publish("stable", "3.6.0.1", kFullSet);
    const fs::path dir = gWork / "install_stale";
    writeInstall(dir, kFullSet, "3.6.0.0", kFullSet);
    auto r = runUpdateCheck(baseUrl(), "stable", "3.6.0.1", nullptr, nullptr, dir.string());
    assert(r.needsHandoff);
    assert(!r.forced);
    printf("  a stale install hands off without being forced\n");
}

void test_a_release_file_the_pointer_names_but_that_is_missing_is_an_error_not_a_handoff() {
    const fs::path dir = gWork / "install_dangling";
    writeInstall(dir, kFullSet, "3.6.0.0", kFullSet);
    auto r = runUpdateCheck(baseUrl(), "stable", "9.9.9.9", nullptr, nullptr, dir.string());
    assert(!r.needsHandoff);
    printf("  a dangling pointer is an error, not a handoff\n");
}

// Nothing is fetched and nothing on disk is touched by a check that only offers an update: an
// install left with its pieces disagreeing would read as broken on the next check and be repaired
// without asking.
void test_an_offered_update_changes_nothing_on_disk() {
    publish("stable", "3.6.0.1", kFullSet);
    const fs::path dir = gWork / "install_untouched";
    writeInstall(dir, kFullSet, "3.6.0.0", kFullSet);
    auto before = snapshotDir(dir);
    auto r = runUpdateCheck(baseUrl(), "stable", "3.6.0.1", nullptr, nullptr, dir.string());
    assert(r.needsHandoff);
    assert(snapshotDir(dir) == before);
    printf("  an offered update changes nothing on disk\n");
}

// The repo owner's decision, 2026-10-23: offered, not forced.
void test_an_install_with_no_record_is_offered_an_update() {
    publish("stable", "3.6.0.0", kFullSet);
    const fs::path dir = gWork / "install_norecord";
    writeInstall(dir, kFullSet, "", kFullSet);           // no release.json at all
    auto r = runUpdateCheck(baseUrl(), "stable", "3.6.0.0", nullptr, nullptr, dir.string());
    assert(r.needsHandoff);
    assert(!r.forced);
    printf("  an install with no record is offered an update\n");
}

// Nothing installed at all is a first run, not a broken install.
void test_an_empty_install_reports_the_layer_missing() {
    publish("stable", "3.6.0.0", kFullSet);
    const fs::path dir = gWork / "install_fresh";
    fs::create_directories(dir);
    auto r = runUpdateCheck(baseUrl(), "stable", "3.6.0.0", nullptr, nullptr, dir.string());
    assert(r.needsHandoff);
    assert(r.compatMissing);
    printf("  an empty install reports the layer missing\n");
}

} // namespace

int main() {
    gWork = fs::temp_directory_path() / "tuxblox_test_updater";
    fs::remove_all(gWork);
    fs::create_directories(gWork);

    test_download_progress_fraction();
    test_reads_the_four_components_it_can_ask();
    test_a_component_that_cannot_be_asked_counts_as_absent();
    test_an_unpublished_or_malformed_pointer_is_nothing_to_update_to();
    test_a_pointer_is_trimmed();
    test_a_coherent_install_on_the_named_release_needs_no_handoff();
    test_an_incoherent_install_is_forced_and_carries_the_release_bytes();
    test_a_stale_install_hands_off_without_being_forced();
    test_a_release_file_the_pointer_names_but_that_is_missing_is_an_error_not_a_handoff();
    test_an_offered_update_changes_nothing_on_disk();
    test_an_install_with_no_record_is_offered_an_update();
    test_an_empty_install_reports_the_layer_missing();

    fs::remove_all(gWork);
    printf("updater: all tests passed\n");
    return 0;
}
