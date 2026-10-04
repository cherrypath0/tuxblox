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

// Checks that a real signed Roblox program is accepted and a changed one is not, so an OpenSSL
// upgrade cannot quietly turn the signature check into something that passes everything.

#include "integrity/authenticode.h"

#include <cassert>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>

#include <openssl/opensslv.h>

namespace {

std::string readFile(const std::filesystem::path& path) {
    std::ifstream file(path, std::ios::binary);
    std::stringstream buffer;
    buffer << file.rdbuf();
    return buffer.str();
}

} // namespace

int main(int argc, char **argv) {
    printf("built against %s\n", OPENSSL_VERSION_TEXT);

    if (argc < 3 || std::string(argv[2]).empty()) {
        printf("skipped: no signed Roblox program given\n");
        return 0;
    }

    const std::filesystem::path rootsPath = argv[1];
    const std::filesystem::path signedExe = argv[2];
    assert(std::filesystem::exists(rootsPath));
    assert(std::filesystem::exists(signedExe));

    const std::string roots = readFile(rootsPath);
    assert(roots.find("-----BEGIN CERTIFICATE-----") != std::string::npos);

    const tuxblox::IntegrityReport signedReport =
        tuxblox::verifyAuthenticode(signedExe, "Roblox Corporation", roots);
    printf("signed  : %s (%s)\n", tuxblox::integrityStatusName(signedReport.status),
           signedReport.detail.c_str());
    assert(signedReport.status == tuxblox::IntegrityStatus::Verified);
    assert(signedReport.publisher == "Roblox Corporation");

    // The same program with one byte changed must be rejected, otherwise the check proves nothing.
    const std::filesystem::path changed =
        std::filesystem::temp_directory_path() / "tuxblox-authenticode-changed.exe";
    std::filesystem::remove(changed);
    std::filesystem::copy_file(signedExe, changed);
    {
        std::fstream patch(changed, std::ios::binary | std::ios::in | std::ios::out);
        assert(patch.good());
        patch.seekp(4096);
        patch.write("\xde\xad\xbe\xef", 4);
        assert(patch.good());
    }

    const tuxblox::IntegrityReport changedReport =
        tuxblox::verifyAuthenticode(changed, "Roblox Corporation", roots);
    printf("changed : %s (%s)\n", tuxblox::integrityStatusName(changedReport.status),
           changedReport.detail.c_str());
    assert(changedReport.status == tuxblox::IntegrityStatus::Modified);
    std::filesystem::remove(changed);

    // A program that is not a Windows program at all must be refused rather than accepted.
    const std::filesystem::path notWindows =
        std::filesystem::temp_directory_path() / "tuxblox-authenticode-notpe.bin";
    {
        std::ofstream file(notWindows, std::ios::binary | std::ios::trunc);
        file << "this is not a Windows program";
    }
    const tuxblox::IntegrityReport notPeReport =
        tuxblox::verifyAuthenticode(notWindows, "Roblox Corporation", roots);
    printf("not a PE: %s (%s)\n", tuxblox::integrityStatusName(notPeReport.status),
           notPeReport.detail.c_str());
    assert(notPeReport.status == tuxblox::IntegrityStatus::Unreadable);
    std::filesystem::remove(notWindows);

    printf("ok\n");
    return 0;
}
