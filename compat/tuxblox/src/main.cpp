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

// Portions derived from Valve's Proton:
// Copyright (c) 2018-2022, Valve Corporation. All rights reserved.
// Licensed under the 3-clause BSD license; see
// third_party_licenses/proton/LICENSE.proton for the full text.

#include <array>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <stdexcept>
#include <string_view>
#include <vector>

#include <signal.h>
#include <unistd.h>

#include "integrity/authenticode.h"
#include "prefix/prefix.h"
#include "support/account_name.h"
#include "launch/proton.h"
#include "launch/session.h"
#include "support/log_limit.h"
#include "support/util.h"
#include "third_party/json.hpp"

#include "embedded_data.h"

#ifndef TUXBLOX_VERSION
#define TUXBLOX_VERSION "unknown"
#endif

namespace {

// Version variables
const std::string TuxBloxVersion = TUXBLOX_VERSION;

const std::array<std::string, 6> RobloxProcesses = {
    // Clients
    "RobloxPlayerBeta.exe",
    "RobloxStudioBeta.exe",

    // Installers
    // The installers are no longer used in favor of the new custom bootstrapper, still kept here just in case
    "RobloxPlayerInstaller.exe",
    "RobloxStudioInstaller.exe",

    // Roblox Crash handler and the Studio MCP Server
    "RobloxCrashHandler.exe",
    "StudioMCP.exe",
};

const std::array<std::string, 8> HelpOutput = {
    "Options:",
    "--help                  Show this help message",
    "--version               Show TuxBlox version",
    "--immediate             Run TuxBlox without draining the prefix, must be used with the \"run\" argument",
    "--destroy               Destroys the prefix",
    "--allow-root            Allow running as root, which is not recommended",
    "\nArguments:",
    "run <executable>        Runs the specified executable"
};

const std::string TuxBloxPublisher = "TuxBlox Project";
const std::string RobloxPublisher = "Roblox Corporation";
const int IntegrityFailureExit = 3;

enum class RunMode {
    None,
    Run,
    Destroy
};

struct CommandLine {
    RunMode mode = RunMode::None;
    bool runImmediately = false;
    bool allowRoot = false;
    bool handled = false;
    std::vector<std::string> target;
};

std::filesystem::path installDir(const char *pArgv0) {
    std::error_code error;
    const std::filesystem::path self =
        std::filesystem::read_symlink("/proc/self/exe", error);
    if (!error && !self.empty()) {
        return self.parent_path();
    }
    return std::filesystem::absolute(std::filesystem::path(pArgv0).parent_path(), error);
}

// Which release this install belongs to, or empty when it does not say. One build of the runtime is published in several releases, so the number cannot be compiled in; the installer records it beside the programs instead, and an install made before it started doing that simply has nothing to report.
std::string recordedRelease(const std::filesystem::path& layerDir) {
    std::error_code error;
    const std::filesystem::path path = layerDir.parent_path() / "release.json";
    if (!std::filesystem::is_regular_file(path, error) || error) {
        return std::string();
    }
    try {
        std::ifstream file(path);
        return nlohmann::json::parse(file).at("release").get<std::string>();
    } catch (const std::exception&) {
        return std::string();
    }
}

// True when the program being launched is one TuxBlox installed itself, rather than
// something that arrived from outside. The folder the layer is installed in is already
// the boundary that matters: anything able to write a file next to the layer could
// replace the layer itself, so requiring a signature in there protects nothing the
// install's own permissions do not. The path is taken from the running binary and not
// from the environment or argv[0], both of which whoever starts the process chooses.
bool isLayerOwnProgram(const std::filesystem::path& target) {
    std::error_code error;
    const std::filesystem::path self =
        std::filesystem::read_symlink("/proc/self/exe", error);
    if (error || self.empty()) {
        return false;
    }
    const std::filesystem::path own =
        std::filesystem::weakly_canonical(self.parent_path() / "files", error);
    if (error || own.empty()) {
        return false;
    }

    // The real file has to be inside, so a link pointing out of the folder does not
    // count as being in it.
    const std::filesystem::path real = std::filesystem::canonical(target, error);
    if (error) {
        return false;
    }

    // Compared a component at a time, so a sibling whose name merely starts the same
    // is not mistaken for a child.
    auto mine = own.begin();
    auto theirs = real.begin();
    for (; mine != own.end(); ++mine, ++theirs) {
        if (theirs == real.end() || *theirs != *mine) {
            return false;
        }
    }
    return true;
}

void printHelp() {
    for (const std::string& line : HelpOutput) {
        std::cout << line << std::endl;
    }
}

CommandLine parseCommandLine(int argc, char *argv[]) {
    CommandLine parsed;

    int index = 1;
    for (; index < argc; index++) {
        const std::string_view argument = argv[index];

        if (argument == "--help") {
            printHelp();
            parsed.handled = true;
            return parsed;
        }

        if (argument == "--version") {
            std::cout << TuxBloxVersion << std::endl;
            parsed.handled = true;
            return parsed;
        }

        if (argument == "--immediate") {
            parsed.runImmediately = true;
        } else if (argument == "--verify-integrity") {
            // The integrity check is no longer optional, but the flag is still accepted: an unknown argument ends the option list and becomes the executable to launch, so a launcher that still passes this would otherwise be told to run it.
        } else if (argument == "--allow-root") {
            parsed.allowRoot = true;
        } else if (argument == "--destroy") {
            parsed.mode = RunMode::Destroy;
        } else if (argument == "run") {
            parsed.mode = RunMode::Run;
        } else if (argument == "runinprefix") {
            tuxblox::log("The \"runinprefix\" option is deprecated, use \"run --immediate\" instead.");
            parsed.mode = RunMode::Run;
            parsed.runImmediately = true;
        } else {
            break;
        }
    }

    for (; index < argc; index++) {
        parsed.target.push_back(argv[index]);
    }
    return parsed;
}

} // namespace

int runMain(int argc, char *argv[]) {
    const CommandLine command = parseCommandLine(argc, argv);
    if (command.handled) {
        return 0;
    }

    // Blocked from the start, so a stop that arrives while the virtual drive is
    // still being set up waits for the loop that knows how to act on it instead
    // of ending this process where it stands.
    sigset_t stopSignal;
    sigemptyset(&stopSignal);
    sigaddset(&stopSignal, SIGUSR1);
    sigprocmask(SIG_BLOCK, &stopSignal, nullptr);

    // After --help and --version, which the launcher asks of every binary on startup and must keep answering whatever user runs them.
    if (::geteuid() == 0) {
        if (!command.allowRoot) {
            tuxblox::log("Refusing to run as root. Re-run as a normal user, or pass --allow-root if you have a specific reason.");
            return 1;
        }
        tuxblox::log("WARNING: The current user is root, it is highly recommended to launch TuxBlox as a normal user unless there is a specific reason why");
    }

    if (command.mode == RunMode::None) {
        tuxblox::log("An option must be specified. Run --help for more details.");
        return 1;
    }

    if (command.mode == RunMode::Destroy && !command.target.empty()) {
        tuxblox::log("\"--destroy\" takes no arguments.");
        return 1;
    }

    if (command.mode == RunMode::Run && command.target.empty()) {
        tuxblox::log("\"run\" needs an executable to launch.");
        return 1;
    }

    const char *pPrefixDir = std::getenv("TUXBLOX_PREFIX");
    if (pPrefixDir == nullptr || *pPrefixDir == '\0') {
        tuxblox::log("TUXBLOX_PREFIX is not set?");
        return 1;
    }

    const std::string invocation = tuxblox::joinCommandLine({argv, argv + argc});

    const std::string release = recordedRelease(installDir(argv[0]));
    tuxblox::log("Runtime version " + TuxBloxVersion +
                 (release.empty() ? "" : " (release " + release + ")"));

    // Authenticode verification to ensure the Roblox executables used are legitimate and not tampered with
    // TODO: Expand this to all third-party executables instead of just the one being launched (including DLLs), potentially even TuxBlox's own DLLs.
    // TuxBlox's own publisher is accepted too, so the project can sign and run its own programs in the virtual drive.
    if (command.mode == RunMode::Run && isLayerOwnProgram(command.target.front())) {
        tuxblox::log("Integrity check skipped: " + command.target.front() +
                     " is part of this TuxBlox installation.");
    } else if (command.mode == RunMode::Run) {
        const tuxblox::IntegrityReport report = tuxblox::verifyAuthenticode(
            command.target.front(), {RobloxPublisher, TuxBloxPublisher},
            tuxblox_data::find("roots.pem"));
        if (report.status != tuxblox::IntegrityStatus::Verified) {
            tuxblox::log(std::string("Integrity check FAILED (") +
                         tuxblox::integrityStatusName(report.status) + "): " + report.detail);
            return IntegrityFailureExit;
        }
        tuxblox::log("Integrity check passed: " + report.detail);
    }

    tuxblox::Proton proton(installDir(argv[0]));
    proton.cleanupLegacyDist();

    const std::filesystem::path prefixDir = std::filesystem::path(pPrefixDir) / "pfx";

    tuxblox::Session session(proton, prefixDir);
    session.buildVersion = TuxBloxVersion;
    session.invocation = invocation;
    session.ownsPrefix = !command.runImmediately;
    session.initWine();

    tuxblox::Prefix prefix(proton, std::filesystem::path(pPrefixDir), TuxBloxVersion);

    if (command.mode == RunMode::Destroy) {
        prefix.removeTrackedFiles();
        return 0;
    }

    if (proton.missingDefaultPrefix()) {
        proton.makeDefaultPrefix(session.env,
                                 [&session](const std::vector<std::string>& command,
                                            const tuxblox::Environment& env) {
                                     return session.runProc(command, env);
                                 });
    }

    session.initSession();

    std::vector<std::string> target = command.target;

    // Before the drive is set up and before anything is launched: Roblox runs
    // one Player session at a time, and a new Player that finds the old one
    // still running closes itself within 50 ms instead of starting. Studio is
    // untouched -- several Studio sessions are expected to be open at once.
    if (command.mode == RunMode::Run && !target.empty()) {
        tuxblox::replaceRunningSession(prefixDir, tuxblox::imageNameOf(target.front()),
                                       installDir(argv[0]) / "main", pPrefixDir);
    }

    if (!command.runImmediately) {
        prefix.setup(session);

        // setup() may have just renamed the drive's account folder, and whoever asked for this launch worked the path out before that happened.
        const std::string account = tuxblox::prefixAccountName(prefixDir, tuxblox::accountName());
        for (std::string& entry : target) {
            entry = tuxblox::retargetAfterAccountRename(entry, prefixDir, account);
        }
    }

    session.applyDllOverrides();

    const int rc = session.run(target);

    // A stop that arrives while the drive is draining ends the run as a success: the user asked for it, so it is not a failure to report.
    if (!command.runImmediately && session.waitForPrefixDrain(tuxblox::PrefixDrainTimeoutSeconds)) {
        return 0;
    }

    // TuxBlox exit codes:
    //   0 - OK (successful)
    //   1 - TuxBlox error
    //   2 - Launched process error
    //   3 - Authenticode verification failure
    return rc == 0 ? 0 : 2;
}

int main(int argc, char *argv[]) {
    // Before anything is written, so the log this process and everything it
    // launches shares cannot outgrow what a person can open and send. Does
    // nothing when the output is a terminal rather than a launcher's log file.
    tuxblox::watchLogFile(STDERR_FILENO);

    try {
        return runMain(argc, argv);
    } catch (const std::exception& failure) {
        tuxblox::log(std::string("TuxBlox could not start: ") + failure.what());
        return 1;
    }
}
