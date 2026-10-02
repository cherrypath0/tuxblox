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

std::string describe(const InstalledVersion &version) {
    std::string text = version.versionString;
    if (!version.channel.empty()) {
        if (!text.empty()) text += " \xC2\xB7 ";
        text += version.channel;
    }
    if (!version.installedAt.empty()) {
        if (!text.empty()) text += " \xC2\xB7 ";
        text += "installed " + version.installedAt.substr(0, 10);
    }
    return text;
}

std::string trimmed(const std::string &text) {
    const size_t first = text.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) return "";
    const size_t last = text.find_last_not_of(" \t\r\n");
    return text.substr(first, last - first + 1);
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
    adw_preferences_page_set_description(ADW_PREFERENCES_PAGE(pRoot_), "Manage Roblox versions here");
    buildInstallGroup();
    adw_preferences_page_add(ADW_PREFERENCES_PAGE(pRoot_), buildList(player_, "Player"));
    adw_preferences_page_add(ADW_PREFERENCES_PAGE(pRoot_), buildList(studio_, "Studio"));
}

GtkWidget *VersionsPage::widget() const {
    return pRoot_;
}

void VersionsPage::buildInstallGroup() {
    GtkWidget *pGroup = adw_preferences_group_new();
    adw_preferences_group_set_title(ADW_PREFERENCES_GROUP(pGroup), "Install a version");

    GtkWidget *pTarget = adw_combo_row_new();
    adw_preferences_row_set_title(ADW_PREFERENCES_ROW(pTarget), "App");
    const char *const targets[] = {"Player", "Studio", nullptr};
    GtkStringList *pTargets = gtk_string_list_new(targets);
    adw_combo_row_set_model(ADW_COMBO_ROW(pTarget), G_LIST_MODEL(pTargets));
    g_object_unref(pTargets);
    pTarget_ = ADW_COMBO_ROW(pTarget);
    g_signal_connect(pTarget, "notify::selected", G_CALLBACK(onTargetChanged), this);
    adw_preferences_group_add(ADW_PREFERENCES_GROUP(pGroup), pTarget);

    GtkWidget *pChannel = entryRow("Channel", "live");
    pChannel_ = GTK_EDITABLE(pChannel);
    g_signal_connect(pChannel, "changed", G_CALLBACK(onChannelChanged), nullptr);
    adw_preferences_group_add(ADW_PREFERENCES_GROUP(pGroup), pChannel);

    GtkWidget *pHash = entryRow("Version hash (leave blank for latest)", "");
    pHash_ = GTK_EDITABLE(pHash);
    adw_preferences_group_add(ADW_PREFERENCES_GROUP(pGroup), pHash);

    pInstall_ = adw_button_row_new();
    adw_preferences_row_set_title(ADW_PREFERENCES_ROW(pInstall_), "Install");
    gtk_widget_add_css_class(pInstall_, "suggested-action");
    g_signal_connect(pInstall_, "activated", G_CALLBACK(onInstall), this);
    adw_preferences_group_add(ADW_PREFERENCES_GROUP(pGroup), pInstall_);

    pPrevious_ = adw_button_row_new();
    adw_preferences_row_set_title(ADW_PREFERENCES_ROW(pPrevious_), "Install previous version");
    g_signal_connect(pPrevious_, "activated", G_CALLBACK(onPrevious), this);
    adw_preferences_group_add(ADW_PREFERENCES_GROUP(pGroup), pPrevious_);

    // Not rows, so the group places them below its list
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
    adw_preferences_group_set_title(ADW_PREFERENCES_GROUP(pGroup), pTitle);
    list.pGroup = ADW_PREFERENCES_GROUP(pGroup);
    return list.pGroup;
}

LaunchTarget VersionsPage::selectedTarget() const {
    return adw_combo_row_get_selected(pTarget_) == 1 ? LaunchTarget::Studio : LaunchTarget::Player;
}

void VersionsPage::connectRowAction(GtkWidget *pButton, GCallback onClicked, LaunchTarget target, const std::string &hash) {
    // Freed with the button, so a rebuilt list leaves nothing behind
    g_signal_connect_data(pButton, "clicked", onClicked, new RowAction{this, target, hash}, freeRowAction,
                          static_cast<GConnectFlags>(0));
}

GtkWidget *VersionsPage::versionRow(LaunchTarget target, const AppVersions &versions, const InstalledVersion &version) {
    GtkWidget *pRow = plainActionRow(version.hash, describe(version));
    gtk_widget_add_css_class(pRow, "monospace");

    if (version.hash == versions.activeHash) {
        GtkWidget *pActive = gtk_label_new("Active");
        gtk_widget_add_css_class(pActive, "accent");
        gtk_widget_add_css_class(pActive, "caption-heading");
        gtk_widget_set_valign(pActive, GTK_ALIGN_CENTER);
        adw_action_row_add_suffix(ADW_ACTION_ROW(pRow), pActive);
    } else {
        GtkWidget *pSetActive = gtk_button_new_with_label("Set active");
        gtk_widget_set_valign(pSetActive, GTK_ALIGN_CENTER);
        connectRowAction(pSetActive, G_CALLBACK(onSetActive), target, version.hash);
        adw_action_row_add_suffix(ADW_ACTION_ROW(pRow), pSetActive);
    }

    GtkWidget *pDelete = gtk_button_new_with_label("Delete");
    gtk_widget_add_css_class(pDelete, "destructive-action");
    gtk_widget_set_valign(pDelete, GTK_ALIGN_CENTER);
    gtk_widget_set_sensitive(pDelete, canDeleteVersion(versions, version.hash));
    connectRowAction(pDelete, G_CALLBACK(onDelete), target, version.hash);
    adw_action_row_add_suffix(ADW_ACTION_ROW(pRow), pDelete);
    return pRow;
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

void VersionsPage::onTargetChanged(GObject *, GParamSpec *, gpointer data) {
    auto *pSelf = static_cast<VersionsPage *>(data);
    pSelf->update(pSelf->app_.snapshot());
}

void VersionsPage::onChannelChanged(GtkEditable *pEditable, gpointer) {
    const std::string typed = gtk_editable_get_text(pEditable);
    std::string folded = typed;
    for (char &letter : folded) {
        if (letter >= 'A' && letter <= 'Z') letter = static_cast<char>(letter - 'A' + 'a');
    }
    if (folded == typed) return;
    // Setting the text moves the cursor to the start, so it is put back where the user was typing
    const int position = gtk_editable_get_position(pEditable);
    gtk_editable_set_text(pEditable, folded.c_str());
    gtk_editable_set_position(pEditable, position);
}

void VersionsPage::onInstall(AdwButtonRow *, gpointer data) {
    auto *pSelf = static_cast<VersionsPage *>(data);
    const std::string channel = normalizedChannel(gtk_editable_get_text(pSelf->pChannel_));
    const std::string hash = trimmed(gtk_editable_get_text(pSelf->pHash_));
    if (hash.empty()) {
        pSelf->app_.requestInstallVersion(pSelf->selectedTarget(), VersionSelectMode::Latest, channel);
        return;
    }
    pSelf->app_.requestInstallVersion(pSelf->selectedTarget(), VersionSelectMode::ManualHash, channel, hash);
}

void VersionsPage::onPrevious(AdwButtonRow *, gpointer data) {
    auto *pSelf = static_cast<VersionsPage *>(data);
    pSelf->app_.requestInstallVersion(pSelf->selectedTarget(), VersionSelectMode::Previous,
                                      normalizedChannel(gtk_editable_get_text(pSelf->pChannel_)));
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
    gtk_widget_set_sensitive(pInstall_, !running);
    gtk_widget_set_sensitive(pPrevious_, !running);
    if (running && forSelected) {
        static const char *const PhaseLabels[] = {"", "Resolving version", "Fetching package manifest",
                                                  "Downloading packages", "Extracting", "", ""};
        setLabelText(GTK_LABEL(pProgressLabel_), PhaseLabels[static_cast<int>(progress.phase)]);
        gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(pProgress_), progress.fraction);
    }
    showError(pError_, progress.phase == VersionInstallPhase::Error && forSelected ? progress.errorMessage : "");

    // Only when the installed versions change, or a confirm dialog opened from a row would lose that row every tick
    if (renderedOnce_ && sameVersions(snap.versions.player, rendered_.player) &&
        sameVersions(snap.versions.studio, rendered_.studio)) {
        return;
    }
    rebuild(player_, snap.versions.player);
    rebuild(studio_, snap.versions.studio);
    rendered_ = snap.versions;
    renderedOnce_ = true;
}

} // namespace tuxblox
