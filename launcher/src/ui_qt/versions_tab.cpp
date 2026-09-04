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
#include "boxed_list.h"
#include "danger_button.h"
#include "icon_utils.h"
#include "theme.h"
#include "versions_tab_state.h"
#include <QComboBox>
#include <QFont>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QProgressBar>
#include <QPushButton>
#include <QScrollArea>
#include <QStringList>
#include <QVBoxLayout>

namespace tuxblox {

namespace {

LaunchTarget selectedTarget(const QComboBox* box) {
    return box->currentIndex() == 0 ? LaunchTarget::Player : LaunchTarget::Studio;
}

// Coarse-grained "did anything about this app type's installed-versions
// data change" check -- not a per-card diff. Used to decide whether
// rebuildVersionList() needs to run at all; see the comment on
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

// "live - installed 2026-08-16", dropping either half when it is missing.
QString describeVersion(const InstalledVersion& version) {
    QStringList parts;
    if (!version.channel.empty()) parts << QString::fromStdString(version.channel);
    if (!version.installedAt.empty()) {
        parts << "installed " + QString::fromStdString(version.installedAt).left(10);
    }
    return parts.join(" \xC2\xB7 ");
}

} // namespace

VersionsTab::VersionsTab(App& app, QWidget* parent) : QWidget(parent), app_(app) {
    auto* outer = new QVBoxLayout(this);
    outer->setContentsMargins(0, 0, 0, 0);

    auto* scrollArea = new QScrollArea(this);
    scrollArea->setWidgetResizable(true);
    scrollArea->setFrameShape(QFrame::NoFrame);
    scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    outer->addWidget(scrollArea);

    auto* content = new QWidget(scrollArea);
    scrollArea->setWidget(content);

    auto* layout = new QVBoxLayout(content);
    layout->setContentsMargins(24, 22, 24, 22);
    layout->setSpacing(0);

    auto* title = new QLabel("Versions", content);
    title->setObjectName("pageTitle");
    title->setFont(theme::displayFont(21, QFont::Bold));
    layout->addWidget(title);

    auto* subtitle = new QLabel("Install and switch between Roblox builds.", content);
    subtitle->setObjectName("pageSubtitle");
    layout->addSpacing(4);
    layout->addWidget(subtitle);

    layout->addSpacing(16);
    layout->addWidget(buildInstallBar(content));

    progressLabel_ = new QLabel(content);
    progressLabel_->setObjectName("rowDesc");
    progressLabel_->hide();
    layout->addSpacing(12);
    layout->addWidget(progressLabel_);

    progressBar_ = new QProgressBar(content);
    progressBar_->setRange(0, 100);
    progressBar_->setTextVisible(false);
    progressBar_->setFixedHeight(6);
    progressBar_->hide();
    layout->addSpacing(6);
    layout->addWidget(progressBar_);

    errorBanner_ = new QLabel(content);
    errorBanner_->setObjectName("errorBanner");
    errorBanner_->setWordWrap(true);
    errorBanner_->hide();
    layout->addSpacing(8);
    layout->addWidget(errorBanner_);

    layout->addSpacing(18);
    layout->addWidget(makeSectionLabel("Player", content));
    layout->addSpacing(8);
    playerGroup_ = new BoxedGroup(content);
    layout->addWidget(playerGroup_);

    layout->addSpacing(18);
    layout->addWidget(makeSectionLabel("Studio", content));
    layout->addSpacing(8);
    studioGroup_ = new BoxedGroup(content);
    layout->addWidget(studioGroup_);

    layout->addStretch(1);
}

QWidget* VersionsTab::buildInstallBar(QWidget* parent) {
    // One row, not the two the old layout needed: the channel field folded
    // into the hash field's placeholder, which frees enough width for the
    // whole bar to fit at the 640px minimum window size.
    auto* bar = new QWidget(parent);
    auto* row = new QHBoxLayout(bar);
    row->setContentsMargins(0, 0, 0, 0);
    row->setSpacing(8);

    targetSelect_ = new QComboBox(bar);
    targetSelect_->addItem("Player");
    targetSelect_->addItem("Studio");
    targetSelect_->setFixedWidth(100);
    connect(targetSelect_, &QComboBox::currentIndexChanged, this,
            [this] { refreshProgressVisibility(); });
    row->addWidget(targetSelect_);

    channelField_ = new QLineEdit("live", bar);
    channelField_->setPlaceholderText("channel");
    channelField_->setFixedWidth(80);
    row->addWidget(channelField_);

    hashField_ = new QLineEdit(bar);
    hashField_->setPlaceholderText("version-\xE2\x80\xA6 or blank for latest");
    row->addWidget(hashField_, 1);

    latestButton_ = new QPushButton(paddedIcon(":/icons/download.png", 14, 6), "Install", bar);
    latestButton_->setObjectName("toolbarButtonBrand");
    latestButton_->setIconSize(iconSizeWithGap(14, 6));
    latestButton_->setCursor(Qt::PointingHandCursor);
    connect(latestButton_, &QPushButton::clicked, this, &VersionsTab::onInstall);
    row->addWidget(latestButton_);

    previousButton_ = new QPushButton("Previous", bar);
    previousButton_->setObjectName("toolbarButton");
    previousButton_->setToolTip("Install the build before the current latest");
    previousButton_->setCursor(Qt::PointingHandCursor);
    connect(previousButton_, &QPushButton::clicked, this, &VersionsTab::onDownloadPrevious);
    row->addWidget(previousButton_);

    return bar;
}

// A blank hash field means "latest"; anything typed is treated as an exact
// version, which is what the old separate Download button did.
void VersionsTab::onInstall() {
    const std::string hash = hashField_->text().trimmed().toStdString();
    if (hash.empty()) {
        app_.requestInstallVersion(selectedTarget(targetSelect_), VersionSelectMode::Latest,
                                    channelField_->text().toStdString());
        return;
    }
    app_.requestInstallVersion(selectedTarget(targetSelect_), VersionSelectMode::ManualHash,
                                channelField_->text().toStdString(), hash);
}

void VersionsTab::onDownloadPrevious() {
    app_.requestInstallVersion(selectedTarget(targetSelect_), VersionSelectMode::Previous,
                                channelField_->text().toStdString());
}

void VersionsTab::refreshProgressVisibility() {
    updateFromSnapshot(app_.snapshot());
}

void VersionsTab::updateFromSnapshot(const AppSnapshot& snap) {
    const auto& progress = snap.versionInstall;
    // Only one install can run at a time system-wide (App enforces this via
    // versionInstall.phase, regardless of target), so button enablement
    // stays keyed off "is any install running" -- but the progress bar and
    // error banner are per-target UI on this tab, and must only reflect an
    // install for the target currently selected in targetSelect_. Without
    // this, a Studio download's progress would render while the user has
    // Player selected.
    const bool anyInstallRunning = progress.phase != VersionInstallPhase::Idle &&
                                    progress.phase != VersionInstallPhase::Done &&
                                    progress.phase != VersionInstallPhase::Error;
    const bool isSelectedTarget = progress.target == selectedTarget(targetSelect_);
    const bool activeForSelectedTarget = anyInstallRunning && isSelectedTarget;

    progressLabel_->setVisible(activeForSelectedTarget);
    progressBar_->setVisible(activeForSelectedTarget);
    latestButton_->setEnabled(!anyInstallRunning);
    previousButton_->setEnabled(!anyInstallRunning);

    if (activeForSelectedTarget) {
        static const char* kPhaseLabels[] = {"", "Resolving version", "Fetching package manifest",
                                              "Downloading packages", "Extracting", "", ""};
        progressLabel_->setText(kPhaseLabels[static_cast<int>(progress.phase)]);
        progressBar_->setValue(static_cast<int>(progress.fraction * 100));
    }

    if (progress.phase == VersionInstallPhase::Error && isSelectedTarget) {
        errorBanner_->setText(QString::fromStdString(progress.errorMessage));
        errorBanner_->show();
    } else {
        errorBanner_->hide();
    }

    // Skip the rebuild entirely when nothing in the underlying data changed
    // since the last render. This tab's updateFromSnapshot() is driven by
    // MainWindow's poll loop (every 100ms) -- rebuilding on every no-op tick
    // would tear down and recreate every row's DangerButton on each poll,
    // collapsing its 5-second two-click confirm window to under 100ms.
    if (!listRendered_ || !appVersionsEqual(snap.versions.player, lastRenderedVersions_.player) ||
        !appVersionsEqual(snap.versions.studio, lastRenderedVersions_.studio)) {
        rebuildVersionList(LaunchTarget::Player, snap.versions.player, playerGroup_);
        rebuildVersionList(LaunchTarget::Studio, snap.versions.studio, studioGroup_);
        lastRenderedVersions_ = snap.versions;
        listRendered_ = true;
    }
}

void VersionsTab::rebuildVersionList(LaunchTarget target, const AppVersions& versions,
                                      BoxedGroup* group) {
    group->clearRows();

    if (versions.installed.empty()) {
        auto* empty = new BoxedRow("No versions installed");
        group->addRow(empty);
        return;
    }

    for (const auto& version : versions.installed) {
        auto* row = new BoxedRow(QString::fromStdString(version.hash), describeVersion(version));
        row->useMonospaceDescription();

        if (version.hash == versions.activeHash) {
            row->setPill("Active");
        } else {
            auto* setActive = new QPushButton("Set active");
            setActive->setObjectName("toolbarButton");
            setActive->setCursor(Qt::PointingHandCursor);
            connect(setActive, &QPushButton::clicked, this,
                    [this, target, hash = version.hash] { app_.requestSetActiveVersion(target, hash); });
            row->addControl(setActive);
        }

        auto* remove = new DangerButton("Delete", "Confirm?", "Deleting...");
        remove->setEnabled(canDeleteVersion(versions, version.hash));
        connect(remove, &DangerButton::confirmed, this,
                [this, target, hash = version.hash] { app_.requestDeleteVersion(target, hash); });
        row->addControl(remove);

        group->addRow(row);
    }
}

} // namespace tuxblox
