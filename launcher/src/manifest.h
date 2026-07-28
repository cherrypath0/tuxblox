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
    std::string protonVersion;
    Artifact protonbuild;
    Artifact launcher;
    Artifact installer;
};

Manifest parseManifest(const std::string& jsonText);
std::string fetchManifestJson(const std::string& url, const std::atomic<bool>* cancel = nullptr);

} // namespace tuxblox
