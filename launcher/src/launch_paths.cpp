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

#include "launch_paths.h"
#include "install_paths.h"
#include "versions_manifest.h"

#include <filesystem>

namespace fs = std::filesystem;

namespace tuxblox {

std::string compatBinaryPath(const std::string& installDir) {
    return compatDirUnder(installDir) + "/main";
}

std::vector<std::string> launchEnvVars(const std::string& installDir, LaunchTarget target) {
    // No DXVK_ASYNC here: the bundled DXVK is 3.0.1, which dropped async
    // pipeline compilation entirely (nothing in compat/submodules/dxvk
    // reads that variable any more), so setting it did nothing. Its
    // replacement -- the graphics pipeline library path -- is on by default
    // (dxvk.enableGraphicsPipelineLibrary defaults to Auto).
    std::vector<std::string> env = {
        "TUXBLOX_PREFIX=" + installDir + "/runtime",
        "TUXBLOX_LOG_DIR=" + installDir + "/logs",
    };
    if (target == LaunchTarget::Studio) {
        // Studio's embedded WebView2 (login/create.roblox.com UI) appears to rely
        // on a DXGI composition swapchain; DXVK's CreateSwapChainForComposition
        // returns E_NOTIMPL unless this is enabled. Scoped to Studio only -- see
        // dxgi_factory.cpp's enableDummyCompositionSwapchain option.
        env.push_back("DXVK_CONFIG=dxgi.enableDummyCompositionSwapchain=True");
    }
    return env;
}

std::string resolveActiveVersionExePath(LaunchTarget target, const std::string& installDir) {
    // loadInstalledVersions, not loadVersionsManifest: with the manifest
    // missing this still finds what's installed in the prefix instead of
    // falling through to a pointless official-installer download.
    VersionsManifest manifest = loadInstalledVersions(installDir);
    const AppVersions& av = appVersionsFor(manifest, target);
    if (av.activeHash.empty()) return "";

    // NOTE: hardcodes "users/user/..." matching this codebase's current
    // convention as of this plan's writing (see roblox_log_capture.cpp:37)
    // -- if the separate Wine-per-user-paths plan lands, this needs the
    // resolved username instead of the literal "user".
    const std::string versionDir =
        installDir + "/runtime/pfx/drive_c/users/user/AppData/Local/Roblox/Versions/" + av.activeHash;
    const std::string exePath = versionDir + "/" + targetExeName(target);

    std::error_code ec;
    if (!fs::exists(exePath, ec) || ec) return "";
    return exePath;
}

} // namespace tuxblox
