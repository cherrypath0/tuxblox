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

#include <cassert>
#include <chrono>
#include <cstdio>
#include <filesystem>
#include <fstream>

namespace fs = std::filesystem;

namespace {

fs::path versionsDir(const fs::path& root) {
    return root / "runtime/pfx/drive_c/users/user/AppData/Local/Roblox/Versions";
}

void touch(const fs::path& path) {
    fs::create_directories(path.parent_path());
    std::ofstream(path) << "x";
}

void ageBy(const fs::path& path, int hours) {
    fs::last_write_time(path, fs::file_time_type::clock::now() - std::chrono::hours(hours));
}

fs::path freshRoot(const char* name) {
    const fs::path root = fs::temp_directory_path() / name;
    fs::remove_all(root);
    fs::create_directories(root);
    return root;
}

} // namespace

int main() {
    using namespace tuxblox;

    {
        // Nothing installed at all: an empty answer rather than a throw, so
        // the caller can print something a user can act on.
        const fs::path root = freshRoot("tuxblox-mcp-empty");
        assert(findStudioMcp(root.string()).empty());
        fs::remove_all(root);
    }

    {
        // Studio present, MCP server absent. Roblox ships them separately and
        // the server is the optional half, so this is still no answer.
        const fs::path root = freshRoot("tuxblox-mcp-studio-only");
        touch(versionsDir(root) / "version-aaa" / "RobloxStudioBeta.exe");
        assert(findStudioMcp(root.string()).empty());

        // With the server beside Studio, that is the answer.
        const fs::path mcp = versionsDir(root) / "version-aaa" / "StudioMCP.exe";
        touch(mcp);
        assert(findStudioMcp(root.string()) == mcp.string());
        fs::remove_all(root);
    }

    {
        // Server copies with no Studio beside them: nothing can be pinned and
        // no shortcut resolves, so the newest one wins. Newest rather than
        // first, so a copy left behind by an update cannot answer.
        const fs::path root = freshRoot("tuxblox-mcp-newest");
        const fs::path older = versionsDir(root) / "version-aaa" / "StudioMCP.exe";
        const fs::path newer = versionsDir(root) / "version-bbb" / "StudioMCP.exe";
        touch(older);
        touch(newer);
        ageBy(older, 48);
        assert(findStudioMcp(root.string()) == newer.string());
        fs::remove_all(root);
    }

    {
        // An active pin outranks "newest", because MCP has to talk to the same
        // Studio the launcher would start.
        const fs::path root = freshRoot("tuxblox-mcp-pinned");
        const fs::path pinned = versionsDir(root) / "version-aaa" / "StudioMCP.exe";
        const fs::path newer = versionsDir(root) / "version-bbb" / "StudioMCP.exe";
        touch(versionsDir(root) / "version-aaa" / "RobloxStudioBeta.exe");
        touch(versionsDir(root) / "version-bbb" / "RobloxStudioBeta.exe");
        touch(pinned);
        touch(newer);
        ageBy(versionsDir(root) / "version-aaa" / "RobloxStudioBeta.exe", 48);
        ageBy(pinned, 48);
        std::ofstream(root / "versions.json")
            << R"({"player":{"active":"","installed":[]},)"
               R"("studio":{"active":"version-aaa","installed":[{"hash":"version-aaa"}]}})";
        assert(findStudioMcp(root.string()) == pinned.string());
        fs::remove_all(root);
    }

    printf("mcp_locate: all tests passed\n");
    return 0;
}
