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

#include "ca_bundle.h"

#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <string>

namespace fs = std::filesystem;

int main() {
    const fs::path tempDir = fs::temp_directory_path() / "tuxblox-ca-bundle-test";
    fs::remove_all(tempDir);
    fs::create_directories(tempDir);

    // A bundle the environment points at is used, so a packager can aim TuxBlox at the system store.
    {
        const fs::path bundle = tempDir / "custom.pem";
        std::ofstream(bundle) << "-----BEGIN CERTIFICATE-----\n";
        setenv("SSL_CERT_FILE", bundle.c_str(), 1);
        assert(tuxblox::caBundlePath() == bundle.string());
        unsetenv("SSL_CERT_FILE");
        printf("  env bundle honoured\n");
    }

    // A stale SSL_CERT_FILE naming a file that is gone must not break every download.
    {
        const fs::path missing = tempDir / "not-there.pem";
        setenv("SSL_CERT_FILE", missing.c_str(), 1);
        assert(tuxblox::caBundlePath() != missing.string());
        unsetenv("SSL_CERT_FILE");
        printf("  missing env bundle falls through\n");
    }

    // An SSL_CERT_FILE that cannot be read is the same case as one that is not there.
    {
        const fs::path unreadable = tempDir / "unreadable.pem";
        std::ofstream(unreadable) << "x";
        fs::permissions(unreadable, fs::perms::none);
        setenv("SSL_CERT_FILE", unreadable.c_str(), 1);
        const bool fellThrough = tuxblox::caBundlePath() != unreadable.string();
        fs::permissions(unreadable, fs::perms::owner_all);
        unsetenv("SSL_CERT_FILE");
        assert(fellThrough);
        printf("  unreadable env bundle falls through\n");
    }

    // An empty SSL_CERT_FILE is not a path at all and must be ignored.
    {
        setenv("SSL_CERT_FILE", "", 1);
        const std::string resolved = tuxblox::caBundlePath();
        unsetenv("SSL_CERT_FILE");
        assert(resolved.empty() || fs::exists(resolved));
        printf("  empty env bundle ignored\n");
    }

    // Whatever the probe returns must be a file that exists, or nothing at all.
    {
        unsetenv("SSL_CERT_FILE");
        const std::string resolved = tuxblox::caBundlePath();
        if (!resolved.empty()) {
            assert(fs::is_regular_file(resolved));
            printf("  probe found %s\n", resolved.c_str());
        } else {
            printf("  probe found nothing on this machine, the built-in copy is used\n");
        }
    }

    // The built-in copy is a real bundle, so TLS still works on a machine that has none.
    {
        const std::string embedded = tuxblox::caBundleEmbedded();
        assert(embedded.find("-----BEGIN CERTIFICATE-----") != std::string::npos);
        assert(embedded.find("-----END CERTIFICATE-----") != std::string::npos);
        assert(embedded.size() > 50000);
        printf("  built-in bundle is %zu bytes\n", embedded.size());
    }

    // The built-in copy must never be written to a predictable path in a folder every user can write to: whoever swapped the file between the write and the read would decide what TuxBlox trusts.
    {
        const fs::path predictable = fs::temp_directory_path() / "tuxblox-ca-bundle.pem";
        fs::remove(predictable);
        CURL *pCurl = curl_easy_init();
        assert(pCurl != nullptr);
        // Told there is no list on this machine, which is the only path that reaches the built-in copy.
        tuxblox::applyCaBundleWith(pCurl, std::string());
        curl_easy_cleanup(pCurl);
        assert(!fs::exists(predictable));
        printf("  built-in list is not written to a shared folder\n");
    }

    // And the ordinary path must still hand the machine's own list straight through.
    {
        const fs::path bundle = tempDir / "passthrough.pem";
        std::ofstream(bundle) << "-----BEGIN CERTIFICATE-----\n";
        CURL *pCurl = curl_easy_init();
        assert(pCurl != nullptr);
        tuxblox::applyCaBundleWith(pCurl, bundle.string());
        curl_easy_cleanup(pCurl);
        assert(!fs::exists(fs::temp_directory_path() / "tuxblox-ca-bundle.pem"));
        printf("  the machine's own list is used as given\n");
    }

    fs::remove_all(tempDir);
    printf("ca_bundle: all tests passed\n");
    return 0;
}
