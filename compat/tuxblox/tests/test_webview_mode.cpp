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

#include "../src/prefix/webview_mode.h"
#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>

namespace fs = std::filesystem;

namespace {

fs::path versionDir(const fs::path& prefixDir) {
    return prefixDir / tuxblox::WebView2ApplicationDir / tuxblox::kWebView2RuntimeVersion;
}

std::string readFileText(const fs::path& file) {
    std::ifstream in(file);
    assert(in && "file not found -- check the relative path");
    std::stringstream buffer;
    buffer << in.rdbuf();
    return buffer.str();
}

// A bare assert would only print the expression, which does not say which path went missing. Written to stderr because abort() does not flush stdout.
void requireInInf(const std::string& inf, const std::string& text) {
    if (inf.find(text) != std::string::npos) {
        return;
    }
    fprintf(stderr, "wine.inf.in does not carry \"%s\"\n", text.c_str());
    assert(false);
}

// The layer writes a key path for a registry file, where every backslash is doubled; wine.inf.in writes the same path with single ones.
std::string singleBackslashes(const std::string& key) {
    std::string text;
    for (size_t at = 0; at < key.size(); at++) {
        text.push_back(key[at]);
        if (key[at] == '\\' && at + 1 < key.size() && key[at + 1] == '\\') {
            at++;
        }
    }
    return text;
}

// wine.inf.in spells a copied file as "<folder>,<name>", so the last separator becomes a comma and the rest become backslashes.
std::string infCopyPath(const std::string& relative) {
    std::string text = relative;
    for (char& character : text) {
        if (character == '/') {
            character = '\\';
        }
    }
    const size_t last = text.rfind('\\');
    assert(last != std::string::npos);
    text[last] = ',';
    return text;
}

// A fake DLL is a PE carrying this marker; that is what tells our removal apart
// from a real Microsoft runtime.
void writeFakeDll(const fs::path& file) {
    fs::create_directories(file.parent_path());
    std::ofstream out(file, std::ios::binary);
    out << "MZ";
    out << std::string(62, '\0');
    out << "Wine builtin DLL";
}

} // namespace

int main() {
    using namespace tuxblox;

    const std::string version = kWebView2RuntimeVersion;

    // The version the layer builds paths from is the one Wine reports.
    {
        const std::string header = readFileText("../../wine/dlls/webview2loader/webview2_version.h");
        assert(header.find("L\"" + version + "\"") != std::string::npos);

        // wine.inf.in spells it out eight times: the x64 and x86 runtime folders, and the six EdgeUpdate values.
        const std::string inf = readFileText("../../wine/loader/wine.inf.in");
        const std::string marker = version.substr(0, version.find('.') + 1);
        size_t copies = 0;
        for (size_t at = inf.find(marker); at != std::string::npos; at = inf.find(marker, at + 1)) {
            assert(inf.compare(at, version.size(), version) == 0);
            copies++;
        }
        assert(copies == 8);

        // Every path the layer reconciles is one wine.inf.in laid down. A typo in either would leave an existing drive with a runtime nothing points at, silently.
        for (const WebViewVersionKey& entry : WebView2VersionKeys) {
            const std::string hive = (entry.file == "system.reg") ? "HKLM," : "HKCU,";
            requireInInf(inf, hive + singleBackslashes(entry.key) + ",\"pv\"");
        }
        for (const std::string& relative : WebView2RuntimeDlls) {
            requireInInf(inf, version + "\\" + infCopyPath(relative));
        }
    }

    // Only "off" values mean builtin. Anything else a person types meaning
    // "on" -- 1, true, yes -- must not silently leave the bridge in place.
    {
        unsetenv("TUXBLOX_USE_MSWEBVIEW");
        assert(!useMicrosoftWebView() && webViewModeName() == "builtin");
        setenv("TUXBLOX_USE_MSWEBVIEW", "1", 1);
        assert(useMicrosoftWebView() && webViewModeName() == "microsoft");
        setenv("TUXBLOX_USE_MSWEBVIEW", "0", 1);
        assert(!useMicrosoftWebView());
        setenv("TUXBLOX_USE_MSWEBVIEW", "", 1);
        assert(!useMicrosoftWebView());
        setenv("TUXBLOX_USE_MSWEBVIEW", "true", 1);
        assert(useMicrosoftWebView());
        unsetenv("TUXBLOX_USE_MSWEBVIEW");
    }

    // What each mode does to a registry value, for every version it can find there.
    {
        const std::string ours = webViewVersionValue();
        const std::string theirs = "\"154.0.4258.37\"";
        assert(ours == "\"" + version + "\"");

        assert(webViewVersionAction("", false) == WebViewVersionAction::Write);
        assert(webViewVersionAction(ours, false) == WebViewVersionAction::Leave);
        assert(webViewVersionAction(theirs, false) == WebViewVersionAction::Leave);

        assert(webViewVersionAction("", true) == WebViewVersionAction::Leave);
        assert(webViewVersionAction(ours, true) == WebViewVersionAction::Remove);
        assert(webViewVersionAction(theirs, true) == WebViewVersionAction::Leave);
    }

    const fs::path base = fs::temp_directory_path() / "tuxblox_test_webview_mode";

    // Our fake runtime goes.
    {
        fs::remove_all(base);
        for (const std::string& relative : WebView2RuntimeDlls) {
            writeFakeDll(versionDir(base) / relative);
        }
        assert(removeFakeWebViewRuntime(base));
        assert(!fs::exists(versionDir(base)));
    }

    // A real Microsoft runtime is never touched -- it is somebody's install,
    // and a real one is what Microsoft mode exists to let them use.
    {
        fs::remove_all(base);
        const fs::path real = base / "drive_c/Program Files (x86)/Microsoft/EdgeWebView/Application/154.0.4258.37/EBWebView/x64";
        fs::create_directories(real);
        std::ofstream(real / "EmbeddedBrowserWebView.dll") << std::string(4096, 'X');
        assert(removeFakeWebViewRuntime(base));
        assert(fs::exists(real / "EmbeddedBrowserWebView.dll"));
    }

    // A real runtime that happens to sit at our version number is also left
    // alone: the marker decides, not the folder name.
    {
        fs::remove_all(base);
        const fs::path f = versionDir(base) / WebView2RuntimeDlls[0];
        fs::create_directories(f.parent_path());
        std::ofstream(f, std::ios::binary) << std::string(4096, 'X');
        assert(!removeFakeWebViewRuntime(base));
        assert(fs::exists(f));
    }

    // Nothing there at all is success, not failure.
    fs::remove_all(base);
    assert(removeFakeWebViewRuntime(base));

    fs::remove_all(base);
    printf("webview_mode: all tests passed\n");
    return 0;
}
