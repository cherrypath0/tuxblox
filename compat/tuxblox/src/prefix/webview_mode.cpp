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

#include <cstdlib>
#include <fstream>

namespace tuxblox {

const char *kWebView2RuntimeVersion = "109.0.1518.140";

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

WebViewVersionAction webViewVersionAction(const std::string& currentValue, bool microsoft) {
    if (!currentValue.empty() && currentValue != webViewVersionValue()) {
        return WebViewVersionAction::Leave;
    }
    if (microsoft) {
        return currentValue.empty() ? WebViewVersionAction::Leave : WebViewVersionAction::Remove;
    }
    // Ours already, so there is nothing to write and no registry file to read back.
    return currentValue.empty() ? WebViewVersionAction::Write : WebViewVersionAction::Leave;
}

bool removeFakeWebViewRuntime(const std::filesystem::path& prefixDir) {
    namespace fs = std::filesystem;

    const fs::path ours = prefixDir / "drive_c/Program Files (x86)/Microsoft/EdgeWebView/Application" /
                          kWebView2RuntimeVersion;
    std::error_code error;
    if (!fs::exists(ours, error)) {
        return true;
    }

    for (const char *pArch : {"x64", "x86"}) {
        const fs::path dll = ours / "EBWebView" / pArch / "EmbeddedBrowserWebView.dll";
        if (fs::exists(dll, error) && !isFakeDll(dll)) {
            return false;
        }
    }

    fs::remove_all(ours, error);
    return !error;
}

} // namespace tuxblox
