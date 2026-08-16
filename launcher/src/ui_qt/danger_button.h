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
#include <QPushButton>
#include <QTimer>

namespace tuxblox {

// A destructive-action button requiring a second click within a window
// (5s in production, overridable for tests) to actually fire -- e.g.
// "Uninstall TuxBlox" -> "Click again to uninstall". Port of ui.cpp's
// renderDangerButton() to a real QWidget: the object being a QPushButton
// (styled via theme.cpp's "#dangerButton" QSS selector, object name set in
// the .cpp) means callers get standard QPushButton::clicked-free behavior
// for free -- this class owns the double-click-confirm state machine
// itself instead of exposing a raw clicked() signal callers would have to
// re-implement the arming logic against.
class DangerButton : public QPushButton {
    Q_OBJECT
public:
    DangerButton(const QString& restLabel, const QString& confirmLabel,
                 const QString& busyLabel, QWidget* parent = nullptr);

    // Shows busyLabel and disables the button (e.g. while a background
    // App:: thread carries out the action from a previous confirm()).
    void setBusy(bool busy);

    // Test-only hook: production code never calls this (always uses the
    // 5000ms default set in the constructor).
    void setConfirmWindowMsForTesting(int ms);

signals:
    // Fired on the frame the second, confirming click lands. The caller is
    // responsible for actually doing the thing (e.g. App::requestWipePrefix()).
    void confirmed();

private slots:
    void handleClicked();
    void revertArm();

private:
    QString restLabel_;
    QString confirmLabel_;
    QString busyLabel_;
    bool pending_ = false;
    int confirmWindowMs_ = 5000;
    QTimer revertTimer_;
};

} // namespace tuxblox
