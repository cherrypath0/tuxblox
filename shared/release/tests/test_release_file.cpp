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
#include <cstdlib>
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

// A release as the release staging writes it.
const char* kRelease = R"({
  "release": "3.6.0.0", "channel": "stable", "changelog": "components",
  "source_commit": "abc123", "source_dirty": false,
  "components": {
    "launcher":   {"version": "3.5.0", "arch": {"x86_64": {"sha256": "aa", "size": 12}, "arm64": null}},
    "compat":     {"version": "3.3.0", "arch": {"x86_64": {"sha256": "bb", "size": 34}, "arm64": null}}
  }})";

// The same release, with compat's x86_64 entry saying where its bytes are. nullptr says nothing,
// which is what every release published before the field existed looks like.
std::string releaseJsonWithCompatPath(const char* path) {
    const std::string said = path == nullptr ? "" : std::string(", \"path\": \"") + path + "\"";
    return R"({"release": "3.6.0.0", "channel": "stable", "components": {
        "compat": {"version": "3.3.0", "arch": {"x86_64": {"sha256": "bb", "size": 34)" + said +
           R"(}, "arm64": null}}}})";
}

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

// client/{component}/ sits beside client/releases/, so a component named "releases" would shadow
// every release file on the CDN. Nothing stops someone adding one except this.
void test_no_component_shadows_the_releases_path() {
    for (const std::string& name : componentNames()) {
        assert(name != "releases");
        assert(name != "v1" && name != "v2");
    }
    printf("  no component shadows the releases path\n");
}

void test_the_layout_table() {
    assert(componentNames().size() == 5);
    assert(componentLayout("compat")->publishedAs == "compat.tar.zst");
    assert(componentLayout("compat")->filename == "compat");
    assert(componentLayout("compat")->isArchive == true);
    assert(componentLayout("launcher")->displayName == "Launcher");
    assert(componentLayout("installer")->displayName == "Installer");
    assert(componentLayout("studio-mcp")->filename == "studio-mcp");
    assert(componentLayout("libtuxblox")->isArchive == true);
    assert(!componentLayout("bootstrapper").has_value());   // folded into the launcher in 3.4.0
    assert(!componentLayout("constructor").has_value());
    printf("  the layout table\n");
}

void test_urls() {
    // Addressed by the release as well as the component version: every release publishes its own
    // copy of a component, because each copy carries the release it was published in.
    assert(componentUrl("https://setup.tuxblox.net", "compat", "3.3.0", "3.6.0.0", "x86_64") ==
           "https://setup.tuxblox.net/client/compat/3.3.0/3.6.0.0/x86_64/compat.tar.zst");
    // Where one copy served every release that named it, which is still where the last release
    // published before that sits.
    assert(legacyComponentUrl("https://setup.tuxblox.net", "compat", "3.3.0", "x86_64") ==
           "https://setup.tuxblox.net/client/compat/3.3.0/x86_64/compat.tar.zst");
    assert(latestPointerUrl("https://setup.tuxblox.net", "canary") ==
           "https://setup.tuxblox.net/client/releases/canary/latest");
    assert(releaseFileUrl("https://setup.tuxblox.net", "stable", "3.6.0.0") ==
           "https://setup.tuxblox.net/client/releases/stable/3.6.0.0.json");
    assert(publishedUrl("https://setup.tuxblox.net", "compat/3.3.0/3.6.0.0/x86_64/compat.tar.zst") ==
           "https://setup.tuxblox.net/client/compat/3.3.0/3.6.0.0/x86_64/compat.tar.zst");
    assert(publishedUrl("https://setup.tuxblox.net", "").empty());
    printf("  urls\n");
}

// A path off the network becomes part of a URL, so anything that is not a relative path under
// client/ is dropped and the address is built instead. Dropped rather than refused: the release is
// still installable without it.
void test_a_published_path_is_held_to_an_allow_list() {
    const char* refused[] = {
        "",                                    // said nothing
        "/compat/3.3.0/x86_64/compat.tar.zst", // absolute, would leave client/
        "../../etc/passwd",                    // climbs out
        "compat/../../../root/x",              // climbs out further in
        "//evil.example/x",                    // reads as a host to something joining URLs
        "https://evil.example/x",              // a scheme of its own
        "compat/3.3.0/x86_64/compat.tar.zst\\n",// escaped in the JSON, so it parses and is then refused
        "compat/3.3.0/x86_64/compat tar.zst",  // a space has no business here
    };
    for (const char* path : refused) {
        const std::string json = releaseJsonWithCompatPath(path);
        assert(parseReleaseFile(json).components.at("compat").x86_64->path.empty());
    }
    // The shape every release actually publishes, and the one the next layout would.
    for (const char* kept : {"compat/3.3.0/3.6.0.0/x86_64/compat.tar.zst",
                              "releases/3.6.0.0/x86_64/compat.tar.zst"}) {
        const std::string json = releaseJsonWithCompatPath(kept);
        assert(parseReleaseFile(json).components.at("compat").x86_64->path == kept);
    }
    printf("  a published path is held to an allow-list\n");
}

// Best first, and every base is offered one tier before the next tier is reached.
void test_component_urls_try_the_release_s_own_address_first() {
    const ReleaseFile said = parseReleaseFile(
        releaseJsonWithCompatPath("releases/3.6.0.0/x86_64/compat.tar.zst"));
    const std::vector<std::string> urls =
        componentUrls({"https://edge.tuxblox.net", "https://setup.tuxblox.net"}, said, "compat",
                       "x86_64");
    assert(urls.size() == 6);
    assert(urls[0] == "https://edge.tuxblox.net/client/releases/3.6.0.0/x86_64/compat.tar.zst");
    assert(urls[1] == "https://setup.tuxblox.net/client/releases/3.6.0.0/x86_64/compat.tar.zst");
    assert(urls[2] == "https://edge.tuxblox.net/client/compat/3.3.0/3.6.0.0/x86_64/compat.tar.zst");
    assert(urls[4] == "https://edge.tuxblox.net/client/compat/3.3.0/x86_64/compat.tar.zst");

    // A release that says nothing still has both addresses this build can construct.
    const ReleaseFile silent = parseReleaseFile(releaseJsonWithCompatPath(nullptr));
    const std::vector<std::string> fallback =
        componentUrls({"https://setup.tuxblox.net"}, silent, "compat", "x86_64");
    assert(fallback.size() == 2);
    assert(fallback[0] == "https://setup.tuxblox.net/client/compat/3.3.0/3.6.0.0/x86_64/compat.tar.zst");
    assert(fallback[1] == "https://setup.tuxblox.net/client/compat/3.3.0/x86_64/compat.tar.zst");

    // Nothing to try for a component this release does not publish.
    assert(componentUrls({"https://setup.tuxblox.net"}, silent, "studio-mcp", "x86_64").empty());
    printf("  componentUrls tries the release's own address first\n");
}

// The two sides of "does this install hold this release?" are built by one function each, so they
// cannot be made to disagree by a caller comparing the wrong pair of things.
// Which release is ahead decides which one an install takes, so getting 3.10 and 3.9 the
// wrong way round would walk installs backwards on the day a MINOR rolls over.
void test_releases_order_by_number_not_by_text() {
    assert(compareReleases("3.9.11.0", "3.10.11.0") < 0);
    assert(compareReleases("3.10.11.0", "3.9.11.0") > 0);
    assert(compareReleases("3.9.11.0", "3.9.11.0") == 0);
    // The counter is the third field and the patch the last, and both order like the rest.
    assert(compareReleases("3.9.11.0", "3.9.12.0") < 0);
    assert(compareReleases("3.9.11.0", "3.9.11.1") < 0);
    // Anything malformed sorts below everything well-formed rather than throwing: this runs
    // on an answer from the server, and a bad one must not stop an install updating.
    assert(compareReleases("not-a-release", "3.9.11.0") < 0);
    assert(compareReleases("3.9.11", "3.9.11.0") < 0);
    assert(compareReleases("3.9.11.0.1", "3.9.11.0") < 0);
    assert(compareReleases("", "3.9.11.0") < 0);
    assert(compareReleases("nonsense", "rubbish") == 0);
    printf("  releases order by number, not by text\n");
}

// The stack is libraries, so it answers through the one thing in it that can be asked. Whether a
// real library answers correctly is a question for a built stack; what matters here is that
// everything else reads as "there is nothing to ask", rather than as a failure.
void test_the_stack_answers_through_its_library() {
    TempDir d;
    assert(!readStackVersions(d.path()).has_value());              // no libtuxblox at all
    makeDir(d.path() + "/libtuxblox");
    assert(!readStackVersions(d.path()).has_value());              // no version.so in it
    writeFile(d.path() + "/libtuxblox/version.so", "not a library");
    assert(!readStackVersions(d.path()).has_value());              // and not one that can be loaded
    printf("  the stack answers through its library\n");
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
    assert(!readComponentIdentity(d.path(), "studio-mcp").has_value());        // absent
    writeFile(d.path() + "/studio-mcp", "not an executable");                 // present, not runnable
    assert(!readComponentIdentity(d.path(), "studio-mcp").has_value());
    writeStub(d.path() + "/studio-mcp", "#!/bin/sh\nexit 1\n");               // runs, fails
    assert(!readComponentIdentity(d.path(), "studio-mcp").has_value());
    writeStub(d.path() + "/studio-mcp", "#!/bin/sh\n");                       // runs, says nothing
    assert(!readComponentIdentity(d.path(), "studio-mcp").has_value());
    writeStub(d.path() + "/studio-mcp", "#!/bin/sh\necho 2.0.1\n");
    assert(*readComponentIdentity(d.path(), "studio-mcp") == "2.0.1");
    printf("  a component that cannot be asked has no version\n");
}

// Leading whitespace too, not only trailing: a version read one way here and another way elsewhere
// is an install that reads as incoherent forever and is repaired by nothing.
void test_a_version_is_trimmed_at_both_ends() {
    TempDir d;
    writeStub(d.path() + "/studio-mcp", "#!/bin/sh\nprintf '  2.0.1 \\n'\n");
    assert(*readComponentIdentity(d.path(), "studio-mcp") == "2.0.1");
    printf("  a version is trimmed at both ends\n");
}

// An install made before the layer's folder was renamed still has it under the old name.
void test_the_layer_is_found_under_either_name() {
    TempDir d;
    writeStub(d.path() + "/proton/main", "#!/bin/sh\necho 3.3.0\n");
    assert(*readComponentIdentity(d.path(), "compat") == "3.3.0");
    writeStub(d.path() + "/compat/main", "#!/bin/sh\necho 3.4.0\n");
    assert(*readComponentIdentity(d.path(), "compat") == "3.4.0");   // the new name wins
    printf("  the layer is found under either name\n");
}

// A component present but unusable must not hang the program that asks it. The installer execs the
// LAUNCHER here, a GTK binary whose loader runs before main(), and the launcher's update check has
// no cancel on this path.
void test_a_component_that_never_exits_is_given_up_on() {
    TempDir d;
    writeStub(d.path() + "/studio-mcp", "#!/bin/sh\nsleep 60\n");
    const auto start = std::chrono::steady_clock::now();
    assert(!readComponentIdentity(d.path(), "studio-mcp").has_value());
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

// ------------------------------------------------- where downloads come from
// TUXBLOX_RELEASES_CDN_HOST redirects them, and only ever to a tuxblox.net name. Anything that can
// set it can usually also replace the binary it is redirecting, so the allow-list is what carries
// the weight here -- an attacker who can set the variable still cannot point it anywhere they
// control, short of owning a tuxblox.net subdomain.
void test_only_tuxblox_hosts_are_accepted() {
    for (const char* ok : {"setup.tuxblox.net", "setupcdn.devtest1.tuxblox.net", "tuxblox.net",
                            "a.b.c.tuxblox.net", "SETUP.TUXBLOX.NET"}) {
        assert(isTuxbloxHost(ok));
    }
    for (const char* bad : {
            "",
            "evil.com",
            "tuxblox.net.evil.com",          // the suffix is not at the end
            "eviltuxblox.net",               // ends with the name but is not a subdomain of it
            "nottuxblox.net",
            "setup.tuxblox.net.",            // trailing dot
            "setup.tuxblox.net/path",        // not a host at all
            "setup.tuxblox.net:8080",        // a port is not a host
            "user@setup.tuxblox.net",        // credentials
            "https://setup.tuxblox.net",     // a URL, not a host
            "setup..tuxblox.net",            // empty label
            ".tuxblox.net",
            "-setup.tuxblox.net",            // a label may not start with a dash
            "setup-.tuxblox.net",
            "setup .tuxblox.net",
            "setup\t.tuxblox.net",
            "127.0.0.1",
            "localhost",
        }) {
        assert(!isTuxbloxHost(bad));
    }
    printf("  only tuxblox hosts are accepted\n");
}

void test_the_default_is_the_real_cdn() {
    unsetenv("TUXBLOX_RELEASES_CDN_HOST");
    // The edge by default: it caches component bytes close to whoever is downloading them, while
    // the pointer that decides whether an update exists still reaches the origin on every request.
    assert(releasesBaseUrlFor(std::getenv("TUXBLOX_RELEASES_CDN_HOST")) ==
           "https://setup-bunny.tuxblox.net");
    // And the origin is what every fetch falls back to, so the edge being down delays an update
    // rather than making one impossible. It is never the edge.
    assert(releasesOriginUrl() == "https://setup.tuxblox.net");
    assert(releasesOriginUrl() != releasesBaseUrlFor(nullptr));
    // Always https, never a scheme the caller could choose: an override that could say http://
    // would turn a redirect into a downgrade.
    assert(releasesBaseUrlFor("setupcdn.devtest1.tuxblox.net") ==
           "https://setupcdn.devtest1.tuxblox.net");
    // Anything that does not pass falls back rather than failing, because a bad value in the
    // environment must not stop TuxBlox from updating itself from the right place.
    for (const char* bad : {"", "evil.com", "http://setup.tuxblox.net", "tuxblox.net.evil.com"}) {
        assert(releasesBaseUrlFor(bad) == "https://setup-bunny.tuxblox.net");
    }
    printf("  the default is the real CDN, and a bad override falls back to it\n");
}

// A name that passes the allow-list but does not exist must not be used either -- otherwise every
// request fails and the launcher reports a network error instead of quietly doing the right thing.
// Negative only: asserting that a real name resolves would make this test need the network.
void test_a_name_that_does_not_resolve_is_not_used() {
    assert(!hostResolves("nothing-here.devtest1.tuxblox.net"));
    assert(isTuxbloxHost("nothing-here.devtest1.tuxblox.net"));   // it passes the allow-list...
    printf("  a name that does not resolve is rejected on its own merits\n");
}

} // namespace

int main() {
    test_parses_a_release();
    test_refuses_a_structurally_wrong_release();
    test_refuses_a_component_published_nowhere();
    test_no_component_shadows_the_releases_path();
    test_the_layout_table();
    test_urls();
    test_a_published_path_is_held_to_an_allow_list();
    test_component_urls_try_the_release_s_own_address_first();
    test_releases_order_by_number_not_by_text();
    test_the_stack_answers_through_its_library();
    test_a_component_that_cannot_be_asked_has_no_version();
    test_a_version_is_trimmed_at_both_ends();
    test_the_layer_is_found_under_either_name();
    test_a_component_that_never_exits_is_given_up_on();
    test_an_unknown_component_is_carried_not_refused();
    test_only_tuxblox_hosts_are_accepted();
    test_the_default_is_the_real_cdn();
    test_a_name_that_does_not_resolve_is_not_used();
    printf("release_file: all tests passed\n");
    return 0;
}
