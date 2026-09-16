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

#include "downloader.h"
#include <atomic>
#include <cassert>
#include <cstdio>
#include <filesystem>
#include <fstream>

namespace fs = std::filesystem;

int main() {
    using namespace tuxblox;

    {
        assert(shouldAbortTransfer(nullptr) == false);
        std::atomic<bool> f{false};
        assert(shouldAbortTransfer(&f) == false);
        std::atomic<bool> t{true};
        assert(shouldAbortTransfer(&t) == true);
    }

    fs::path srcPath = fs::temp_directory_path() / "tuxblox_test_downloader_src.bin";
    fs::path destPath = fs::temp_directory_path() / "tuxblox_test_downloader_dest.bin";

    {
        std::ofstream out(srcPath, std::ios::binary);
        out << "hello tuxblox";
    }

    // Happy path via file:// (no network needed).
    {
        std::atomic<bool> cancel{false};
        auto outcome = downloadFile("file://" + srcPath.string(), destPath.string(),
            [](uint64_t, uint64_t) {}, &cancel);
        assert(outcome.result == DownloadResult::Ok);

        std::ifstream in(destPath, std::ios::binary);
        std::string content((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
        assert(content == "hello tuxblox");
    }
    fs::remove(destPath);

    // Failure path: nonexistent source.
    {
        std::atomic<bool> cancel{false};
        auto outcome = downloadFile("file:///nonexistent/tuxblox_test_missing.bin",
            destPath.string(), [](uint64_t, uint64_t) {}, &cancel);
        assert(outcome.result == DownloadResult::Failed);
        assert(!outcome.errorMessage.empty());
    }

    fs::remove(srcPath);
    fs::remove(destPath);

    printf("downloader: all tests passed\n");
    return 0;
}
