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
#include <QAbstractButton>

namespace tuxblox {

// A pill-shaped on/off switch -- Qt Widgets has no built-in equivalent
// (QCheckBox always renders as a checkbox regardless of QSS). Deliberately
// framework-agnostic of App/AppSnapshot (constructed with just a parent, no
// Settings dependency) so it's reusable wherever a persisted bool toggle is
// needed -- see SettingsTab's "Send Crash Report Data" row for the pattern
// any future toggle (e.g. a future Auto-Update setting) should follow:
// construct, setChecked() from the current value, connect toggled(bool) to
// a handler that persists the new value.
class ToggleSwitch : public QAbstractButton {
    Q_OBJECT
public:
    explicit ToggleSwitch(QWidget* parent = nullptr);

    QSize sizeHint() const override;

protected:
    void paintEvent(QPaintEvent* event) override;

private:
    static constexpr int kWidth = 44;
    static constexpr int kHeight = 24;
};

} // namespace tuxblox
