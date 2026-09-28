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

#pragma once
#include <filesystem>
#include <string>

namespace tuxblox {

// The layer's copy of the WebView2 runtime version, kept in step with Wine's own webview2_version.h by a test because the two sit on opposite sides of the licence boundary and cannot share a header.
extern const char *kWebView2RuntimeVersion;

// True when the user asked for Microsoft's own WebView2 instead of TuxBlox's.
bool useMicrosoftWebView();

// What the drive records, so the parts of the runtime that cannot read the environment still know which one was asked for.
std::string webViewModeName();

// The version as a registry file spells it, quotes included.
std::string webViewVersionValue();

// What should happen to one of the runtime's registry values.
enum class WebViewVersionAction {
    Leave,
    Write,
    Remove,
};

// Decides that from the raw text the value holds now, empty when it is absent. A version that is not ours belongs to a real Microsoft runtime, which neither mode may touch.
WebViewVersionAction webViewVersionAction(const std::string& currentValue, bool microsoft);

// Removes the WebView2 runtime TuxBlox put in the drive, leaving a real Microsoft one alone, since a real install is the whole point of Microsoft mode. True when nothing of ours is left, including when there never was any.
bool removeFakeWebViewRuntime(const std::filesystem::path& prefixDir);

} // namespace tuxblox
