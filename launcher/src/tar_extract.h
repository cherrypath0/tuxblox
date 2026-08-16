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
#include <cstdint>
#include <functional>
#include <string>

namespace tuxblox {

// Progress callback for extractTarZst: (compressedBytesConsumed,
// compressedTotalBytes) -- an approximation of overall progress, since
// per-entry compression ratio varies, but reported per-archive-entry so a
// multi-thousand-file archive still advances visibly.
using TarExtractProgressFn = std::function<void(uint64_t, uint64_t)>;

// Extracts the zstd-compressed tarball at `archivePath` into `destDir`
// (created if missing), calling `onProgress` after each entry is written.
// `onProgress` defaults to a no-op so callers that don't need progress
// don't have to change. Throws std::runtime_error on any libarchive error.
void extractTarZst(const std::string& archivePath, const std::string& destDir,
                    const TarExtractProgressFn& onProgress = TarExtractProgressFn());

// Extracts a .zip archive at `archivePath` into `destDir` (created if
// missing), calling `onProgress` after each entry is written. Same
// hostile-archive-entry protection as extractTarZst (rejects absolute
// paths and ".." components) -- these packages come from a remote CDN
// (Roblox's setup.rbxcdn.com), not a locally-trusted source. Throws
// std::runtime_error on any libarchive error.
void extractZip(const std::string& archivePath, const std::string& destDir,
                 const TarExtractProgressFn& onProgress = TarExtractProgressFn());

} // namespace tuxblox
