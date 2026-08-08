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
#include <vector>

namespace tuxblox {

// A minimal freedesktop.org system tray (XEmbed) icon. On KDE/GNOME this is
// automatically bridged to a proper StatusNotifierItem by the desktop's own
// XEmbed->SNI proxy (e.g. KDE's xembedsniproxy), so this doesn't need to
// speak the SNI D-Bus protocol itself, and stays a small, dependency-free
// addition (just libX11) instead of pulling in GTK/Qt/glib for a tray icon.
//
// Opens its own X11 connection, entirely independent of SDL's -- sharing
// SDL's Display and pumping events on it here would race with, and
// silently steal, events SDL's own event loop needs for the main window.
class TrayIcon {
public:
    TrayIcon();
    ~TrayIcon();
    TrayIcon(const TrayIcon&) = delete;
    TrayIcon& operator=(const TrayIcon&) = delete;

    // `rgba` is iconSize*iconSize*4 bytes, straight (non-premultiplied)
    // RGBA, row-major top-to-bottom. Returns false (and the icon is simply
    // not shown) if no system tray manager is currently running on this
    // desktop.
    bool init(const unsigned char* rgba, int iconSize);
    void shutdown();
    bool isActive() const;

    // Pumps this tray icon's own X11 event queue. Returns true on the frame
    // the icon was left-clicked -- the caller's cue to restore the main
    // window.
    bool pollClicked();

private:
    void redraw(); // (re-)packs iconRgba_ into the window's pixel format and XPutImage's it

    void* display_ = nullptr;   // Display*, kept opaque so X11/Xlib.h stays out of this header
    unsigned long window_ = 0;  // Window
    void* visual_ = nullptr;    // Visual*
    unsigned long gc_ = 0;      // GC
    int depth_ = 0;
    bool haveArgb_ = false;
    std::vector<unsigned char> iconRgba_;
    int iconSize_ = 0;
};

} // namespace tuxblox
