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

#include <cstdlib>
#include <filesystem>
#include <fstream>

#include "tuxblox_ca_bundle_pem.h"

namespace tuxblox {

namespace {

// Where each family of Linux keeps its list, first match wins. Only whole files are looked for,
// because a download takes one path and a folder of certificates needs the machine's own index intact.
const char *const MachineBundlePaths[] = {
    "/etc/ssl/certs/ca-certificates.crt",
    "/etc/pki/tls/certs/ca-bundle.crt",
    "/etc/ssl/ca-bundle.pem",
    "/etc/ssl/cert.pem",
};

bool isReadableFile(const std::filesystem::path& path) {
    std::error_code error;
    if (!std::filesystem::is_regular_file(path, error) || error) return false;
    std::ifstream probe(path, std::ios::binary);
    return probe.good();
}

} // namespace

std::string caBundlePath() {
    // A list the environment points at wins, but only when it is really there: a leftover setting
    // in somebody's shell profile must not stop every download instead.
    if (const char *pFromEnvironment = std::getenv("SSL_CERT_FILE")) {
        if (*pFromEnvironment && isReadableFile(pFromEnvironment)) return pFromEnvironment;
    }
    for (const char *pCandidate : MachineBundlePaths) {
        if (isReadableFile(pCandidate)) return pCandidate;
    }
    return std::string();
}

std::string caBundleEmbedded() {
    return std::string(reinterpret_cast<const char *>(kTuxBloxCaBundlePem),
                       kTuxBloxCaBundlePemLen);
}

void applyCaBundle(CURL *pCurl) {
    // A folder of certificates is passed through untouched, since that is the machine's own layout.
    if (const char *pDirectory = std::getenv("SSL_CERT_DIR")) {
        if (*pDirectory) curl_easy_setopt(pCurl, CURLOPT_CAPATH, pDirectory);
    }

    const std::string bundle = caBundlePath();
    if (!bundle.empty()) {
        curl_easy_setopt(pCurl, CURLOPT_CAINFO, bundle.c_str());
        return;
    }

    // Nothing on this machine was usable, so the copy built into TuxBlox is written out once and used.
    static std::string embeddedPath;
    if (embeddedPath.empty()) {
        std::error_code error;
        const std::filesystem::path out =
            std::filesystem::temp_directory_path(error) / "tuxblox-ca-bundle.pem";
        const std::string pem = caBundleEmbedded();
        std::ofstream file(out, std::ios::binary | std::ios::trunc);
        file.write(pem.data(), static_cast<std::streamsize>(pem.size()));
        if (file.good()) embeddedPath = out.string();
    }
    if (!embeddedPath.empty()) {
        curl_easy_setopt(pCurl, CURLOPT_CAINFO, embeddedPath.c_str());
    }
}

} // namespace tuxblox
