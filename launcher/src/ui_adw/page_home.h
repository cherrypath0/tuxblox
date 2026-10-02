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

#include <cstddef>
#include <string>

namespace tuxblox {

class HomePage : public Page {
public:
    explicit HomePage(App &app);
    GtkWidget *widget() const override;
    void update(const AppSnapshot &snap) override;

private:
    struct Card {
        LaunchTarget target;
        GtkLabel *pMeta = nullptr;
        GtkLabel *pSessions = nullptr;
        GtkButton *pButton = nullptr;
        GtkLabel *pButtonLabel = nullptr;
        GtkWidget *pButtonIcon = nullptr;
        // Studio only: a second button, since a new session can start while others are open.
        GtkButton *pStopButton = nullptr;
        GtkLabel *pStopLabel = nullptr;
        // What was last applied, so the icon and the style classes are only touched when they change.
        std::string appliedLaunchLabel;
        bool appliedStops = false;
        bool appliedInstalled = false;
        bool applied = false;
    };

    GtkWidget *buildCard(Card &card, const char *pTitle, const unsigned char *pIcon, size_t iconLength);
    void setCard(Card &card, bool installed, const std::string &versionLabel, int sessions, bool stopping);
    void showUpdateButton(bool shown);
    void setUpdateState(const std::string &text, const std::string &styleClass);
    static void onLaunchClicked(GtkButton *pButton, gpointer data);
    static void onStopClicked(GtkButton *pButton, gpointer data);
    static void onUpdateClicked(GtkButton *, gpointer data);
    // Both buttons go quiet for a moment after a launch. With "Minimize to background" off the window
    // stays open, so without this a double click would start Roblox twice.
    static gboolean onLaunchCooldownOver(gpointer data);
    void beginLaunchCooldown();

    App &app_;
    GtkWidget *pRoot_ = nullptr;
    GtkLabel *pTitle_ = nullptr;
    GtkWidget *pSubtitle_ = nullptr;
    GtkWidget *pCards_ = nullptr;
    GtkWidget *pUpdateStatus_ = nullptr;
    GtkWidget *pUpdateBar_ = nullptr;
    GtkWidget *pError_ = nullptr;
    GtkLabel *pUpdateState_ = nullptr;
    GtkWidget *pUpdateButton_ = nullptr;
    GtkWidget *pUpdateDot_ = nullptr;
    std::string updateStateClass_;
    Card player_{LaunchTarget::Player};
    Card studio_{LaunchTarget::Studio};
};

} // namespace tuxblox
