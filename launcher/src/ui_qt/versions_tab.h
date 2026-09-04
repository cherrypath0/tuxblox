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

class QComboBox;
class QLineEdit;
class QPushButton;
class QLabel;
class QProgressBar;
class QVBoxLayout;

namespace tuxblox {

class BoxedGroup;

// An install bar (target, channel, hash, Install/Previous) above one group
// of rows per app type listing what is already installed.
//
// The hash field doubles as the version selector: leave it blank to take
// the latest build on the chosen channel, or paste an exact
// "version-..." hash to install that one.
class VersionsTab : public QWidget {
    Q_OBJECT
public:
    explicit VersionsTab(App& app, QWidget* parent = nullptr);

    void updateFromSnapshot(const AppSnapshot& snap);

private:
    QWidget* buildInstallBar(QWidget* parent);
    void onInstall();
    void onDownloadPrevious();
    void refreshProgressVisibility();
    void rebuildVersionList(LaunchTarget target, const AppVersions& versions, BoxedGroup* group);

    App& app_;
    QComboBox* targetSelect_ = nullptr;   // Player / Studio
    QLineEdit* channelField_ = nullptr;   // defaults to "live"
    QLineEdit* hashField_ = nullptr;      // blank means "latest"
    QPushButton* latestButton_ = nullptr;
    QPushButton* previousButton_ = nullptr;
    QLabel* progressLabel_ = nullptr;
    QProgressBar* progressBar_ = nullptr;
    QLabel* errorBanner_ = nullptr;
    BoxedGroup* playerGroup_ = nullptr;
    BoxedGroup* studioGroup_ = nullptr;

    // What rebuildVersionList() last rendered from, so updateFromSnapshot()
    // can skip the rebuild (and the live widget state loss that comes with
    // it -- e.g. an armed DangerButton mid-confirm) when nothing about the
    // installed-versions data actually changed since the last poll tick.
    VersionsManifest lastRenderedVersions_;
    bool listRendered_ = false;
};

} // namespace tuxblox
