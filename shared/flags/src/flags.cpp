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

#include "flags.h"
#include "flag_table.h"
#include "tuxblox_default_flags.h"
#include <chrono>
#include <condition_variable>
#include <fstream>
#include <mutex>
#include <sstream>
#include <thread>
#include <curl/curl.h>

namespace tuxblox {

namespace {

const char* const kConfigBase = "https://config.tuxblox.net";
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

std::string curlFetch(const std::string& url, int timeoutSeconds) {
    CURL* curl = curl_easy_init();
    if (!curl) return {};
    std::string body;
    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
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
    // Last, so it wins: this file is the one a person wrote by hand.
    const std::string override = readFile(s.installDir + "/flags.json");
    if (!override.empty()) mergeFlagsOver(s.table, parseFlagJson(override, false));
}

const FlagValue* lookup(const std::string& name) {
    std::lock_guard<std::mutex> lock(stateMutex());
    State& s = state();
    if (s.table.empty() && !s.sealed) s.table = defaultTable();
    sealLocked(s);
    auto it = s.table.find(name);
    return it == s.table.end() ? nullptr : &it->second;
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

void initFlags(const std::string& installDir, const std::string& channel,
               bool isRecordedRelease, int timeoutSeconds,
               FlagFetch fetch, const std::string& baseUrl) {
    std::lock_guard<std::mutex> lock(stateMutex());
    State& s = state();
    s.table = defaultTable();
    s.installDir = installDir;
    s.timeoutSeconds = timeoutSeconds > 0 ? timeoutSeconds : 5;
    s.sealed = false;
    s.pending.reset();

    if (!isRecordedRelease) return;

    const std::string base = baseUrl.empty() ? kConfigBase : baseUrl;
    const std::string url = base + "/v1/tuxblox/client/" + channel + "/" + flagArch() + "/flags";
    auto pending = std::make_shared<Pending>();
    s.pending = pending;
    FlagFetch run = fetch ? fetch : FlagFetch(curlFetch);

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
    const FlagValue* value = lookup(name);
    return value && value->type == FlagType::Bool && value->boolean;
}

int64_t flagInt(const std::string& name) {
    const FlagValue* value = lookup(name);
    return value && value->type == FlagType::Int ? value->integer : 0;
}

std::string flagString(const std::string& name) {
    const FlagValue* value = lookup(name);
    return value && value->type == FlagType::String ? value->text : std::string();
}

} // namespace tuxblox
