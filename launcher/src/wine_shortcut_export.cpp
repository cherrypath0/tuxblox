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

#include "wine_shortcut_export.h"
#include <cctype>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <system_error>
#include <unistd.h>
#include <vector>

namespace fs = std::filesystem;

namespace tuxblox {

namespace {

struct ExportTarget {
    const char* exeLeaf;   // lowercased basename to match
    const char* slug;      // "studio" / "player"
};

const std::vector<ExportTarget>& exportTargets() {
    static const std::vector<ExportTarget> targets = {
        {"robloxstudiobeta.exe", "studio"},
        {"robloxplayerbeta.exe", "player"},
    };
    return targets;
}

// Same buckets writeDesktopEntries() populates -- see its own comment for why
// every size gets a copy rather than only 256x256.
const char* const kIconSizes[] = {"16x16", "24x24", "32x32", "48x48",
                                   "64x64", "96x96", "128x128", "256x256"};

std::string toLower(std::string s) {
    for (char& c : s) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return s;
}

std::string trimCR(std::string s) {
    while (!s.empty() && (s.back() == '\r' || s.back() == '\n')) s.pop_back();
    return s;
}

std::string windowsBasename(const std::string& winPath) {
    const size_t slash = winPath.find_last_of("\\/");
    return slash == std::string::npos ? winPath : winPath.substr(slash + 1);
}

// Reads a flat key=value .desktop file. proton_shortcuts entries have exactly
// one [Desktop Entry] group, so no group tracking is needed.
std::string desktopValue(const std::string& contents, const std::string& key) {
    size_t pos = 0;
    while (pos <= contents.size()) {
        size_t end = contents.find('\n', pos);
        if (end == std::string::npos) end = contents.size();
        const std::string line = trimCR(contents.substr(pos, end - pos));
        if (line.compare(0, key.size(), key) == 0 && line.size() > key.size() &&
            line[key.size()] == '=')
            return line.substr(key.size() + 1);
        if (end == contents.size()) break;
        pos = end + 1;
    }
    return "";
}

std::string readWholeFile(const fs::path& p) {
    std::ifstream f(p, std::ios::binary);
    if (!f) return "";
    return std::string((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
}

// Suffix for a same-directory temp path, unique enough that two concurrently
// running launcher processes (allowed by design -- multiple Studio instances)
// never collide on it.
std::string tempSuffix() {
    return ".tmp-" + std::to_string(static_cast<long>(getpid()));
}

// Writes `dest` via a same-directory temp file + rename(), so a reader (e.g.
// a desktop environment's inotify watcher on ~/.local/share/applications)
// never observes a half-written file. rename() is atomic only within one
// filesystem, hence "same directory".
bool writeFileAtomic(const fs::path& dest, const std::string& contents) {
    const fs::path tmpPath = fs::path(dest.string() + tempSuffix());
    {
        std::ofstream f(tmpPath, std::ios::binary | std::ios::trunc);
        if (!f) return false;
        f << contents;
        if (!f) return false;
    }
    std::error_code ec;
    fs::rename(tmpPath, dest, ec);
    if (ec) {
        fs::remove(tmpPath, ec);
        return false;
    }
    return true;
}

// Copies the icon named by the source entry into every size bucket that has
// one, under a deterministic TuxBlox-owned name. Returns the Icon= value to
// write, or "" if nothing was copied. Each copy lands via a temp file +
// rename() for the same reason writeFileAtomic() does.
std::string copyIcons(const fs::path& srcIconsRoot, const std::string& sourceIconName,
                       const fs::path& iconsDir, const std::string& slug) {
    if (sourceIconName.empty()) return "";
    const std::string destName = std::string("tuxblox-roblox-") + slug;
    bool copiedAny = false;
    for (const char* size : kIconSizes) {
        const fs::path src = srcIconsRoot / size / "apps" / (sourceIconName + ".png");
        std::error_code ec;
        if (!fs::exists(src, ec) || ec) continue;
        const fs::path destDir = iconsDir / size / "apps";
        fs::create_directories(destDir, ec);
        if (ec) continue;
        const fs::path dest = destDir / (destName + ".png");
        const fs::path tmp = fs::path(dest.string() + tempSuffix());
        fs::copy_file(src, tmp, fs::copy_options::overwrite_existing, ec);
        if (ec) continue;
        fs::rename(tmp, dest, ec);
        if (ec) {
            fs::remove(tmp, ec);
            continue;
        }
        copiedAny = true;
    }
    return copiedAny ? destName : "";
}

} // namespace

std::string unescapeWinemenubuilderPath(const std::string& escaped) {
    std::string out;
    size_t i = 0;
    while (i < escaped.size()) {
        if (escaped.compare(i, 4, "\\\\\\\\") == 0) {
            out.push_back('\\');
            i += 4;
        } else {
            out.push_back(escaped[i]);
            ++i;
        }
    }
    return out;
}

std::string quotedExecValue(const std::string& execValue) {
    // The value is `"<escaped windows path>" ""`. Backslashes are escaped but
    // quotes are not, so the next '"' really is the closing delimiter.
    const size_t open = execValue.find('"');
    if (open == std::string::npos) return "";
    const size_t close = execValue.find('"', open + 1);
    if (close == std::string::npos) return "";
    return execValue.substr(open + 1, close - open - 1);
}

std::string exeFromDesktopExecLine(const std::string& execValue) {
    return unescapeWinemenubuilderPath(quotedExecValue(execValue));
}

void exportPrefixShortcutsTo(const std::string& installDir, const std::string& launcherExePath,
                             const std::string& appsDir, const std::string& iconsDir) {
    try {
        const fs::path shortcutsDir =
            fs::path(installDir) / "runtime" / "pfx" / "drive_c" / "proton_shortcuts";

        // Correction to the brief: the real call path (watch_launch.cpp, after
        // a session ends) has no reason to have created ~/.local/share/applications
        // first, so this must create it rather than assume the caller did.
        std::error_code mkEc;
        fs::create_directories(appsDir, mkEc);

        std::vector<std::string> produced;

        std::error_code ec;
        fs::directory_iterator it(shortcutsDir, ec);
        if (!ec) {
            for (const auto& entry : it) {
                if (entry.path().extension() != ".desktop") continue;

                const std::string contents = readWholeFile(entry.path());
                // Keep the escaped substring as-is for the Exec= line we write
                // below (a desktop-entry value needs backslashes re-escaped,
                // and winemenubuilder's own four-backslash form already
                // round-trips through GIO correctly -- see
                // unescapeWinemenubuilderPath()'s comment for where that form
                // comes from). Only the basename match needs the unescaped
                // path.
                const std::string escapedExe = quotedExecValue(desktopValue(contents, "Exec"));
                const std::string exe = unescapeWinemenubuilderPath(escapedExe);
                if (exe.empty()) continue;

                const std::string leaf = toLower(windowsBasename(exe));
                const char* slug = nullptr;
                for (const auto& t : exportTargets())
                    if (leaf == t.exeLeaf) slug = t.slug;
                if (!slug) continue; // not Roblox -- don't publish it

                std::string name = desktopValue(contents, "Name");
                if (name.empty()) name = windowsBasename(exe);
                const std::string wmClass = desktopValue(contents, "StartupWMClass");

                std::string icon = copyIcons(shortcutsDir / "icons",
                                              desktopValue(contents, "Icon"), iconsDir, slug);
                if (icon.empty()) icon = "tuxblox";

                const std::string id = std::string("tuxblox-roblox-") + slug + ".desktop";
                std::ostringstream out;
                out << "[Desktop Entry]\n"
                       "Type=Application\n"
                       "Name=" << name << "\n"
                       "Comment=via TuxBlox\n"
                       // --run-exe re-resolves the current version rather than
                       // launching this recorded path, so the entry keeps
                       // working after Roblox updates -- see main.cpp. The
                       // path is written back in its still-escaped form
                       // (escapedExe), not the human-readable `exe` -- see
                       // the comment above where escapedExe is computed.
                       "Exec=\"" << launcherExePath << "\" --run-exe \"" << escapedExe << "\"\n"
                       "Icon=" << icon << "\n"
                       "Terminal=false\n"
                       "StartupNotify=true\n";
                if (!wmClass.empty()) out << "StartupWMClass=" << wmClass << "\n";
                out << "Categories=Game;\n";
                if (writeFileAtomic(fs::path(appsDir) / id, out.str())) produced.push_back(id);
            }
        }

        // Prune exports whose source entry has gone (Roblox uninstalled, prefix
        // wiped). Only ever touches the ids this function owns.
        for (const auto& t : exportTargets()) {
            const std::string id = std::string("tuxblox-roblox-") + t.slug + ".desktop";
            bool stillThere = false;
            for (const auto& p : produced) if (p == id) stillThere = true;
            if (stillThere) continue;
            std::error_code rmEc;
            fs::remove(fs::path(appsDir) / id, rmEc);
        }
    } catch (...) {
        // Best-effort -- must never fail an otherwise-working launch.
    }
}

void exportPrefixShortcuts(const std::string& installDir, const std::string& launcherExePath) {
    const char* home = std::getenv("HOME");
    if (!home || home[0] == '\0') return;
    exportPrefixShortcutsTo(installDir, launcherExePath,
                             std::string(home) + "/.local/share/applications",
                             std::string(home) + "/.local/share/icons/hicolor");
}

} // namespace tuxblox
