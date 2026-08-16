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

#include "versions_tab.h"
#include "danger_button.h"
#include "icon_utils.h"
#include "versions_tab_state.h"
#include <QComboBox>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QProgressBar>
#include <QPushButton>
#include <QVBoxLayout>

namespace tuxblox {

namespace {
LaunchTarget selectedTarget(const QComboBox* box) {
    return box->currentIndex() == 0 ? LaunchTarget::Player : LaunchTarget::Studio;
}

// Coarse-grained "did anything about this app type's installed-versions
// data change" check -- not a per-card diff. Used to decide whether
// rebuildCardList() needs to run at all; see the comment on
// VersionsTab::lastRenderedVersions_ for why that matters.
bool appVersionsEqual(const AppVersions& a, const AppVersions& b) {
    if (a.activeHash != b.activeHash) return false;
    if (a.installed.size() != b.installed.size()) return false;
    for (size_t i = 0; i < a.installed.size(); ++i) {
        const auto& x = a.installed[i];
        const auto& y = b.installed[i];
        if (x.hash != y.hash || x.channel != y.channel || x.installedAt != y.installedAt) return false;
    }
    return true;
}
} // namespace

VersionsTab::VersionsTab(App& app, QWidget* parent) : QWidget(parent), app_(app) {
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(24, 20, 24, 16);
    layout->setSpacing(12);

    // Two rows, not one -- six controls (combo, channel field, two quick-
    // download buttons, hash field, manual-download button) crammed into a
    // single QHBoxLayout don't fit this window's minimum width (640px):
    // Qt's layout engine has no wrapping/eliding fallback, so once the row's
    // total minimum-size-hint width exceeds what's available it just
    // compresses everything below its natural size, visibly overlapping and
    // truncating button text. Splitting into a "quick actions" row and a
    // "manual hash" row gives every control room to render at its natural
    // size at both the minimum and default window width.
    auto* quickRow = new QHBoxLayout();
    quickRow->setSpacing(8);

    targetSelect_ = new QComboBox(this);
    targetSelect_->addItem("Player");
    targetSelect_->addItem("Studio");
    quickRow->addWidget(targetSelect_);

    channelField_ = new QLineEdit("live", this);
    channelField_->setPlaceholderText("channel (default: live)");
    channelField_->setMaximumWidth(140);
    quickRow->addWidget(channelField_);

    latestButton_ = new QPushButton(paddedIcon(":/icons/download.png", 18, 6), "Latest", this);
    latestButton_->setObjectName("toolbarButton");
    latestButton_->setIconSize(iconSizeWithGap(18, 6));
    connect(latestButton_, &QPushButton::clicked, this, &VersionsTab::onDownloadLatest);
    quickRow->addWidget(latestButton_);

    previousButton_ = new QPushButton(paddedIcon(":/icons/download.png", 18, 6), "Previous", this);
    previousButton_->setObjectName("toolbarButton");
    previousButton_->setIconSize(iconSizeWithGap(18, 6));
    connect(previousButton_, &QPushButton::clicked, this, &VersionsTab::onDownloadPrevious);
    quickRow->addWidget(previousButton_);

    quickRow->addStretch(1);
    layout->addLayout(quickRow);

    auto* manualRow = new QHBoxLayout();
    manualRow->setSpacing(8);

    hashField_ = new QLineEdit(this);
    hashField_->setPlaceholderText("version-... (manual hash)");
    manualRow->addWidget(hashField_, 1);

    manualButton_ = new QPushButton(paddedIcon(":/icons/download.png", 18, 6), "Download", this);
    manualButton_->setObjectName("toolbarButton");
    manualButton_->setIconSize(iconSizeWithGap(18, 6));
    connect(manualButton_, &QPushButton::clicked, this, &VersionsTab::onDownloadManualHash);
    manualRow->addWidget(manualButton_);

    layout->addLayout(manualRow);

    progressLabel_ = new QLabel(this);
    progressLabel_->hide();
    layout->addWidget(progressLabel_);

    progressBar_ = new QProgressBar(this);
    progressBar_->setRange(0, 100);
    progressBar_->hide();
    layout->addWidget(progressBar_);

    errorBanner_ = new QLabel(this);
    errorBanner_->setObjectName("errorBanner");
    errorBanner_->setWordWrap(true);
    errorBanner_->hide();
    layout->addWidget(errorBanner_);

    cardListLayout_ = new QVBoxLayout();
    cardListLayout_->setSpacing(8);
    layout->addLayout(cardListLayout_);
    layout->addStretch(1);
}

void VersionsTab::onDownloadLatest() {
    app_.requestInstallVersion(selectedTarget(targetSelect_), VersionSelectMode::Latest,
                                channelField_->text().toStdString());
}

void VersionsTab::onDownloadPrevious() {
    app_.requestInstallVersion(selectedTarget(targetSelect_), VersionSelectMode::Previous,
                                channelField_->text().toStdString());
}

void VersionsTab::onDownloadManualHash() {
    app_.requestInstallVersion(selectedTarget(targetSelect_), VersionSelectMode::ManualHash,
                                channelField_->text().toStdString(), hashField_->text().toStdString());
}

void VersionsTab::updateFromSnapshot(const AppSnapshot& snap) {
    const auto& p = snap.versionInstall;
    // Only one install can run at a time system-wide (App enforces this via
    // versionInstall.phase, regardless of target), so button enablement
    // stays keyed off "is any install running" -- but the progress bar and
    // error banner are per-target UI on this tab, and must only reflect an
    // install for the target currently selected in targetSelect_ (Finding
    // 8, 2026-08-16 final review). Without this, e.g. a Studio download's
    // progress/error would render while the user has Player selected.
    const bool anyInstallRunning = p.phase != VersionInstallPhase::Idle &&
                                    p.phase != VersionInstallPhase::Done &&
                                    p.phase != VersionInstallPhase::Error;
    const bool isSelectedTarget = p.target == selectedTarget(targetSelect_);
    const bool activeForSelectedTarget = anyInstallRunning && isSelectedTarget;

    progressLabel_->setVisible(activeForSelectedTarget);
    progressBar_->setVisible(activeForSelectedTarget);
    latestButton_->setEnabled(!anyInstallRunning);
    previousButton_->setEnabled(!anyInstallRunning);
    manualButton_->setEnabled(!anyInstallRunning);

    if (activeForSelectedTarget) {
        static const char* kPhaseLabels[] = {"", "Resolving version", "Fetching package manifest",
                                              "Downloading packages", "Extracting", "", ""};
        progressLabel_->setText(kPhaseLabels[static_cast<int>(p.phase)]);
        progressBar_->setValue(static_cast<int>(p.fraction * 100));
    }

    if (p.phase == VersionInstallPhase::Error && isSelectedTarget) {
        errorBanner_->setText(QString::fromStdString(p.errorMessage));
        errorBanner_->show();
    } else {
        errorBanner_->hide();
    }

    // Skip the rebuild entirely when nothing in the underlying data changed
    // since the last render. This tab's updateFromSnapshot() is driven by
    // MainWindow's poll loop (every 100ms) -- rebuilding on every no-op tick
    // would tear down and recreate every card's DangerButton on each poll,
    // collapsing its 5-second two-click confirm window to under 100ms.
    if (!cardListRendered_ || !appVersionsEqual(snap.versions.player, lastRenderedVersions_.player) ||
        !appVersionsEqual(snap.versions.studio, lastRenderedVersions_.studio)) {
        rebuildCardList(snap.versions);
        lastRenderedVersions_ = snap.versions;
        cardListRendered_ = true;
    }
}

void VersionsTab::rebuildCardList(const VersionsManifest& versions) {
    QLayoutItem* item;
    while ((item = cardListLayout_->takeAt(0)) != nullptr) {
        delete item->widget();
        delete item;
    }

    auto addAppCards = [&](LaunchTarget target, const AppVersions& av, const char* label) {
        auto* header = new QLabel(label, this);
        header->setStyleSheet("font-weight: 600;");
        cardListLayout_->addWidget(header);

        for (const auto& v : av.installed) {
            auto* card = new QFrame(this);
            card->setObjectName("versionCard");
            auto* row = new QHBoxLayout(card);

            auto* info = new QLabel(
                QString("%1 %2").arg(QString::fromStdString(v.hash), QString::fromStdString(v.channel)), card);
            row->addWidget(info, 1);

            if (v.hash == av.activeHash) {
                auto* activeLabel = new QLabel("Active", card);
                row->addWidget(activeLabel);
            } else {
                auto* pinButton = new QPushButton("Set Active", card);
                pinButton->setObjectName("toolbarButton");
                connect(pinButton, &QPushButton::clicked, this,
                        [this, target, hash = v.hash] { app_.requestSetActiveVersion(target, hash); });
                row->addWidget(pinButton);
            }

            auto* del = new DangerButton("Delete", "Confirm Delete?", "Deleting...", card);
            del->setEnabled(canDeleteVersion(av, v.hash));
            connect(del, &DangerButton::confirmed, this,
                    [this, target, hash = v.hash] { app_.requestDeleteVersion(target, hash); });
            row->addWidget(del);

            cardListLayout_->addWidget(card);
        }
    };

    addAppCards(LaunchTarget::Player, versions.player, "Player");
    addAppCards(LaunchTarget::Studio, versions.studio, "Studio");
}

} // namespace tuxblox
