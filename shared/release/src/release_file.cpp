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

#include "json.hpp"

#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <netdb.h>
#include <sys/socket.h>
#include <filesystem>
#include <chrono>
#include <cerrno>
#include <csignal>
#include <poll.h>
#include <unistd.h>
#include <sys/wait.h>
#include <fstream>
#include <sstream>
#include <stdexcept>

namespace fs = std::filesystem;

namespace tuxblox {

namespace {

const char* kArches[] = {"x86_64", "arm64"};

struct LayoutEntry {
    const char* name;
    ComponentLayout layout;
};

// The order is the order componentNames() reports and the order an install is written in.
const LayoutEntry kLayouts[] = {
    {"launcher",   {"TuxBloxLauncher",    "TuxBloxLauncher",  "/", "Launcher",            "TuxBloxLauncher",  false}},
    {"installer",  {"TuxBloxInstaller",   "TuxBloxInstaller", "/", "Installer",           "TuxBloxInstaller", false}},
    {"studio-mcp", {"studio-mcp",         "studio-mcp",       "/", "Studio MCP",          "studio-mcp",       false}},
    {"compat",     {"compat.tar.zst",     "compat",           "/", "Runtime", "compat/main",      true}},
    {"libtuxblox", {"libtuxblox.tar.zst", "libtuxblox",       "/", "Libraries",           "",                 true}},
};

bool hasDottedFields(const std::string& value, int fields) {
    if (value.empty()) return false;
    int seen = 1;
    size_t digits = 0;
    for (char c : value) {
        if (c == '.') {
            if (digits == 0) return false;
            digits = 0;
            seen++;
        } else if (std::isdigit(static_cast<unsigned char>(c))) {
            digits++;
        } else {
            return false;
        }
    }
    return digits > 0 && seen == fields;
}

std::string trimmed(const std::string& value) {
    const size_t first = value.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) return "";
    return value.substr(first, value.find_last_not_of(" \t\r\n") - first + 1);
}

std::string readWholeFile(const fs::path& path) {
    std::ifstream in(path, std::ios::binary);
    if (!in) throw std::runtime_error("cannot read " + path.string());
    std::ostringstream body;
    body << in.rdbuf();
    return body.str();
}

// A component's entry, once the file as a whole has been shown to be a release. Every message names
// the component, because a release has five of them and "has no version" alone says nothing.
ReleaseComponent parseComponent(const std::string& name, const nlohmann::json& entry) {
    const std::string prefix = "component \"" + name + "\" ";
    if (!entry.is_object()) throw std::runtime_error(prefix + "is not an object");

    ReleaseComponent component;
    if (!entry.contains("version") || !entry["version"].is_string()) {
        throw std::runtime_error(prefix + "has no version");
    }
    component.version = entry["version"].get<std::string>();
    if (!hasDottedFields(component.version, 3)) {
        throw std::runtime_error(prefix + "has version \"" + component.version +
                                  "\", which is not three numbers");
    }
    if (!entry.contains("arch") || !entry["arch"].is_object()) {
        throw std::runtime_error(prefix + "names no architectures");
    }

    const nlohmann::json& arch = entry["arch"];
    int published = 0;
    for (const char* name_of_arch : kArches) {
        if (!arch.contains(name_of_arch) || arch[name_of_arch].is_null()) continue;
        const nlohmann::json& bytes = arch[name_of_arch];
        const std::string where = prefix + "(" + name_of_arch + ") ";
        if (!bytes.is_object()) throw std::runtime_error(where + "is not an object");
        if (!bytes.contains("sha256") || !bytes["sha256"].is_string()) {
            throw std::runtime_error(where + "has no sha256");
        }
        if (!bytes.contains("size") || !bytes["size"].is_number_unsigned()) {
            throw std::runtime_error(where + "has no size");
        }
        ComponentBytes value{bytes["sha256"].get<std::string>(), bytes["size"].get<uint64_t>()};
        if (std::string(name_of_arch) == "x86_64") {
            component.x86_64 = value;
        } else {
            component.arm64 = value;
        }
        published++;
    }
    if (published == 0) {
        throw std::runtime_error(prefix + "is published for no architecture");
    }
    return component;
}

} // namespace

const std::vector<std::string>& componentNames() {
    static const std::vector<std::string> names = [] {
        std::vector<std::string> all;
        for (const LayoutEntry& entry : kLayouts) all.push_back(entry.name);
        return all;
    }();
    return names;
}

std::optional<ComponentLayout> componentLayout(const std::string& name) {
    for (const LayoutEntry& entry : kLayouts) {
        if (name == entry.name) return entry.layout;
    }
    return std::nullopt;
}

ReleaseFile parseReleaseFile(const std::string& jsonText) {
    nlohmann::json body;
    try {
        body = nlohmann::json::parse(jsonText);
    } catch (const std::exception& e) {
        throw std::runtime_error(std::string("release file does not parse: ") + e.what());
    }
    if (!body.is_object()) throw std::runtime_error("release file is not an object");

    ReleaseFile release;
    if (!body.contains("release") || !body["release"].is_string()) {
        throw std::runtime_error("release file states no release version");
    }
    release.release = body["release"].get<std::string>();
    if (!hasDottedFields(release.release, 4)) {
        throw std::runtime_error("\"" + release.release + "\" is not a four-field release version");
    }
    if (!body.contains("channel") || !body["channel"].is_string()) {
        throw std::runtime_error("release file states no channel");
    }
    release.channel = body["channel"].get<std::string>();
    if (release.channel != "stable" && release.channel != "canary" && release.channel != "experimental") {
        throw std::runtime_error("release file names channel \"" + release.channel +
                                  "\", which is not one TuxBlox publishes");
    }
    if (!body.contains("components") || !body["components"].is_object() || body["components"].empty()) {
        throw std::runtime_error("release file lists no components");
    }

    if (body.contains("source_commit") && body["source_commit"].is_string()) {
        release.sourceCommit = body["source_commit"].get<std::string>();
    }
    // Unknown reads as dirty, so a release whose source cannot be attributed is never taken for a
    // clean one -- the same direction the staging side errs in.
    release.sourceDirty = !body.contains("source_dirty") || !body["source_dirty"].is_boolean() ||
                           body["source_dirty"].get<bool>();

    for (const auto& [name, entry] : body["components"].items()) {
        release.components[name] = parseComponent(name, entry);
    }
    return release;
}

namespace {

// The edge by default; the origin is what everything falls back to.
const char* kReleasesEdgeBaseUrl = "https://setup-bunny.tuxblox.net";
const char* kReleasesOriginBaseUrl = "https://setup.tuxblox.net";
const char* kTuxbloxSuffix = ".tuxblox.net";

} // namespace

bool isTuxbloxHost(const std::string& host) {
    if (host.empty() || host.size() > 253) return false;

    std::string lower;
    lower.reserve(host.size());
    for (char c : host) lower += static_cast<char>(std::tolower(static_cast<unsigned char>(c)));

    // Every label is letters, digits and inner dashes. This rejects a scheme, a port, credentials,
    // a path and a trailing dot in one pass, because none of them are made of those characters.
    size_t labelLength = 0;
    for (size_t i = 0; i < lower.size(); i++) {
        const char c = lower[i];
        if (c == '.') {
            if (labelLength == 0 || lower[i - 1] == '-') return false;
            labelLength = 0;
            continue;
        }
        const bool alnum = (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9');
        if (!alnum && c != '-') return false;
        if (c == '-' && labelLength == 0) return false;     // a label may not start with a dash
        labelLength++;
    }
    if (labelLength == 0 || lower.back() == '-') return false;

    if (lower == "tuxblox.net") return true;
    // Not endsWith("tuxblox.net"): that accepts "eviltuxblox.net". The dot has to be there.
    return lower.size() > strlen(kTuxbloxSuffix) &&
           lower.compare(lower.size() - strlen(kTuxbloxSuffix), strlen(kTuxbloxSuffix),
                          kTuxbloxSuffix) == 0;
}

bool hostResolves(const std::string& host) {
    struct addrinfo hints {};
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    struct addrinfo* found = nullptr;
    if (getaddrinfo(host.c_str(), nullptr, &hints, &found) != 0) return false;
    freeaddrinfo(found);
    return true;
}

std::string releasesBaseUrlFor(const char* host) {
    if (host == nullptr || host[0] == '\0') return kReleasesEdgeBaseUrl;
    const std::string name = host;
    if (!isTuxbloxHost(name)) return kReleasesEdgeBaseUrl;
    return "https://" + name;
}

const std::string& releasesBaseUrl() {
    static const std::string resolved = [] {
        const char* host = std::getenv("TUXBLOX_RELEASES_CDN_HOST");
        const std::string chosen = releasesBaseUrlFor(host);
        if (chosen == kReleasesEdgeBaseUrl) {
            // Say why, but only when something was actually asked for and ignored.
            if (host != nullptr && host[0] != '\0') {
                fprintf(stderr, "TuxBlox: ignoring TUXBLOX_RELEASES_CDN_HOST=\"%s\" -- not a "
                                 "tuxblox.net address; using %s\n", host, kReleasesEdgeBaseUrl);
            }
            return std::string(kReleasesEdgeBaseUrl);
        }
        // Only an override is ever resolved, so an ordinary launch makes no extra query.
        if (!hostResolves(host)) {
            fprintf(stderr, "TuxBlox: TUXBLOX_RELEASES_CDN_HOST=\"%s\" does not resolve; using %s\n",
                     host, kReleasesEdgeBaseUrl);
            return std::string(kReleasesEdgeBaseUrl);
        }
        // Never silent: this decides where TuxBlox downloads itself from.
        fprintf(stderr, "TuxBlox: downloading from %s (TUXBLOX_RELEASES_CDN_HOST)\n", chosen.c_str());
        return chosen;
    }();
    return resolved;
}

const std::string& releasesEdgeUrl() {
    static const std::string edge = kReleasesEdgeBaseUrl;
    return edge;
}

const std::string& releasesOriginUrl() {
    static const std::string origin = kReleasesOriginBaseUrl;
    return origin;
}

const char* hostArch() {
#if defined(__aarch64__)
    return "arm64";
#else
    return "x86_64";
#endif
}

std::string componentUrl(const std::string& baseUrl, const std::string& name,
                          const std::string& version, const std::string& arch) {
    const std::optional<ComponentLayout> layout = componentLayout(name);
    if (!layout.has_value()) {
        throw std::runtime_error("\"" + name + "\" is not a component of TuxBlox");
    }
    return baseUrl + "/client/" + name + "/" + version + "/" + arch + "/" + layout->publishedAs;
}

std::string latestPointerUrl(const std::string& baseUrl, const std::string& channel) {
    return baseUrl + "/client/releases/" + channel + "/latest";
}

std::string releaseFileUrl(const std::string& baseUrl, const std::string& channel,
                            const std::string& release) {
    return baseUrl + "/client/releases/" + channel + "/" + release + ".json";
}

std::string recordedReleasePath(const std::string& installDir) {
    return installDir + "/release.json";
}

RecordedRelease readRecordedRelease(const std::string& installDir) {
    try {
        RecordedRelease recorded;
        recorded.file = parseReleaseFile(readWholeFile(recordedReleasePath(installDir)));
        recorded.present = true;
        return recorded;
    } catch (const std::exception&) {
        // Every failure is the same answer to the caller: this install does not say what it is.
        return {};
    }
}

bool writeRecordedRelease(const std::string& installDir, const std::string& publishedJson) {
    const std::string target = recordedReleasePath(installDir);
    const std::string incoming = target + ".incoming";
    try {
        {
            std::ofstream out(incoming, std::ios::binary | std::ios::trunc);
            if (!out) return false;
            out << publishedJson;
            if (!out) return false;
        }
        // Renamed into place, so a launcher reading it while an update writes it never sees half a
        // release -- which would read as an install that does not know what it is.
        std::error_code ec;
        fs::rename(incoming, target, ec);
        if (ec) {
            fs::remove(incoming, ec);
            return false;
        }
        return true;
    } catch (const std::exception&) {
        std::error_code ec;
        fs::remove(incoming, ec);
        return false;
    }
}

// fork/exec rather than popen: no shell is involved, so an install path needs no quoting.
std::optional<std::string> askBinaryVersion(const std::string& bin) {
    if (access(bin.c_str(), X_OK) != 0) return std::nullopt;

    int fds[2];
    if (pipe(fds) != 0) return std::nullopt;
    const pid_t pid = fork();
    if (pid < 0) {
        close(fds[0]);
        close(fds[1]);
        return std::nullopt;
    }
    if (pid == 0) {
        // Its own process group, so a component that spawns something is cleaned up with it rather
        // than leaving a descendant holding the inherited handles of whatever asked.
        setpgid(0, 0);
        dup2(fds[1], STDOUT_FILENO);
        close(fds[0]);
        close(fds[1]);
        execl(bin.c_str(), bin.c_str(), "--version", static_cast<char*>(nullptr));
        _exit(127);
    }
    close(fds[1]);

    // Bounded, because this is on the launcher's startup path and the installer's critical path,
    // and the installer asks the LAUNCHER here -- a graphical binary whose loader runs before
    // main(). A component that never exits, or one that reads the stdin it inherited, would
    // otherwise hang the program asking with no timeout and nothing to cancel.
    constexpr int kVersionTimeoutMs = 5000;
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(kVersionTimeoutMs);

    std::string out;
    char buf[256];
    bool timedOut = false;
    for (;;) {
        const auto left = std::chrono::duration_cast<std::chrono::milliseconds>(
            deadline - std::chrono::steady_clock::now()).count();
        if (left <= 0) { timedOut = true; break; }

        struct pollfd pfd{fds[0], POLLIN, 0};
        const int ready = poll(&pfd, 1, static_cast<int>(left));
        if (ready < 0) {
            if (errno == EINTR) continue;
            timedOut = true;
            break;
        }
        if (ready == 0) { timedOut = true; break; }

        const ssize_t n = read(fds[0], buf, sizeof buf);
        if (n < 0) {
            if (errno == EINTR) continue;
            break;
        }
        if (n == 0) break;                       // the child closed its end
        out.append(buf, static_cast<size_t>(n));
        // One line is all that is wanted, and a component that prints without end must not be
        // allowed to fill this process's memory.
        if (out.find('\n') != std::string::npos || out.size() > 4096) break;
    }
    close(fds[0]);

    if (timedOut) kill(-pid, SIGKILL);   // the group, so nothing it started outlives it

    int status = 0;
    while (waitpid(pid, &status, 0) < 0) {
        if (errno != EINTR) return std::nullopt;
    }
    if (timedOut) return std::nullopt;
    if (!WIFEXITED(status) || WEXITSTATUS(status) != 0) return std::nullopt;

    const std::string version = trimmed(out.substr(0, out.find('\n')));
    if (version.empty()) return std::nullopt;
    return version;
}

std::optional<std::string> readComponentVersion(const std::string& installDir,
                                                 const std::string& name) {
    const std::optional<ComponentLayout> layout = componentLayout(name);
    if (!layout.has_value()) return std::nullopt;
    if (layout->versionPath.empty()) return readStackVersion(installDir);

    const std::optional<std::string> version =
        askBinaryVersion(installDir + "/" + layout->versionPath);
    if (version.has_value() || name != "compat") return version;
    // An install made before the layer's folder was renamed still has it under the old name.
    return askBinaryVersion(installDir + "/proton/main");
}

std::optional<std::string> readStackVersion(const std::string& installDir) {
    try {
        const std::string version = trimmed(readWholeFile(fs::path(installDir) / "libtuxblox" / "VERSION"));
        if (version.empty()) return std::nullopt;
        return version;
    } catch (const std::exception&) {
        return std::nullopt;
    }
}

} // namespace tuxblox
