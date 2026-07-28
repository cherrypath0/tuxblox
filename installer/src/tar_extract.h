#pragma once
#include <string>

namespace tuxblox {

// Extracts the gzip tarball at `archivePath` into `destDir` (created if
// missing). Throws std::runtime_error on any libarchive error.
void extractTarGz(const std::string& archivePath, const std::string& destDir);

} // namespace tuxblox
