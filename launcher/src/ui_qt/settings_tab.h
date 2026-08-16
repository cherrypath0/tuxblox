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
class QLabel;

namespace tuxblox {

class ToggleSwitch;
class DangerButton;

// Bloxstrap-style grouped settings -- port of ui.cpp's renderSettingsTab().
// A future plan (Auto-Update) adds one more ToggleSwitch row near the
// Update Channel combo; this class's constructor lays out each row as an
// independent block specifically so that insertion doesn't require
// reflowing anything below it (QVBoxLayout, not manual y-cursor math like
// the ImGui original).
class SettingsTab : public QWidget {
    Q_OBJECT
public:
    explicit SettingsTab(App& app, QWidget* parent = nullptr);

    void updateFromSnapshot(const AppSnapshot& snap);

private:
    void commitSettings(const Settings& updated);

    App& app_;
    QComboBox* channelCombo_ = nullptr;
    ToggleSwitch* autoUpdateToggle_ = nullptr;
    QLineEdit* protonEnvEdit_ = nullptr;
    QLineEdit* globalEnvEdit_ = nullptr;
    ToggleSwitch* crashReportsToggle_ = nullptr;
    DangerButton* wipePrefixButton_ = nullptr;
    QLabel* wipePrefixError_ = nullptr;
    DangerButton* uninstallButton_ = nullptr;
    QLabel* uninstallError_ = nullptr;

    // Guards against re-seeding protonEnvEdit_/globalEnvEdit_ from
    // App::snapshot() on every poll tick while the user is mid-edit --
    // same problem ui.cpp's settingsBuffersInitialized_ solved, but scoped
    // per-field here since Qt's QLineEdit (unlike ImGui's InputText) is
    // itself the source of truth for in-progress text; this just tracks
    // whether the initial seed from Settings has happened at all.
    bool fieldsSeeded_ = false;
};

} // namespace tuxblox
