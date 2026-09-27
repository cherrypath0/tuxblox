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

#include "../src/support/host_folders.h"
#include <cassert>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>

namespace fs = std::filesystem;

namespace {

std::string readFileText(const fs::path& file) {
    std::ifstream in(file);
    std::string text((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    return text;
}

void writeFile(const fs::path& file, const std::string& text) {
    fs::create_directories(file.parent_path());
    std::ofstream(file) << text;
}

} // namespace

int main() {
    using namespace tuxblox;

    const fs::path base = fs::temp_directory_path() / "tuxblox_test_host_folders";
    fs::remove_all(base);
    const fs::path home = base / "home";
    fs::create_directories(home);

    // The desktop writes this file. Values are quoted and start with $HOME.
    {
        writeFile(home / ".config/user-dirs.dirs",
                  "# created by xdg-user-dirs-update\n"
                  "XDG_DESKTOP_DIR=\"$HOME/Skrivbord\"\n"
                  "XDG_DOWNLOAD_DIR=\"$HOME/Hamtningar\"\n"
                  "XDG_MUSIC_DIR=\"$HOME/Musik\"\n");

        assert(hostFolderPath("XDG_DESKTOP_DIR", "Desktop", home.string()) ==
               (home / "Skrivbord").string());
        assert(hostFolderPath("XDG_DOWNLOAD_DIR", "Downloads", home.string()) ==
               (home / "Hamtningar").string());
        // Not named in the file: falls back to the English name under home.
        assert(hostFolderPath("XDG_VIDEOS_DIR", "Videos", home.string()) ==
               (home / "Videos").string());
    }

    // An absolute value is taken as written.
    {
        writeFile(home / ".config/user-dirs.dirs", "XDG_MUSIC_DIR=\"/mnt/media/music\"\n");
        assert(hostFolderPath("XDG_MUSIC_DIR", "Music", home.string()) == "/mnt/media/music");
    }

    // A value pointing at the home folder itself means "no such folder" in the
    // spec, and must never be linked -- that would expose the whole home.
    {
        writeFile(home / ".config/user-dirs.dirs", "XDG_DESKTOP_DIR=\"$HOME/\"\n");
        assert(hostFolderPath("XDG_DESKTOP_DIR", "Desktop", home.string()).empty());
    }

    // No file at all: every folder falls back to its English name.
    {
        fs::remove(home / ".config/user-dirs.dirs");
        assert(hostFolderPath("XDG_DOCUMENTS_DIR", "Documents", home.string()) ==
               (home / "Documents").string());
    }

    // Unset HOME is not something to guess about.
    assert(hostFolderPath("XDG_DOCUMENTS_DIR", "Documents", "").empty());

    // The mapping table is the six folders, in the drive's own spelling.
    {
        const auto& folders = hostFolderMappings();
        assert(folders.size() == 6);
        bool sawDocuments = false, sawPictures = false;
        for (const HostFolder& folder : folders) {
            assert(!folder.windowsName.empty() && !folder.xdgKey.empty());
            if (folder.windowsName == "Documents") sawDocuments = true;
            if (folder.windowsName == "Pictures") sawPictures = true;
        }
        assert(sawDocuments && sawPictures);
    }

    // Pointing a folder in the drive at one in the home folder.
    {
        const fs::path drive = base / "drive";
        const fs::path hostDocs = base / "hostdocs";

        // No such folder on the host: left exactly as it was, never created.
        fs::remove_all(drive);
        fs::create_directories(drive / "Documents");
        assert(linkHostFolder(drive / "Documents", hostDocs) == HostFolderLink::NoHostFolder);
        assert(!fs::exists(hostDocs));
        assert(!fs::is_symlink(drive / "Documents"));

        // Empty folder: replaced by the link.
        fs::create_directories(hostDocs);
        assert(linkHostFolder(drive / "Documents", hostDocs) == HostFolderLink::Linked);
        assert(fs::is_symlink(drive / "Documents"));
        assert(fs::read_symlink(drive / "Documents") == hostDocs);

        // Running again changes nothing.
        assert(linkHostFolder(drive / "Documents", hostDocs) == HostFolderLink::AlreadyLinked);

        // A link pointing somewhere else is corrected.
        fs::remove(drive / "Documents");
        fs::create_directory_symlink(base / "elsewhere", drive / "Documents");
        assert(linkHostFolder(drive / "Documents", hostDocs) == HostFolderLink::Linked);
        assert(fs::read_symlink(drive / "Documents") == hostDocs);

        // Missing entirely: just linked.
        fs::remove(drive / "Documents");
        assert(linkHostFolder(drive / "Documents", hostDocs) == HostFolderLink::Linked);
    }

    // A folder with files in it: they move to the host folder and come with.
    {
        const fs::path drive = base / "drive2";
        const fs::path hostDocs = base / "hostdocs2";
        fs::create_directories(drive / "Documents" / "Roblox");
        fs::create_directories(hostDocs);
        std::ofstream(drive / "Documents" / "MyPlace.rbxl").put('x');
        std::ofstream(drive / "Documents" / "Roblox" / "AutoSave.rbxl").put('x');

        assert(linkHostFolder(drive / "Documents", hostDocs) == HostFolderLink::Linked);
        assert(fs::is_symlink(drive / "Documents"));
        assert(fs::exists(hostDocs / "MyPlace.rbxl"));
        assert(fs::exists(hostDocs / "Roblox" / "AutoSave.rbxl"));
    }

    // Wine's own empty scaffolding inside Documents is dropped rather than
    // moved -- nobody wants an empty Templates folder appearing in their real
    // Documents, and an empty directory carries nothing to lose.
    {
        const fs::path drive = base / "drive4";
        const fs::path hostDocs = base / "hostdocs4";
        fs::create_directories(drive / "Documents" / "Templates");
        fs::create_directories(drive / "Documents" / "My Music");
        fs::create_directories(hostDocs);
        // A directory with something in it is still real data and moves.
        fs::create_directories(drive / "Documents" / "Roblox" / "AutoSaves");

        assert(linkHostFolder(drive / "Documents", hostDocs) == HostFolderLink::Linked);
        assert(!fs::exists(hostDocs / "Templates"));
        assert(!fs::exists(hostDocs / "My Music"));
        assert(fs::is_directory(hostDocs / "Roblox" / "AutoSaves"));
    }

    // A name already taken on the host is never overwritten, and the folder
    // stays a real one rather than the file being stranded behind a link.
    {
        const fs::path drive = base / "drive3";
        const fs::path hostDocs = base / "hostdocs3";
        fs::create_directories(drive / "Documents");
        fs::create_directories(hostDocs);
        std::ofstream(drive / "Documents" / "notes.txt") << "from the drive";
        std::ofstream(hostDocs / "notes.txt") << "yours";
        std::ofstream(drive / "Documents" / "moved.txt").put('x');

        assert(linkHostFolder(drive / "Documents", hostDocs) == HostFolderLink::Kept);
        assert(!fs::is_symlink(drive / "Documents"));
        // Yours is untouched, and the clashing one stayed in the drive.
        assert(readFileText(hostDocs / "notes.txt") == "yours");
        assert(fs::exists(drive / "Documents" / "notes.txt"));
        // Everything that could move did.
        assert(fs::exists(hostDocs / "moved.txt"));
        assert(!fs::exists(drive / "Documents" / "moved.txt"));
    }

    // TuxBlox used to keep its bridged-in files under Documents. Once
    // Documents points at the real home folder that would be carried into it,
    // so the old folder goes first -- everything in it is a symlink TuxBlox
    // recreates on demand.
    {
        const fs::path docs = base / "olddocs";
        const fs::path old = docs / "TuxBlox Files";
        fs::create_directories(old);
        fs::create_directories(base / "somewhere");
        fs::create_directory_symlink(base / "somewhere", old / "places");

        assert(removeLegacyBridgeRoot(docs));
        assert(!fs::exists(old));
        // The folder it lived in is untouched.
        assert(fs::is_directory(docs));

        // Nothing there: nothing to do, and that is not a failure.
        assert(removeLegacyBridgeRoot(docs));

        // Anything that is not a symlink means it is not ours to delete.
        fs::create_directories(old);
        std::ofstream(old / "someones-file.txt").put('x');
        assert(!removeLegacyBridgeRoot(docs));
        assert(fs::exists(old / "someones-file.txt"));
    }

    fs::remove_all(base);
    printf("host_folders: all tests passed\n");
    return 0;
}
