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
#include "adw_env.h"
#include "adw_look.h"
#include "config.h"
#include "tuxblox_logo_png.h"

#include <string>

// Third Parties
#include <adwaita.h>

namespace tuxblox {

namespace {

struct ProgressUi {
    App *pApp = nullptr;
    GtkWindow *pWindow = nullptr;
    GtkLabel *pStatus = nullptr;
    GtkProgressBar *pBar = nullptr;
    GtkLabel *pErrorLabel = nullptr;
    GtkButton *pButton = nullptr;
    guint tickId = 0;
    bool activated = false;
};

// Polled rather than signalled because App already owns a worker thread and exposes a mutex-guarded snapshot.
gboolean onTick(gpointer data) {
    auto *pUi = static_cast<ProgressUi *>(data);
    const Snapshot snap = pUi->pApp->snapshot();

    if (snap.phase == Phase::Error) {
        pUi->tickId = 0;
        gtk_label_set_text(pUi->pStatus, "TuxBlox could not finish updating Roblox");
        gtk_label_set_text(pUi->pErrorLabel, snap.errorMessage.c_str());
        gtk_widget_set_visible(GTK_WIDGET(pUi->pErrorLabel), TRUE);
        gtk_widget_set_visible(GTK_WIDGET(pUi->pBar), FALSE);
        gtk_button_set_label(pUi->pButton, "Close");
        return G_SOURCE_REMOVE;
    }

    gtk_label_set_text(pUi->pStatus, snap.status.c_str());
    gtk_progress_bar_set_fraction(pUi->pBar, snap.overallPercent / 100.0);

    // Preview stays up so the window can actually be looked at; the working modes close themselves because the launcher is waiting on this process.
    if (snap.phase == Phase::Done && !pUi->pApp->holdsOpenWhenDone()) {
        pUi->tickId = 0;
        gtk_window_close(pUi->pWindow);
        return G_SOURCE_REMOVE;
    }
    return G_SOURCE_CONTINUE;
}

// Cancel, the window's own close button and a finished run all arrive here, so the worker is stopped in exactly one place.
gboolean onCloseRequest(GtkWindow *, gpointer data) {
    auto *pUi = static_cast<ProgressUi *>(data);
    if (pUi->tickId != 0) {
        g_source_remove(pUi->tickId);
        pUi->tickId = 0;
    }
    if (!pUi->pApp->finished()) pUi->pApp->cancel();
    return FALSE;
}

void onActivate(GtkApplication *pGtkApp, gpointer data) {
    applyLook();
    auto *pUi = static_cast<ProgressUi *>(data);
    pUi->activated = true;

    GtkWidget *pWindow = adw_application_window_new(pGtkApp);
    pUi->pWindow = GTK_WINDOW(pWindow);
    gtk_window_set_title(pUi->pWindow, "TuxBlox");
    gtk_window_set_default_size(pUi->pWindow, 520, 180);
    gtk_window_set_resizable(pUi->pWindow, FALSE);
    g_signal_connect(pWindow, "close-request", G_CALLBACK(onCloseRequest), pUi);

    GdkPaintable *pLogo = logoPaintable(kTuxbloxLogoPng, kTuxbloxLogoPngLen, 72);
    // GtkImage rather than GtkPicture: a picture grows to its paintable's natural size, which is the 440px logo, and a size request is only a minimum.
    GtkWidget *pPicture = gtk_image_new_from_paintable(pLogo);
    g_object_unref(pLogo);
    gtk_image_set_pixel_size(GTK_IMAGE(pPicture), 72);
    gtk_widget_set_valign(pPicture, GTK_ALIGN_CENTER);
    gtk_widget_set_hexpand(pPicture, FALSE);
    gtk_widget_set_vexpand(pPicture, FALSE);

    GtkWidget *pStatus = gtk_label_new("Starting");
    pUi->pStatus = GTK_LABEL(pStatus);
    gtk_label_set_xalign(pUi->pStatus, 0.0);
    gtk_label_set_wrap(pUi->pStatus, TRUE);
    gtk_widget_add_css_class(pStatus, "title-2");

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
    gtk_box_append(GTK_BOX(pText), pStatus);
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

    GtkWidget *pButton = gtk_button_new_with_label("Cancel");
    pUi->pButton = GTK_BUTTON(pButton);
    gtk_widget_set_halign(pButton, GTK_ALIGN_END);
    gtk_widget_set_margin_end(pButton, 18);
    gtk_widget_set_margin_bottom(pButton, 14);
    g_signal_connect_swapped(pButton, "clicked", G_CALLBACK(gtk_window_close), pWindow);

    GtkWidget *pOuter = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_box_append(GTK_BOX(pOuter), pRow);
    gtk_box_append(GTK_BOX(pOuter), pButton);

    // Without a title bar there is nothing to drag the window by, so the whole surface becomes the drag handle; the button inside it still receives its click.
    GtkWidget *pHandle = gtk_window_handle_new();
    gtk_window_handle_set_child(GTK_WINDOW_HANDLE(pHandle), pOuter);

    GtkWidget *pView = adw_toolbar_view_new();
    adw_toolbar_view_set_content(ADW_TOOLBAR_VIEW(pView), pHandle);
    adw_application_window_set_content(ADW_APPLICATION_WINDOW(pWindow), pView);

    pUi->tickId = g_timeout_add(100, onTick, pUi);
    gtk_window_present(pUi->pWindow);
}

// The interface libraries are installed in the libtuxblox folder beside the binary.
std::string stackRoot() {
    const std::string self = selfExePath();
    return self.substr(0, self.rfind('/')) + "/libtuxblox";
}

} // namespace

bool runAdwProgress(App &app) {
    useBundledEnvironment(stackRoot());

    // GtkApplication starts GTK with gtk_init(), which exits the whole process when there is no display and would abandon the download with it.
    if (!gtk_init_check()) return false;

    ProgressUi ui;
    ui.pApp = &app;

    AdwApplication *pGtkApp = adw_application_new("net.tuxblox.Bootstrapper", G_APPLICATION_NON_UNIQUE);
    g_signal_connect(pGtkApp, "activate", G_CALLBACK(onActivate), &ui);
    g_application_run(G_APPLICATION(pGtkApp), 0, nullptr);
    g_object_unref(pGtkApp);
    return ui.activated;
}

} // namespace tuxblox
