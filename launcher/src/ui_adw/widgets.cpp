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

#include "widgets.h"

#include <cstring>
#include <utility>

// Third Parties
#include <adwaita.h>

G_DECLARE_FINAL_TYPE(TbSymbolicIcon, tb_symbolic_icon, TB, SYMBOLIC_ICON, GObject)

struct _TbSymbolicIcon {
    GObject parent_instance;
    GdkTexture *pTexture;
};

static void tb_symbolic_icon_snapshot_symbolic(GtkSymbolicPaintable *pPaintable, GdkSnapshot *pSnapshot, double width,
                                               double height, const GdkRGBA *pColors, gsize) {
    TbSymbolicIcon *pSelf = TB_SYMBOLIC_ICON(pPaintable);
    if (pSelf->pTexture == nullptr) return;
    graphene_rect_t bounds;
    graphene_rect_init(&bounds, 0.0f, 0.0f, static_cast<float>(width), static_cast<float>(height));
    GtkSnapshot *pGtkSnapshot = GTK_SNAPSHOT(pSnapshot);
    // The PNG only decides where the icon is; its colour always comes from the text around it
    gtk_snapshot_push_mask(pGtkSnapshot, GSK_MASK_MODE_ALPHA);
    gtk_snapshot_append_scaled_texture(pGtkSnapshot, pSelf->pTexture, GSK_SCALING_FILTER_TRILINEAR, &bounds);
    gtk_snapshot_pop(pGtkSnapshot);
    gtk_snapshot_append_color(pGtkSnapshot, &pColors[0], &bounds);
    gtk_snapshot_pop(pGtkSnapshot);
}

static void tb_symbolic_icon_snapshot(GdkPaintable *pPaintable, GdkSnapshot *pSnapshot, double width, double height) {
    const GdkRGBA black = {0.0f, 0.0f, 0.0f, 1.0f};
    tb_symbolic_icon_snapshot_symbolic(GTK_SYMBOLIC_PAINTABLE(pPaintable), pSnapshot, width, height, &black, 1);
}

static int tb_symbolic_icon_get_intrinsic_width(GdkPaintable *pPaintable) {
    TbSymbolicIcon *pSelf = TB_SYMBOLIC_ICON(pPaintable);
    return pSelf->pTexture ? gdk_texture_get_width(pSelf->pTexture) : 0;
}

static int tb_symbolic_icon_get_intrinsic_height(GdkPaintable *pPaintable) {
    TbSymbolicIcon *pSelf = TB_SYMBOLIC_ICON(pPaintable);
    return pSelf->pTexture ? gdk_texture_get_height(pSelf->pTexture) : 0;
}

static GdkPaintableFlags tb_symbolic_icon_get_flags(GdkPaintable *) {
    return static_cast<GdkPaintableFlags>(GDK_PAINTABLE_STATIC_SIZE | GDK_PAINTABLE_STATIC_CONTENTS);
}

static void tb_symbolic_icon_paintable_init(GdkPaintableInterface *pInterface) {
    pInterface->snapshot = tb_symbolic_icon_snapshot;
    pInterface->get_flags = tb_symbolic_icon_get_flags;
    pInterface->get_intrinsic_width = tb_symbolic_icon_get_intrinsic_width;
    pInterface->get_intrinsic_height = tb_symbolic_icon_get_intrinsic_height;
}

static void tb_symbolic_icon_symbolic_init(GtkSymbolicPaintableInterface *pInterface) {
    pInterface->snapshot_symbolic = tb_symbolic_icon_snapshot_symbolic;
}

G_DEFINE_FINAL_TYPE_WITH_CODE(TbSymbolicIcon, tb_symbolic_icon, G_TYPE_OBJECT,
                              G_IMPLEMENT_INTERFACE(GDK_TYPE_PAINTABLE, tb_symbolic_icon_paintable_init)
                              G_IMPLEMENT_INTERFACE(GTK_TYPE_SYMBOLIC_PAINTABLE, tb_symbolic_icon_symbolic_init))

static void tb_symbolic_icon_finalize(GObject *pObject) {
    g_clear_object(&TB_SYMBOLIC_ICON(pObject)->pTexture);
    G_OBJECT_CLASS(tb_symbolic_icon_parent_class)->finalize(pObject);
}

static void tb_symbolic_icon_class_init(TbSymbolicIconClass *pClass) {
    G_OBJECT_CLASS(pClass)->finalize = tb_symbolic_icon_finalize;
}

static void tb_symbolic_icon_init(TbSymbolicIcon *pSelf) {
    pSelf->pTexture = nullptr;
}

namespace tuxblox {

namespace {

struct Confirmation {
    std::function<void()> onConfirm;
};

void onConfirmResponse(AdwAlertDialog *, const char *pResponse, gpointer data) {
    if (g_strcmp0(pResponse, "confirm") == 0) static_cast<Confirmation *>(data)->onConfirm();
}

void freeConfirmation(gpointer data, GClosure *) {
    delete static_cast<Confirmation *>(data);
}

} // namespace

GdkPaintable *symbolicIcon(const unsigned char *pPng, size_t length) {
    auto *pIcon = static_cast<TbSymbolicIcon *>(g_object_new(tb_symbolic_icon_get_type(), nullptr));
    GBytes *pBytes = g_bytes_new_static(pPng, length);
    pIcon->pTexture = gdk_texture_new_from_bytes(pBytes, nullptr);
    g_bytes_unref(pBytes);
    return GDK_PAINTABLE(pIcon);
}

GtkWidget *iconImage(const unsigned char *pPng, size_t length, int pixelSize) {
    GdkPaintable *pIcon = symbolicIcon(pPng, length);
    GtkWidget *pImage = gtk_image_new_from_paintable(pIcon);
    g_object_unref(pIcon);
    gtk_image_set_pixel_size(GTK_IMAGE(pImage), pixelSize);
    return pImage;
}

GtkWidget *plainActionRow(const std::string &title, const std::string &subtitle) {
    GtkWidget *pRow = adw_action_row_new();
    adw_preferences_row_set_use_markup(ADW_PREFERENCES_ROW(pRow), FALSE);
    adw_preferences_row_set_title(ADW_PREFERENCES_ROW(pRow), title.c_str());
    if (!subtitle.empty()) adw_action_row_set_subtitle(ADW_ACTION_ROW(pRow), subtitle.c_str());
    return pRow;
}

GtkWidget *plainSwitchRow(const std::string &title, const std::string &subtitle) {
    GtkWidget *pRow = adw_switch_row_new();
    adw_preferences_row_set_use_markup(ADW_PREFERENCES_ROW(pRow), FALSE);
    adw_preferences_row_set_title(ADW_PREFERENCES_ROW(pRow), title.c_str());
    if (!subtitle.empty()) adw_action_row_set_subtitle(ADW_ACTION_ROW(pRow), subtitle.c_str());
    return pRow;
}

GtkWidget *errorLabel() {
    GtkWidget *pLabel = gtk_label_new(nullptr);
    gtk_label_set_wrap(GTK_LABEL(pLabel), TRUE);
    gtk_label_set_wrap_mode(GTK_LABEL(pLabel), PANGO_WRAP_WORD_CHAR);
    gtk_label_set_selectable(GTK_LABEL(pLabel), TRUE);
    gtk_label_set_xalign(GTK_LABEL(pLabel), 0.0f);
    gtk_widget_add_css_class(pLabel, "error");
    gtk_widget_set_visible(pLabel, FALSE);
    return pLabel;
}

void showError(GtkWidget *pLabel, const std::string &message) {
    if (!message.empty()) setLabelText(GTK_LABEL(pLabel), message);
    gtk_widget_set_visible(pLabel, !message.empty());
}

void setLabelText(GtkLabel *pLabel, const std::string &text) {
    if (text != gtk_label_get_text(pLabel)) gtk_label_set_text(pLabel, text.c_str());
}

void setButtonLabel(GtkButton *pButton, const std::string &text) {
    const char *pCurrent = gtk_button_get_label(pButton);
    if (pCurrent == nullptr || text != pCurrent) gtk_button_set_label(pButton, text.c_str());
}

void installLauncherStyles() {
    GtkCssProvider *pProvider = gtk_css_provider_new();
    gtk_css_provider_load_from_string(pProvider,
        "button.launch-green { background-color: @success_bg_color; color: @success_fg_color; }\n"
        "button.launch-green:hover { background-image: image(alpha(white, 0.1)); }\n"
        "button.launch-green:active { background-image: image(alpha(black, 0.1)); }\n"
        "button.update-button { font-weight: 700; font-size: 0.9rem; min-height: 0; padding: 3px 12px; background-color: @warning_bg_color; color: @warning_fg_color; }\n"
        "button.update-button:hover { background-image: image(alpha(white, 0.15)); }\n"
        "button.update-button:active { background-image: image(alpha(black, 0.1)); }\n");
    gtk_style_context_add_provider_for_display(gdk_display_get_default(), GTK_STYLE_PROVIDER(pProvider),
                                               GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
    g_object_unref(pProvider);
}

void showNotice(GtkWidget *pParent, const std::string &heading, const std::string &body) {
    AdwDialog *pDialog = adw_alert_dialog_new(heading.c_str(), body.c_str());
    adw_alert_dialog_add_response(ADW_ALERT_DIALOG(pDialog), "ok", "OK");
    adw_dialog_present(pDialog, pParent);
}

void confirmDestructive(GtkWidget *pParent, const std::string &heading, const std::string &body,
                        const std::string &confirmLabel, std::function<void()> onConfirm) {
    AdwDialog *pDialog = adw_alert_dialog_new(heading.c_str(), body.c_str());
    AdwAlertDialog *pAlert = ADW_ALERT_DIALOG(pDialog);
    adw_alert_dialog_add_response(pAlert, "cancel", "Cancel");
    adw_alert_dialog_add_response(pAlert, "confirm", confirmLabel.c_str());
    adw_alert_dialog_set_response_appearance(pAlert, "confirm", ADW_RESPONSE_DESTRUCTIVE);
    adw_alert_dialog_set_default_response(pAlert, "cancel");
    adw_alert_dialog_set_close_response(pAlert, "cancel");
    g_signal_connect_data(pDialog, "response", G_CALLBACK(onConfirmResponse), new Confirmation{std::move(onConfirm)},
                          freeConfirmation, static_cast<GConnectFlags>(0));
    adw_dialog_present(pDialog, pParent);
}

} // namespace tuxblox
