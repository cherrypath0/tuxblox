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

#include "main_window.h"
#include "about_tab.h"
#include "icon_utils.h"
#include "message_box.h"
#include "settings_tab.h"
#include "sidebar.h"
#include "start_tab.h"
#include "update_popup.h"
#include "versions_tab.h"
#include <QHBoxLayout>
#include <QIcon>
#include <QResizeEvent>
#include <QStackedWidget>
#include <QTimer>
#include <QWidget>

namespace tuxblox {

MainWindow::MainWindow(App& app, QWidget* parent) : QMainWindow(parent), app_(app) {
    setWindowTitle("TuxBlox");
    // multiSizeWindowIcon(), not a plain QIcon(path) -- see its own doc
    // comment in icon_utils.h.
    setWindowIcon(multiSizeWindowIcon(":/branding/tuxblox_window_icon.png"));
    resize(760, 480);
    setMinimumSize(640, 420);

    auto* central = new QWidget(this);
    central->setObjectName("centralWidget");
    setCentralWidget(central);

    auto* layout = new QHBoxLayout(central);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    sidebar_ = new Sidebar(central);
    layout->addWidget(sidebar_);

    stack_ = new QStackedWidget(central);
    startTab_ = new StartTab(app_, stack_);
    settingsTab_ = new SettingsTab(app_, stack_);
    aboutTab_ = new AboutTab(stack_);
    versionsTab_ = new VersionsTab(app_, stack_);
    stack_->addWidget(startTab_);   // index 0
    stack_->addWidget(settingsTab_); // index 1
    stack_->addWidget(aboutTab_);   // index 2
    stack_->addWidget(versionsTab_); // index 3
    layout->addWidget(stack_, 1);

    connect(sidebar_, &Sidebar::tabSelected, this, [this](Tab tab) {
        app_.setActiveTab(tab);
    });

    pollTimer_ = new QTimer(this);
    connect(pollTimer_, &QTimer::timeout, this, &MainWindow::poll);
    pollTimer_->start(100);

    updatePopup_ = new UpdatePopup(central);
    connect(updatePopup_, &UpdatePopup::updateRequested, this, [this] { app_.requestUpdateNow(); });
    connect(updatePopup_, &UpdatePopup::dismissRequested, this, [this] { app_.dismissUpdateNotification(); });
}

void MainWindow::poll() {
    AppSnapshot snap = app_.snapshot();

    sidebar_->setActiveTab(snap.activeTab);
    switch (snap.activeTab) {
        case Tab::Start:    stack_->setCurrentWidget(startTab_); break;
        case Tab::Settings: stack_->setCurrentWidget(settingsTab_); break;
        case Tab::About:    stack_->setCurrentWidget(aboutTab_); break;
        case Tab::Versions: stack_->setCurrentWidget(versionsTab_); break;
    }

    startTab_->updateFromSnapshot(snap);
    settingsTab_->updateFromSnapshot(snap);
    versionsTab_->updateFromSnapshot(snap);
    updatePopup_->setAvailableVersion(snap.updateAvailableVersion);
    repositionUpdatePopup();

    // One-time startup notice, same "never cleared, shown-once" behavior as
    // ui.cpp's containerWarningShown_.
    if (!snap.containerWarning.empty() && !containerWarningShown_) {
        containerWarningShown_ = true;
        showWarningMessageBox("Distrobox GPU Passthrough", snap.containerWarning);
    }

    // App::requestLaunch() has spawned a detached watcher process and wants
    // this whole launcher to close now -- same as ui.cpp's
    // `if (app.shouldQuit()) shouldClose_ = true;`, but driving
    // QApplication::quit() instead of falling out of a manual loop.
    if (app_.shouldQuit() || app_.needsInstallerHandoff() || app_.needsUninstallHandoff()) {
        pollTimer_->stop();
        close();
    }
}

void MainWindow::resizeEvent(QResizeEvent* event) {
    QMainWindow::resizeEvent(event);
    repositionUpdatePopup();
}

void MainWindow::repositionUpdatePopup() {
    if (!updatePopup_ || !updatePopup_->isVisible()) return;
    const int margin = 16;
    updatePopup_->move(width() - updatePopup_->width() - margin,
                        height() - updatePopup_->height() - margin);
}

} // namespace tuxblox
