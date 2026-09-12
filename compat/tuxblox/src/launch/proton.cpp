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
//
// Portions derived from Proton's proton.py:
// Copyright (c) 2018-2022, Valve Corporation. All rights reserved.
// Licensed under the 3-clause BSD license; see
// third_party_licenses/proton/LICENSE.proton for the full text.

#include "launch/proton.h"

#include <cstdlib>
#include <system_error>

#include <sys/utsname.h>

namespace fs = std::filesystem;

namespace tuxblox {

namespace {

std::string envOrDefault(const char *pName, const std::string& fallback) {
    const char *pValue = std::getenv(pName);
    return pValue != nullptr ? std::string(pValue) : fallback;
}

std::string machineName() {
    struct utsname info = {};
    if (::uname(&info) != 0) {
        return "";
    }
    return info.machine;
}

} // namespace

Proton::Proton(const fs::path& base)
    : baseDir(base),
      distDir(base / "files"),
      binDir(base / "files" / "bin"),
      libDir(base / "files" / "lib"),
      fontsDir(base / "files" / "share" / "fonts"),
      mediaDir(base / "files" / "share" / "media"),
      wineFontsDir(base / "files" / "share" / "wine" / "fonts"),
      wineInf(base / "files" / "share" / "wine" / "wine.inf"),
      defaultPfxDir(base / "files" / "share" / "default_pfx"),
      wineBin(binDir / "wine"),
      wineserverBin(binDir / "wineserver"),
      distLock(base / "dist.lock") {
    const bool arm64Allowed = envOrDefault("PROTON_USE_ARM64", "1") == "1";
    if (arm64Allowed && machineName() == "aarch64" &&
        fileExists(base / "files" / "bin-arm64", true)) {
        hostPeArch = "aarch64-windows";
        defaultPfxDir = base / "files" / "share" / "default_pfx_arm64";
        binDir = base / "files" / "bin-arm64";
        wineBin = binDir / "wine";
        wineserverBin = binDir / "wineserver";
    }
}

fs::path Proton::path(const std::string& relative) const {
    return baseDir / relative;
}

fs::path Proton::archPeDir(const std::string& component, bool wow64) const {
    return libDir / component / (wow64 ? wow64PeArch : hostPeArch);
}

void Proton::cleanupLegacyDist() {
    const fs::path oldDistDir = baseDir / "dist";
    if (!fileExists(oldDistDir, true)) {
        return;
    }

    FileLock::Guard held(distLock);
    // Re-checked under the lock: another launch may have removed it while this
    // one was waiting.
    if (!fileExists(oldDistDir, true)) {
        return;
    }

    std::error_code error;
    fs::remove_all(oldDistDir, error);
    if (error) {
        log("Could not remove legacy dist directory \"" + oldDistDir.string() +
            "\": " + error.message());
    }
}

bool Proton::missingDefaultPrefix() const {
    std::error_code error;
    return !fs::is_directory(defaultPfxDir, error);
}

void Proton::makeDefaultPrefix(const Environment& sessionEnv, const ProcessRunner& runProc) {
    FileLock::Guard held(distLock);
    if (!missingDefaultPrefix()) {
        return;
    }

    Environment localEnv = sessionEnv;
    localEnv["WINEPREFIX"] = defaultPfxDir.string();
    localEnv["WINEDEBUG"] = "-all";

    runProc({wineBin.string(), "wineboot"}, localEnv);
    runProc({wineserverBin.string(), "-w"}, localEnv);
}

} // namespace tuxblox
