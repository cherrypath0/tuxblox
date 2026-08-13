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

#include "license_file.h"
#include "tuxblox_license_txt.h" // generated at build time
#include <filesystem>
#include <fstream>

namespace tuxblox {

void ensureLicenseFile(const std::string& installDir) {
    try {
        std::filesystem::path path = std::filesystem::path(installDir) / "LICENSE";
        if (std::filesystem::exists(path)) return;

        std::ofstream file(path, std::ios::binary);
        if (!file) return;
        file.write(reinterpret_cast<const char*>(kTuxBloxLicenseTxt),
                    static_cast<std::streamsize>(kTuxBloxLicenseTxtLen));
    } catch (...) {
        // Best-effort -- a missing LICENSE file must not fail an otherwise
        // successful launch.
    }
}

} // namespace tuxblox
