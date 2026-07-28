#include "headless_launch.h"
#include "process_launcher.h"
#include <cstdio>
#include <unistd.h>
#include <vector>

namespace tuxblox {

int runHeadlessQuickLaunch(const std::string& installDir, LaunchTarget target, const std::string& uri) {
    std::string exePath = resolveOrBootstrapExePath(target, installDir);
    if (exePath.empty()) {
        fprintf(stderr, "TuxBlox: could not resolve or download the Roblox executable\n");
        return 1;
    }

    setLaunchEnv(installDir);

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
