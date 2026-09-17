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

#include "mcp_locate.h"

#include "launch_paths.h"
#include "lnk_resolver.h"

#include <filesystem>

namespace fs = std::filesystem;

namespace tuxblox {
namespace {

const char* kMcpExeName = "StudioMCP.exe";

} // namespace

std::string findStudioMcp(const std::string& installDir) {
    const std::string driveC = installDir + "/runtime/pfx/drive_c";

    // The version pinned in the Versions tab first, so MCP talks to the same
    // Studio the launcher would start rather than whatever a scan reaches.
    std::string studioExe = resolveActiveVersionExePath(LaunchTarget::Studio, installDir);
    if (studioExe.empty()) {
        studioExe = resolveExePath(LaunchTarget::Studio, driveC);
    }
    if (!studioExe.empty()) {
        const std::string candidate =
            (fs::path(studioExe).parent_path() / kMcpExeName).string();
        std::error_code ec;
        if (fs::exists(candidate, ec) && !ec) return candidate;
    }

    // Last resort: the newest version folder that has one. Newest rather than
    // first, so a version left behind by an update cannot win.
    const fs::path versions =
        fs::path(driveC) / "users/user/AppData/Local/Roblox/Versions";
    std::error_code ec;
    std::string newest;
    fs::file_time_type newestTime{};
    fs::directory_iterator it(versions, ec);
    if (ec) return "";
    for (const auto& entry : it) {
        const fs::path candidate = entry.path() / kMcpExeName;
        std::error_code itemEc;
        if (!fs::exists(candidate, itemEc) || itemEc) continue;
        const auto written = fs::last_write_time(candidate, itemEc);
        if (itemEc) continue;
        if (newest.empty() || written > newestTime) {
            newest = candidate.string();
            newestTime = written;
        }
    }
    return newest;
}

} // namespace tuxblox
