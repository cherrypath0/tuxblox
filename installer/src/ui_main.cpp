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

// What FONTCONFIG_FILE held when the process started, so the launcher and everything after it can be given the same environment the installer got.
bool HadFontConfigFile = false;
std::string OriginalFontConfigFile;

// Fontconfig has no configuration of its own here, so without this the bundled fonts next to the binary are never scanned on a machine that lacks /etc/fonts.
void useBundledFonts() {
    const char *pOriginal = getenv("FONTCONFIG_FILE");
    HadFontConfigFile = pOriginal != nullptr;
    if (pOriginal) OriginalFontConfigFile = pOriginal;

    char path[PATH_MAX];
    const ssize_t length = readlink("/proc/self/exe", path, sizeof(path) - 1);
    if (length <= 0) return;
    path[length] = '\0';

    std::string config = path;
    config = config.substr(0, config.rfind('/')) + "/fonts/fonts.conf";
    if (access(config.c_str(), R_OK) == 0) setenv("FONTCONFIG_FILE", config.c_str(), 1);
}

// The setting is only for this window; the launcher, Wine and Roblox must not inherit a path into a cache folder that is later pruned.
void restoreFontConfigEnvironment() {
    if (HadFontConfigFile) {
        setenv("FONTCONFIG_FILE", OriginalFontConfigFile.c_str(), 1);
    } else {
        unsetenv("FONTCONFIG_FILE");
    }
}

} // namespace

int main(int argc, char **argv) {
    using namespace tuxblox;

    useBundledFonts();

    // Passed by the outer binary after it has already done the removal, purely so the result can be shown in a window.
    if (argc == 2 && std::string(argv[1]) == "--uninstall-result-ok") return runAdwUninstallResult(true);
    if (argc == 2 && std::string(argv[1]) == "--uninstall-result-failed") return runAdwUninstallResult(false);

    // Passed by the outer binary when something failed after the interface was unpacked, so the reason reaches a user who has no terminal.
    if (argc == 3 && std::string(argv[1]) == "--show-error") return runAdwError(argv[2]);

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
    restoreFontConfigEnvironment();
    if (options.allowRoot) {
        execl(launcher.c_str(), launcher.c_str(), "--allow-root", (char *)nullptr);
    } else {
        execl(launcher.c_str(), launcher.c_str(), (char *)nullptr);
    }
    fprintf(stderr, "TuxBlox was installed successfully, but failed to launch %s.\n", launcher.c_str());
    return 1;
}
