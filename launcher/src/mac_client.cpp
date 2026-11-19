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

#include "mac_client.h"

#include "checksum.h"
#include "downloader.h"
#include "mac_signature.h"
#include "roblox_deploy.h"
#include "tar_extract.h"

#include <algorithm>
#include <array>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>
#include <system_error>
#include <vector>

namespace fs = std::filesystem;
namespace tuxblox::macclient {

namespace {

constexpr const char* kPlayerIdentifier = "com.roblox.RobloxPlayer";
constexpr const char* kAppName = "RobloxPlayer.app";
constexpr const char* kExeName = "RobloxPlayer";
constexpr const char* kZipName = "RobloxPlayer.zip";

const std::array<const char*, 3> kSetupHosts = {{
    "https://setup.rbxcdn.com",
    "https://setup-aws.rbxcdn.com",
    "https://setup-ak.rbxcdn.com",
}};

std::string readFirstLines(const fs::path& path, size_t count) {
    std::ifstream in(path);
    std::string line;
    std::string out;
    for (size_t i = 0; i < count && std::getline(in, line); ++i) out += line + "\n";
    return out;
}

void writeAtomically(const fs::path& path, const std::string& contents) {
    const fs::path temp = path.string() + ".new";
    {
        std::ofstream out(temp, std::ios::binary | std::ios::trunc);
        if (!out) throw std::runtime_error("cannot write " + temp.string());
        out << contents;
        out.flush();
        if (!out) throw std::runtime_error("cannot write " + temp.string());
    }
    fs::rename(temp, path);
}

// A manifest of the sealed part of the bundle. ClientSettings is the one
// directory TuxBlox itself writes into after installation, so it is not part
// of the manifest; everything else has to stay byte for byte as shipped.
std::string manifestFor(const fs::path& app) {
    std::vector<std::string> lines;
    const fs::path contents = app / "Contents";
    fs::recursive_directory_iterator it(contents), end;
    for (; it != end; ++it) {
        if (!it->is_regular_file()) continue;
        const std::string rel = fs::relative(it->path(), contents).generic_string();
        if (rel.rfind("MacOS/ClientSettings/", 0) == 0) continue;
        lines.push_back(sha256File(it->path().string()) + "  " + rel);
    }
    std::sort(lines.begin(), lines.end());
    std::string out;
    for (const std::string& line : lines) out += line + "\n";
    return out;
}

Build buildAt(const fs::path& root, const std::string& upload, const std::string& version) {
    const fs::path dir = root / upload;
    const fs::path app = dir / kAppName;
    return Build{upload, version, dir.string(), app.string(),
                 (app / "Contents" / "MacOS" / kExeName).string()};
}

} // namespace

std::string playerRoot(const std::string& installDir) {
    return (fs::path(installDir) / "mac-client" / "player").string();
}

bool validUpload(const std::string& upload) {
    if (upload.rfind("version-", 0) != 0 || upload.size() <= 8) return false;
    return std::all_of(upload.begin() + 8, upload.end(), [](char c) {
        return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F');
    });
}

std::string setupUrl(const std::string& host, const std::string& channel,
                     const std::string& upload) {
    std::string base = host;
    while (!base.empty() && base.back() == '/') base.pop_back();
    if (!channel.empty() && channel != "live" && channel != "production")
        base += "/channel/" + channel + "/common";
    return base + "/mac/" + upload + "-" + kZipName;
}

bool installedBuild(const std::string& installDir, Build& out) {
    const fs::path root(playerRoot(installDir));
    const std::string marker = readFirstLines(root / "current", 2);
    std::istringstream in(marker);
    std::string upload, version;
    if (!std::getline(in, upload) || !std::getline(in, version)) return false;
    if (!validUpload(upload)) return false;
    const Build build = buildAt(root, upload, version);
    std::error_code ec;
    if (!fs::is_regular_file(build.exe, ec) || ec) return false;
    out = build;
    return true;
}

bool hasInstalledClient(const std::string& installDir) {
    Build build;
    return installedBuild(installDir, build);
}

bool integrityIntact(const std::string& installDir) {
    Build build;
    if (!installedBuild(installDir, build)) return false;
    const fs::path root(playerRoot(installDir));
    std::ifstream saved(root / (build.upload + ".integrity"));
    if (!saved) return false;
    std::set<std::string> expected;
    std::string line;
    while (std::getline(saved, line))
        if (!line.empty()) expected.insert(line);

    std::set<std::string> actual;
    std::istringstream now(manifestFor(build.app));
    while (std::getline(now, line))
        if (!line.empty()) actual.insert(line);
    // Both an added file and a removed one are a change.
    return expected == actual;
}

namespace {

// Each step below has one job and reports a reason instead of throwing. The
// whole point of the order is that nothing replaces an installed build until
// the new one has been verified.
bool fetchVersion(const std::string& channel, std::string& upload, std::string& version,
                  std::string& error) {
    try {
        const std::string json = fetchClientVersionJson("MacPlayer", normalizeChannel(channel));
        upload = parseClientVersionHash(json);
        version = parseClientVersionNumber(json);
    } catch (const std::exception& e) {
        error = std::string("cannot get the current version: ") + e.what();
        return false;
    }
    if (!validUpload(upload)) {
        error = "Roblox returned an invalid version identifier";
        return false;
    }
    return true;
}

bool downloadZip(const fs::path& root, const std::string& channel, const std::string& upload,
                 const std::function<void(uint64_t, uint64_t)>& onProgress, std::string& error) {
    const fs::path zip = root / (upload + ".zip");
    for (const char* host : kSetupHosts) {
        const DownloadOutcome outcome = downloadFile(
            setupUrl(host, normalizeChannel(channel), upload), zip.string(), onProgress, nullptr);
        std::error_code ec;
        if (outcome.result == DownloadResult::Ok && fs::is_regular_file(zip, ec) &&
            fs::file_size(zip, ec) > 0)
            return true;
    }
    error = "cannot download the Roblox client";
    return false;
}

bool stageAndVerify(const fs::path& root, const std::string& upload, const std::string& version,
                    Build& staged, std::string& error) {
    std::error_code ec;
    const fs::path staging = root / (upload + ".partial");
    fs::remove_all(staging, ec);
    fs::create_directories(staging, ec);
    if (ec) {
        error = "cannot create the staging directory";
        return false;
    }
    try {
        extractZip((root / (upload + ".zip")).string(), staging.string());
    } catch (const std::exception& e) {
        fs::remove_all(staging, ec);
        error = std::string("cannot unpack the client: ") + e.what();
        return false;
    }

    // extractZip puts RobloxPlayer.app directly in staging; the upload-named
    // parent is only added when staging is renamed into place later.
    const fs::path app = staging / kAppName;
    staged = Build{upload, version, staging.string(), app.string(),
                   (app / "Contents" / "MacOS" / kExeName).string()};
    auto refuse = [&](const std::string& why) {
        fs::remove_all(staging, ec);
        error = why;
        return false;
    };
    if (!fs::is_regular_file(app / "Contents" / "Info.plist"))
        return refuse("the downloaded bundle has no Info.plist");
    const std::string reason = verifyAppSignature(staged.app, kPlayerIdentifier);
    if (!reason.empty()) return refuse("client signature refused: " + reason);
    return true;
}

bool installVerified(const fs::path& root, const Build& staged, std::string& error) {
    std::error_code ec;
    try {
        writeAtomically(root / (staged.upload + ".integrity"), manifestFor(staged.app));
        fs::remove_all(root / staged.upload, ec);
        fs::rename(staged.dir, root / staged.upload);
        writeAtomically(root / "current", staged.upload + "\n" + staged.version + "\n");
    } catch (const std::exception& e) {
        error = std::string("cannot install the verified client: ") + e.what();
        return false;
    }
    fs::remove_all(root / (staged.upload + ".zip"), ec);
    return true;
}

void removeStaleBuilds(const fs::path& root, const std::string& upload) {
    std::error_code ec;
    fs::directory_iterator dir(root), end;
    for (; dir != end; dir.increment(ec)) {
        if (ec) break;
        const std::string name = dir->path().filename().string();
        if (name == upload || name == "current" || name == upload + ".integrity") continue;
        if (name.rfind("version-", 0) == 0 || name.find(".zip") != std::string::npos ||
            name.find(".partial") != std::string::npos || name.find(".integrity") != std::string::npos)
            fs::remove_all(dir->path(), ec);
    }
}

} // namespace

EnsureResult ensureCurrent(const std::string& installDir, const std::string& channel,
                           const std::function<void(uint64_t, uint64_t)>& onProgress) {
    EnsureResult result;
    const fs::path root(playerRoot(installDir));
    std::string error;

    std::string upload, version;
    if (!fetchVersion(channel, upload, version, error)) {
        result.error = error;
        return result;
    }

    Build current;
    if (installedBuild(installDir, current) && current.upload == upload &&
        current.version == version && integrityIntact(installDir)) {
        result.ok = true;
        result.build = current;
        return result;
    }

    std::error_code ec;
    fs::create_directories(root, ec);
    if (ec) {
        result.error = "cannot create the client directory";
        return result;
    }

    if (!downloadZip(root, channel, upload, onProgress, error)) {
        result.error = error;
        return result;
    }
    Build staged;
    if (!stageAndVerify(root, upload, version, staged, error)) {
        result.error = error;
        return result;
    }
    if (!installVerified(root, staged, error)) {
        result.error = error;
        return result;
    }
    removeStaleBuilds(root, upload);

    result.ok = true;
    result.build = buildAt(root, upload, version);
    return result;
}

std::string writeAppSettings(const Build& build, const std::string& settingsJson) {
    std::error_code ec;
    const fs::path dir = fs::path(build.exe).parent_path() / "ClientSettings";
    fs::create_directories(dir, ec);
    if (ec) return "cannot create ClientSettings";
    try {
        writeAtomically(dir / "ClientAppSettings.json", settingsJson);
    } catch (const std::exception& e) {
        return e.what();
    }
    return std::string();
}

std::string resolveRosemary(const std::string& installDir) {
    if (const char* env = std::getenv("TUXBLOX_ROSEMARY"))
        if (*env != '\0') return env;
    const fs::path beside = fs::path(installDir) / "rosemary" / "rosemary";
    std::error_code ec;
    if (fs::is_regular_file(beside, ec) &&
        (fs::status(beside, ec).permissions() & fs::perms::owner_exec) != fs::perms::none)
        return beside.string();
    return "rosemary";
}

std::vector<std::string> playerLaunchArgs(const std::string& executable,
                                          const std::string& deepLink) {
    std::vector<std::string> args{"run", executable};
    if (!deepLink.empty()) {
        args.push_back("-protocolString");
        args.push_back(deepLink);
    }
    return args;
}

} // namespace tuxblox::macclient
