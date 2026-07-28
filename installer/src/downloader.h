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
