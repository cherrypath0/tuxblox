#pragma once
#include <cstdint>
#include <optional>
#include <string>

namespace tuxblox {

// Returns the fixed TuxBlox install directory: $HOME/.local/share/tuxblox
// Throws std::runtime_error if HOME is not set.
std::string installDir();

// True if the filesystem containing `path` has at least `minBytes` free.
bool hasEnoughDiskSpace(const std::string& path, uint64_t minBytes);

// installDir + "/ProtonBuild"
std::string protonBuildDirUnder(const std::string& installDir);

// protonBuildDirUnder(installDir) + "/dist/version"
std::string protonVersionFilePathUnder(const std::string& installDir);

// Reads protonVersionFilePathUnder(installDir), a "<epoch> <version>" text
// file written by the root build.sh. Returns just the version token, or
// std::nullopt if the file is missing or doesn't contain two
// whitespace-separated tokens.
std::optional<std::string> readInstalledProtonVersion(const std::string& installDir);

} // namespace tuxblox
