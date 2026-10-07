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

#include "client_peer.h"

#include "server_region.h"

#include <cctype>
#include <dirent.h>
#include <fstream>
#include <sstream>
#include <string>
#include <unistd.h>

namespace tuxblox {
namespace {

const char PlayerExe[] = "robloxplayerbeta.exe";
const char StudioExe[] = "robloxstudiobeta.exe";

bool allHexDigits(const std::string& text) {
    for (const char c : text) {
        if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F'))) {
            return false;
        }
    }
    return !text.empty();
}

std::string readWholeFile(const std::string& path) {
    std::ifstream in(path, std::ios::binary);
    if (!in) {
        return std::string();
    }
    std::ostringstream out;
    out << in.rdbuf();
    return out.str();
}

std::string trimmed(const std::string& text) {
    const std::size_t first = text.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) {
        return std::string();
    }
    const std::size_t last = text.find_last_not_of(" \t\r\n");
    return text.substr(first, last - first + 1);
}

std::set<std::string> socketInodesForPid(const std::string& pid) {
    std::set<std::string> inodes;
    const std::string dir = "/proc/" + pid + "/fd";
    DIR *pDir = opendir(dir.c_str());
    if (pDir == nullptr) {
        return inodes;
    }
    while (const dirent *pEntry = readdir(pDir)) {
        char target[128] = {};
        const std::string link = dir + "/" + pEntry->d_name;
        const ssize_t got = readlink(link.c_str(), target, sizeof(target) - 1);
        if (got <= 0) {
            continue;
        }
        const std::string inode = socketInodeFromLink(std::string(target, static_cast<std::size_t>(got)));
        if (!inode.empty()) {
            inodes.insert(inode);
        }
    }
    closedir(pDir);
    return inodes;
}

} // namespace

std::string ipv4FromProcHex(const std::string& hex) {
    if (hex.size() != 8 || !allHexDigits(hex)) {
        return std::string();
    }
    std::string address;
    for (int at = 3; at >= 0; at--) {
        const int value = std::stoi(hex.substr(static_cast<std::size_t>(at) * 2, 2), nullptr, 16);
        address += std::to_string(value);
        if (at > 0) {
            address += ".";
        }
    }
    return address;
}

std::vector<std::string> routablePeersFromProcNet(const std::string& table,
                                                  const std::set<std::string>& inodes) {
    std::vector<std::string> peers;
    std::istringstream lines(table);
    std::string line;
    while (std::getline(lines, line)) {
        std::istringstream fields(line);
        std::vector<std::string> field;
        std::string one;
        while (fields >> one) {
            field.push_back(one);
        }
        // sl local rem st tx:rx tr:when retrnsmt uid timeout inode ...
        if (field.size() < 10) {
            continue;
        }
        if (inodes.find(field[9]) == inodes.end()) {
            continue;
        }
        const std::size_t colon = field[2].find(':');
        if (colon == std::string::npos) {
            continue;
        }
        const std::string peer = serverAddressForLookup(ipv4FromProcHex(field[2].substr(0, colon)));
        if (!peer.empty()) {
            peers.push_back(peer);
        }
    }
    return peers;
}

std::string socketInodeFromLink(const std::string& target) {
    const char prefix[] = "socket:[";
    if (target.rfind(prefix, 0) != 0 || target.back() != ']') {
        return std::string();
    }
    const std::string inode =
        target.substr(sizeof(prefix) - 1, target.size() - sizeof(prefix));
    for (const char c : inode) {
        if (c < '0' || c > '9') {
            return std::string();
        }
    }
    return inode;
}

bool cmdlineNamesRobloxClient(const std::string& cmdline) {
    const std::size_t end = cmdline.find('\0');
    std::string first = cmdline.substr(0, end);
    for (char& c : first) {
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    }
    for (const char *pExe : {PlayerExe, StudioExe}) {
        const std::string name = pExe;
        if (first.size() >= name.size() &&
            first.compare(first.size() - name.size(), name.size(), name) == 0) {
            return true;
        }
    }
    return false;
}

bool environNamesPrefix(const std::string& environ, const std::string& prefixDir) {
    if (prefixDir.empty()) {
        return false;
    }
    const std::string wanted = "WINEPREFIX=" + prefixDir;
    std::size_t at = 0;
    while (at < environ.size()) {
        const std::size_t end = environ.find('\0', at);
        const std::string entry = environ.substr(at, end == std::string::npos ? end : end - at);
        if (entry == wanted) {
            return true;
        }
        if (end == std::string::npos) {
            break;
        }
        at = end + 1;
    }
    return false;
}

std::string clientPeerAddress(const std::string& prefixDir) {
    DIR *pProc = opendir("/proc");
    if (pProc == nullptr) {
        return std::string();
    }

    std::string found;
    while (const dirent *pEntry = readdir(pProc)) {
        const std::string pid = pEntry->d_name;
        if (pid.empty() || pid.front() < '0' || pid.front() > '9') {
            continue;
        }
        if (!cmdlineNamesRobloxClient(readWholeFile("/proc/" + pid + "/cmdline"))) {
            continue;
        }
        // Another session of the same program, in another virtual drive, is not ours to report.
        if (!environNamesPrefix(readWholeFile("/proc/" + pid + "/environ"), prefixDir)) {
            continue;
        }

        const std::set<std::string> inodes = socketInodesForPid(pid);
        if (inodes.empty()) {
            continue;
        }
        for (const char *pTable : {"/net/udp", "/net/tcp"}) {
            const std::vector<std::string> peers =
                routablePeersFromProcNet(readWholeFile("/proc/" + pid + pTable), inodes);
            if (!peers.empty()) {
                found = peers.front();
                break;
            }
        }
        if (!found.empty()) {
            break;
        }
    }
    closedir(pProc);
    return found;
}

} // namespace tuxblox
