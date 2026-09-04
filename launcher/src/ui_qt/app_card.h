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
#include <QFrame>
#include <QString>

class QLabel;
class QPushButton;

namespace tuxblox {

// One of the two cards on the Home tab: the app's icon and name, the
// version currently installed, and the button that starts it.
//
// The card is the launch button -- there is no separate "installed" badge,
// because the version line already says it. When nothing is installed the
// meta line says so in the warning colour and the button offers to install
// first, which is what requestLaunch() does anyway.
class AppCard : public QFrame {
    Q_OBJECT
public:
    AppCard(const QString& title, const QString& iconResourcePath,
            const QString& launchLabel, const QString& installLabel,
            QWidget* parent = nullptr);

    // `versionLabel` is shown as-is when installed and ignored otherwise.
    void setState(bool installed, const QString& versionLabel);

signals:
    void launchRequested();

private:
    QLabel* meta_ = nullptr;
    QPushButton* button_ = nullptr;
    QString launchLabel_;
    QString installLabel_;

    // Tracks what setState() last applied, so a poll tick that changes
    // nothing doesn't restyle the widget on every pass.
    bool installed_ = false;
    bool stateApplied_ = false;
};

} // namespace tuxblox
