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
#include "app_card.h"
#include "theme.h"
#include "version.h"
#include <QStyle>
#include <QFont>
#include <QHBoxLayout>
#include <QLabel>
#include <QProgressBar>
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

// "version-9f2a41c8 - live", or just the hash when the channel is unknown.
QString versionLabel(const AppVersions& versions) {
    QString label = QString::fromStdString(versions.activeHash);
    for (const auto& installed : versions.installed) {
        if (installed.hash == versions.activeHash && !installed.channel.empty()) {
            label += " \xC2\xB7 " + QString::fromStdString(installed.channel);
            break;
        }
    }
    return label;
}

} // namespace

StartTab::StartTab(App& app, QWidget* parent) : QWidget(parent), app_(app) {
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(24, 22, 24, 16);
    layout->setSpacing(0);

    title_ = new QLabel("Ready to play", this);
    title_->setObjectName("pageTitle");
    title_->setFont(theme::displayFont(21, QFont::Bold));
    layout->addWidget(title_);

    subtitle_ = new QLabel("Launch Roblox on Linux via TuxBlox", this);
    subtitle_->setObjectName("pageSubtitle");
    subtitle_->setWordWrap(true);
    layout->addSpacing(4);
    layout->addWidget(subtitle_);

    auto* cards = new QHBoxLayout();
    cards->setContentsMargins(0, 0, 0, 0);
    cards->setSpacing(12);

    playerCard_ = new AppCard("Roblox Player", ":/icons/roblox-player.png",
                               "Launch Player", "Install && Launch", this);
    connect(playerCard_, &AppCard::launchRequested, this,
            [this] { app_.requestLaunch(LaunchTarget::Player); });
    cards->addWidget(playerCard_);

    studioCard_ = new AppCard("Roblox Studio", ":/icons/roblox-studio.png",
                               "Launch Studio", "Install && Launch", this);
    connect(studioCard_, &AppCard::launchRequested, this,
            [this] { app_.requestLaunch(LaunchTarget::Studio); });
    cards->addWidget(studioCard_);

    cardRow_ = new QWidget(this);
    cardRow_->setLayout(cards);
    layout->addSpacing(18);
    layout->addWidget(cardRow_);

    // Shown in place of the cards while an update is downloading, so the
    // window never offers a launch that is about to be interrupted.
    updateStatusLabel_ = new QLabel(this);
    updateStatusLabel_->setObjectName("rowTitle");
    updateStatusLabel_->hide();
    layout->addWidget(updateStatusLabel_);

    updateProgress_ = new QProgressBar(this);
    updateProgress_->setRange(0, 100);
    updateProgress_->setTextVisible(false);
    updateProgress_->setFixedHeight(6);
    updateProgress_->hide();
    layout->addSpacing(10);
    layout->addWidget(updateProgress_);

    errorBanner_ = new QLabel(this);
    errorBanner_->setObjectName("errorBanner");
    errorBanner_->setWordWrap(true);
    errorBanner_->hide();
    layout->addSpacing(12);
    layout->addWidget(errorBanner_);

    layout->addStretch(1);
    layout->addWidget(buildStatusStrip());
}

QWidget* StartTab::buildStatusStrip() {
    auto* strip = new QWidget(this);
    strip->setObjectName("statusStrip");
    strip->setAttribute(Qt::WA_StyledBackground, true);

    auto* row = new QHBoxLayout(strip);
    row->setContentsMargins(0, 11, 0, 0);
    row->setSpacing(6);

    // One version number covers both halves of the product: the launcher
    // and the Proton build ship together and always carry the same one.
    auto* version = new QLabel(QString("TuxBlox %1").arg(kTuxBloxVersion), strip);
    version->setObjectName("statusText");
    row->addWidget(version);

    channelLabel_ = new QLabel(strip);
    channelLabel_->setObjectName("statusMuted");
    row->addWidget(channelLabel_);
    row->addStretch(1);

    // A 6px dot, coloured by the same three states the label describes.
    updateStateDot_ = new QLabel(strip);
    updateStateDot_->setObjectName("statusDot");
    updateStateDot_->setFixedSize(6, 6);
    row->addWidget(updateStateDot_);
    row->addSpacing(2);

    updateStateLabel_ = new QLabel(strip);
    updateStateLabel_->setObjectName("statusOk");
    row->addWidget(updateStateLabel_);

    return strip;
}

void StartTab::updateFromSnapshot(const AppSnapshot& snap) {
    // activeHash comes from loadInstalledVersions(), which derives it from
    // the version directories actually present in the prefix -- so a missing
    // or stale versions.json can't make an installed Roblox render as
    // uninstalled.
    const bool playerInstalled = !snap.versions.player.activeHash.empty();
    const bool studioInstalled = !snap.versions.studio.activeHash.empty();
    playerCard_->setState(playerInstalled, versionLabel(snap.versions.player));
    studioCard_->setState(studioInstalled, versionLabel(snap.versions.studio));

    channelLabel_->setText(
        QString("\xC2\xB7 %1 channel").arg(QString::fromStdString(snap.settings.channel)));

    const bool updating = snap.update.phase == UpdatePhase::CheckingManifest ||
                           snap.update.phase == UpdatePhase::PreparingUpdater;

    updateStatusLabel_->setVisible(updating);
    updateProgress_->setVisible(updating);
    cardRow_->setVisible(!updating);
    subtitle_->setVisible(!updating);

    if (updating) {
        title_->setText("Updating TuxBlox");
        updateStatusLabel_->setText(updatePhaseLabel(snap.update.phase));
        updateProgress_->setValue(static_cast<int>(snap.update.fraction * 100));
        setUpdateState("pending", "Updating");
        errorBanner_->hide();
        return;
    }

    title_->setText("Ready to play");

    if (snap.update.phase == UpdatePhase::Error) {
        errorBanner_->setText(QString::fromStdString(snap.update.errorMessage));
        errorBanner_->show();
        setUpdateState("error", "Update check failed");
        return;
    }

    errorBanner_->hide();
    if (snap.updateAvailableVersion) {
        setUpdateState("pending", QString("Version %1 available")
                                       .arg(QString::fromStdString(*snap.updateAvailableVersion)));
    } else {
        setUpdateState("ok", "Up to date");
    }
}

// The dot's colour comes from a style property, which Qt only re-evaluates
// when the widget is repolished.
void StartTab::setUpdateState(const QString& state, const QString& text) {
    updateStateLabel_->setText(text);
    if (updateStateDot_->property("state").toString() == state) return;
    updateStateDot_->setProperty("state", state);
    updateStateDot_->style()->unpolish(updateStateDot_);
    updateStateDot_->style()->polish(updateStateDot_);
}

} // namespace tuxblox
