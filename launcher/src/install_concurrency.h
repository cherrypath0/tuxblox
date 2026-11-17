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
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace tuxblox {

// Shared by both ways Roblox gets installed -- the bootstrapper and the launcher's
// own Versions tab -- which keep their own loops but need the same rules. Plain
// strings on purpose: neither side can see the other's package type.

// Every destination directory once, in the order first seen.
std::vector<std::string> distinctDestDirs(const std::vector<std::string>& destDirs);

// Which extraction lock a package takes. Seven Roblox packages unpack into the
// version root and must never write it at once; the rest own a directory each.
// Keyed on the exact path, so it serialises packages sharing a destination, NOT ones whose
// destinations merely share an ancestor -- libarchive tolerates that, two archives into one
// directory it does not.
size_t destDirIndex(const std::vector<std::string>& dests, const std::string& destDir);

// One measure across downloading and extracting, which now overlap, so there is no longer a
// moment where one ends and the other starts. Rises with either argument, but is NOT monotonic
// over a run: a mirror retry restarts a package's byte count at zero. Publish through the ratchet below.
double installFraction(uint64_t bytesDownloaded, uint64_t bytesTotal, size_t packagesExtracted,
                        size_t packagesTotal);

// What a progress bar should actually be given. `highest` is the caller's running peak, kept under
// whatever lock guards the publish; a value below it is dropped rather than shown.
double monotonicInstallFraction(double& highest, uint64_t bytesDownloaded, uint64_t bytesTotal,
                                 size_t packagesExtracted, size_t packagesTotal);

} // namespace tuxblox
