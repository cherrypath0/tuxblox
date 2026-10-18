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

#include "headless_launch.h"
#include "fastflag_file.h"
#include "root_guard.h"
#include "install_paths.h"
#include "prefix_session.h"
#include "presence_loop.h"
#include "process_launcher.h"
#include "process_name.h"
#include "roblox_autoupdate.h"
#include "server_region.h"
#include "wine_shortcut_export.h"
#include <cstdio>
#include "settings.h"

#include <cstdlib>
#include <ctime>
#include <unistd.h>
#include <vector>

namespace tuxblox {

int runHeadlessQuickLaunch(const std::string& installDir, LaunchTarget target, const std::string& uri) {
    const Settings settings = loadSettings(installDir);

    // Before the exe is resolved, so a version installed by this check is the
    // one that starts.
    runRobloxUpdateCheck(installDir, target, settings);
    // Needed here as much as on the watched path: a link in a web browser, a menu entry and a desktop icon all come through here, and without it the file keeps whatever the last launch from the TuxBlox window put in it, so a flag deleted since then goes on applying.
    applyFastFlags(installDir, target, settings);
    // This process execs into the game below, so the menu entry has to be
    // written here rather than after the session, as the watched path does.
    exportPrefixShortcuts(installDir, selfExePath());

    // The version picked in the Versions tab, not whatever the prefix scan
    // reaches first: a menu entry or a roblox: link has to start the same
    // version the launcher itself would, which is what the watched path does.
    std::string exePath = resolveActiveVersionExePath(target, installDir);
    if (exePath.empty()) exePath = resolveOrBootstrapExePath(target, installDir);
    if (exePath.empty()) {
        fprintf(stderr, "TuxBlox: could not resolve or download the Roblox executable\n");
        return 1;
    }

    // Both features are driven by reading the session log as it is written, which needs a process that outlives the launch, so when either is on the launch is tracked here rather than handed this process by the exec below -- exactly as the launcher's own window does it.
    // With both off nothing has to be watched and the exec is reached as before, which is what a default install does.
    if (presenceWatchWanted(settings)) {
        // Same reason the watched launch does it: this process now lives for the whole session, and without a name of its own anything looking for the launcher would find a running Roblox session instead.
        setProcessName("tuxbloxWatcher");

        ProcessLauncher launcher(installDir);
        const std::time_t launchStart = std::time(nullptr);
        const LaunchOutcome outcome = launcher.launch(target, uri);
        if (!outcome.ok) {
            fprintf(stderr, "TuxBlox: %s\n", outcome.errorMessage.c_str());
            return 1;
        }
        runPresenceLoop(installDir, target, settings, launchStart,
                        [&] { return launcher.pollIsRunning(target); },
                        makeServerRegionWatcher(settings.serverRegionNotify,
                                                presenceNotificationIcon(target)));
        return 0;
    }

    for (const auto& kv : launchEnvVars(installDir, target)) {
        auto pos = kv.find('=');
        setenv(kv.substr(0, pos).c_str(), kv.substr(pos + 1).c_str(), 1);
    }

    std::string proton = compatBinaryPath(installDir);
    std::vector<std::string> argvStrings = {proton, "run"};
    // Without this a link or a menu entry opened during a session sets the drive
    // up underneath the session already using it, and then waits for every
    // Roblox process to leave before this process can exit -- which is the rest
    // of that session. The watched path has always made this choice; this one
    // never did.
    if (launchJoinsPrefix(target, prefixSessions(installDir + "/runtime/pfx"))) {
        argvStrings.push_back("--immediate");
    }
    if (allowRoot()) argvStrings.push_back("--allow-root");
    argvStrings.push_back(exePath);
    if (!uri.empty()) argvStrings.push_back(uri);

    std::vector<char*> argv;
    argv.reserve(argvStrings.size() + 1);
    for (auto& s : argvStrings) argv.push_back(const_cast<char*>(s.c_str()));
    argv.push_back(nullptr);

    execv(proton.c_str(), argv.data());

    // Only reached if execv() itself failed (e.g. proton missing/not executable).
    fprintf(stderr, "TuxBlox: failed to exec %s\n", proton.c_str());
    return 1;
}

} // namespace tuxblox
