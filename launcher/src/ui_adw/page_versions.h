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

    // The install sheet while it is open, freed when it closes.
    struct InstallSheet {
        VersionsPage *pOwner = nullptr;
        AdwDialog *pDialog = nullptr;
        GtkWidget *pLatest = nullptr;
        GtkWidget *pPrevious = nullptr;
        GtkWidget *pNext = nullptr;
        GtkWidget *pCustom = nullptr;
        GtkEditable *pChannel = nullptr;
        GtkEditable *pHash = nullptr;
        GtkWidget *pHashBox = nullptr;
        GtkWidget *pChannelBox = nullptr;
        GtkWidget *pNextWarning = nullptr;
    };

    void applyReadOnly(bool readOnly);
    GtkWidget *buildToolbar();
    void buildProgressGroup();
    AdwPreferencesGroup *buildList(List &list, const char *pTitle);
    void rebuild(List &list, const AppVersions &versions);
    GtkWidget *versionRow(LaunchTarget target, const AppVersions &versions, const InstalledVersion &version);
    LaunchTarget selectedTarget() const;
    // Which list is on screen.
    void applyActive();
    void openInstallSheet();
    void connectRowAction(GtkWidget *pButton, GCallback onClicked, LaunchTarget target, const std::string &hash);

    static void onTargetToggled(GtkToggleButton *pButton, gpointer data);
    static void onInstallPressed(GtkButton *, gpointer data);
    static void onSheetModeToggled(GtkToggleButton *pButton, gpointer data);
    static VersionSelectMode sheetMode(const InstallSheet *pSheet);
    static void onSheetInstall(GtkButton *, gpointer data);
    static void onSheetCancel(GtkButton *, gpointer data);
    static void onSheetClosed(AdwDialog *, gpointer data);
    static void onSetActive(GtkButton *, gpointer data);
    static void onDelete(GtkButton *pButton, gpointer data);
    static void freeRowAction(gpointer data, GClosure *);

    App &app_;
    GtkWidget *pRoot_ = nullptr;
    GtkWidget *pPlayerToggle_ = nullptr;
    GtkWidget *pStudioToggle_ = nullptr;
    GtkWidget *pInstallButton_ = nullptr;
    GtkWidget *pProgressLabel_ = nullptr;
    GtkWidget *pProgress_ = nullptr;
    GtkWidget *pError_ = nullptr;
    LaunchTarget active_ = LaunchTarget::Player;
    // Remembered between sheet openings, so a channel typed once is still there next time.
    std::string channel_ = "live";
    List player_{LaunchTarget::Player};
    List studio_{LaunchTarget::Studio};
    VersionsManifest rendered_;
    bool readOnly_ = false;
    bool renderedOnce_ = false;
};

} // namespace tuxblox
