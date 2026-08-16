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

// Toolbar (target/channel/hash + Download Latest/Previous) plus a
// per-app-type card list of installed versions. Port of the spec's
// "Versions Tab — Downloader Mechanics" UI section.
class VersionsTab : public QWidget {
    Q_OBJECT
public:
    explicit VersionsTab(App& app, QWidget* parent = nullptr);

    void updateFromSnapshot(const AppSnapshot& snap);

private:
    void onDownloadLatest();
    void onDownloadPrevious();
    void onDownloadManualHash();
    void rebuildCardList(const VersionsManifest& versions);

    App& app_;
    QComboBox* targetSelect_ = nullptr;   // Player / Studio
    QLineEdit* channelField_ = nullptr;   // defaults to "live"
    QLineEdit* hashField_ = nullptr;      // manual hash entry
    QPushButton* latestButton_ = nullptr;
    QPushButton* previousButton_ = nullptr;
    QPushButton* manualButton_ = nullptr;
    QLabel* progressLabel_ = nullptr;
    QProgressBar* progressBar_ = nullptr;
    QLabel* errorBanner_ = nullptr;
    QVBoxLayout* cardListLayout_ = nullptr;

    // What rebuildCardList() last rendered from, so updateFromSnapshot() can
    // skip the rebuild (and the live widget state loss that comes with it --
    // e.g. an armed DangerButton mid-confirm) when nothing about the
    // installed-versions data actually changed since the last poll tick.
    VersionsManifest lastRenderedVersions_;
    bool cardListRendered_ = false;
};

} // namespace tuxblox
