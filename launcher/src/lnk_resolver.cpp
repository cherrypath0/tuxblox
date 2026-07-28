#include "lnk_resolver.h"
#include <cctype>
#include <filesystem>
#include <fstream>
#include <iterator>

namespace fs = std::filesystem;

namespace tuxblox {

namespace {

bool isPrintableAscii(unsigned char c) {
    return c >= 0x20 && c <= 0x7E;
}

char toLowerChar(char c) {
    return static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
}

std::string toLowerStr(const std::string& s) {
    std::string out = s;
    for (char& c : out) c = toLowerChar(c);
    return out;
}

size_t findCaseInsensitive(const std::string& haystack, const std::string& needle) {
    if (needle.empty() || haystack.size() < needle.size()) return std::string::npos;
    for (size_t i = 0; i + needle.size() <= haystack.size(); ++i) {
        bool match = true;
        for (size_t j = 0; j < needle.size(); ++j) {
            if (toLowerChar(haystack[i + j]) != toLowerChar(needle[j])) {
                match = false;
                break;
            }
        }
        if (match) return i;
    }
    return std::string::npos;
}

// Within one already-decoded ASCII run, finds "<letter>:\...\<targetExeName>"
// and returns it normalized (backslashes -> forward slashes, drive stripped).
std::string matchPathInRun(const std::string& run, const std::string& targetExeName) {
    size_t exePos = findCaseInsensitive(run, targetExeName);
    if (exePos == std::string::npos) return "";

    for (size_t start = exePos; start > 0; ) {
        --start;
        if (start + 2 < run.size() &&
            std::isalpha(static_cast<unsigned char>(run[start])) &&
            run[start + 1] == ':' && run[start + 2] == '\\') {
            std::string path = run.substr(start, exePos + targetExeName.size() - start);
            std::string normalized = path.substr(2); // drop "<letter>:"
            for (char& c : normalized) {
                if (c == '\\') c = '/';
            }
            return normalized;
        }
    }
    return "";
}

std::vector<std::string> extractAsciiRuns(const std::vector<unsigned char>& bytes, size_t minLen) {
    std::vector<std::string> runs;
    std::string current;
    for (unsigned char b : bytes) {
        if (isPrintableAscii(b)) {
            current.push_back(static_cast<char>(b));
        } else {
            if (current.size() >= minLen) runs.push_back(current);
            current.clear();
        }
    }
    if (current.size() >= minLen) runs.push_back(current);
    return runs;
}

std::vector<std::string> extractUtf16LeRuns(const std::vector<unsigned char>& bytes, size_t minLen) {
    std::vector<std::string> runs;
    std::string current;
    size_t i = 0;
    while (i + 1 < bytes.size()) {
        unsigned char lo = bytes[i];
        unsigned char hi = bytes[i + 1];
        if (hi == 0x00 && isPrintableAscii(lo)) {
            current.push_back(static_cast<char>(lo));
            i += 2;
        } else {
            if (current.size() >= minLen) runs.push_back(current);
            current.clear();
            i += 1;
        }
    }
    if (current.size() >= minLen) runs.push_back(current);
    return runs;
}

} // namespace

const char* targetExeName(LaunchTarget target) {
    return target == LaunchTarget::Player ? "RobloxPlayerBeta.exe" : "RobloxStudioBeta.exe";
}

const char* targetLnkRelPath(LaunchTarget target) {
    return target == LaunchTarget::Player
        ? "users/steamuser/Desktop/Roblox Player.lnk"
        : "users/steamuser/Desktop/Roblox Studio.lnk";
}

std::string extractExeRelPathFromLnkBytes(const std::vector<unsigned char>& lnkBytes,
                                           const std::string& targetExeName) {
    for (const auto& run : extractAsciiRuns(lnkBytes, 4)) {
        std::string match = matchPathInRun(run, targetExeName);
        if (!match.empty()) return match;
    }
    for (const auto& run : extractUtf16LeRuns(lnkBytes, 4)) {
        std::string match = matchPathInRun(run, targetExeName);
        if (!match.empty()) return match;
    }
    return "";
}

std::string resolveExePath(LaunchTarget target, const std::string& driveCRoot) {
    const std::string exeName = targetExeName(target);
    const fs::path lnkPath = fs::path(driveCRoot) / targetLnkRelPath(target);

    if (fs::exists(lnkPath)) {
        std::ifstream in(lnkPath, std::ios::binary);
        if (in) {
            std::vector<unsigned char> bytes(
                (std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
            std::string relPath = extractExeRelPathFromLnkBytes(bytes, exeName);
            if (!relPath.empty()) {
                std::string wantSuffix = toLowerStr(relPath);
                std::error_code ec;
                fs::recursive_directory_iterator it(
                    driveCRoot, fs::directory_options::skip_permission_denied, ec);
                fs::recursive_directory_iterator end;
                for (; !ec && it != end; it.increment(ec)) {
                    std::error_code fileEc;
                    if (!it->is_regular_file(fileEc) || fileEc) continue;
                    std::string full = toLowerStr(it->path().string());
                    if (full.size() >= wantSuffix.size() &&
                        full.compare(full.size() - wantSuffix.size(), wantSuffix.size(), wantSuffix) == 0) {
                        return it->path().string();
                    }
                }
            }
        }
    }

    std::error_code ec;
    fs::recursive_directory_iterator it(
        driveCRoot, fs::directory_options::skip_permission_denied, ec);
    fs::recursive_directory_iterator end;
    for (; !ec && it != end; it.increment(ec)) {
        std::error_code fileEc;
        if (!it->is_regular_file(fileEc) || fileEc) continue;
        if (it->path().filename() != exeName) continue;
        if (it->path().string().find("Installer") != std::string::npos) continue;
        return it->path().string();
    }

    return "";
}

} // namespace tuxblox
