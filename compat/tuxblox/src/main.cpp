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
#include <iostream>
#include <string>
#include <stdexcept>
#include <string_view>
#include <vector>

#include <unistd.h>

#include "integrity/authenticode.h"
#include "prefix/prefix.h"
#include "launch/proton.h"
#include "launch/session.h"
#include "support/log_limit.h"
#include "support/util.h"

#include "embedded_data.h"

#ifndef TUXBLOX_VERSION
#define TUXBLOX_VERSION "unknown"
#endif

#ifndef TUXBLOX_CHANNEL
#define TUXBLOX_CHANNEL "stable"
#endif

namespace {

// Version variables
const std::string TuxBloxVersion = TUXBLOX_VERSION;
const std::string TuxBloxChannel = TUXBLOX_CHANNEL;
const std::string PrefixVersion = TuxBloxVersion + "-" + TuxBloxChannel;

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
    "--verify-integrity      Check that the executable is signed by Roblox before running it",
    "\nArguments:",
    "run <executable>        Runs the specified executable"
};

const int PrefixDrainTimeoutSeconds = 15;

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
    bool verifyIntegrity = false;
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
            std::cout << PrefixVersion << std::endl;
            parsed.handled = true;
            return parsed;
        }

        if (argument == "--immediate") {
            parsed.runImmediately = true;
        } else if (argument == "--verify-integrity") {
            parsed.verifyIntegrity = true;
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

    tuxblox::log("Running TuxBlox version " + TuxBloxVersion + "-" + TuxBloxChannel);
    tuxblox::log("Commandline: " + invocation);

    // Authenticode verification ("Verify Roblox Integrity" setting) to ensure the Roblox executables used are legitimate and not tampered with
    // TODO: Expand this to all third-party executables instead of just the one being launched (including DLLs), potentially even TuxBlox's own DLLs.
    if (command.verifyIntegrity) {
        const tuxblox::IntegrityReport report = tuxblox::verifyAuthenticode(
            command.target.front(), RobloxPublisher, tuxblox_data::find("roots.pem"));
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
    session.buildVersion = TuxBloxVersion + "-" + TuxBloxChannel;
    session.invocation = invocation;
    session.initWine();

    tuxblox::Prefix prefix(proton, std::filesystem::path(pPrefixDir), PrefixVersion);

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

    if (!command.runImmediately) {
        prefix.setup(session);
    }

    session.applyDllOverrides();

    const int rc = session.run(command.target);

    if (!command.runImmediately) {
        session.waitForPrefixDrain(PrefixDrainTimeoutSeconds);
    }

    // TuxBlox exit codes:
    //   0 - OK (successful)
    //   1 - TuxBlox error
    //   2 - Launched process error
    //   3 - Authenticode verification failure (--verify-integrity)
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
