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

#include "downloader.h"
#include <curl/curl.h>
#include <cstdio>

namespace tuxblox {

namespace {

struct WriteContext {
    FILE* file = nullptr;
};

size_t curlWriteToFile(char* ptr, size_t size, size_t nmemb, void* userdata) {
    auto* ctx = static_cast<WriteContext*>(userdata);
    return fwrite(ptr, size, nmemb, ctx->file);
}

struct ProgressContext {
    const DownloadProgressFn* onProgress;
    const std::atomic<bool>* cancel;
};

int curlProgressCallback(void* clientp, curl_off_t dltotal, curl_off_t dlnow,
                          curl_off_t /*ultotal*/, curl_off_t /*ulnow*/) {
    auto* ctx = static_cast<ProgressContext*>(clientp);
    if (shouldAbortTransfer(ctx->cancel)) {
        return 1;
    }
    if (ctx->onProgress && *ctx->onProgress) {
        (*ctx->onProgress)(static_cast<uint64_t>(dlnow), static_cast<uint64_t>(dltotal));
    }
    return 0;
}

} // namespace

bool shouldAbortTransfer(const std::atomic<bool>* cancel) {
    return cancel != nullptr && cancel->load();
}

DownloadOutcome downloadFile(const std::string& url,
                              const std::string& destPath,
                              const DownloadProgressFn& onProgress,
                              const std::atomic<bool>* cancel) {
    CURL* curl = curl_easy_init();
    if (!curl) {
        return {DownloadResult::Failed, "downloadFile: curl_easy_init failed"};
    }

    WriteContext writeCtx;
    writeCtx.file = fopen(destPath.c_str(), "wb");
    if (!writeCtx.file) {
        curl_easy_cleanup(curl);
        return {DownloadResult::Failed, "downloadFile: cannot open " + destPath + " for writing"};
    }

    ProgressContext progressCtx{&onProgress, cancel};
    char errBuf[CURL_ERROR_SIZE] = {0};

    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_USERAGENT, "TuxBlox-Client/1.0");
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, curlWriteToFile);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &writeCtx);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_ERRORBUFFER, errBuf);
    curl_easy_setopt(curl, CURLOPT_FAILONERROR, 1L);
    curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 0L);
    curl_easy_setopt(curl, CURLOPT_XFERINFOFUNCTION, curlProgressCallback);
    curl_easy_setopt(curl, CURLOPT_XFERINFODATA, &progressCtx);
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 30L);

    CURLcode res = curl_easy_perform(curl);
    fclose(writeCtx.file);

    long httpCode = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &httpCode);
    curl_easy_cleanup(curl);

    if (res == CURLE_ABORTED_BY_CALLBACK) {
        std::remove(destPath.c_str());
        return {DownloadResult::Cancelled, ""};
    }
    if (res != CURLE_OK) {
        std::remove(destPath.c_str());
        return {DownloadResult::Failed, errBuf[0] ? errBuf : curl_easy_strerror(res)};
    }
    if (httpCode != 0 && (httpCode < 200 || httpCode >= 300)) {
        std::remove(destPath.c_str());
        return {DownloadResult::Failed, "HTTP " + std::to_string(httpCode)};
    }

    return {DownloadResult::Ok, ""};
}

} // namespace tuxblox
