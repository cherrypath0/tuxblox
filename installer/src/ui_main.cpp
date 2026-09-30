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
#include "app.h"
#include "cli.h"
#include "ui_adw.h"
#include "version.h"

#include <climits>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <unistd.h>

namespace {

// Fontconfig has no configuration of its own here, so without this the bundled fonts next to the binary are never scanned on a machine that lacks /etc/fonts.
void useBundledFonts() {
    char path[PATH_MAX];
    const ssize_t length = readlink("/proc/self/exe", path, sizeof(path) - 1);
    if (length <= 0) return;
    path[length] = '\0';

    std::string config = path;
    config = config.substr(0, config.rfind('/')) + "/fonts/fonts.conf";
    if (access(config.c_str(), R_OK) == 0) setenv("FONTCONFIG_FILE", config.c_str(), 1);
}

} // namespace

int main(int argc, char **argv) {
    using namespace tuxblox;

    useBundledFonts();

    // Passed by the outer binary after it has already done the removal, purely so the result can be shown in a window.
    if (argc == 2 && std::string(argv[1]) == "--uninstall-result-ok") return runAdwUninstallResult(true);
    if (argc == 2 && std::string(argv[1]) == "--uninstall-result-failed") return runAdwUninstallResult(false);

    CliOptions options = parseArgs(argc, argv);
    if (!options.error.empty()) {
        fprintf(stderr, "%s\n", options.error.c_str());
        return 1;
    }

    if (options.help) {
        printf("%s", usageText());
        return 0;
    }
    if (options.version) {
        printf("%s-%s\n", kTuxBloxVersion, kTuxBloxChannel);
        return 0;
    }

    App app(options.channel, options.latest, options.dir);
    app.start();
    const int result = runAdwInstall(app, options);
    if (result != 0) {
        app.cancel();
        return result == 2 ? 0 : 1;
    }
    if (options.noLaunch) return 0;

    const std::string launcher = app.launcherPath();
    if (options.allowRoot) {
        execl(launcher.c_str(), launcher.c_str(), "--allow-root", (char *)nullptr);
    } else {
        execl(launcher.c_str(), launcher.c_str(), (char *)nullptr);
    }
    fprintf(stderr, "TuxBlox was installed successfully, but failed to launch %s.\n", launcher.c_str());
    return 1;
}
