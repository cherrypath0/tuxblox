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

#include "webview_mode.h"

#include "prefix/registry.h"

#include <array>
#include <cstdlib>
#include <fstream>
#include <map>
#include <vector>

namespace tuxblox {

const char *kWebView2RuntimeVersion = "109.0.1518.140";

const std::string WebView2ApplicationDir =
    "drive_c/Program Files (x86)/Microsoft/EdgeWebView/Application";

const std::array<std::string, 2> WebView2RuntimeDlls = {
    "EBWebView/x64/EmbeddedBrowserWebView.dll",
    "EBWebView/x86/EmbeddedBrowserWebView.dll",
};

const std::array<WebViewVersionKey, 6> WebView2VersionKeys = {{
    {"system.reg", "Software\\\\Microsoft\\\\EdgeUpdate\\\\Clients\\\\{F3017226-FE2A-4295-8BDF-00C3A9A7E4C5}", false},
    {"system.reg", "Software\\\\Microsoft\\\\EdgeUpdate\\\\ClientState\\\\{F3017226-FE2A-4295-8BDF-00C3A9A7E4C5}", true},
    {"system.reg", "Software\\\\Wow6432Node\\\\Microsoft\\\\EdgeUpdate\\\\Clients\\\\{F3017226-FE2A-4295-8BDF-00C3A9A7E4C5}", false},
    {"system.reg", "Software\\\\Wow6432Node\\\\Microsoft\\\\EdgeUpdate\\\\ClientState\\\\{F3017226-FE2A-4295-8BDF-00C3A9A7E4C5}", true},
    {"user.reg", "Software\\\\Microsoft\\\\EdgeUpdate\\\\Clients\\\\{F3017226-FE2A-4295-8BDF-00C3A9A7E4C5}", false},
    {"user.reg", "Software\\\\Microsoft\\\\EdgeUpdate\\\\ClientState\\\\{F3017226-FE2A-4295-8BDF-00C3A9A7E4C5}", true},
}};

namespace {

// The marker every Wine builtin carries. Its presence is what says a file is ours to delete rather than part of somebody's real install.
const std::string FakeDllMarker = "Wine builtin DLL";

bool isFakeDll(const std::filesystem::path& file) {
    std::ifstream in(file, std::ios::binary);
    if (!in) {
        return false;
    }
    char head[256] = {};
    in.read(head, sizeof(head));
    const std::string text(head, static_cast<std::size_t>(in.gcount()));
    return text.find(FakeDllMarker) != std::string::npos;
}

// The values a key should carry, as a registry file spells them.
std::map<std::string, std::string> ourWebViewValues(const WebViewVersionKey& entry) {
    std::map<std::string, std::string> values = {{"pv", webViewVersionValue()}};
    if (entry.namesInstallFolder) {
        values["EBWebView"] = webViewInstallPathValue();
    }
    return values;
}

} // namespace

bool useMicrosoftWebView() {
    const char *pValue = std::getenv("TUXBLOX_USE_MSWEBVIEW");
    return pValue != nullptr && pValue[0] != '\0' && std::string(pValue) != "0";
}

std::string webViewModeName() {
    return useMicrosoftWebView() ? "microsoft" : "builtin";
}

std::string webViewVersionValue() {
    return "\"" + std::string(kWebView2RuntimeVersion) + "\"";
}

// Derived from the one folder the files are actually written to, so renaming that folder cannot leave the registry naming a place nothing is in.
std::string webViewInstallPathValue() {
    const std::string path = WebView2ApplicationDir + "/" + kWebView2RuntimeVersion;
    const size_t afterDriveFolder = path.find('/');
    std::string value = "\"C:";
    for (size_t at = afterDriveFolder; at < path.size(); at++) {
        if (path[at] == '/') {
            value += "\\\\"; // a registry file doubles every separator
        } else {
            value += path[at];
        }
    }
    return value + "\"";
}

bool foreignWebViewVersion(const std::string& currentValue) {
    return !currentValue.empty() && currentValue != webViewVersionValue();
}

WebViewVersionAction webViewValueAction(const std::string& currentValue, const std::string& ourValue,
                                        bool microsoft) {
    if (microsoft) {
        // Only ever take back what is exactly ours, wherever it is, so a real install keeps everything it wrote.
        return currentValue == ourValue ? WebViewVersionAction::Remove : WebViewVersionAction::Leave;
    }
    // Missing rather than not-ours, because a key can carry the version and still be missing the folder.
    return currentValue.empty() ? WebViewVersionAction::Write : WebViewVersionAction::Leave;
}

bool syncWebViewValues(const std::filesystem::path& prefixDir, bool microsoft) {
    // Whether the drive holds somebody's real runtime is a fact about the whole install and not about one key, so every version is read before anything is written. Their installer leaves keys of its own with no version in them, and stamping ours into one cannot be undone: Microsoft mode would later take back what we had overwritten and leave their install advertised by nothing at all.
    if (!microsoft) {
        for (const WebViewVersionKey& entry : WebView2VersionKeys) {
            if (foreignWebViewVersion(getRegValue(prefixDir / entry.file, entry.key, "pv"))) {
                return true;
            }
        }
    }

    for (const WebViewVersionKey& entry : WebView2VersionKeys) {
        const std::filesystem::path file = prefixDir / entry.file;
        std::map<std::string, std::string> write;
        std::vector<std::string> remove;
        for (const auto& [name, ours] : ourWebViewValues(entry)) {
            switch (webViewValueAction(getRegValue(file, entry.key, name), ours, microsoft)) {
            case WebViewVersionAction::Write:
                write[name] = ours;
                break;
            case WebViewVersionAction::Remove:
                remove.push_back(name);
                break;
            case WebViewVersionAction::Leave:
                break;
            }
        }
        if (!write.empty()) {
            setRegKeyValues(file, entry.key, write);
        }
        if (!remove.empty()) {
            removeRegKeyValues(file, entry.key, remove);
        }
    }
    return false;
}

bool removeFakeWebViewRuntime(const std::filesystem::path& prefixDir) {
    namespace fs = std::filesystem;

    const fs::path ours = prefixDir / WebView2ApplicationDir / kWebView2RuntimeVersion;
    std::error_code error;
    if (!fs::exists(ours, error)) {
        return true;
    }

    for (const std::string& relative : WebView2RuntimeDlls) {
        const fs::path dll = ours / relative;
        if (fs::exists(dll, error) && !isFakeDll(dll)) {
            return false;
        }
    }

    fs::remove_all(ours, error);
    return !error;
}

} // namespace tuxblox
