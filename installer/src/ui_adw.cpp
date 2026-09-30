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
#include "ui_adw.h"
#include "tuxblox_logo_png.h"

// Third Parties
#include <adwaita.h>

namespace tuxblox {

namespace {

struct InstallUi {
    App *pApp = nullptr;
    AdwStatusPage *pPage = nullptr;
    GtkProgressBar *pBar = nullptr;
    GtkWindow *pWindow = nullptr;
    int result = 2;
};

struct ResultUi {
    bool ok = false;
    GtkWindow *pWindow = nullptr;
};

// Dark as the previous interface was, and the bundled font so it looks the same on every desktop instead of inheriting the host's.
void applyLook() {
    adw_style_manager_set_color_scheme(adw_style_manager_get_default(), ADW_COLOR_SCHEME_PREFER_DARK);
    g_object_set(gtk_settings_get_default(), "gtk-font-name", "Adwaita Sans 11", nullptr);
}

// The logo is the status page's image, which is also what keeps this free of any icon theme: adwaita-icon-theme is a separate package and its symbolic icons are SVG, which would need librsvg.
GdkPaintable *logoPaintable() {
    GBytes *pBytes = g_bytes_new_static(kTuxbloxLogoPng, kTuxbloxLogoPngLen);
    GdkTexture *pTexture = gdk_texture_new_from_bytes(pBytes, nullptr);
    g_bytes_unref(pBytes);
    return GDK_PAINTABLE(pTexture);
}

// Polled rather than signalled because App already owns a background thread and exposes a mutex-guarded snapshot, exactly as the launcher's own window does.
gboolean onTick(gpointer data) {
    auto *pUi = static_cast<InstallUi *>(data);
    const AppSnapshot snap = pUi->pApp->snapshot();

    gtk_progress_bar_set_fraction(pUi->pBar, snap.overallPercent / 100.0);
    adw_status_page_set_description(pUi->pPage, snap.currentStepLabel.c_str());
    adw_status_page_set_title(pUi->pPage, snap.isUpgrade ? "Updating TuxBlox" : "Installing TuxBlox");

    if (snap.phase == AppPhase::Error) {
        adw_status_page_set_title(pUi->pPage, "TuxBlox could not be installed");
        adw_status_page_set_description(pUi->pPage, snap.errorMessage.c_str());
        gtk_widget_set_visible(GTK_WIDGET(pUi->pBar), FALSE);
        pUi->result = 1;
        return G_SOURCE_REMOVE;
    }
    if (pUi->pApp->readyToLaunch()) {
        pUi->result = 0;
        gtk_window_close(pUi->pWindow);
        return G_SOURCE_REMOVE;
    }
    return G_SOURCE_CONTINUE;
}

void onActivate(GtkApplication *pGtkApp, gpointer data) {
    applyLook();
    auto *pUi = static_cast<InstallUi *>(data);

    GtkWidget *pWindow = adw_application_window_new(pGtkApp);
    pUi->pWindow = GTK_WINDOW(pWindow);
    gtk_window_set_title(pUi->pWindow, "TuxBlox Installer");
    gtk_window_set_default_size(pUi->pWindow, 480, 360);
    gtk_window_set_resizable(pUi->pWindow, FALSE);

    GtkWidget *pView = adw_toolbar_view_new();
    adw_toolbar_view_add_top_bar(ADW_TOOLBAR_VIEW(pView), adw_header_bar_new());

    GtkWidget *pPage = adw_status_page_new();
    pUi->pPage = ADW_STATUS_PAGE(pPage);
    GdkPaintable *pLogo = logoPaintable();
    adw_status_page_set_paintable(pUi->pPage, pLogo);
    g_object_unref(pLogo);
    adw_status_page_set_title(pUi->pPage, "Installing TuxBlox");

    GtkWidget *pBar = gtk_progress_bar_new();
    pUi->pBar = GTK_PROGRESS_BAR(pBar);
    gtk_widget_set_margin_start(pBar, 24);
    gtk_widget_set_margin_end(pBar, 24);
    adw_status_page_set_child(pUi->pPage, pBar);

    adw_toolbar_view_set_content(ADW_TOOLBAR_VIEW(pView), pPage);
    adw_application_window_set_content(ADW_APPLICATION_WINDOW(pWindow), pView);

    g_timeout_add(100, onTick, pUi);
    gtk_window_present(pUi->pWindow);
}

void onResultActivate(GtkApplication *pGtkApp, gpointer data) {
    applyLook();
    auto *pUi = static_cast<ResultUi *>(data);

    GtkWidget *pWindow = adw_application_window_new(pGtkApp);
    pUi->pWindow = GTK_WINDOW(pWindow);
    gtk_window_set_title(pUi->pWindow, "TuxBlox");
    gtk_window_set_default_size(pUi->pWindow, 480, 360);

    AdwDialog *pDialog = adw_alert_dialog_new(
        pUi->ok ? "TuxBlox Uninstalled" : "TuxBlox Error",
        pUi->ok ? "TuxBlox has been completely removed from this system."
                : "Desktop shortcuts and URL handlers were removed, but the TuxBlox folder could "
                  "not be fully deleted. You may need to remove it manually.");
    adw_alert_dialog_add_response(ADW_ALERT_DIALOG(pDialog), "close", "Close");
    adw_alert_dialog_set_default_response(ADW_ALERT_DIALOG(pDialog), "close");
    // Closing the dialog is the only way out, so the window goes with it rather than being left behind empty.
    g_signal_connect_swapped(pDialog, "closed", G_CALLBACK(gtk_window_close), pWindow);
    gtk_window_present(pUi->pWindow);
    adw_dialog_present(pDialog, pWindow);
}

} // namespace

int runAdwInstall(App &app, const CliOptions &) {
    InstallUi ui;
    ui.pApp = &app;

    AdwApplication *pGtkApp = adw_application_new("net.tuxblox.Installer", G_APPLICATION_NON_UNIQUE);
    g_signal_connect(pGtkApp, "activate", G_CALLBACK(onActivate), &ui);
    g_application_run(G_APPLICATION(pGtkApp), 0, nullptr);
    g_object_unref(pGtkApp);
    return ui.result;
}

int runAdwUninstallResult(bool ok) {
    ResultUi ui;
    ui.ok = ok;

    AdwApplication *pGtkApp = adw_application_new("net.tuxblox.Installer", G_APPLICATION_NON_UNIQUE);
    g_signal_connect(pGtkApp, "activate", G_CALLBACK(onResultActivate), &ui);
    g_application_run(G_APPLICATION(pGtkApp), 0, nullptr);
    g_object_unref(pGtkApp);
    return ok ? 0 : 1;
}

} // namespace tuxblox
