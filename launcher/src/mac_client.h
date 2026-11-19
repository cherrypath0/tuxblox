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
#include <vector>

namespace tuxblox::macclient {

struct Build {
    std::string upload;   // Roblox's "version-<hash>" identifier
    std::string version;  // "0.748.0.7481526"
    std::string dir;      // directory holding RobloxPlayer.app
    std::string app;      // .../RobloxPlayer.app
    std::string exe;      // .../RobloxPlayer.app/Contents/MacOS/RobloxPlayer
};

struct EnsureResult {
    bool ok = false;
    std::string error;   // reason when ok is false
    Build build;
};

// Root for the macOS Player build, kept apart from the Windows prefix and
// its version scanner: <install>/mac-client/player.
std::string playerRoot(const std::string& installDir);

// The build named by the current marker, if it is still complete. Does not
// contact the network and does not check the signature again; use
// integrityIntact() for that.
bool installedBuild(const std::string& installDir, Build& out);

// Cheap check for the UI: a current marker and an executable are enough to
// say the client has been installed. The full integrity check runs on launch.
bool hasInstalledClient(const std::string& installDir);

// Compares the installed bundle with the manifest written at installation.
// Anything added, removed or changed refuses the build.
bool integrityIntact(const std::string& installDir);

// Fetches the current version from Roblox, downloads the bundle from the
// setup CDN and verifies its signature before replacing anything. On any
// failure the previously installed build is left untouched. Never patches
// the client: the only file written inside it afterwards is
// ClientAppSettings.json, through writeAppSettings().
EnsureResult ensureCurrent(const std::string& installDir, const std::string& channel,
                           const std::function<void(uint64_t, uint64_t)>& onProgress = {});

// Writes engine flags inside the bundle. Called on every launch, because a
// client update replaces the file. Returns an error string, empty on success.
std::string writeAppSettings(const Build& build, const std::string& settingsJson);

// Where the Rosemary executable is. TUXBLOX_ROSEMARY wins, then the copy
// beside this install, then "rosemary" on PATH.
std::string resolveRosemary(const std::string& installDir);

// "run <executable>" plus "-protocolString <link>" when a deep link was
// supplied. Arguments after the executable belong to the guest.
std::vector<std::string> playerLaunchArgs(const std::string& executable,
                                          const std::string& deepLink);

// Pure helpers, exposed for tests.
bool validUpload(const std::string& upload);
std::string setupUrl(const std::string& host, const std::string& channel,
                     const std::string& upload);

} // namespace tuxblox::macclient
