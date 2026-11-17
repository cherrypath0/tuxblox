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

#include "install_concurrency.h"

#include <algorithm>

namespace tuxblox {

std::vector<std::string> distinctDestDirs(const std::vector<std::string>& destDirs) {
    std::vector<std::string> distinct;
    for (const std::string& dir : destDirs) {
        if (std::find(distinct.begin(), distinct.end(), dir) != distinct.end()) continue;
        distinct.push_back(dir);
    }
    return distinct;
}

size_t destDirIndex(const std::vector<std::string>& dests, const std::string& destDir) {
    const auto at = std::find(dests.begin(), dests.end(), destDir);
    // A directory nobody listed shares the first lock rather than going unguarded.
    if (at == dests.end()) return 0;
    return static_cast<size_t>(at - dests.begin());
}

double installFraction(uint64_t bytesDownloaded, uint64_t bytesTotal, size_t packagesExtracted,
                        size_t packagesTotal) {
    // Roughly what the old two-phase split gave each half, so the bar moves as it used to.
    const double downloadShare = 0.7;
    const double downloaded =
        bytesTotal == 0 ? 1.0 : static_cast<double>(bytesDownloaded) / static_cast<double>(bytesTotal);
    const double extracted = packagesTotal == 0
                                  ? 1.0
                                  : static_cast<double>(packagesExtracted) / static_cast<double>(packagesTotal);
    const double fraction = downloadShare * downloaded + (1.0 - downloadShare) * extracted;
    // Clamped, because the two shares do not add to exactly one in binary.
    return fraction > 1.0 ? 1.0 : fraction;
}

double monotonicInstallFraction(double& highest, uint64_t bytesDownloaded, uint64_t bytesTotal,
                                 size_t packagesExtracted, size_t packagesTotal) {
    const double fraction = installFraction(bytesDownloaded, bytesTotal, packagesExtracted, packagesTotal);
    if (fraction > highest) highest = fraction;
    return highest;
}

} // namespace tuxblox
