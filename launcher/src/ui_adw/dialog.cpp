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


#include "dialog.h"
#include "adw_look.h"
#include "tuxblox_logo_png.h"

#include <utility>

namespace tuxblox {

namespace {

struct Press {
    AdwDialog *pDialog = nullptr;
    std::function<void()> onPress;
};

void onPressed(GtkButton *, gpointer data) {
    auto *pPress = static_cast<Press *>(data);
    // Closing the dialog can take the button, and this closure with it, so the callback is held onto first.
    const std::function<void()> callback = pPress->onPress;
    adw_dialog_close(pPress->pDialog);
    if (callback) callback();
}

void freePress(gpointer data, GClosure *) {
    delete static_cast<Press *>(data);
}

GtkWidget *bodyLabel(const std::string &text) {
    GtkWidget *pLabel = gtk_label_new(text.c_str());
    gtk_label_set_xalign(GTK_LABEL(pLabel), 0.0);
    gtk_label_set_wrap(GTK_LABEL(pLabel), TRUE);
    // Arbitrary text from a failure, so it breaks inside a long path and can be selected into a bug report.
    gtk_label_set_wrap_mode(GTK_LABEL(pLabel), PANGO_WRAP_WORD_CHAR);
    gtk_label_set_selectable(GTK_LABEL(pLabel), TRUE);
    return pLabel;
}

} // namespace

AdwDialog *buildDialog(const std::string &heading, const std::string &body, std::vector<DialogButton> buttons) {
    AdwDialog *pDialog = adw_dialog_new();

    GdkPaintable *pLogo = logoPaintable(kTuxbloxLogoPng, kTuxbloxLogoPngLen, 72);
    GtkWidget *pImage;
    if (pLogo != nullptr) {
        pImage = gtk_image_new_from_paintable(pLogo);
        g_object_unref(pLogo);
    } else {
        // The desktop's own copy of the icon, so that a logo this machine cannot decode leaves the dialog looking like itself rather than with a hole beside the message.
        pImage = gtk_image_new_from_icon_name("tuxblox");
    }
    gtk_image_set_pixel_size(GTK_IMAGE(pImage), 72);
    gtk_widget_set_valign(pImage, GTK_ALIGN_CENTER);

    GtkWidget *pHeading = gtk_label_new(heading.c_str());
    gtk_label_set_xalign(GTK_LABEL(pHeading), 0.0);
    gtk_label_set_wrap(GTK_LABEL(pHeading), TRUE);
    gtk_widget_add_css_class(pHeading, "title-2");

    GtkWidget *pText = gtk_box_new(GTK_ORIENTATION_VERTICAL, 6);
    gtk_widget_set_valign(pText, GTK_ALIGN_CENTER);
    gtk_widget_set_hexpand(pText, TRUE);
    gtk_box_append(GTK_BOX(pText), pHeading);
    if (!body.empty()) gtk_box_append(GTK_BOX(pText), bodyLabel(body));

    GtkWidget *pRow = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 20);
    gtk_widget_set_margin_start(pRow, 24);
    gtk_widget_set_margin_end(pRow, 24);
    gtk_widget_set_margin_top(pRow, 30);
    gtk_widget_set_margin_bottom(pRow, 14);
    gtk_widget_set_valign(pRow, GTK_ALIGN_CENTER);
    gtk_widget_set_vexpand(pRow, TRUE);
    gtk_box_append(GTK_BOX(pRow), pImage);
    gtk_box_append(GTK_BOX(pRow), pText);

    GtkWidget *pActions = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    gtk_widget_set_halign(pActions, GTK_ALIGN_END);
    gtk_widget_set_margin_end(pActions, 18);
    gtk_widget_set_margin_bottom(pActions, 14);

    GtkWidget *pLast = nullptr;
    GtkWidget *pMarked = nullptr;
    for (DialogButton &button : buttons) {
        GtkWidget *pButton = gtk_button_new_with_label(button.label.c_str());
        if (button.pStyleClass != nullptr) gtk_widget_add_css_class(pButton, button.pStyleClass);
        g_signal_connect_data(pButton, "clicked", G_CALLBACK(onPressed), new Press{pDialog, std::move(button.onPress)},
                              freePress, static_cast<GConnectFlags>(0));
        gtk_box_append(GTK_BOX(pActions), pButton);
        if (button.isDefault) pMarked = pButton;
        pLast = pButton;
    }

    GtkWidget *pOuter = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_box_append(GTK_BOX(pOuter), pRow);
    gtk_box_append(GTK_BOX(pOuter), pActions);

    // Presented without a parent this is a window with no title bar, so the whole surface becomes the drag handle; the buttons inside it still receive their clicks.
    GtkWidget *pHandle = gtk_window_handle_new();
    gtk_window_handle_set_child(GTK_WINDOW_HANDLE(pHandle), pOuter);

    adw_dialog_set_title(pDialog, heading.c_str());
    // The installer's window width. Height is left at its natural size, so a long message grows the dialog instead of being cut off.
    adw_dialog_set_content_width(pDialog, 520);
    adw_dialog_set_child(pDialog, pHandle);
    GtkWidget *pDefault = pMarked != nullptr ? pMarked : pLast;
    if (pDefault != nullptr) {
        adw_dialog_set_default_widget(pDialog, pDefault);
        adw_dialog_set_focus(pDialog, pDefault);
    }
    return pDialog;
}

} // namespace tuxblox
