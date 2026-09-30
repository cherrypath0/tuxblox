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
#include "install_paths.h"

#include <cstdio>

// Third Parties
#include <adwaita.h>

namespace tuxblox {

namespace {

void onClosed(AdwDialog *, gpointer data) {
    g_main_loop_quit(static_cast<GMainLoop *>(data));
}

} // namespace

void showErrorMessageBox(const std::string &title, const std::string &message) {
    // GTK already running means the launcher window set the bundled environment up, and it is not this function's to undo
    const bool startsGtk = !gtk_is_initialized();
    if (startsGtk) {
        useBundledEnvironment(interfaceStackRoot());
        // gtk_init() would exit the whole process when there is no display, taking the caller's remaining work with it
        if (!gtk_init_check()) {
            restoreBundledEnvironment();
            fprintf(stderr, "TuxBlox: %s\n%s\n", title.c_str(), message.c_str());
            return;
        }
        adw_init();
        applyLook();
    }

    AdwDialog *pDialog = adw_alert_dialog_new(title.c_str(), message.c_str());
    adw_alert_dialog_add_response(ADW_ALERT_DIALOG(pDialog), "ok", "OK");
    adw_alert_dialog_set_close_response(ADW_ALERT_DIALOG(pDialog), "ok");
    GMainLoop *pLoop = g_main_loop_new(nullptr, FALSE);
    g_signal_connect(pDialog, "closed", G_CALLBACK(onClosed), pLoop);
    // With no parent, libadwaita gives the dialog a small window of its own
    adw_dialog_present(pDialog, nullptr);
    g_main_loop_run(pLoop);
    g_main_loop_unref(pLoop);

    if (startsGtk) restoreBundledEnvironment();
}

} // namespace tuxblox
