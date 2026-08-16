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

// Persistent left navigation -- Start and Settings pinned to the top,
// About pinned to the bottom via a stretch, matching ui.cpp's
// renderSidebar() layout. A future plan adding the Versions tab inserts
// one more addEntry() call between Start and Settings in the .cpp's
// constructor body -- entries_ already generalizes over any Tab value, so
// no other change is needed here.
class Sidebar : public QWidget {
    Q_OBJECT
public:
    explicit Sidebar(QWidget* parent = nullptr);

    void setActiveTab(Tab tab);

signals:
    void tabSelected(Tab tab);

private:
    QPushButton* addEntry(const QString& label, const QString& iconResourcePath, Tab tab);

    QVBoxLayout* layout_ = nullptr;
    QButtonGroup* group_ = nullptr;
    QMap<Tab, QPushButton*> entries_;
};

} // namespace tuxblox
