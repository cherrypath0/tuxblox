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
#include <QMap>
#include <QWidget>

class QPushButton;
class QButtonGroup;
class QVBoxLayout;

namespace tuxblox {

// Persistent left navigation: the TuxBlox brand lockup, then Home,
// Versions and Settings, with About below a hairline at the bottom.
//
// The styling mirrors the docs sidebar on tuxblox.net -- muted labels that
// brighten on hover, and a brand-tinted background with brand-coloured
// text for the current page.
class Sidebar : public QWidget {
    Q_OBJECT
public:
    explicit Sidebar(QWidget* parent = nullptr);

    void setActiveTab(Tab tab);

signals:
    void tabSelected(Tab tab);

private:
    QWidget* buildBrandLockup();
    QPushButton* addEntry(const QString& label, const QString& iconResourcePath, Tab tab,
                           QVBoxLayout* target);

    QVBoxLayout* layout_ = nullptr;
    QButtonGroup* group_ = nullptr;
    QMap<Tab, QPushButton*> entries_;
};

} // namespace tuxblox
