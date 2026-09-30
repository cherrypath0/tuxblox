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

#include "page_about.h"
#include "adw_look.h"
#include "asset_discord.h"
#include "asset_docs.h"
#include "asset_github.h"
#include "asset_globe.h"
#include "asset_privacy.h"
#include "open_url.h"
#include "tuxblox_logo_png.h"
#include "version.h"
#include "widgets.h"

#include <string>

namespace tuxblox {

namespace {

struct Link {
    const char *pLabel;
    const char *pUrl;
    const unsigned char *pIcon;
    size_t iconLength;
};

const Link Links[] = {
    {"Website", "https://tuxblox.net", kAssetGlobe, kAssetGlobeLen},
    {"Documentation", "https://tuxblox.net/docs", kAssetDocs, kAssetDocsLen},
    {"GitHub", "https://tuxblox.net/github", kAssetGithub, kAssetGithubLen},
    {"Discord", "https://tuxblox.net/discord", kAssetDiscord, kAssetDiscordLen},
    {"Privacy Policy", "https://tuxblox.net/privacy", kAssetPrivacy, kAssetPrivacyLen},
};

GtkWidget *centredLabel(const std::string &text, const char *pStyleClass) {
    GtkWidget *pLabel = gtk_label_new(text.c_str());
    gtk_label_set_wrap(GTK_LABEL(pLabel), TRUE);
    gtk_label_set_justify(GTK_LABEL(pLabel), GTK_JUSTIFY_CENTER);
    if (pStyleClass != nullptr) gtk_widget_add_css_class(pLabel, pStyleClass);
    return pLabel;
}

} // namespace

AboutPage::AboutPage() {
    pRoot_ = adw_preferences_page_new();

    GdkPaintable *pLogo = logoPaintable(kTuxbloxLogoPng, kTuxbloxLogoPngLen);
    GtkWidget *pLogoImage = gtk_image_new_from_paintable(pLogo);
    g_object_unref(pLogo);
    gtk_image_set_pixel_size(GTK_IMAGE(pLogoImage), 96);

    GtkWidget *pHeader = gtk_box_new(GTK_ORIENTATION_VERTICAL, 6);
    gtk_box_append(GTK_BOX(pHeader), pLogoImage);
    gtk_box_append(GTK_BOX(pHeader), centredLabel("TuxBlox", "title-1"));
    gtk_box_append(GTK_BOX(pHeader),
                   centredLabel(std::string("Version ") + kTuxBloxVersion + " \xC2\xB7 " + kTuxBloxChannel + " channel",
                                "dim-label"));
    gtk_box_append(GTK_BOX(pHeader),
                   centredLabel("A free, open source launcher and compatibility layer for running Roblox on Linux.",
                                nullptr));
    GtkWidget *pHeaderGroup = adw_preferences_group_new();
    adw_preferences_group_add(ADW_PREFERENCES_GROUP(pHeaderGroup), pHeader);
    adw_preferences_page_add(ADW_PREFERENCES_PAGE(pRoot_), ADW_PREFERENCES_GROUP(pHeaderGroup));

    GtkWidget *pLinks = adw_preferences_group_new();
    adw_preferences_group_set_title(ADW_PREFERENCES_GROUP(pLinks), "Links");
    for (const Link &link : Links) {
        GtkWidget *pRow = plainActionRow(link.pLabel, "");
        adw_action_row_add_prefix(ADW_ACTION_ROW(pRow), iconImage(link.pIcon, link.iconLength, 16));
        gtk_list_box_row_set_activatable(GTK_LIST_BOX_ROW(pRow), TRUE);
        g_object_set_data(G_OBJECT(pRow), "tuxblox-url", const_cast<char *>(link.pUrl));
        g_signal_connect(pRow, "activated", G_CALLBACK(onLinkActivated), nullptr);
        adw_preferences_group_add(ADW_PREFERENCES_GROUP(pLinks), pRow);
    }
    adw_preferences_page_add(ADW_PREFERENCES_PAGE(pRoot_), ADW_PREFERENCES_GROUP(pLinks));

    GtkWidget *pFooter = adw_preferences_group_new();
    adw_preferences_group_add(ADW_PREFERENCES_GROUP(pFooter), centredLabel("\xC2\xA9 2026 TuxBlox Project", "dim-label"));
    adw_preferences_page_add(ADW_PREFERENCES_PAGE(pRoot_), ADW_PREFERENCES_GROUP(pFooter));
}

GtkWidget *AboutPage::widget() const {
    return pRoot_;
}

void AboutPage::onLinkActivated(AdwActionRow *pRow, gpointer) {
    openUrl(static_cast<const char *>(g_object_get_data(G_OBJECT(pRow), "tuxblox-url")));
}

} // namespace tuxblox
