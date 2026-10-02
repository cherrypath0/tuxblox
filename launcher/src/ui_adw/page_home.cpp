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

#include "page_home.h"
#include "asset_download.h"
#include "asset_play.h"
#include "asset_roblox_player.h"
#include "asset_roblox_studio.h"
#include "version.h"
#include "widgets.h"

// Third Parties
#include <adwaita.h>

namespace tuxblox {

namespace {

std::string versionLabel(const AppVersions &versions) {
    std::string label = versions.activeHash;
    for (const auto &installed : versions.installed) {
        if (installed.hash != versions.activeHash) continue;
        // The version Roblox reports for itself reads as a version; the hash is only an identifier, so it is the fallback
        if (!installed.versionString.empty()) label = installed.versionString;
        if (!installed.channel.empty()) label += " \xC2\xB7 " + installed.channel;
        break;
    }
    return label;
}

GtkWidget *textLabel(const std::string &text, const char *pStyleClass) {
    GtkWidget *pLabel = gtk_label_new(text.c_str());
    gtk_label_set_xalign(GTK_LABEL(pLabel), 0.0f);
    if (pStyleClass != nullptr) gtk_widget_add_css_class(pLabel, pStyleClass);
    return pLabel;
}

} // namespace

HomePage::HomePage(App &app) : app_(app) {
    GtkWidget *pBox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_widget_set_margin_start(pBox, 24);
    gtk_widget_set_margin_end(pBox, 24);
    gtk_widget_set_margin_top(pBox, 22);
    gtk_widget_set_margin_bottom(pBox, 16);

    GtkWidget *pTitle = textLabel("Ready to play", "title-1");
    pTitle_ = GTK_LABEL(pTitle);
    gtk_box_append(GTK_BOX(pBox), pTitle);

    pSubtitle_ = textLabel("Launch Roblox on Linux via TuxBlox", "dim-label");
    gtk_label_set_wrap(GTK_LABEL(pSubtitle_), TRUE);
    gtk_widget_set_margin_top(pSubtitle_, 4);
    gtk_box_append(GTK_BOX(pBox), pSubtitle_);

    pCards_ = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 12);
    gtk_box_set_homogeneous(GTK_BOX(pCards_), TRUE);
    gtk_widget_set_margin_top(pCards_, 18);
    gtk_box_append(GTK_BOX(pCards_), buildCard(player_, "Roblox Player", kAssetRobloxPlayer, kAssetRobloxPlayerLen));
    gtk_box_append(GTK_BOX(pCards_), buildCard(studio_, "Roblox Studio", kAssetRobloxStudio, kAssetRobloxStudioLen));
    gtk_box_append(GTK_BOX(pBox), pCards_);

    pUpdateStatus_ = textLabel("", "heading");
    gtk_widget_set_margin_top(pUpdateStatus_, 18);
    gtk_widget_set_visible(pUpdateStatus_, FALSE);
    gtk_box_append(GTK_BOX(pBox), pUpdateStatus_);

    pUpdateBar_ = gtk_progress_bar_new();
    gtk_widget_set_margin_top(pUpdateBar_, 10);
    gtk_widget_set_visible(pUpdateBar_, FALSE);
    gtk_box_append(GTK_BOX(pBox), pUpdateBar_);

    pError_ = errorLabel();
    gtk_widget_set_margin_top(pError_, 12);
    gtk_box_append(GTK_BOX(pBox), pError_);

    GtkWidget *pSpacer = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_widget_set_vexpand(pSpacer, TRUE);
    gtk_box_append(GTK_BOX(pBox), pSpacer);

    GtkWidget *pStrip = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
    gtk_box_append(GTK_BOX(pStrip), textLabel(std::string("TuxBlox ") + kTuxBloxVersion, "caption"));
    GtkWidget *pChannel = textLabel(std::string("\xC2\xB7 ") + kTuxBloxChannel + " channel", "caption");
    gtk_widget_add_css_class(pChannel, "dim-label");
    gtk_widget_set_hexpand(pChannel, TRUE);
    gtk_box_append(GTK_BOX(pStrip), pChannel);
    GtkWidget *pState = textLabel("", "caption");
    pUpdateState_ = GTK_LABEL(pState);
    gtk_box_append(GTK_BOX(pStrip), pState);
    // The same dot that separates the version from the channel, shown only while the button is
    pUpdateDot_ = textLabel("\xC2\xB7", "caption");
    gtk_widget_add_css_class(pUpdateDot_, "dim-label");
    gtk_widget_set_visible(pUpdateDot_, FALSE);
    gtk_box_append(GTK_BOX(pStrip), pUpdateDot_);
    pUpdateButton_ = gtk_button_new_with_label("Update");
    gtk_widget_add_css_class(pUpdateButton_, "update-button");
    gtk_widget_set_visible(pUpdateButton_, FALSE);
    g_signal_connect(pUpdateButton_, "clicked", G_CALLBACK(onUpdateClicked), this);
    gtk_box_append(GTK_BOX(pStrip), pUpdateButton_);
    gtk_box_append(GTK_BOX(pBox), pStrip);

    pRoot_ = adw_clamp_new();
    adw_clamp_set_maximum_size(ADW_CLAMP(pRoot_), 720);
    adw_clamp_set_child(ADW_CLAMP(pRoot_), pBox);
}

GtkWidget *HomePage::widget() const {
    return pRoot_;
}

GtkWidget *HomePage::buildCard(Card &card, const char *pTitle, const unsigned char *pIcon, size_t iconLength) {
    GtkWidget *pIconImage = iconImage(pIcon, iconLength, 34);
    gtk_widget_set_valign(pIconImage, GTK_ALIGN_START);

    GtkWidget *pMeta = textLabel("Not installed yet", "caption");
    gtk_widget_add_css_class(pMeta, "dim-label");
    gtk_widget_add_css_class(pMeta, "version-meta");
    gtk_label_set_ellipsize(GTK_LABEL(pMeta), PANGO_ELLIPSIZE_END);
    card.pMeta = GTK_LABEL(pMeta);

    GtkWidget *pText = gtk_box_new(GTK_ORIENTATION_VERTICAL, 2);
    gtk_widget_set_hexpand(pText, TRUE);
    gtk_box_append(GTK_BOX(pText), textLabel(pTitle, "heading"));
    gtk_box_append(GTK_BOX(pText), pMeta);

    GtkWidget *pHeader = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 11);
    gtk_box_append(GTK_BOX(pHeader), pIconImage);
    gtk_box_append(GTK_BOX(pHeader), pText);

    GtkWidget *pButtonIcon = iconImage(kAssetDownload, kAssetDownloadLen, 16);
    GtkWidget *pButtonLabel = gtk_label_new("Install & Launch");
    card.pButtonIcon = pButtonIcon;
    card.pButtonLabel = GTK_LABEL(pButtonLabel);
    GtkWidget *pButtonContent = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    gtk_widget_set_halign(pButtonContent, GTK_ALIGN_CENTER);
    gtk_box_append(GTK_BOX(pButtonContent), pButtonIcon);
    gtk_box_append(GTK_BOX(pButtonContent), pButtonLabel);
    GtkWidget *pButton = gtk_button_new();
    gtk_button_set_child(GTK_BUTTON(pButton), pButtonContent);
    card.pButton = GTK_BUTTON(pButton);
    gtk_widget_set_margin_top(pButton, 12);
    g_object_set_data(G_OBJECT(pButton), "tuxblox-target", GINT_TO_POINTER(static_cast<int>(card.target)));
    g_signal_connect(pButton, "clicked", G_CALLBACK(onLaunchClicked), this);

    GtkWidget *pInner = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_widget_set_margin_start(pInner, 16);
    gtk_widget_set_margin_end(pInner, 16);
    gtk_widget_set_margin_top(pInner, 15);
    gtk_widget_set_margin_bottom(pInner, 15);
    gtk_box_append(GTK_BOX(pInner), pHeader);
    gtk_box_append(GTK_BOX(pInner), pButton);

    GtkWidget *pCard = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_widget_add_css_class(pCard, "card");
    gtk_box_append(GTK_BOX(pCard), pInner);
    return pCard;
}

void HomePage::onLaunchClicked(GtkButton *pButton, gpointer data) {
    auto *pSelf = static_cast<HomePage *>(data);
    const int target = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(pButton), "tuxblox-target"));
    pSelf->beginLaunchCooldown();
    pSelf->app_.requestLaunch(static_cast<LaunchTarget>(target));
}

void HomePage::beginLaunchCooldown() {
    gtk_widget_set_sensitive(GTK_WIDGET(player_.pButton), FALSE);
    gtk_widget_set_sensitive(GTK_WIDGET(studio_.pButton), FALSE);
    g_timeout_add(2500, onLaunchCooldownOver, this);
}

gboolean HomePage::onLaunchCooldownOver(gpointer data) {
    auto *pSelf = static_cast<HomePage *>(data);
    gtk_widget_set_sensitive(GTK_WIDGET(pSelf->player_.pButton), TRUE);
    gtk_widget_set_sensitive(GTK_WIDGET(pSelf->studio_.pButton), TRUE);
    return G_SOURCE_REMOVE;
}

void HomePage::showUpdateButton(bool shown) {
    gtk_widget_set_visible(pUpdateButton_, shown);
    gtk_widget_set_visible(pUpdateDot_, shown);
}

void HomePage::onUpdateClicked(GtkButton *, gpointer data) {
    static_cast<HomePage *>(data)->app_.requestUpdateNow();
}

void HomePage::setCard(Card &card, bool installed, const std::string &versionLabel) {
    setLabelText(card.pMeta, installed ? versionLabel : "Not installed yet");
    if (card.applied && card.installed == installed) return;
    card.installed = installed;
    card.applied = true;

    setLabelText(card.pButtonLabel, installed ? card.launchLabel : "Install & Launch");
    // Installing is the neutral grey button with a download arrow; launching is the green one with a play symbol
    GdkPaintable *pIcon = symbolicIcon(installed ? kAssetPlay : kAssetDownload, installed ? kAssetPlayLen : kAssetDownloadLen);
    gtk_image_set_from_paintable(GTK_IMAGE(card.pButtonIcon), pIcon);
    g_object_unref(pIcon);
    if (installed) {
        gtk_widget_add_css_class(GTK_WIDGET(card.pButton), "launch-green");
    } else {
        gtk_widget_remove_css_class(GTK_WIDGET(card.pButton), "launch-green");
    }
}

void HomePage::setUpdateState(const std::string &text, const std::string &styleClass) {
    setLabelText(pUpdateState_, text);
    if (styleClass == updateStateClass_) return;
    if (!updateStateClass_.empty()) gtk_widget_remove_css_class(GTK_WIDGET(pUpdateState_), updateStateClass_.c_str());
    gtk_widget_add_css_class(GTK_WIDGET(pUpdateState_), styleClass.c_str());
    updateStateClass_ = styleClass;
}

void HomePage::update(const AppSnapshot &snap) {
    setCard(player_, !snap.versions.player.activeHash.empty(), versionLabel(snap.versions.player));
    setCard(studio_, !snap.versions.studio.activeHash.empty(), versionLabel(snap.versions.studio));

    // Only an update actually being applied takes the screen over. Merely checking leaves the cards
    // there and says so in the strip at the bottom, so nothing flickers in and out on startup.
    const bool applying = snap.update.phase == UpdatePhase::PreparingUpdater;
    gtk_widget_set_visible(pUpdateStatus_, applying);
    gtk_widget_set_visible(pUpdateBar_, applying);
    gtk_widget_set_visible(pCards_, !applying);
    gtk_widget_set_visible(pSubtitle_, !applying);

    if (applying) {
        showUpdateButton(false);
        setLabelText(pTitle_, "Updating TuxBlox");
        setLabelText(GTK_LABEL(pUpdateStatus_), "Preparing updater");
        gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(pUpdateBar_), snap.update.fraction);
        setUpdateState("Updating", "warning");
        showError(pError_, "");
        return;
    }

    setLabelText(pTitle_, "Ready to play");
    showUpdateButton(snap.updateAvailableVersion.has_value());
    if (snap.update.phase == UpdatePhase::Error) {
        showError(pError_, snap.update.errorMessage);
        setUpdateState("Update check failed", "error");
        return;
    }

    showError(pError_, "");
    const bool checking = snap.update.phase == UpdatePhase::Idle ||
                          snap.update.phase == UpdatePhase::CheckingManifest;
    if (checking) {
        setUpdateState("Checking\xE2\x80\xA6", "dim-label");
    } else if (snap.updateAvailableVersion) {
        setUpdateState("Version " + *snap.updateAvailableVersion + " available", "warning");
    } else {
        setUpdateState("Up to date", "success");
    }
}

} // namespace tuxblox
