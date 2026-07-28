#pragma once
#include <cstdint>
#include <string>

namespace tuxblox {

// Returns the fixed TuxBlox install directory: $HOME/.local/share/tuxblox
// Throws std::runtime_error if HOME is not set.
std::string installDir();

// True if the filesystem containing `path` has at least `minBytes` free.
// `path` must already exist (its containing filesystem is checked).
bool hasEnoughDiskSpace(const std::string& path, uint64_t minBytes);

} // namespace tuxblox
