#pragma once
#include <atomic>
#include <cstdint>
#include <functional>
#include <string>

namespace tuxblox {

enum class DownloadResult { Ok, Cancelled, Failed };

struct DownloadOutcome {
    DownloadResult result;
    std::string errorMessage;
};

using DownloadProgressFn = std::function<void(uint64_t, uint64_t)>;

bool shouldAbortTransfer(const std::atomic<bool>* cancel);

DownloadOutcome downloadFile(const std::string& url,
                              const std::string& destPath,
                              const DownloadProgressFn& onProgress,
                              const std::atomic<bool>* cancel);

} // namespace tuxblox
