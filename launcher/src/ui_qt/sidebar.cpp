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

#include "sidebar.h"
#include "icon_utils.h"
#include <QButtonGroup>
#include <QIcon>
#include <QPushButton>
#include <QVBoxLayout>

namespace tuxblox {

Sidebar::Sidebar(QWidget* parent) : QWidget(parent) {
    setObjectName("sidebar");
    setFixedWidth(160);

    layout_ = new QVBoxLayout(this);
    layout_->setContentsMargins(8, 8, 8, 8);
    layout_->setSpacing(4);

    group_ = new QButtonGroup(this);
    group_->setExclusive(true);

    addEntry("Home", ":/icons/home.png", Tab::Start);
    addEntry("Versions", ":/icons/roblox-rdd.png", Tab::Versions);
    addEntry("Settings", ":/icons/settings.png", Tab::Settings);

    layout_->addStretch(1);

    addEntry("About", ":/icons/info.png", Tab::About);

    setActiveTab(Tab::Start);
}

QPushButton* Sidebar::addEntry(const QString& label, const QString& iconResourcePath, Tab tab) {
    auto* button = new QPushButton(paddedIcon(iconResourcePath, 20, 6), label, this);
    button->setObjectName("sidebarItem");
    button->setCheckable(true);
    button->setIconSize(iconSizeWithGap(20, 6));
    group_->addButton(button);
    layout_->addWidget(button);
    entries_[tab] = button;

    connect(button, &QPushButton::clicked, this, [this, tab] { emit tabSelected(tab); });

    return button;
}

void Sidebar::setActiveTab(Tab tab) {
    if (auto it = entries_.find(tab); it != entries_.end()) {
        it.value()->setChecked(true);
    }
}

} // namespace tuxblox
