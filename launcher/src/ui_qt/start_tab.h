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
#include <QWidget>

class QLabel;
class QProgressBar;
class QPushButton;

namespace tuxblox {

// Logo, version footer, and either an update-progress indicator or the two
// launch buttons -- port of ui.cpp's renderStartTab(). Holds a reference to
// App (not a copy) since it calls App::requestLaunch() directly from the
// launch buttons' clicked() handlers, same division of responsibility the
// ImGui version used (render functions took App& and called into it
// directly on click).
class StartTab : public QWidget {
    Q_OBJECT
public:
    explicit StartTab(App& app, QWidget* parent = nullptr);

    void updateFromSnapshot(const AppSnapshot& snap);

private:
    App& app_;
    QLabel* logo_ = nullptr;
    QLabel* updateStatusLabel_ = nullptr;
    QProgressBar* updateProgress_ = nullptr;
    QLabel* errorBanner_ = nullptr;
    QPushButton* playerButton_ = nullptr;
    QPushButton* studioButton_ = nullptr;
    QLabel* footer_ = nullptr;
};

} // namespace tuxblox
