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

#include "page_versions.h"
#include "versions_tab_state.h"
#include "widgets.h"

namespace tuxblox {

namespace {

bool sameVersions(const AppVersions &a, const AppVersions &b) {
    if (a.activeHash != b.activeHash || a.installed.size() != b.installed.size()) return false;
    for (size_t i = 0; i < a.installed.size(); ++i) {
        const InstalledVersion &x = a.installed[i];
        const InstalledVersion &y = b.installed[i];
        if (x.hash != y.hash || x.channel != y.channel || x.installedAt != y.installedAt ||
            x.versionString != y.versionString) {
            return false;
        }
    }
    return true;
}



GtkWidget *entryRow(const char *pTitle, const char *pText) {
    GtkWidget *pRow = adw_entry_row_new();
    adw_preferences_row_set_title(ADW_PREFERENCES_ROW(pRow), pTitle);
    gtk_editable_set_text(GTK_EDITABLE(pRow), pText);
    return pRow;
}

} // namespace

VersionsPage::VersionsPage(App &app) : app_(app) {
    pRoot_ = adw_preferences_page_new();
    adw_preferences_page_set_description(ADW_PREFERENCES_PAGE(pRoot_),
                                         "Choose which build of Roblox you play on, install an older one, or remove "
                                         "the ones you no longer need.");
    GtkWidget *pToolbarGroup = adw_preferences_group_new();
    adw_preferences_group_add(ADW_PREFERENCES_GROUP(pToolbarGroup), buildToolbar());
    adw_preferences_page_add(ADW_PREFERENCES_PAGE(pRoot_), ADW_PREFERENCES_GROUP(pToolbarGroup));

    adw_preferences_page_add(ADW_PREFERENCES_PAGE(pRoot_), buildList(player_, "Player"));
    adw_preferences_page_add(ADW_PREFERENCES_PAGE(pRoot_), buildList(studio_, "Studio"));

    // Below the lists, and on the page rather than in the sheet, so closing the
    // sheet never hides an install that is still running.
    buildProgressGroup();
    applyActive();
}

GtkWidget *VersionsPage::widget() const {
    return pRoot_;
}

GtkWidget *VersionsPage::buildToolbar() {
    pPlayerToggle_ = gtk_toggle_button_new_with_label("Player");
    pStudioToggle_ = gtk_toggle_button_new_with_label("Studio");
    gtk_toggle_button_set_group(GTK_TOGGLE_BUTTON(pStudioToggle_), GTK_TOGGLE_BUTTON(pPlayerToggle_));
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(pPlayerToggle_), TRUE);
    g_signal_connect(pPlayerToggle_, "toggled", G_CALLBACK(onTargetToggled), this);
    g_signal_connect(pStudioToggle_, "toggled", G_CALLBACK(onTargetToggled), this);

    GtkWidget *pSegmented = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    gtk_widget_add_css_class(pSegmented, "linked");
    gtk_box_append(GTK_BOX(pSegmented), pPlayerToggle_);
    gtk_box_append(GTK_BOX(pSegmented), pStudioToggle_);

    GtkWidget *pSpacer = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    gtk_widget_set_hexpand(pSpacer, TRUE);

    pInstallButton_ = gtk_button_new_with_label("Install a version");
    gtk_widget_add_css_class(pInstallButton_, "suggested-action");
    g_signal_connect(pInstallButton_, "clicked", G_CALLBACK(onInstallPressed), this);

    GtkWidget *pBar = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    gtk_box_append(GTK_BOX(pBar), pSegmented);
    gtk_box_append(GTK_BOX(pBar), pSpacer);
    gtk_box_append(GTK_BOX(pBar), pInstallButton_);
    return pBar;
}

void VersionsPage::applyActive() {
    const bool player = active_ == LaunchTarget::Player;
    gtk_widget_set_visible(GTK_WIDGET(player_.pGroup), player);
    gtk_widget_set_visible(GTK_WIDGET(studio_.pGroup), !player);
}

void VersionsPage::onTargetToggled(GtkToggleButton *pButton, gpointer data) {
    if (!gtk_toggle_button_get_active(pButton)) return;
    auto *pSelf = static_cast<VersionsPage *>(data);
    pSelf->active_ = pButton == GTK_TOGGLE_BUTTON(pSelf->pStudioToggle_) ? LaunchTarget::Studio
                                                                         : LaunchTarget::Player;
    pSelf->applyActive();
}

void VersionsPage::onInstallPressed(GtkButton *, gpointer data) {
    static_cast<VersionsPage *>(data)->openInstallSheet();
}

void VersionsPage::buildProgressGroup() {
    GtkWidget *pGroup = adw_preferences_group_new();

    pProgressLabel_ = gtk_label_new(nullptr);
    gtk_label_set_xalign(GTK_LABEL(pProgressLabel_), 0.0f);
    gtk_widget_add_css_class(pProgressLabel_, "dim-label");
    gtk_widget_set_visible(pProgressLabel_, FALSE);
    adw_preferences_group_add(ADW_PREFERENCES_GROUP(pGroup), pProgressLabel_);

    pProgress_ = gtk_progress_bar_new();
    gtk_widget_set_visible(pProgress_, FALSE);
    adw_preferences_group_add(ADW_PREFERENCES_GROUP(pGroup), pProgress_);

    pError_ = errorLabel();
    adw_preferences_group_add(ADW_PREFERENCES_GROUP(pGroup), pError_);

    adw_preferences_page_add(ADW_PREFERENCES_PAGE(pRoot_), ADW_PREFERENCES_GROUP(pGroup));
}

AdwPreferencesGroup *VersionsPage::buildList(List &list, const char *pTitle) {
    GtkWidget *pGroup = adw_preferences_group_new();
    // Untitled: the toggle above already names which list this is.
    (void)pTitle;
    list.pGroup = ADW_PREFERENCES_GROUP(pGroup);
    return list.pGroup;
}

LaunchTarget VersionsPage::selectedTarget() const {
    return active_;
}

void VersionsPage::onSheetModeToggled(GtkToggleButton *pButton, gpointer data) {
    if (!gtk_toggle_button_get_active(pButton)) return;
    auto *pSheet = static_cast<InstallSheet *>(data);
    // Only a named build needs one typed, so the field is offered only for Custom.
    gtk_widget_set_visible(pSheet->pHashBox, pButton == GTK_TOGGLE_BUTTON(pSheet->pCustom));
}

void VersionsPage::onSheetCancel(GtkButton *, gpointer data) {
    adw_dialog_close(static_cast<InstallSheet *>(data)->pDialog);
}

void VersionsPage::onSheetClosed(AdwDialog *, gpointer data) {
    delete static_cast<InstallSheet *>(data);
}

void VersionsPage::onSheetInstall(GtkButton *, gpointer data) {
    auto *pSheet = static_cast<InstallSheet *>(data);
    VersionsPage *pSelf = pSheet->pOwner;

    VersionSelectMode mode = VersionSelectMode::Latest;
    if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(pSheet->pPrevious))) {
        mode = VersionSelectMode::Previous;
    } else if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(pSheet->pCustom))) {
        mode = VersionSelectMode::ManualHash;
    }

    pSelf->channel_ = normalizedChannel(gtk_editable_get_text(pSheet->pChannel));
    const std::string hash = trimmedHash(gtk_editable_get_text(pSheet->pHash));
    adw_dialog_close(pSheet->pDialog);
    pSelf->app_.requestInstallVersion(pSelf->active_, mode, pSelf->channel_, hash);
}

void VersionsPage::openInstallSheet() {
    auto *pSheet = new InstallSheet;
    pSheet->pOwner = this;
    pSheet->pDialog = adw_dialog_new();

    GtkWidget *pHeading = gtk_label_new("Install a version");
    gtk_label_set_xalign(GTK_LABEL(pHeading), 0.0f);
    gtk_widget_add_css_class(pHeading, "title-2");

    pSheet->pLatest = gtk_toggle_button_new_with_label("Latest");
    pSheet->pPrevious = gtk_toggle_button_new_with_label("Previous");
    pSheet->pCustom = gtk_toggle_button_new_with_label("Custom");
    gtk_toggle_button_set_group(GTK_TOGGLE_BUTTON(pSheet->pPrevious), GTK_TOGGLE_BUTTON(pSheet->pLatest));
    gtk_toggle_button_set_group(GTK_TOGGLE_BUTTON(pSheet->pCustom), GTK_TOGGLE_BUTTON(pSheet->pLatest));
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(pSheet->pLatest), TRUE);

    GtkWidget *pModes = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    gtk_widget_add_css_class(pModes, "linked");
    gtk_widget_set_halign(pModes, GTK_ALIGN_START);
    for (GtkWidget *pMode : {pSheet->pLatest, pSheet->pPrevious, pSheet->pCustom}) {
        gtk_box_append(GTK_BOX(pModes), pMode);
        g_signal_connect(pMode, "toggled", G_CALLBACK(onSheetModeToggled), pSheet);
    }

    GtkWidget *pChannelLabel = gtk_label_new("Channel");
    gtk_label_set_xalign(GTK_LABEL(pChannelLabel), 0.0f);
    gtk_widget_add_css_class(pChannelLabel, "caption-heading");
    gtk_widget_add_css_class(pChannelLabel, "dim-label");
    GtkWidget *pChannel = gtk_entry_new();
    gtk_editable_set_text(GTK_EDITABLE(pChannel), channel_.c_str());
    pSheet->pChannel = GTK_EDITABLE(pChannel);

    GtkWidget *pHashLabel = gtk_label_new("Build");
    gtk_label_set_xalign(GTK_LABEL(pHashLabel), 0.0f);
    gtk_widget_add_css_class(pHashLabel, "caption-heading");
    gtk_widget_add_css_class(pHashLabel, "dim-label");
    GtkWidget *pHash = gtk_entry_new();
    gtk_entry_set_placeholder_text(GTK_ENTRY(pHash), "version-0000000000000000");
    pSheet->pHash = GTK_EDITABLE(pHash);

    pSheet->pHashBox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
    gtk_box_append(GTK_BOX(pSheet->pHashBox), pHashLabel);
    gtk_box_append(GTK_BOX(pSheet->pHashBox), pHash);
    gtk_widget_set_visible(pSheet->pHashBox, FALSE);

    GtkWidget *pCancel = gtk_button_new_with_label("Cancel");
    g_signal_connect(pCancel, "clicked", G_CALLBACK(onSheetCancel), pSheet);
    GtkWidget *pConfirm = gtk_button_new_with_label("Install");
    gtk_widget_add_css_class(pConfirm, "suggested-action");
    g_signal_connect(pConfirm, "clicked", G_CALLBACK(onSheetInstall), pSheet);

    GtkWidget *pButtons = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    gtk_widget_set_halign(pButtons, GTK_ALIGN_END);
    gtk_widget_set_margin_top(pButtons, 6);
    gtk_box_append(GTK_BOX(pButtons), pCancel);
    gtk_box_append(GTK_BOX(pButtons), pConfirm);

    GtkWidget *pBox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 14);
    gtk_widget_set_margin_start(pBox, 22);
    gtk_widget_set_margin_end(pBox, 22);
    gtk_widget_set_margin_top(pBox, 22);
    gtk_widget_set_margin_bottom(pBox, 18);
    gtk_box_append(GTK_BOX(pBox), pHeading);
    gtk_box_append(GTK_BOX(pBox), pModes);
    GtkWidget *pChannelBox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
    gtk_box_append(GTK_BOX(pChannelBox), pChannelLabel);
    gtk_box_append(GTK_BOX(pChannelBox), pChannel);
    gtk_box_append(GTK_BOX(pBox), pChannelBox);
    gtk_box_append(GTK_BOX(pBox), pSheet->pHashBox);
    gtk_box_append(GTK_BOX(pBox), pButtons);

    adw_dialog_set_title(pSheet->pDialog, "Install a version");
    adw_dialog_set_content_width(pSheet->pDialog, 420);
    adw_dialog_set_child(pSheet->pDialog, pBox);
    adw_dialog_set_default_widget(pSheet->pDialog, pConfirm);
    g_signal_connect(pSheet->pDialog, "closed", G_CALLBACK(onSheetClosed), pSheet);
    adw_dialog_present(pSheet->pDialog, pRoot_);
}

void VersionsPage::connectRowAction(GtkWidget *pButton, GCallback onClicked, LaunchTarget target, const std::string &hash) {
    // Freed with the button, so a rebuilt list leaves nothing behind
    g_signal_connect_data(pButton, "clicked", onClicked, new RowAction{this, target, hash}, freeRowAction,
                          static_cast<GConnectFlags>(0));
}

GtkWidget *VersionsPage::versionRow(LaunchTarget target, const AppVersions &versions, const InstalledVersion &version) {
    // The bundled font, not the monospace family: TuxBlox ships no monospace font, so what that family picks is the machine's, and on this one it draws a 7 as a bare slash
    GtkWidget *pRow = plainActionRow(version.hash, versionRowMeta(version));

    if (versionIsActive(versions, version.hash)) {
        GtkWidget *pActive = gtk_label_new("ACTIVE");
        gtk_widget_add_css_class(pActive, "accent");
        gtk_widget_add_css_class(pActive, "caption-heading");
        gtk_widget_set_valign(pActive, GTK_ALIGN_CENTER);
        adw_action_row_add_suffix(ADW_ACTION_ROW(pRow), pActive);
    } else {
        GtkWidget *pSetActive = gtk_button_new_with_label("Set active");
        gtk_widget_set_valign(pSetActive, GTK_ALIGN_CENTER);
        gtk_widget_set_sensitive(pSetActive, !readOnly_);
        connectRowAction(pSetActive, G_CALLBACK(onSetActive), target, version.hash);
        adw_action_row_add_suffix(ADW_ACTION_ROW(pRow), pSetActive);
    }

    // Behind an overflow: removing a build is rare and destructive, so it does not sit under the pointer.
    GtkWidget *pDelete = gtk_button_new_with_label("Remove");
    gtk_widget_add_css_class(pDelete, "destructive-action");
    gtk_widget_add_css_class(pDelete, "flat");
    gtk_widget_set_sensitive(pDelete, !readOnly_ && canDeleteVersion(versions, version.hash));
    connectRowAction(pDelete, G_CALLBACK(onDelete), target, version.hash);

    GtkWidget *pMenuBox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_widget_set_margin_start(pMenuBox, 6);
    gtk_widget_set_margin_end(pMenuBox, 6);
    gtk_widget_set_margin_top(pMenuBox, 6);
    gtk_widget_set_margin_bottom(pMenuBox, 6);
    gtk_box_append(GTK_BOX(pMenuBox), pDelete);
    GtkWidget *pPopover = gtk_popover_new();
    gtk_popover_set_child(GTK_POPOVER(pPopover), pMenuBox);

    GtkWidget *pMore = gtk_menu_button_new();
    gtk_menu_button_set_icon_name(GTK_MENU_BUTTON(pMore), "view-more-symbolic");
    gtk_menu_button_set_popover(GTK_MENU_BUTTON(pMore), pPopover);
    gtk_widget_add_css_class(pMore, "flat");
    gtk_widget_set_valign(pMore, GTK_ALIGN_CENTER);
    gtk_widget_set_tooltip_text(pMore, ("More actions for " + version.hash).c_str());
    adw_action_row_add_suffix(ADW_ACTION_ROW(pRow), pMore);
    return pRow;
}

void VersionsPage::applyReadOnly(bool readOnly) {
    adw_preferences_page_set_description(
        ADW_PREFERENCES_PAGE(pRoot_),
        readOnly ? "Auto-Update Roblox keeps these up to date. Turn it off in Settings to pick a build yourself."
                 : "Choose which build of Roblox you play on, install an older one, or remove the ones you no "
                   "longer need.");

    gtk_widget_set_sensitive(pInstallButton_, !readOnly);
}

void VersionsPage::rebuild(List &list, const AppVersions &versions) {
    for (GtkWidget *pRow : list.rows) adw_preferences_group_remove(list.pGroup, pRow);
    list.rows.clear();

    if (versions.installed.empty()) {
        GtkWidget *pEmpty = plainActionRow("No versions installed", "");
        adw_preferences_group_add(list.pGroup, pEmpty);
        list.rows.push_back(pEmpty);
        return;
    }
    for (const InstalledVersion &version : versions.installed) {
        GtkWidget *pRow = versionRow(list.target, versions, version);
        adw_preferences_group_add(list.pGroup, pRow);
        list.rows.push_back(pRow);
    }
}

void VersionsPage::onSetActive(GtkButton *, gpointer data) {
    auto *pAction = static_cast<RowAction *>(data);
    pAction->pOwner->app_.requestSetActiveVersion(pAction->target, pAction->hash);
}

void VersionsPage::onDelete(GtkButton *pButton, gpointer data) {
    auto *pAction = static_cast<RowAction *>(data);
    App *pApp = &pAction->pOwner->app_;
    const LaunchTarget target = pAction->target;
    const std::string hash = pAction->hash;
    // The row can be rebuilt while the dialog is open, so the dialog keeps its own copy of what to delete
    confirmDestructive(GTK_WIDGET(pButton), "Delete this version?",
                       hash + " will be removed from the virtual drive.", "Delete",
                       [pApp, target, hash] { pApp->requestDeleteVersion(target, hash); });
}

void VersionsPage::freeRowAction(gpointer data, GClosure *) {
    delete static_cast<RowAction *>(data);
}

void VersionsPage::update(const AppSnapshot &snap) {
    const VersionInstallProgress &progress = snap.versionInstall;
    const bool running = progress.phase != VersionInstallPhase::Idle && progress.phase != VersionInstallPhase::Done &&
                         progress.phase != VersionInstallPhase::Error;
    const bool forSelected = progress.target == selectedTarget();

    gtk_widget_set_visible(pProgressLabel_, running && forSelected);
    gtk_widget_set_visible(pProgress_, running && forSelected);
    const bool readOnlyChanged = snap.settings.autoUpdateRoblox != readOnly_;
    readOnly_ = snap.settings.autoUpdateRoblox;
    if (readOnlyChanged || !renderedOnce_) applyReadOnly(readOnly_);

    gtk_widget_set_sensitive(pInstallButton_, !running && !readOnly_);
    if (running && forSelected) {
        static const char *const PhaseLabels[] = {"", "Resolving version", "Fetching package manifest",
                                                  "Downloading packages", "Extracting", "", ""};
        setLabelText(GTK_LABEL(pProgressLabel_), PhaseLabels[static_cast<int>(progress.phase)]);
        gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(pProgress_), progress.fraction);
    }
    showError(pError_, progress.phase == VersionInstallPhase::Error && forSelected ? progress.errorMessage : "");

    // Only when the installed versions change, or a confirm dialog opened from a row would lose that row every tick
    if (renderedOnce_ && !readOnlyChanged && sameVersions(snap.versions.player, rendered_.player) &&
        sameVersions(snap.versions.studio, rendered_.studio)) {
        return;
    }
    rebuild(player_, snap.versions.player);
    rebuild(studio_, snap.versions.studio);
    rendered_ = snap.versions;
    renderedOnce_ = true;
}

} // namespace tuxblox
