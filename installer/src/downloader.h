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

#pragma once
#include <atomic>
#include <cstdint>
#include <functional>
#include <string>

namespace tuxblox {

enum class DownloadResult {
    Ok,
    Cancelled,
    Failed
};

struct DownloadOutcome {
    DownloadResult result;
    std::string errorMessage; // populated when result == Failed
};

// Progress callback: (bytesDownloaded, totalBytes). totalBytes may be 0 if unknown.
using DownloadProgressFn = std::function<void(uint64_t, uint64_t)>;

// Exposed for unit testing: true if a transfer guarded by `cancel` should
// abort right now.
bool shouldAbortTransfer(const std::atomic<bool>* cancel);

// Downloads `url` to `destPath`, overwriting it if present. Calls `onProgress`
// periodically. If `*cancel` becomes true during the transfer, the download
// aborts and returns DownloadResult::Cancelled (destPath is removed).
DownloadOutcome downloadFile(const std::string& url,
                              const std::string& destPath,
                              const DownloadProgressFn& onProgress,
                              const std::atomic<bool>* cancel);

} // namespace tuxblox
