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

#include "settings_tab.h"
#include "boxed_list.h"
#include "danger_button.h"
#include "theme.h"
#include "toggle_switch.h"
#include <QComboBox>
#include <QFont>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QScrollArea>
#include <QTimer>
#include <QVBoxLayout>
#include <QWidget>

namespace tuxblox {

SettingsTab::SettingsTab(App& app, QWidget* parent) : QWidget(parent), app_(app) {
    // Before any group is built: buildEnvironmentGroup() fills the graphics
    // card list from this.
    gpus_ = enumerateGpus("/sys/class/drm");

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

    auto* title = new QLabel("Settings", content);
    title->setObjectName("pageTitle");
    title->setFont(theme::displayFont(21, QFont::Bold));
    layout->addWidget(title);

    layout->addSpacing(18);
    layout->addWidget(makeSectionLabel("Updates", content));
    layout->addSpacing(8);
    layout->addWidget(buildUpdatesGroup(content));

    layout->addSpacing(18);
    layout->addWidget(makeSectionLabel("Environment", content));
    layout->addSpacing(8);
    layout->addWidget(buildEnvironmentGroup(content));

    layout->addSpacing(18);
    layout->addWidget(makeSectionLabel("Privacy", content));
    layout->addSpacing(8);
    layout->addWidget(buildPrivacyGroup(content));

    layout->addSpacing(18);
    layout->addWidget(makeSectionLabel("Danger zone", content));
    layout->addSpacing(8);
    layout->addWidget(buildDangerGroup(content));

    layout->addStretch(1);
}

QWidget* SettingsTab::buildUpdatesGroup(QWidget* parent) {
    auto* group = new BoxedGroup(parent);

    auto* channelRow = new BoxedRow("Update channel", "Which release stream TuxBlox follows.");
    channelCombo_ = new QComboBox();
    channelCombo_->addItems({"stable", "canary", "dev"});
    channelCombo_->setFixedWidth(110);
    connect(channelCombo_, &QComboBox::currentTextChanged, this, [this](const QString& text) {
        Settings updated = app_.snapshot().settings;
        updated.channel = text.toStdString();
        commitSettings(updated);
    });
    channelRow->addControl(channelCombo_);
    group->addRow(channelRow);

    auto* autoUpdateRow = new BoxedRow(
        "Automatic updates",
        "Install updates without asking. When off, you get a notification instead.");
    autoUpdateToggle_ = new ToggleSwitch();
    connect(autoUpdateToggle_, &ToggleSwitch::toggled, this, [this](bool checked) {
        Settings updated = app_.snapshot().settings;
        updated.autoUpdate = checked;
        commitSettings(updated);
    });
    autoUpdateRow->addControl(autoUpdateToggle_);
    group->addRow(autoUpdateRow);

    return group;
}

QWidget* SettingsTab::buildEnvironmentGroup(QWidget* parent) {
    auto* group = new BoxedGroup(parent);

    // Above the raw environment box, because it is the friendly form of the
    // same thing: picking a card here sets the variables a user would
    // otherwise have to know to type in themselves.
    auto* gpuRow = new BoxedRow(
        "Graphics card",
        "Which graphics card Roblox uses. Only matters if this computer has more than one.");
    gpuCombo_ = new QComboBox();
    // The empty userData is the "let the system decide" default, and is what
    // an unrecognised saved value falls back to.
    gpuCombo_->addItem("Automatic", QString());
    for (const auto& gpu : gpus_) {
        gpuCombo_->addItem(QString::fromStdString(gpu.label),
                           QString::fromStdString(gpu.pciAddress));
    }
    gpuCombo_->setMinimumWidth(210);
    connect(gpuCombo_, &QComboBox::currentIndexChanged, this, [this](int index) {
        if (index < 0) return;
        Settings updated = app_.snapshot().settings;
        updated.gpu = gpuCombo_->itemData(index).toString().toStdString();
        commitSettings(updated);
    });
    gpuRow->addControl(gpuCombo_);
    group->addRow(gpuRow);

    auto* envRow = new BoxedRow("Environment variables",
                                 "These variables will be passed to Roblox and the compatibility layer.");
    envEdit_ = new QLineEdit();
    envEdit_->setPlaceholderText("VARIABLE=value");
    envEdit_->setMinimumWidth(210);
    connect(envEdit_, &QLineEdit::editingFinished, this, [this] {
        Settings updated = app_.snapshot().settings;
        updated.envVars = envEdit_->text().toStdString();
        commitSettings(updated);
    });
    envRow->addControl(envEdit_);
    group->addRow(envRow);

    return group;
}

QWidget* SettingsTab::buildPrivacyGroup(QWidget* parent) {
    auto* group = new BoxedGroup(parent);

    auto* crashRow = new BoxedRow(
        "Send crash reports",
        "Crash reports include exit code, Roblox and TuxBlox versions, basic system info, and a copy of the session "
        "log. See tuxblox.net/privacy");
    crashReportsToggle_ = new ToggleSwitch();
    connect(crashReportsToggle_, &ToggleSwitch::toggled, this, [this](bool checked) {
        Settings updated = app_.snapshot().settings;
        updated.sendCrashReports = checked;
        commitSettings(updated);
    });
    crashRow->addControl(crashReportsToggle_);
    group->addRow(crashRow);

    return group;
}

QWidget* SettingsTab::buildDangerGroup(QWidget* parent) {
    // A container, not just the group: the two error banners sit outside
    // the bordered box so a failure message doesn't render as a nested
    // panel inside it.
    auto* container = new QWidget(parent);
    auto* layout = new QVBoxLayout(container);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(8);

    auto* group = new BoxedGroup(container);
    // Replaces "boxedGroup" -- the danger zone carries its own red-tinted
    // background and border instead of the neutral one.
    group->setObjectName("dangerZone");

    auto* terminateRow = new BoxedRow(
        "Terminate Roblox",
        "Stops all Roblox processes running");
    terminateButton_ = new QPushButton("Terminate", this);
    terminateButton_->setObjectName("dangerButton");
    terminateButton_->setCursor(Qt::PointingHandCursor);
    connect(terminateButton_, &QPushButton::clicked, this, &SettingsTab::onTerminate);
    terminateRow->addControl(terminateButton_);
    group->addRow(terminateRow);

    auto* wipeRow = new BoxedRow("Wipe prefix",
                                  "Deletes the virtual drive for TuxBlox. Note that this will wipe Roblox installations");
    wipePrefixButton_ = new DangerButton("Wipe prefix", "Click again to wipe", "Wiping...");
    wipeRow->addControl(wipePrefixButton_);
    connect(wipePrefixButton_, &DangerButton::confirmed, this, [this] { app_.requestWipePrefix(); });
    group->addRow(wipeRow);

    auto* uninstallRow = new BoxedRow("Uninstall TuxBlox",
                                       "This will delete everything including the virtual drive, shortcuts, and TuxBlox itself.");
    uninstallButton_ = new DangerButton("Uninstall", "Click again to uninstall", "Uninstalling...");
    uninstallRow->addControl(uninstallButton_);
    connect(uninstallButton_, &DangerButton::confirmed, this, [this] { app_.requestUninstall(); });
    group->addRow(uninstallRow);

    layout->addWidget(group);

    wipePrefixError_ = new QLabel(container);
    wipePrefixError_->setObjectName("errorBanner");
    wipePrefixError_->setWordWrap(true);
    wipePrefixError_->hide();
    layout->addWidget(wipePrefixError_);

    uninstallError_ = new QLabel(container);
    uninstallError_->setObjectName("errorBanner");
    uninstallError_->setWordWrap(true);
    uninstallError_->hide();
    layout->addWidget(uninstallError_);

    return container;
}

void SettingsTab::onTerminate() {
    const int signalled = app_.requestTerminateProcesses();
    terminateButton_->setText(signalled == 0 ? "Nothing running"
                                              : QString("Stopped %1").arg(signalled));
    terminateButton_->setEnabled(false);
    // Back to a normal button after a moment -- the label is a result, not a
    // new state to stay in.
    QTimer::singleShot(2500, this, [this] {
        terminateButton_->setText("Terminate");
        terminateButton_->setEnabled(true);
    });
}

void SettingsTab::commitSettings(const Settings& updated) {
    app_.updateSettings(updated);
}

void SettingsTab::updateFromSnapshot(const AppSnapshot& snap) {
    if (!fieldsSeeded_) {
        channelCombo_->setCurrentText(QString::fromStdString(snap.settings.channel));
        envEdit_->setText(QString::fromStdString(snap.settings.envVars));
        crashReportsToggle_->setChecked(snap.settings.sendCrashReports);
        autoUpdateToggle_->setChecked(snap.settings.autoUpdate);
        // Matched on the stored PCI slot, not on position: a card that has
        // been removed since the setting was saved has no entry here, and
        // findData returns -1, which correctly lands back on Automatic.
        int gpuIndex = gpuCombo_->findData(QString::fromStdString(snap.settings.gpu));
        gpuCombo_->setCurrentIndex(gpuIndex >= 0 ? gpuIndex : 0);
        fieldsSeeded_ = true;
    }

    wipePrefixButton_->setBusy(snap.wipePrefix.inProgress);
    if (!snap.wipePrefix.errorMessage.empty()) {
        wipePrefixError_->setText(QString::fromStdString(snap.wipePrefix.errorMessage));
        wipePrefixError_->show();
    } else {
        wipePrefixError_->hide();
    }

    uninstallButton_->setBusy(snap.uninstall.inProgress);
    if (!snap.uninstall.errorMessage.empty()) {
        uninstallError_->setText(QString::fromStdString(snap.uninstall.errorMessage));
        uninstallError_->show();
    } else {
        uninstallError_->hide();
    }
}

} // namespace tuxblox
