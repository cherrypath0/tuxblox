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

#include "FlagManager.h"

#include "ca_bundle.h"
#include "json.hpp"
#include "tuxblox_default_flags.h"
#include <array>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <cstdio>
#include <fstream>
#include <mutex>
#include <optional>
#include <sstream>
#include <thread>
#include <curl/curl.h>

namespace tuxblox {

namespace {

constexpr std::array<std::pair<const char*, FlagType>, 3> kPrefixes = {{
    {"tbxFlag", FlagType::Bool},
    {"tbxString", FlagType::String},
    {"tbxInt", FlagType::Int},
}};

// Flags the server may not set, enforced here rather than trusted to the server. Adding
// to this list takes a release, which is the point.
constexpr std::array<const char*, 1> kLocalOnly = {{
    "tbxFlagDisableUpdater",
}};

bool startsWith(const std::string& name, const char* prefix) {
    const std::string p = prefix;
    return name.size() > p.size() && name.compare(0, p.size(), p) == 0;
}

} // namespace

std::optional<FlagType> flagTypeForName(const std::string& name) {
    for (const auto& entry : kPrefixes) {
        if (startsWith(name, entry.first)) return entry.second;
    }
    return std::nullopt;
}

bool isLocalOnlyFlag(const std::string& name) {
    for (const char* local : kLocalOnly) {
        if (name == local) return true;
    }
    return false;
}

FlagTable parseFlagJson(const std::string& json, bool fromServer) {
    FlagTable table;
    nlohmann::json parsed;
    try {
        parsed = nlohmann::json::parse(json);
    } catch (const nlohmann::json::exception& failure) {
        // Said out loud: a flag that was dropped and a flag that was never written look
        // identical from the outside, and support cannot tell a user which they have.
        if (!json.empty()) {
            fprintf(stderr, "TuxBlox: ignoring flags that are not valid JSON (%s)\n", failure.what());
        }
        return table;
    }
    if (!parsed.is_object()) {
        fprintf(stderr, "TuxBlox: ignoring flags that are not a JSON object\n");
        return table;
    }

    for (auto it = parsed.begin(); it != parsed.end(); ++it) {
        const std::optional<FlagType> type = flagTypeForName(it.key());
        if (!type.has_value()) {
            fprintf(stderr, "TuxBlox: ignoring the flag \"%s\": its name says no type\n",
                    it.key().c_str());
            continue;
        }
        if (fromServer && isLocalOnlyFlag(it.key())) {
            fprintf(stderr, "TuxBlox: ignoring the flag \"%s\" from the server: it can only be "
                            "set on this machine\n", it.key().c_str());
            continue;
        }

        FlagValue value;
        value.type = *type;
        bool usable = true;
        if (*type == FlagType::Bool) {
            if (!it->is_boolean()) usable = false;
            else value.boolean = it->get<bool>();
        } else if (*type == FlagType::String) {
            if (!it->is_string()) usable = false;
            else value.text = it->get<std::string>();
        } else {
            // is_number_integer() is false for a boolean and for 4.5, which is what keeps
            // "true" from arriving as 1 and a fraction from being truncated.
            if (!it->is_number_integer() || it->is_boolean()) usable = false;
            // It is also TRUE for an unsigned past int64, where get<int64_t>() wraps: a
            // flag set large and positive would arrive as its large negative twin.
            else if (it->is_number_unsigned() &&
                     it->get<uint64_t>() > static_cast<uint64_t>(INT64_MAX)) usable = false;
            else value.integer = it->get<int64_t>();
        }
        if (!usable) {
            fprintf(stderr, "TuxBlox: ignoring the flag \"%s\": its value is not the kind its "
                            "name says\n", it.key().c_str());
            continue;
        }
        table[it.key()] = value;
    }
    return table;
}

void mergeFlagsOver(FlagTable& base, const FlagTable& higher) {
    for (const auto& entry : higher) base[entry.first] = entry.second;
}

// --- what the flags decide about a request ------------------------------

namespace {

bool hostEndsWith(const std::string& host, const char* suffix) {
    const std::string tail = suffix;
    return host.size() >= tail.size() &&
           host.compare(host.size() - tail.size(), tail.size(), tail) == 0;
}

// The host of a URL, lowercased, or empty when there is not one to read.
std::string hostOf(const std::string& url) {
    const std::string::size_type scheme = url.find("://");
    if (scheme == std::string::npos) return std::string();
    const std::string::size_type start = scheme + 3;
    const std::string::size_type end = url.find_first_of("/?#", start);
    std::string host = url.substr(start, end == std::string::npos ? std::string::npos : end - start);
    // Credentials and a port are not part of the name being matched.
    const std::string::size_type at = host.rfind('@');
    if (at != std::string::npos) host = host.substr(at + 1);
    const std::string::size_type colon = host.rfind(':');
    if (colon != std::string::npos) host = host.substr(0, colon);
    for (char& c : host) {
        if (c >= 'A' && c <= 'Z') c = static_cast<char>(c - 'A' + 'a');
    }
    return host;
}

} // namespace

HttpDestination httpDestinationFor(const std::string& url) {
    const std::string host = hostOf(url);
    if (hostEndsWith(host, "rbxcdn.com")) return HttpDestination::RobloxCdn;
    // The download CDN only, not every name under tuxblox.net: the flag server and
    // telemetry are ordinary requests and answer to the third flag.
    if (hostEndsWith(host, "tuxblox.net") && host.compare(0, 5, "setup") == 0) {
        return HttpDestination::TuxbloxCdn;
    }
    return HttpDestination::Other;
}

void applyFlaggedHttpVersion(void* curl, const std::string& url) {
    if (curl == nullptr) return;
    std::string asked;
    switch (httpDestinationFor(url)) {
        case HttpDestination::RobloxCdn: asked = flagString("tbxStringHttpProtocolRbxCdn"); break;
        case HttpDestination::TuxbloxCdn: asked = flagString("tbxStringHttpProtocolCdn"); break;
        case HttpDestination::Other: asked = flagString("tbxStringHttpProtocolOther"); break;
    }
    long version = 0;
    if (asked == "1.0") version = CURL_HTTP_VERSION_1_0;
    else if (asked == "1.1") version = CURL_HTTP_VERSION_1_1;
    // 2TLS rather than 2: over plain HTTP, asking for 2 means a connection nothing here
    // speaks, and every one of these URLs is https anyway.
    else if (asked == "2") version = CURL_HTTP_VERSION_2TLS;
    else if (asked == "3") version = CURL_HTTP_VERSION_3;
    if (version == 0) return;
    curl_easy_setopt(static_cast<CURL*>(curl), CURLOPT_HTTP_VERSION, version);
}

// --- reading a flag -----------------------------------------------------

namespace {

const char* const kConfigBase = "https://config.tuxblox.net";
const char* const kFallbackUserAgent = "TuxBlox";
// A flag set is a short object; anything larger is not one.
constexpr long kMaxBody = 64 * 1024;

// What the fetch thread writes and the first read collects. Shared by pointer so a fetch
// that outran its budget can finish writing into something still alive instead of into a
// table nobody is allowed to touch any more.
struct Pending {
    std::mutex mutex;
    std::condition_variable done;
    bool finished = false;
    std::string body;
};

struct State {
    FlagTable table;
    std::shared_ptr<Pending> pending;
    std::string installDir;
    int timeoutSeconds = 5;
    bool sealed = false;
    bool initialised = false;
};

std::mutex& stateMutex() {
    static std::mutex m;
    return m;
}

State& state() {
    static State s;
    return s;
}

FlagTable defaultTable() {
    return parseFlagJson(kTuxBloxDefaultFlagsJson, false);
}

size_t writeToString(char* ptr, size_t size, size_t nmemb, void* userdata) {
    auto* out = static_cast<std::string*>(userdata);
    if (out->size() + size * nmemb > static_cast<size_t>(kMaxBody)) return 0;
    out->append(ptr, size * nmemb);
    return size * nmemb;
}

std::string curlFetch(const std::string& url, int timeoutSeconds, const std::string& agent) {
    CURL* curl = curl_easy_init();
    if (!curl) return {};
    std::string body;
    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    // Not optional: the libcurl TuxBlox ships is built --without-ca-bundle on purpose,
    // so a request that does not ask for the certificate list fails verification on
    // every machine, forever, and fails soft with nothing said.
    applyCaBundle(curl);
    curl_easy_setopt(curl, CURLOPT_USERAGENT, agent.c_str());
    // This is the one handle in the tree created on a thread spawned for it, which is
    // the case libcurl documents NOSIGNAL for.
    curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1L);
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, static_cast<long>(timeoutSeconds));
    // HTTP/2 where the server offers it, by ALPN. For one small request the gain is
    // marginal -- a TLS handshake dominates -- but it costs nothing to ask for.
    curl_easy_setopt(curl, CURLOPT_HTTP_VERSION, CURL_HTTP_VERSION_2TLS);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writeToString);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &body);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, static_cast<long>(timeoutSeconds));
    const CURLcode result = curl_easy_perform(curl);
    long status = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &status);
    curl_easy_cleanup(curl);
    if (result != CURLE_OK || status < 200 || status >= 300) return {};
    return body;
}

std::string readFile(const std::string& path) {
    std::ifstream file(path, std::ios::binary);
    if (!file) return {};
    std::ostringstream buffer;
    buffer << file.rdbuf();
    return buffer.str();
}

// Collects whatever the fetch produced, applies the server's answer and then the user's
// override, and seals. Called once, by the first read; the caller holds stateMutex().
void sealLocked(State& s) {
    if (s.sealed) return;
    s.sealed = true;

    if (s.pending) {
        std::unique_lock<std::mutex> lock(s.pending->mutex);
        s.pending->done.wait_for(lock, std::chrono::seconds(s.timeoutSeconds),
                                  [&] { return s.pending->finished; });
        if (s.pending->finished && !s.pending->body.empty()) {
            mergeFlagsOver(s.table, parseFlagJson(s.pending->body, true));
        }
    }
    // Last, so it wins: this file is the one a person wrote by hand. Skipped entirely
    // when there is no install directory, rather than consulting the filesystem root.
    if (s.installDir.empty()) return;
    const std::string override = readFile(s.installDir + "/flags.json");
    if (!override.empty()) {
        fprintf(stderr, "TuxBlox: reading flag overrides from %s/flags.json\n", s.installDir.c_str());
        mergeFlagsOver(s.table, parseFlagJson(override, false));
    }
}

// Returns a COPY, not a pointer: a pointer dereferenced after the lock is released is a
// use-after-free the moment anything clears the table.
std::optional<FlagValue> lookup(const std::string& name) {
    std::lock_guard<std::mutex> lock(stateMutex());
    State& s = state();
    if (s.table.empty() && !s.sealed) s.table = defaultTable();
    sealLocked(s);
    auto it = s.table.find(name);
    if (it == s.table.end()) return std::nullopt;
    return it->second;
}

} // namespace

const char* flagArch() {
#if defined(__aarch64__)
    return "arm64";
#else
    return "x86_64";
#endif
}

std::string clientFlagsUrl(const std::string& channel, const std::string& arch) {
    return std::string(kConfigBase) + "/v1/tuxblox/client/" + channel + "/" + arch + "/flags";
}

void resetFlags() {
    std::lock_guard<std::mutex> lock(stateMutex());
    State& s = state();
    s.table.clear();
    s.pending.reset();
    s.installDir.clear();
    s.sealed = false;
    s.initialised = false;
}

void initFlags(const std::string& installDir, const std::string& channel,
               bool isRecordedRelease, int timeoutSeconds,
               FlagFetch fetch, const std::string& baseUrl, const std::string& userAgent) {
    std::lock_guard<std::mutex> lock(stateMutex());
    State& s = state();
    // Ignored rather than obeyed: once anything has read a flag the answer is fixed for
    // the run, and a second init would hand the next reader a different one.
    if (s.initialised || s.sealed) {
        fprintf(stderr, "TuxBlox: flags are already loaded; this call is ignored\n");
        return;
    }
    s.initialised = true;
    s.table = defaultTable();
    s.installDir = installDir;
    s.timeoutSeconds = timeoutSeconds > 0 ? timeoutSeconds : 5;
    s.sealed = false;
    s.pending.reset();

    if (!isRecordedRelease) return;

    // Touched on this thread so its statics are constructed before the detached fetch
    // can reach them, and cannot be destroyed underneath it at exit.
    (void)caBundlePath();

    const std::string base = baseUrl.empty() ? kConfigBase : baseUrl;
    const std::string url = base + "/v1/tuxblox/client/" + channel + "/" + flagArch() + "/flags";
    auto pending = std::make_shared<Pending>();
    s.pending = pending;
    const std::string agent = userAgent.empty() ? kFallbackUserAgent : userAgent;
    FlagFetch run = fetch ? fetch
                          : FlagFetch([agent](const std::string& target, int seconds) {
                                return curlFetch(target, seconds, agent);
                            });

    // Detached rather than joined: a fetch that outran its budget must not hold up the
    // program, and it writes into `pending`, which outlives this scope by shared_ptr.
    std::thread([pending, run, url, seconds = s.timeoutSeconds] {
        std::string body;
        try {
            body = run(url, seconds);
        } catch (...) {
            // A failed fetch is routine: the sources below it already have an answer.
        }
        {
            std::lock_guard<std::mutex> guard(pending->mutex);
            pending->body = std::move(body);
            pending->finished = true;
        }
        pending->done.notify_all();
    }).detach();
}

bool flagBool(const std::string& name) {
    const std::optional<FlagValue> value = lookup(name);
    return value && value->type == FlagType::Bool && value->boolean;
}

int64_t flagInt(const std::string& name) {
    const std::optional<FlagValue> value = lookup(name);
    return value && value->type == FlagType::Int ? value->integer : 0;
}

std::string flagString(const std::string& name) {
    const std::optional<FlagValue> value = lookup(name);
    return value && value->type == FlagType::String ? value->text : std::string();
}

} // namespace tuxblox
