#pragma once
#include <atomic>
#include <cstdint>
#include <string>

namespace tuxblox {

struct Artifact {
    std::string url;
    std::string sha256;
    uint64_t sizeBytes = 0;
};

struct Manifest {
    int manifestVersion = 0;
    std::string tuxbloxVersion;
    Artifact protonbuild;
    Artifact launcher;
    Artifact installer;
};

// Parses manifest JSON text into a Manifest. Throws std::runtime_error
// (with a human-readable message) if the JSON is malformed or missing
// required fields.
Manifest parseManifest(const std::string& jsonText);

// Fetches manifest.json from `url` over HTTPS and returns the raw body.
// Throws std::runtime_error on network failure or non-2xx HTTP status.
// If `cancel` is non-null and becomes true mid-transfer, the request is
// aborted promptly (rather than blocking until CURLOPT_TIMEOUT) and a
// "cancelled" std::runtime_error is thrown.
std::string fetchManifestJson(const std::string& url,
                               const std::atomic<bool>* cancel = nullptr);

} // namespace tuxblox
