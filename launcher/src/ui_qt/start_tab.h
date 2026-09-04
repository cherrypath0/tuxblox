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
#include "app.h"
#include <QString>
#include <QWidget>

class QLabel;
class QProgressBar;

namespace tuxblox {

class AppCard;

// The Home tab: a heading, one card per Roblox app showing what is
// installed and offering to start it, and a status strip along the bottom.
//
// While an update is downloading the cards are hidden and a progress bar
// takes their place, so the window never offers a launch that is about to
// be interrupted. Holds a reference to App (not a copy) since the cards
// call App::requestLaunch() directly.
class StartTab : public QWidget {
    Q_OBJECT
public:
    explicit StartTab(App& app, QWidget* parent = nullptr);

    void updateFromSnapshot(const AppSnapshot& snap);

private:
    QWidget* buildStatusStrip();
    void setUpdateState(const QString& state, const QString& text);

    App& app_;
    QLabel* title_ = nullptr;
    QLabel* subtitle_ = nullptr;
    QWidget* cardRow_ = nullptr;
    AppCard* playerCard_ = nullptr;
    AppCard* studioCard_ = nullptr;
    QLabel* updateStatusLabel_ = nullptr;
    QProgressBar* updateProgress_ = nullptr;
    QLabel* errorBanner_ = nullptr;
    QLabel* channelLabel_ = nullptr;
    QLabel* updateStateDot_ = nullptr;
    QLabel* updateStateLabel_ = nullptr;
};

} // namespace tuxblox
