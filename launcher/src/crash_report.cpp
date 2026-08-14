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
constexpr long kMaxLogTailBytes = 1 * 1024 * 1024;

std::string isoTimestampUtc() {
    std::time_t t = std::time(nullptr);
    std::tm tmBuf{};
    gmtime_r(&t, &tmBuf);
    char buf[32];
    std::strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%SZ", &tmBuf);
    return buf;
}

size_t curlDiscardResponse(char* /*ptr*/, size_t size, size_t nmemb, void* /*userdata*/) {
    return size * nmemb; // response body is irrelevant -- just drain it
}

} // namespace

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

nlohmann::json buildReportJson(const CrashReport& report, const std::string& timestampIso) {
    nlohmann::json j;
    j["launcher_version"] = report.launcherVersion;
    j["proton_version"] = report.protonVersion;
    j["target"] = report.target == LaunchTarget::Player ? "player" : "studio";
    j["proton_exit_code"] = report.protonExitCode;
    j["roblox_exit_code"] = report.robloxExitCode ? nlohmann::json(*report.robloxExitCode) : nlohmann::json(nullptr);
    j["timestamp"] = timestampIso;
    j["os"] = report.systemInfo.os;
    j["display_server"] = report.systemInfo.displayServer;
    j["desktop_environment"] = report.systemInfo.desktopEnvironment;
    j["gpu"] = report.systemInfo.gpu;
    j["has_root"] = report.systemInfo.hasRootPrivileges;
    return j;
}

void uploadCrashReport(const CrashReport& report) {
    CURL* curl = curl_easy_init();
    if (!curl) return;

    std::string jsonBody = buildReportJson(report, isoTimestampUtc()).dump();
    std::string logTail = readLogTail(report.logPath, kMaxLogTailBytes);

    // multipart/form-data, not a JSON body -- the log travels as an actual
    // file part instead of being escaped into a JSON string field.
    curl_mime* mime = curl_mime_init(curl);

    curl_mimepart* dataPart = curl_mime_addpart(mime);
    curl_mime_name(dataPart, "data");
    curl_mime_type(dataPart, "application/json");
    curl_mime_data(dataPart, jsonBody.c_str(), jsonBody.size());

    curl_mimepart* logPart = curl_mime_addpart(mime);
    curl_mime_name(logPart, "logfile");
    curl_mime_filename(logPart, "log.txt");
    curl_mime_type(logPart, "text/plain");
    curl_mime_data(logPart, logTail.c_str(), logTail.size());

    curl_easy_setopt(curl, CURLOPT_URL, kTelemetryUrl);
    curl_easy_setopt(curl, CURLOPT_USERAGENT, "TuxBlox-Client/1.0");
    curl_easy_setopt(curl, CURLOPT_MIMEPOST, mime);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, curlDiscardResponse);
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 5L);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 10L);

    curl_easy_perform(curl); // fire-and-forget -- failure is silently ignored

    curl_mime_free(mime);
    curl_easy_cleanup(curl);
}

} // namespace tuxblox
