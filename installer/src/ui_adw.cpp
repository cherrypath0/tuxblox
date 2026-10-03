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
#include "adw_look.h"
#include <cstdio>
#include <string>
#include "tuxblox_logo_png.h"

// Third Parties
#include <adwaita.h>

namespace tuxblox {

namespace {

struct InstallUi {
    App *pApp = nullptr;
    GtkLabel *pTitle = nullptr;
    GtkLabel *pStep = nullptr;
    GtkProgressBar *pBar = nullptr;
    GtkLabel *pErrorLabel = nullptr;
    GtkWindow *pWindow = nullptr;
    int result = 2;
    bool activated = false;
};

struct ResultUi {
    bool ok = false;
    GtkWindow *pWindow = nullptr;
};

// Polled rather than signalled because App already owns a background thread and exposes a mutex-guarded snapshot, exactly as the launcher's own window does.
gboolean onTick(gpointer data) {
    auto *pUi = static_cast<InstallUi *>(data);
    const AppSnapshot snap = pUi->pApp->snapshot();

    gtk_progress_bar_set_fraction(pUi->pBar, snap.overallPercent / 100.0);
    gtk_label_set_text(pUi->pStep, snap.currentStepLabel.c_str());
    gtk_label_set_text(pUi->pTitle, snap.isUpgrade ? "Updating TuxBlox" : "Installing TuxBlox");

    if (snap.phase == AppPhase::Error) {
        gtk_label_set_text(pUi->pTitle, "TuxBlox could not be installed");
        gtk_widget_set_visible(GTK_WIDGET(pUi->pStep), FALSE);
        gtk_label_set_text(pUi->pErrorLabel, snap.errorMessage.c_str());
        gtk_widget_set_visible(GTK_WIDGET(pUi->pErrorLabel), TRUE);
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
    applyTheme(settingsTheme());
    auto *pUi = static_cast<InstallUi *>(data);
    pUi->activated = true;

    GtkWidget *pWindow = adw_application_window_new(pGtkApp);
    pUi->pWindow = GTK_WINDOW(pWindow);
    gtk_window_set_title(pUi->pWindow, "TuxBlox Installer");
    gtk_window_set_default_size(pUi->pWindow, 520, 180);
    gtk_window_set_resizable(pUi->pWindow, FALSE);

    GtkWidget *pView = adw_toolbar_view_new();


    GdkPaintable *pLogo = logoPaintable(kTuxbloxLogoPng, kTuxbloxLogoPngLen, 72);
    // GtkImage rather than GtkPicture: a picture grows to its paintable's natural size, which is the 440px logo, and a size request is only a minimum.
    GtkWidget *pPicture = gtk_image_new_from_paintable(pLogo);
    g_object_unref(pLogo);
    gtk_image_set_pixel_size(GTK_IMAGE(pPicture), 72);
    gtk_widget_set_valign(pPicture, GTK_ALIGN_CENTER);
    gtk_widget_set_hexpand(pPicture, FALSE);
    gtk_widget_set_vexpand(pPicture, FALSE);

    GtkWidget *pTitle = gtk_label_new("Installing TuxBlox");
    pUi->pTitle = GTK_LABEL(pTitle);
    gtk_label_set_xalign(pUi->pTitle, 0.0);
    gtk_label_set_wrap(pUi->pTitle, TRUE);
    gtk_widget_add_css_class(pTitle, "title-2");

    GtkWidget *pStep = gtk_label_new("Preparing");
    pUi->pStep = GTK_LABEL(pStep);
    gtk_label_set_xalign(pUi->pStep, 0.0);
    gtk_label_set_wrap(pUi->pStep, TRUE);
    gtk_widget_add_css_class(pStep, "dim-label");

    GtkWidget *pBar = gtk_progress_bar_new();
    pUi->pBar = GTK_PROGRESS_BAR(pBar);
    gtk_widget_set_margin_top(pBar, 6);

    // Arbitrary text from network or archive failures, so it wraps and can be selected into a bug report; the window grows rather than scrolling.
    GtkWidget *pErrorLabel = gtk_label_new(nullptr);
    pUi->pErrorLabel = GTK_LABEL(pErrorLabel);
    gtk_label_set_wrap(pUi->pErrorLabel, TRUE);
    gtk_label_set_wrap_mode(pUi->pErrorLabel, PANGO_WRAP_WORD_CHAR);
    gtk_label_set_selectable(pUi->pErrorLabel, TRUE);
    gtk_label_set_xalign(pUi->pErrorLabel, 0.0);
    gtk_widget_set_visible(pErrorLabel, FALSE);

    GtkWidget *pText = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4);
    gtk_widget_set_valign(pText, GTK_ALIGN_CENTER);
    gtk_widget_set_hexpand(pText, TRUE);
    gtk_box_append(GTK_BOX(pText), pTitle);
    gtk_box_append(GTK_BOX(pText), pStep);
    gtk_box_append(GTK_BOX(pText), pErrorLabel);
    gtk_box_append(GTK_BOX(pText), pBar);

    GtkWidget *pRow = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 20);
    gtk_widget_set_margin_start(pRow, 24);
    gtk_widget_set_margin_end(pRow, 24);
    gtk_widget_set_margin_top(pRow, 20);
    gtk_widget_set_margin_bottom(pRow, 4);
    gtk_widget_set_valign(pRow, GTK_ALIGN_CENTER);
    gtk_widget_set_vexpand(pRow, TRUE);
    gtk_box_append(GTK_BOX(pRow), pPicture);
    gtk_box_append(GTK_BOX(pRow), pText);

    GtkWidget *pCancel = gtk_button_new_with_label("Cancel");
    gtk_widget_set_halign(pCancel, GTK_ALIGN_END);
    gtk_widget_set_margin_end(pCancel, 18);
    gtk_widget_set_margin_bottom(pCancel, 14);
    g_signal_connect_swapped(pCancel, "clicked", G_CALLBACK(gtk_window_close), pWindow);

    GtkWidget *pOuter = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_box_append(GTK_BOX(pOuter), pRow);
    gtk_box_append(GTK_BOX(pOuter), pCancel);

    // Without a title bar there is nothing to drag the window by, so the whole surface becomes the drag handle; buttons inside it still receive their clicks.
    GtkWidget *pHandle = gtk_window_handle_new();
    gtk_window_handle_set_child(GTK_WINDOW_HANDLE(pHandle), pOuter);

    adw_toolbar_view_set_content(ADW_TOOLBAR_VIEW(pView), pHandle);

    adw_application_window_set_content(ADW_APPLICATION_WINDOW(pWindow), pView);

    g_timeout_add(100, onTick, pUi);
    gtk_window_present(pUi->pWindow);
}

void onResultActivate(GtkApplication *pGtkApp, gpointer data) {
    applyLook();
    applyTheme(settingsTheme());
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
    // A window that never appeared means nothing was installed, which must not look like the user closing it.
    if (!ui.activated) {
        fprintf(stderr, "TuxBlox could not open its window, so nothing was installed.\n");
        return 1;
    }
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
