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

#include "flag_startup.h"
#include "seasonal.h"
#include "adw_look.h"
#include "dialog.h"
#include "i18n.h"

#include <cstring>
#include <utility>

// Third Parties
#include <adwaita.h>
#include <gdk-pixbuf/gdk-pixbuf.h>

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

const char LauncherStyles[] =
    // libadwaita puts a pencil on every editable row, from an icon theme TuxBlox does not ship, so it drew as a broken-image box
    ".editable-area > .edit-icon { -gtk-icon-size: 0; min-width: 0; min-height: 0; padding: 0; margin: 0; opacity: 0; }\n"
    "button.launch-green { background-color: @success_bg_color; color: @success_fg_color; }\n"
    "button.launch-green:hover { background-image: image(alpha(white, 0.1)); }\n"
    "button.launch-green:active { background-image: image(alpha(black, 0.1)); }\n"
    "button.stop-red { background-color: @error_bg_color; color: @error_fg_color; }\n"
    "button.stop-red:hover { background-image: image(alpha(white, 0.1)); }\n"
    "button.stop-red:active { background-image: image(alpha(black, 0.1)); }\n"
    "button.update-button { min-height: 0; min-width: 0; padding: 5px; background-color: @warning_bg_color; color: @warning_fg_color; }\n"
    "button.update-button:hover { background-image: image(alpha(white, 0.15)); }\n"
    "button.update-button:active { background-image: image(alpha(black, 0.1)); }\n"
    // Deliberately not the monospace family: TuxBlox ships no monospace font, so that family is whatever the machine happens to have, and a version number needs no fixed width anyway
    "button.session-open { font-size: 0.8rem; min-height: 0; padding: 2px 11px; }\n"
    "label.version-meta { font-size: 0.75rem; line-height: 1.0; }\n"
    // The welcome tour's theme previews: one element each, the sidebar's colour hard-stopped against the page's
    "box.theme-swatch { min-height: 34px; border-radius: 6px; box-shadow: inset 0 0 0 1px @card_shade_color; }\n"
    "box.theme-swatch-dark { background: linear-gradient(to right, #0e1117 0%, #0e1117 30%, #0a0c11 30%, #0a0c11 100%); }\n"
    "box.theme-swatch-grey { background: linear-gradient(to right, #303030 0%, #303030 30%, #242424 30%, #242424 100%); }\n"
    "box.theme-swatch-light { background: linear-gradient(to right, #ffffff 0%, #ffffff 30%, #f5f6f8 30%, #f5f6f8 100%); }\n"
    "button.theme-tile { padding: 6px; }\n"
    "button.theme-tile:checked { box-shadow: inset 0 0 0 2px @accent_bg_color; }\n";

// The structural panes paint the window colour over the whole area, which would bury the snow drawn behind them. Only the panes are cleared; every surface that carries content keeps the see-through colour the palette gives it.
const char ChristmasStyles[] =
    "navigation-split-view, navigation-split-view > page, toolbarview, toolbarview > box, stack { background-color: transparent; }\n"
    "navigation-split-view > page > toolbarview > box > stack { background-color: transparent; }\n"
    // The window is the only solid surface left, so the depth every pane is tinted over comes from here
    "window.background { background-image: radial-gradient(ellipse at 18% -12%, #131f36 0%, rgba(19, 31, 54, 0) 55%), linear-gradient(160deg, #080d18 0%, #04060a 55%, #0a1019 100%); }\n"
    // What makes a pane read as glass rather than as a darker box: a sheen down its face, a lit top edge and a brighter rim
    ".card, list.boxed-list, list.boxed-list-separate > row { background-image: linear-gradient(rgba(255, 255, 255, 0.08), rgba(255, 255, 255, 0.02)); box-shadow: inset 0 1px 0 rgba(255, 255, 255, 0.12), 0 0 0 1px rgba(255, 255, 255, 0.13); }\n";

} // namespace

GdkPaintable *symbolicIcon(const unsigned char *pPng, size_t length, int pixelSize) {
    auto *pIcon = static_cast<TbSymbolicIcon *>(g_object_new(tb_symbolic_icon_get_type(), nullptr));
    pIcon->pTexture = iconTexture(pPng, length, pixelSize);
    return GDK_PAINTABLE(pIcon);
}

GtkWidget *iconImage(const unsigned char *pPng, size_t length, int pixelSize) {
    GdkPaintable *pIcon = symbolicIcon(pPng, length, pixelSize);
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

namespace {

bool SystemDark = false;

} // namespace

void captureSystemColourScheme() {
    // Read while the scheme is still the desktop's, so forcing one later cannot overwrite the answer.
    SystemDark = adw_style_manager_get_dark(adw_style_manager_get_default()) != FALSE;
}

bool systemPrefersDark() {
    return SystemDark;
}

std::string launcherThemeFor(const std::string &theme) {
    if (theme != "christmas") return theme;
    return christmasThemeAvailable() && launcherFlags().christmasTheme ? theme : "system";
}

void applyLauncherTheme(const std::string &wanted) {
    const std::string theme = launcherThemeFor(wanted);
    applyTheme(theme);

    // The buttons are the launcher's own and are the same in every theme, so they are loaded once and left alone
    static bool installed = false;
    if (!installed) {
        installed = true;
        GtkCssProvider *pProvider = gtk_css_provider_new();
        gtk_css_provider_load_from_string(pProvider, LauncherStyles);
        gtk_style_context_add_provider_for_display(gdk_display_get_default(), GTK_STYLE_PROVIDER(pProvider),
                                                   GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
        g_object_unref(pProvider);
    }

    // Reloaded on every theme change rather than installed once, because it has to come back off again when the theme changes away from Christmas
    static GtkCssProvider *pSeasonal = nullptr;
    if (pSeasonal == nullptr) {
        pSeasonal = gtk_css_provider_new();
        gtk_style_context_add_provider_for_display(gdk_display_get_default(), GTK_STYLE_PROVIDER(pSeasonal),
                                                   GTK_STYLE_PROVIDER_PRIORITY_APPLICATION + 1);
    }
    gtk_css_provider_load_from_string(pSeasonal, theme == "christmas" ? ChristmasStyles : "");
}

void showNotice(GtkWidget *pParent, const std::string &heading, const std::string &body) {
    adw_dialog_present(buildDialog(heading, body, {{i18n::tr("OK"), nullptr, nullptr}}), pParent);
}

void confirmDestructive(GtkWidget *pParent, const std::string &heading, const std::string &body,
                        const std::string &confirmLabel, std::function<void()> onConfirm) {
    // Escape and the close both leave onConfirm unrun, which is what makes them count as cancelling.
    adw_dialog_present(buildDialog(heading, body,
                                   {{i18n::tr("Cancel"), nullptr, nullptr},
                                    {confirmLabel, "destructive-action", std::move(onConfirm)}}),
                       pParent);
}

} // namespace tuxblox
