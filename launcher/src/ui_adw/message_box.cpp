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

#include "message_box.h"
#include "adw_env.h"
#include "adw_look.h"
#include "dialog.h"
#include "install_paths.h"
#include "widgets.h"

#include <cerrno>
#include <cstdio>
#include <string>
#include <utility>
#include <vector>

#include <unistd.h>

// Third Parties
#include <adwaita.h>

namespace tuxblox {

namespace {

void onClosed(AdwDialog *, gpointer data) {
    g_main_loop_quit(static_cast<GMainLoop *>(data));
}

// Brings the interface up for a process that has no window yet, and says whether there is a display to draw on. True with *pStarted false when somebody else started it already.
bool startInterface(bool *pStarted) {
    // GTK cannot be started again after it failed to find a display, so the second message goes straight to stderr
    static bool noDisplay = false;

    *pStarted = false;
    if (noDisplay) return false;
    // GTK already running means the launcher window set the bundled environment up, and it is not this function's to undo
    if (gtk_is_initialized()) return true;

    useBundledEnvironment(interfaceStackRoot());
    // gtk_init() would exit the whole process when there is no display, taking the caller's remaining work with it
    if (!gtk_init_check()) {
        restoreBundledEnvironment();
        noDisplay = true;
        return false;
    }
    adw_init();
    applyLook();
    // A dialog shown before the window exists has no settings to read, so it takes the default look
    applyLauncherTheme("system");
    *pStarted = true;
    return true;
}

// The icon another program handed over, as the interface wants it. Null when there is nothing there or it does not describe an icon.
GdkTexture *readIconFd(int fd) {
    if (fd < 0) return nullptr;

    std::string contents;
    char buffer[8192];
    for (;;) {
        const ssize_t got = read(fd, buffer, sizeof(buffer));
        if (got > 0) contents.append(buffer, static_cast<size_t>(got));
        else if (got == 0 || errno != EINTR) break;
        // An icon is a few kilobytes, so a file that keeps coming is not one
        if (contents.size() > 4u * 1024 * 1024) { contents.clear(); break; }
    }
    close(fd);

    const gsize length = contents.size();
    const auto *pBytes = reinterpret_cast<const guchar *>(contents.data());
    const auto read32 = [pBytes](int at) {
        return static_cast<gsize>(pBytes[at]) | static_cast<gsize>(pBytes[at + 1]) << 8 |
               static_cast<gsize>(pBytes[at + 2]) << 16 | static_cast<gsize>(pBytes[at + 3]) << 24;
    };

    GdkTexture *pTexture = nullptr;
    if (length > 8) {
        const gsize width = read32(0);
        const gsize height = read32(4);
        // Guards a width and height that came from another program's resources against describing more than was handed over
        if (width > 0 && height > 0 && width <= 512 && height <= 512 && length - 8 >= width * height * 4) {
            GBytes *pRows = g_bytes_new(contents.data() + 8, width * height * 4);
            pTexture = gdk_memory_texture_new(static_cast<int>(width), static_cast<int>(height),
                                              GDK_MEMORY_R8G8B8A8, pRows, width * 4);
            g_bytes_unref(pRows);
        }
    }
    return pTexture;
}

// Shows the dialog and returns once it is closed, however it was closed.
void runDialog(AdwDialog *pDialog, GdkTexture *pIcon) {
    GMainLoop *pLoop = g_main_loop_new(nullptr, FALSE);

    g_signal_connect(pDialog, "closed", G_CALLBACK(onClosed), pLoop);
    // With no parent, libadwaita gives the dialog a small window of its own
    adw_dialog_present(pDialog, nullptr);

    // Only once it is on screen does the window exist to carry an icon. A desktop that has no way to show one ignores it.
    if (pIcon != nullptr) {
        GtkRoot *pRoot = gtk_widget_get_root(GTK_WIDGET(pDialog));
        GdkSurface *pSurface = GTK_IS_NATIVE(pRoot) ? gtk_native_get_surface(GTK_NATIVE(pRoot)) : nullptr;
        if (pSurface != nullptr && GDK_IS_TOPLEVEL(pSurface)) {
            GList *pOne = g_list_append(nullptr, pIcon);
            gdk_toplevel_set_icon_list(GDK_TOPLEVEL(pSurface), pOne);
            g_list_free(pOne);
        }
    }

    g_main_loop_run(pLoop);
    g_main_loop_unref(pLoop);
}

} // namespace

bool showErrorMessageBoxWithAction(const std::string &title, const std::string &message, const std::string &actionLabel) {
    bool startedGtk = false;
    if (!startInterface(&startedGtk)) {
        fprintf(stderr, "TuxBlox: %s\n%s\n", title.c_str(), message.c_str());
        return false;
    }

    bool actionChosen = false;
    std::vector<DialogButton> buttons;
    buttons.push_back({"OK", nullptr, nullptr, false});
    if (!actionLabel.empty()) {
        buttons.push_back({actionLabel, "suggested-action", [&actionChosen] { actionChosen = true; }, false});
    }

    runDialog(buildDialog(title, message, std::move(buttons)), nullptr);

    if (startedGtk) restoreBundledEnvironment();
    return actionChosen;
}

void showErrorMessageBox(const std::string &title, const std::string &message) {
    showErrorMessageBoxWithAction(title, message, "");
}

MessageBoxOutcome showRequestedMessageBox(const std::string &title, const std::string &message,
                                          const std::string &buttons, int defaultButton, bool canDismiss,
                                          int iconFd) {
    MessageBoxOutcome outcome;

    bool startedGtk = false;
    if (!startInterface(&startedGtk)) return outcome;

    // The caller sent them in the order they are to be read, so the one that carries on with whatever the message is about is the rightmost, as it is everywhere else in TuxBlox
    std::vector<std::string> labels;
    for (size_t at = 0; at <= buttons.size();) {
        const size_t comma = buttons.find(',', at);
        const size_t end = comma == std::string::npos ? buttons.size() : comma;
        if (end > at) labels.push_back(buttons.substr(at, end - at));
        if (comma == std::string::npos) break;
        at = comma + 1;
    }
    if (labels.empty()) labels.push_back("OK");

    std::vector<DialogButton> made;
    for (size_t at = 0; at < labels.size(); ++at) {
        const bool last = at + 1 == labels.size();
        const int which = static_cast<int>(at);
        made.push_back({labels[at], last ? "suggested-action" : nullptr,
                        [&outcome, which] {
                            outcome.how = MessageBoxOutcome::Pressed;
                            outcome.button = which;
                        },
                        defaultButton < 0 ? last : which == defaultButton});
    }

    GdkTexture *pIcon = readIconFd(iconFd);
    outcome.how = MessageBoxOutcome::Dismissed;
    AdwDialog *pDialog = buildDialog(title, message, std::move(made));
    adw_dialog_set_can_close(pDialog, canDismiss);
    runDialog(pDialog, pIcon);
    if (pIcon != nullptr) g_object_unref(pIcon);

    if (startedGtk) restoreBundledEnvironment();
    return outcome;
}

} // namespace tuxblox
