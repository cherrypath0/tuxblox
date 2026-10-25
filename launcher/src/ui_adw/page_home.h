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
#include <vector>

namespace tuxblox {

class HomePage : public Page {
public:
    HomePage(App &app, std::string installDir);
    GtkWidget *widget() const override;
    void update(const AppSnapshot &snap) override;

private:
    struct Card {
        LaunchTarget target;
        GtkLabel *pMeta = nullptr;
        GtkButton *pButton = nullptr;
        GtkLabel *pButtonLabel = nullptr;
        GtkWidget *pButtonIcon = nullptr;
        // What was last applied, so the icon and the style classes are only touched when they change.
        std::string appliedLaunchLabel;
        bool appliedStops = false;
        bool appliedInstalled = false;
        bool applied = false;
    };

    GtkWidget *buildCard(Card &card, const char *pTitle, const unsigned char *pIcon, size_t iconLength);
    void setCard(Card &card, bool installed, const std::string &versionLabel, int sessions, bool stopping, bool launchPending);
    GtkWidget *buildSessions();
    void setSessions(const std::vector<Session> &sessions);
    void showUpdateButton(bool shown);
    void setUpdateState(const std::string &text, const std::string &styleClass);
    static void onLaunchClicked(GtkButton *pButton, gpointer data);
    static void onOpenSessionClicked(GtkButton *pButton, gpointer data);
    static void onUpdateClicked(GtkButton *, gpointer data);
    // Both buttons go quiet for a moment after a launch. With "Minimize to background" off the window
    // stays open, so without this a double click would start Roblox twice.
    static gboolean onLaunchCooldownOver(gpointer data);
    void beginLaunchCooldown();

    App &app_;
    GtkWidget *pRoot_ = nullptr;
    GtkLabel *pTitle_ = nullptr;
    GtkWidget *pCards_ = nullptr;
    GtkWidget *pSessions_ = nullptr;
    GtkWidget *pSessionRows_ = nullptr;
    GtkWidget *pSessionEmpty_ = nullptr;
    // One per row shown, in the order they are shown. Only the ages and sizes change on a poll, so the rows themselves are built again only when a session starts or ends.
    std::vector<GtkLabel *> pSessionMetas_;
    std::vector<int> sessionShape_;
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
