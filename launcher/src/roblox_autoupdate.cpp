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

#include "roblox_autoupdate.h"

#include "desktop_notify.h"

#include <cstdio>
#include <filesystem>
#include <sys/wait.h>
#include <unistd.h>

namespace tuxblox {

std::string bootstrapperPath(const std::string& installDir) {
    return installDir + "/TuxBloxBootstrapper";
}

std::string robloxVersionsDir(const std::string& installDir) {
    return installDir + "/runtime/pfx/drive_c/users/user/AppData/Local/Roblox/Versions";
}

bool shouldRunUpdateCheck(const Settings& settings, bool bootstrapperExists) {
    return settings.autoUpdateRoblox && bootstrapperExists;
}

std::vector<std::string> updateEnvironment(const std::string& installDir, LaunchTarget target) {
    return {
        std::string("TUXBLOX_BOOTSTRAPPER_DOWNLOAD_APP=") +
            (target == LaunchTarget::Player ? "player" : "studio"),
        "TUXBLOX_BOOTSTRAPPER_INSTALL_DIR=" + robloxVersionsDir(installDir),
    };
}

void runRobloxUpdateCheck(const std::string& installDir, LaunchTarget target,
                           const Settings& settings) {
    const std::string bootstrapper = bootstrapperPath(installDir);
    std::error_code error;
    if (!shouldRunUpdateCheck(settings, std::filesystem::exists(bootstrapper, error))) return;

    pid_t pid = fork();
    if (pid < 0) {
        fprintf(stderr, "TuxBlox: could not start the update check, launching anyway\n");
        return;
    }
    if (pid == 0) {
        for (const auto& pair : updateEnvironment(installDir, target)) {
            const auto split = pair.find('=');
            setenv(pair.substr(0, split).c_str(), pair.substr(split + 1).c_str(), 1);
        }
        const char* argv[] = {bootstrapper.c_str(), "--update", nullptr};
        execv(bootstrapper.c_str(), const_cast<char* const*>(argv));
        _exit(127); // only reached if execv failed
    }

    int status = 0;
    if (waitpid(pid, &status, 0) < 0) return;
    if (!WIFEXITED(status) || WEXITSTATUS(status) != 0) {
        fprintf(stderr, "TuxBlox: the Roblox update check did not finish, launching anyway\n");
        // The check runs without a window of its own, so there is nothing on
        // screen for this to have appeared on. It says the update was skipped
        // rather than that Roblox failed to start: the launch goes ahead on
        // the version already installed either way.
        showDesktopNotification(
            "Failed to Update Roblox",
            "Something went wrong while attempting to update Roblox, so the update "
            "was skipped. Please check your internet connection and try again.");
    }
}

} // namespace tuxblox
