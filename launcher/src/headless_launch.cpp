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
#include "process_launcher.h"
#include <cstdio>
#include <cstdlib>
#include <unistd.h>
#include <vector>

namespace tuxblox {

int runHeadlessQuickLaunch(const std::string& installDir, LaunchTarget target, const std::string& uri) {
    std::string exePath = resolveOrBootstrapExePath(target, installDir);
    if (exePath.empty()) {
        fprintf(stderr, "TuxBlox: could not resolve or download the Roblox executable\n");
        return 1;
    }

    // setenv(), not an explicit execve() envp: Proton still needs the rest
    // of this process's inherited environment (PATH for its python3
    // shebang, DISPLAY/WAYLAND_DISPLAY/XDG_RUNTIME_DIR, etc.). That's safe
    // here specifically because nothing else runs in this process between
    // setenv() and execv() below -- this process becomes Proton immediately,
    // so these vars never reach any other, non-Proton child. See item 33 in
    // plan/plan.txt.
    for (const auto& kv : launchEnvVars(installDir, target)) {
        auto pos = kv.find('=');
        setenv(kv.substr(0, pos).c_str(), kv.substr(pos + 1).c_str(), 1);
    }

    std::string proton = protonBinaryPath(installDir);
    std::vector<std::string> argvStrings = {proton, "run", exePath};
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
