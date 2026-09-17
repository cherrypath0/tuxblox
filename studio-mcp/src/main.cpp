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

// studio-mcp: the stdio gateway between an MCP client on Linux and Roblox's
// own StudioMCP.exe, which is a Windows program and has to run through the
// compatibility layer.
//
// Standard output carries JSON-RPC, so nothing here may ever write to it.
// Every message this program produces goes to standard error, and the layer
// is started with TUXBLOX_LOG=-1 so it stays silent too.

#include "install_paths.h"
#include "launch_paths.h"
#include "lnk_resolver.h"
#include "mcp_locate.h"
#include "settings.h"
#include "version.h"

#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <string>
#include <unistd.h>
#include <vector>

namespace fs = std::filesystem;
using namespace tuxblox;

namespace {

void printUsage() {
    fprintf(stderr,
            "TuxBlox Studio MCP gateway\n"
            "\n"
            "Runs Roblox's Studio MCP server through TuxBlox and connects it to\n"
            "whatever started it. Point your AI client's MCP configuration at this\n"
            "program; it takes no arguments of its own, and anything passed is\n"
            "handed to the Roblox server unchanged.\n"
            "\n"
            "  --version   Print the TuxBlox version this was built from\n"
            "  --help      Show this message\n");
}

} // namespace

int main(int argc, char** argv) {
    std::vector<std::string> passthrough;
    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        if (arg == "--version") {
            printf("%s\n", kTuxBloxBuildId);
            return 0;
        }
        if (arg == "--help" || arg == "-h") {
            printUsage();
            return 0;
        }
        passthrough.push_back(arg);
    }

    std::string dir;
    try {
        dir = installDir();
    } catch (const std::exception& e) {
        fprintf(stderr, "TuxBlox: %s\n", e.what());
        return 1;
    }

    const std::string compat = compatBinaryPath(dir);
    std::error_code ec;
    if (!fs::exists(compat, ec) || ec) {
        fprintf(stderr, "TuxBlox: the compatibility layer is missing from %s.\n"
                        "Open TuxBlox once to finish installing it.\n",
                dir.c_str());
        return 1;
    }

    const std::string mcpExe = findStudioMcp(dir);
    if (mcpExe.empty()) {
        fprintf(stderr,
                "TuxBlox: Roblox's Studio MCP server is not installed.\n"
                "Install it by following Roblox's own guide at\n"
                "https://create.roblox.com/docs/studio/mcp, with Studio started\n"
                "through TuxBlox at least once first.\n");
        return 1;
    }

    // Applied to this process because it is about to become the layer. Nothing
    // else runs here afterwards, so there is no other child to leak them to.
    for (const std::string& kv : launchEnvVars(dir, LaunchTarget::Studio)) {
        const size_t split = kv.find('=');
        setenv(kv.substr(0, split).c_str(), kv.substr(split + 1).c_str(), 1);
    }
    // Silences the layer and every graphics backend under it. Standard output
    // is a protocol stream here, and a stray line on either stream is a parse
    // error at the client rather than a log entry.
    setenv("TUXBLOX_LOG", "-1", 1);

    // "run --immediate" waits on this one process. Plain "run" holds off its
    // exit for as long as anything holds the prefix, and an MCP server is used
    // precisely while Studio is open -- it would never return, and the client
    // would hang on shutdown waiting for pipes this process still owned.
    std::vector<std::string> args = {compat, "run", "--immediate"};
    // Options go before the executable: the layer stops reading them at the
    // first non-option argument.
    if (loadSettings(dir).verifyIntegrity) args.push_back("--verify-integrity");
    args.push_back(mcpExe);
    for (const std::string& arg : passthrough) args.push_back(arg);

    std::vector<char*> raw;
    raw.reserve(args.size() + 1);
    for (std::string& arg : args) raw.push_back(const_cast<char*>(arg.c_str()));
    raw.push_back(nullptr);

    execv(compat.c_str(), raw.data());

    fprintf(stderr, "TuxBlox: could not start the compatibility layer at %s\n", compat.c_str());
    return 1;
}
