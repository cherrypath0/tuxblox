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

#pragma once
#include "page.h"

#include <string>
#include <vector>

// Third Parties
#include <adwaita.h>

namespace tuxblox {

class VersionsPage : public Page {
public:
    explicit VersionsPage(App &app);
    GtkWidget *widget() const override;
    void update(const AppSnapshot &snap) override;

private:
    struct List {
        LaunchTarget target;
        AdwPreferencesGroup *pGroup = nullptr;
        std::vector<GtkWidget *> rows;
    };

    struct RowAction {
        VersionsPage *pOwner;
        LaunchTarget target;
        std::string hash;
    };

    void buildInstallGroup();
    AdwPreferencesGroup *buildList(List &list, const char *pTitle);
    void rebuild(List &list, const AppVersions &versions);
    GtkWidget *versionRow(LaunchTarget target, const AppVersions &versions, const InstalledVersion &version);
    LaunchTarget selectedTarget() const;
    void connectRowAction(GtkWidget *pButton, GCallback onClicked, LaunchTarget target, const std::string &hash);

    static void onTargetChanged(GObject *, GParamSpec *, gpointer data);
    static void onChannelChanged(GtkEditable *pEditable, gpointer);
    static void onInstall(AdwButtonRow *, gpointer data);
    static void onPrevious(AdwButtonRow *, gpointer data);
    static void onSetActive(GtkButton *, gpointer data);
    static void onDelete(GtkButton *pButton, gpointer data);
    static void freeRowAction(gpointer data, GClosure *);

    App &app_;
    GtkWidget *pRoot_ = nullptr;
    AdwComboRow *pTarget_ = nullptr;
    GtkEditable *pChannel_ = nullptr;
    GtkEditable *pHash_ = nullptr;
    GtkWidget *pInstall_ = nullptr;
    GtkWidget *pPrevious_ = nullptr;
    GtkWidget *pProgressLabel_ = nullptr;
    GtkWidget *pProgress_ = nullptr;
    GtkWidget *pError_ = nullptr;
    List player_{LaunchTarget::Player};
    List studio_{LaunchTarget::Studio};
    VersionsManifest rendered_;
    bool renderedOnce_ = false;
};

} // namespace tuxblox
