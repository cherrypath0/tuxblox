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
#include "danger_button.h"
#include "toggle_switch.h"
#include <QComboBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QScrollArea>
#include <QVBoxLayout>
#include <QWidget>

namespace tuxblox {

SettingsTab::SettingsTab(App& app, QWidget* parent) : QWidget(parent), app_(app) {
    auto* outer = new QVBoxLayout(this);
    outer->setContentsMargins(0, 0, 0, 0);

    auto* scrollArea = new QScrollArea(this);
    scrollArea->setWidgetResizable(true);
    scrollArea->setFrameShape(QFrame::NoFrame);
    outer->addWidget(scrollArea);

    auto* content = new QWidget(scrollArea);
    scrollArea->setWidget(content);

    auto* layout = new QVBoxLayout(content);
    layout->setContentsMargins(24, 20, 24, 20);
    layout->setSpacing(20);

    auto* title = new QLabel("Settings", content);
    title->setObjectName("sectionTitle");
    layout->addWidget(title);

    auto* channelLabel = new QLabel("Update Channel", content);
    channelLabel->setObjectName("fieldLabel");
    layout->addWidget(channelLabel);
    channelCombo_ = new QComboBox(content);
    channelCombo_->addItems({"stable", "canary", "dev"});
    channelCombo_->setFixedWidth(200);
    layout->addWidget(channelCombo_);
    connect(channelCombo_, &QComboBox::currentTextChanged, this, [this](const QString& text) {
        Settings s = app_.snapshot().settings;
        s.channel = text.toStdString();
        commitSettings(s);
    });

    auto* autoUpdateRow = new QHBoxLayout();
    autoUpdateToggle_ = new ToggleSwitch(content);
    connect(autoUpdateToggle_, &ToggleSwitch::toggled, this, [this](bool checked) {
        Settings s = app_.snapshot().settings;
        s.autoUpdate = checked;
        commitSettings(s);
    });
    autoUpdateRow->addWidget(autoUpdateToggle_);
    auto* autoUpdateLabel = new QLabel("Auto-Update", content);
    autoUpdateLabel->setObjectName("fieldLabel");
    autoUpdateRow->addWidget(autoUpdateLabel);
    autoUpdateRow->addStretch(1);
    layout->addLayout(autoUpdateRow);

    auto* autoUpdateHint = new QLabel(
        "Automatically install updates without asking. When off, you'll get a notification instead.",
        content);
    autoUpdateHint->setObjectName("fieldHint");
    autoUpdateHint->setWordWrap(true);
    layout->addWidget(autoUpdateHint);

    auto* protonLabel = new QLabel("Proton Environment Variables", content);
    protonLabel->setObjectName("fieldLabel");
    layout->addWidget(protonLabel);
    protonEnvEdit_ = new QLineEdit(content);
    layout->addWidget(protonEnvEdit_);
    connect(protonEnvEdit_, &QLineEdit::editingFinished, this, [this] {
        Settings s = app_.snapshot().settings;
        s.protonEnvVars = protonEnvEdit_->text().toStdString();
        commitSettings(s);
    });

    auto* globalLabel = new QLabel("Global Environment Variables", content);
    globalLabel->setObjectName("fieldLabel");
    layout->addWidget(globalLabel);
    globalEnvEdit_ = new QLineEdit(content);
    layout->addWidget(globalEnvEdit_);
    connect(globalEnvEdit_, &QLineEdit::editingFinished, this, [this] {
        Settings s = app_.snapshot().settings;
        s.globalEnvVars = globalEnvEdit_->text().toStdString();
        commitSettings(s);
    });

    auto* crashRow = new QHBoxLayout();
    crashReportsToggle_ = new ToggleSwitch(content);
    connect(crashReportsToggle_, &ToggleSwitch::toggled, this, [this](bool checked) {
        Settings s = app_.snapshot().settings;
        s.sendCrashReports = checked;
        commitSettings(s);
    });
    crashRow->addWidget(crashReportsToggle_);
    auto* crashLabel = new QLabel("Send Crash Report Data", content);
    crashLabel->setObjectName("fieldLabel");
    crashRow->addWidget(crashLabel);
    crashRow->addStretch(1);
    layout->addLayout(crashRow);

    auto* crashHint = new QLabel(
        "Crash reports include the exit code, Roblox/Proton version, basic system info, and a "
        "copy of the session log -- see our privacy policy at tuxblox.net/privacy",
        content);
    crashHint->setObjectName("fieldHint");
    crashHint->setWordWrap(true);
    layout->addWidget(crashHint);

    auto* dangerZone = new QWidget(content);
    dangerZone->setObjectName("dangerZone");
    auto* dangerLayout = new QVBoxLayout(dangerZone);
    dangerLayout->setContentsMargins(16, 16, 16, 16);
    dangerLayout->setSpacing(8);

    auto* dangerTitle = new QLabel("Danger Zone", dangerZone);
    dangerTitle->setObjectName("fieldLabel");
    dangerLayout->addWidget(dangerTitle);

    wipePrefixButton_ = new DangerButton("Wipe Prefix", "Click again to wipe prefix", "Wiping...", dangerZone);
    dangerLayout->addWidget(wipePrefixButton_);
    connect(wipePrefixButton_, &DangerButton::confirmed, this, [this] { app_.requestWipePrefix(); });

    wipePrefixError_ = new QLabel(dangerZone);
    wipePrefixError_->setObjectName("errorBanner");
    wipePrefixError_->setWordWrap(true);
    wipePrefixError_->hide();
    dangerLayout->addWidget(wipePrefixError_);

    uninstallButton_ = new DangerButton("Uninstall TuxBlox", "Click again to uninstall", "Uninstalling...", dangerZone);
    dangerLayout->addWidget(uninstallButton_);
    connect(uninstallButton_, &DangerButton::confirmed, this, [this] { app_.requestUninstall(); });

    uninstallError_ = new QLabel(dangerZone);
    uninstallError_->setObjectName("errorBanner");
    uninstallError_->setWordWrap(true);
    uninstallError_->hide();
    dangerLayout->addWidget(uninstallError_);

    layout->addWidget(dangerZone);
    layout->addStretch(1);
}

void SettingsTab::commitSettings(const Settings& updated) {
    app_.updateSettings(updated);
}

void SettingsTab::updateFromSnapshot(const AppSnapshot& snap) {
    if (!fieldsSeeded_) {
        channelCombo_->setCurrentText(QString::fromStdString(snap.settings.channel));
        protonEnvEdit_->setText(QString::fromStdString(snap.settings.protonEnvVars));
        globalEnvEdit_->setText(QString::fromStdString(snap.settings.globalEnvVars));
        crashReportsToggle_->setChecked(snap.settings.sendCrashReports);
        autoUpdateToggle_->setChecked(snap.settings.autoUpdate);
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
