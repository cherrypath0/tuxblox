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

#include "tray_icon.h"

#include <X11/Xlib.h>
#include <X11/Xutil.h>

#include <cstdint>
#include <cstdio>

namespace tuxblox {

namespace {
constexpr long kSystemTrayRequestDock = 0;
constexpr uint32_t kXEmbedMapped = 1;

int shiftFor(unsigned long mask) {
    int shift = 0;
    while (mask && !(mask & 1)) {
        mask >>= 1;
        shift++;
    }
    return shift;
}
} // namespace

TrayIcon::TrayIcon() = default;
TrayIcon::~TrayIcon() { shutdown(); }

bool TrayIcon::init(const unsigned char* rgba, int iconSize) {
    shutdown(); // idempotent if already inactive

    Display* display = XOpenDisplay(nullptr);
    if (!display) return false;
    int screen = DefaultScreen(display);

    char selAtomName[32];
    snprintf(selAtomName, sizeof(selAtomName), "_NET_SYSTEM_TRAY_S%d", screen);
    Atom trayAtom = XInternAtom(display, selAtomName, False);
    Window trayOwner = XGetSelectionOwner(display, trayAtom);
    if (trayOwner == None) {
        // No tray manager running on this desktop -- nothing to dock into.
        XCloseDisplay(display);
        return false;
    }

    // Prefer a real 32-bit ARGB visual (the convention every XEmbed tray
    // implementation expects for a transparent icon background); fall back
    // to the screen's default visual -- still works, just opaque -- if one
    // isn't available.
    XVisualInfo vinfo;
    bool haveArgb = XMatchVisualInfo(display, screen, 32, TrueColor, &vinfo) != 0;
    Visual* visual = haveArgb ? vinfo.visual : DefaultVisual(display, screen);
    int depth = haveArgb ? vinfo.depth : DefaultDepth(display, screen);
    Colormap colormap = haveArgb ? XCreateColormap(display, RootWindow(display, screen), visual, AllocNone)
                                  : DefaultColormap(display, screen);

    XSetWindowAttributes attrs = {};
    attrs.colormap = colormap;
    attrs.border_pixel = 0;
    attrs.background_pixel = 0;
    attrs.event_mask = ExposureMask | ButtonPressMask | ButtonReleaseMask;

    Window win = XCreateWindow(display, RootWindow(display, screen), 0, 0,
        static_cast<unsigned>(iconSize), static_cast<unsigned>(iconSize), 0, depth, InputOutput,
        visual, CWColormap | CWBorderPixel | CWBackPixel | CWEventMask, &attrs);
    if (!win) {
        if (haveArgb) XFreeColormap(display, colormap);
        XCloseDisplay(display);
        return false;
    }

    // Must be set before mapping/docking -- see the XEmbed spec.
    Atom xembedInfoAtom = XInternAtom(display, "_XEMBED_INFO", False);
    uint32_t xembedInfo[2] = {0, kXEmbedMapped};
    XChangeProperty(display, win, xembedInfoAtom, xembedInfoAtom, 32, PropModeReplace,
        reinterpret_cast<unsigned char*>(xembedInfo), 2);

    GC gc = XCreateGC(display, win, 0, nullptr);

    display_ = display;
    window_ = win;
    visual_ = visual;
    gc_ = reinterpret_cast<unsigned long>(gc);
    depth_ = depth;
    haveArgb_ = haveArgb;
    iconRgba_.assign(rgba, rgba + static_cast<size_t>(iconSize) * static_cast<size_t>(iconSize) * 4);
    iconSize_ = iconSize;

    // Draw once now so there's real content as soon as the window maps --
    // but this alone is NOT sufficient (see redraw()'s own comment): the
    // real, tray-triggered redraw on Expose below is what actually gets the
    // icon's content into the composited pixmap the SNI proxy exposes.
    redraw();

    XMapWindow(display, win);

    Atom opcodeAtom = XInternAtom(display, "_NET_SYSTEM_TRAY_OPCODE", False);
    XClientMessageEvent ev = {};
    ev.type = ClientMessage;
    ev.window = trayOwner;
    ev.message_type = opcodeAtom;
    ev.format = 32;
    ev.data.l[0] = CurrentTime;
    ev.data.l[1] = kSystemTrayRequestDock;
    ev.data.l[2] = static_cast<long>(win);
    XSendEvent(display, trayOwner, False, NoEventMask, reinterpret_cast<XEvent*>(&ev));
    XFlush(display);

    return true;
}

// Packs iconRgba_ into the window's actual pixel format (via the visual's
// own channel masks, correct whether we got the ARGB32 visual or the opaque
// default fallback) and XPutImage's it.
//
// Must run in response to Expose, not just once at init(): a freedesktop
// system tray captures an embedded window's content via the X Composite
// extension (XCompositeRedirectWindow), which the tray manager only sets up
// *after* it receives our dock request and finishes embedding the window --
// strictly later than the one-shot draw init() used to do before this fix.
// Content drawn before that redirect is established is not guaranteed to be
// in the composited buffer the tray reads from; confirmed live via D-Bus
// (org.kde.StatusNotifierItem's IconPixmap came back a literal 0x0 empty
// image with only the init()-time single draw in place). The tray manager
// reliably generates an Expose once it has the window ready to actually
// display, which is what this responds to.
void TrayIcon::redraw() {
    if (!display_) return;
    auto* display = static_cast<Display*>(display_);
    auto* visual = static_cast<Visual*>(visual_);
    auto gc = reinterpret_cast<GC>(gc_);

    XImage* image = XCreateImage(display, visual, static_cast<unsigned>(depth_), ZPixmap, 0,
        nullptr, static_cast<unsigned>(iconSize_), static_cast<unsigned>(iconSize_), 32, 0);
    if (!image) return;
    std::vector<char> buf(static_cast<size_t>(image->bytes_per_line) * static_cast<size_t>(iconSize_));
    image->data = buf.data();

    int rShift = shiftFor(visual->red_mask);
    int gShift = shiftFor(visual->green_mask);
    int bShift = shiftFor(visual->blue_mask);
    constexpr int kAlphaShift = 24; // top byte -- matches every XEmbed tray implementation's convention

    for (int y = 0; y < iconSize_; y++) {
        for (int x = 0; x < iconSize_; x++) {
            const unsigned char* px = iconRgba_.data() + (static_cast<size_t>(y) * iconSize_ + x) * 4;
            unsigned long pixel = (haveArgb_ ? (static_cast<unsigned long>(px[3]) << kAlphaShift) : 0) |
                (static_cast<unsigned long>(px[0]) << rShift) |
                (static_cast<unsigned long>(px[1]) << gShift) |
                (static_cast<unsigned long>(px[2]) << bShift);
            XPutPixel(image, x, y, pixel);
        }
    }

    XPutImage(display, static_cast<Window>(window_), gc, image, 0, 0, 0, 0,
        static_cast<unsigned>(iconSize_), static_cast<unsigned>(iconSize_));
    image->data = nullptr; // buf still owns it -- don't let XDestroyImage free it
    XDestroyImage(image);
    XFlush(display);
}

bool TrayIcon::isActive() const { return display_ != nullptr; }

bool TrayIcon::pollClicked() {
    if (!display_) return false;
    auto* display = static_cast<Display*>(display_);
    bool clicked = false;
    while (XPending(display)) {
        XEvent ev;
        XNextEvent(display, &ev);
        if (ev.xany.window != static_cast<Window>(window_)) continue;
        if (ev.type == Expose) {
            redraw();
        } else if (ev.type == ButtonRelease && ev.xbutton.button == Button1) {
            clicked = true;
        }
    }
    return clicked;
}

void TrayIcon::shutdown() {
    if (display_) {
        auto* display = static_cast<Display*>(display_);
        if (gc_) XFreeGC(display, reinterpret_cast<GC>(gc_));
        if (window_) XDestroyWindow(display, static_cast<Window>(window_));
        XCloseDisplay(display);
    }
    display_ = nullptr;
    window_ = 0;
    visual_ = nullptr;
    gc_ = 0;
    depth_ = 0;
    haveArgb_ = false;
    iconRgba_.clear();
    iconSize_ = 0;
}

} // namespace tuxblox
