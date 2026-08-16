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
#include <QMainWindow>

class QStackedWidget;
class QTimer;
class QResizeEvent;

namespace tuxblox {

class Sidebar;
class StartTab;
class SettingsTab;
class AboutTab;
class VersionsTab;
class UpdatePopup;

// Replaces Ui/ui.cpp entirely. Owns the QTimer that polls App::snapshot()
// -- the direct equivalent of ui.cpp's renderFrame() being called once per
// SDL frame, just event-driven instead of a manual render loop. main.cpp
// constructs one instance, calls show(), then hands control to
// QApplication::exec() (see Task 13) instead of ui.cpp's old `while
// (running) ui.renderFrame(app)` loop.
class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(App& app, QWidget* parent = nullptr);

protected:
    void resizeEvent(QResizeEvent* event) override;

private slots:
    void poll();

private:
    void repositionUpdatePopup();

    App& app_;
    Sidebar* sidebar_ = nullptr;
    QStackedWidget* stack_ = nullptr;
    StartTab* startTab_ = nullptr;
    SettingsTab* settingsTab_ = nullptr;
    AboutTab* aboutTab_ = nullptr;
    VersionsTab* versionsTab_ = nullptr;
    UpdatePopup* updatePopup_ = nullptr;
    QTimer* pollTimer_ = nullptr;
    bool containerWarningShown_ = false;
};

} // namespace tuxblox
