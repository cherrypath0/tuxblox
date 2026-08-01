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

#include "crash_report.h"
#include "json.hpp"
#include <cstdio>
#include <ctime>
#include <curl/curl.h>
#include <fstream>
#include <sstream>

namespace tuxblox {

namespace {

constexpr const char* kTelemetryUrl = "https://telemetry.tuxblox.net/report";
// Matches the server's own accepted-body cap (see the telemetry.js module) --
// keeping the client cap in sync avoids uploading data the server will just
// reject.
constexpr long kMaxLogTailBytes = 512 * 1024;

std::string isoTimestampUtc() {
    std::time_t t = std::time(nullptr);
    std::tm tmBuf{};
    gmtime_r(&t, &tmBuf);
    char buf[32];
    std::strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%SZ", &tmBuf);
    return buf;
}

// Reads at most the last `maxBytes` of `path`. Best-effort: a missing or
// unreadable log file just means an empty tail, not a failed upload.
std::string readLogTail(const std::string& path, long maxBytes) {
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file) return "";

    std::streamoff size = file.tellg();
    std::streamoff start = size > maxBytes ? size - maxBytes : 0;
    file.seekg(start);

    std::ostringstream buf;
    buf << file.rdbuf();
    return buf.str();
}

size_t curlDiscardResponse(char* /*ptr*/, size_t size, size_t nmemb, void* /*userdata*/) {
    return size * nmemb; // response body is irrelevant -- just drain it
}

} // namespace

void uploadCrashReport(const CrashReport& report) {
    CURL* curl = curl_easy_init();
    if (!curl) return;

    nlohmann::json j;
    j["launcher_version"] = report.launcherVersion;
    j["proton_version"] = report.protonVersion;
    j["target"] = report.target == LaunchTarget::Player ? "player" : "studio";
    j["exit_code"] = report.exitCode;
    j["timestamp"] = isoTimestampUtc();
    j["log_tail"] = readLogTail(report.logPath, kMaxLogTailBytes);

    std::string body = j.dump();

    struct curl_slist* headers = nullptr;
    headers = curl_slist_append(headers, "Content-Type: application/json");

    curl_easy_setopt(curl, CURLOPT_URL, kTelemetryUrl);
    curl_easy_setopt(curl, CURLOPT_USERAGENT, "TuxBlox-Client/1.0");
    curl_easy_setopt(curl, CURLOPT_POST, 1L);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, body.c_str());
    curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, static_cast<long>(body.size()));
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, curlDiscardResponse);
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 5L);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 10L);

    curl_easy_perform(curl); // fire-and-forget -- failure is silently ignored

    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);
}

} // namespace tuxblox
