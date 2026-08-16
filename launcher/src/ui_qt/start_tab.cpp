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

#include "start_tab.h"
#include "icon_utils.h"
#include "version.h"
#include <QHBoxLayout>
#include <QIcon>
#include <QLabel>
#include <QProgressBar>
#include <QPushButton>
#include <QVBoxLayout>

namespace tuxblox {

namespace {
const char* updatePhaseLabel(UpdatePhase phase) {
    switch (phase) {
        case UpdatePhase::CheckingManifest: return "Checking for updates";
        case UpdatePhase::PreparingUpdater: return "Preparing updater";
        case UpdatePhase::Error:            return "Update check failed";
        default:                            return "";
    }
}
} // namespace

StartTab::StartTab(App& app, QWidget* parent) : QWidget(parent), app_(app) {
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(24, 20, 24, 16);
    layout->setSpacing(16);

    logo_ = new QLabel(this);
    logo_->setPixmap(QIcon(":/branding/tuxblox_logo.png").pixmap(72, 72));
    logo_->setAlignment(Qt::AlignHCenter);
    layout->addWidget(logo_);

    updateStatusLabel_ = new QLabel(this);
    updateStatusLabel_->setAlignment(Qt::AlignHCenter);
    updateStatusLabel_->setStyleSheet("font-weight: 600;");
    layout->addWidget(updateStatusLabel_);

    updateProgress_ = new QProgressBar(this);
    updateProgress_->setRange(0, 100);
    updateProgress_->setTextVisible(false);
    layout->addWidget(updateProgress_);

    errorBanner_ = new QLabel(this);
    errorBanner_->setObjectName("errorBanner");
    errorBanner_->setWordWrap(true);
    errorBanner_->hide();
    layout->addWidget(errorBanner_);

    auto* buttonRow = new QHBoxLayout();
    buttonRow->setSpacing(16);

    playerButton_ = new QPushButton(paddedIcon(":/icons/roblox-player.png", 28, 8), "Launch Player", this);
    playerButton_->setObjectName("launchButton");
    playerButton_->setIconSize(iconSizeWithGap(28, 8));
    playerButton_->setMinimumHeight(48);
    connect(playerButton_, &QPushButton::clicked, this, [this] {
        // requestLaunch() itself already falls through to the existing
        // official-installer bootstrap when nothing is installed yet (see
        // process_launcher.cpp's resolveOrBootstrapExePath) -- this handler
        // doesn't need its own separate "install first" branch. The
        // "Install & Launch" label above is purely informational for cases
        // where that fallback is about to kick in.
        app_.requestLaunch(LaunchTarget::Player);
    });
    buttonRow->addWidget(playerButton_);

    studioButton_ = new QPushButton(paddedIcon(":/icons/roblox-studio.png", 28, 8), "Launch Studio", this);
    studioButton_->setObjectName("launchButton");
    studioButton_->setIconSize(iconSizeWithGap(28, 8));
    studioButton_->setMinimumHeight(48);
    connect(studioButton_, &QPushButton::clicked, this, [this] {
        // See playerButton_'s connect() above -- same reasoning applies.
        app_.requestLaunch(LaunchTarget::Studio);
    });
    buttonRow->addWidget(studioButton_);

    layout->addLayout(buttonRow);
    layout->addStretch(1);

    footer_ = new QLabel(this);
    footer_->setObjectName("footer");
    layout->addWidget(footer_);

    footer_->setText(QString("TuxBlox v%1").arg(kTuxBloxVersion));
}

void StartTab::updateFromSnapshot(const AppSnapshot& snap) {
    const bool playerNeedsInstall = snap.versions.player.activeHash.empty();
    const bool studioNeedsInstall = snap.versions.studio.activeHash.empty();
    // "&&" not "&" -- QPushButton::setText() treats a single '&' as a
    // keyboard-mnemonic marker (the following character gets underlined and
    // the '&' itself is dropped from display), not literal text. A plain
    // "Install & Launch" rendered as "Install _aunch" with the L consumed as
    // the mnemonic character instead of showing an ampersand at all.
    playerButton_->setText(playerNeedsInstall ? "Install && Launch Player" : "Launch Player");
    studioButton_->setText(studioNeedsInstall ? "Install && Launch Studio" : "Launch Studio");

    const bool updating = snap.update.phase == UpdatePhase::CheckingManifest ||
                           snap.update.phase == UpdatePhase::PreparingUpdater;

    updateStatusLabel_->setVisible(updating);
    updateProgress_->setVisible(updating);
    playerButton_->setVisible(!updating);
    studioButton_->setVisible(!updating);

    if (updating) {
        updateStatusLabel_->setText(updatePhaseLabel(snap.update.phase));
        updateProgress_->setValue(static_cast<int>(snap.update.fraction * 100));
        errorBanner_->hide();
        return;
    }

    if (snap.update.phase == UpdatePhase::Error) {
        errorBanner_->setText(QString::fromStdString(snap.update.errorMessage));
        errorBanner_->show();
    } else {
        errorBanner_->hide();
    }
}

} // namespace tuxblox
