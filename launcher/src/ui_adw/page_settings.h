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
#include "system_info.h"

#include <memory>
#include <vector>

// Third Parties
#include <adwaita.h>

namespace tuxblox {

class SettingsPage : public Page {
public:
    explicit SettingsPage(App &app);
    GtkWidget *widget() const override;
    void update(const AppSnapshot &snap) override;

private:
    struct Toggle {
        SettingsPage *pOwner;
        bool Settings::*pField;
        AdwSwitchRow *pRow;
    };

    AdwPreferencesGroup *addGroup(const char *pTitle);
    void addToggle(AdwPreferencesGroup *pGroup, const char *pTitle, const char *pSubtitle, bool Settings::*pField);
    void buildTheme(AdwPreferencesGroup *pGroup);
    void buildUpdates();
    void buildEnvironment();
    void buildDangerZone();
    void seed(const Settings &settings);
    void commitEnvironmentVariables();
    GtkButton *dangerButton(GtkWidget *pRow, const char *pLabel, GCallback onClicked);

    static void onToggle(GObject *pRow, GParamSpec *, gpointer data);
    static void onChannel(GObject *pRow, GParamSpec *, gpointer data);
    static void onTheme(GObject *pRow, GParamSpec *, gpointer data);
    static void onGpu(GObject *pRow, GParamSpec *, gpointer data);
    static void onEnvironmentActivate(GtkEntry *, gpointer data);
    static void onEnvironmentFocusLeft(GtkEventControllerFocus *, gpointer data);
    static void onTerminate(GtkButton *pButton, gpointer data);
    static gboolean onTerminateReset(gpointer data);
    static void onWipe(GtkButton *pButton, gpointer data);
    static void onUninstall(GtkButton *pButton, gpointer data);

    App &app_;
    GtkWidget *pRoot_ = nullptr;
    std::vector<std::unique_ptr<Toggle>> toggles_;
    // Read once when the page is built: the list cannot change without a reboot or a hotplug, neither of which happens while the window is open
    std::vector<GpuDevice> gpus_;
    AdwComboRow *pTheme_ = nullptr;
    AdwComboRow *pChannel_ = nullptr;
    AdwComboRow *pGpu_ = nullptr;
    GtkEntry *pEnvironment_ = nullptr;
    GtkButton *pTerminate_ = nullptr;
    // True from a Terminate click until its result has been shown on the button.
    bool terminateAwaited_ = false;
    GtkButton *pWipe_ = nullptr;
    GtkWidget *pWipeError_ = nullptr;
    GtkButton *pUninstall_ = nullptr;
    GtkWidget *pUninstallError_ = nullptr;
    bool seeded_ = false;
    bool seeding_ = false;
};

} // namespace tuxblox
