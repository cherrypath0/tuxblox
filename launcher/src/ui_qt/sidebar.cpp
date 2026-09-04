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
#include "theme.h"
#include <QButtonGroup>
#include <QHBoxLayout>
#include <QIcon>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>

namespace tuxblox {

Sidebar::Sidebar(QWidget* parent) : QWidget(parent) {
    setObjectName("sidebar");
    // Without this a plain QWidget never paints its QSS background or
    // border -- see the note in update_popup.cpp.
    setAttribute(Qt::WA_StyledBackground, true);
    setFixedWidth(168);

    layout_ = new QVBoxLayout(this);
    layout_->setContentsMargins(10, 10, 10, 12);
    layout_->setSpacing(2);

    group_ = new QButtonGroup(this);
    group_->setExclusive(true);

    layout_->addWidget(buildBrandLockup());
    layout_->addSpacing(6);

    addEntry("Home", ":/icons/home.png", Tab::Start, layout_);
    addEntry("Versions", ":/icons/roblox-rdd.png", Tab::Versions, layout_);
    addEntry("FastFlags", ":/icons/fastflags.png", Tab::FastFlags, layout_);
    addEntry("Settings", ":/icons/settings.png", Tab::Settings, layout_);

    layout_->addStretch(1);

    // About sits below a hairline rather than floating at the bottom of the
    // same list -- it is about the app, not a place to configure it.
    auto* footer = new QWidget(this);
    footer->setObjectName("sidebarFooter");
    footer->setAttribute(Qt::WA_StyledBackground, true);
    auto* footerLayout = new QVBoxLayout(footer);
    footerLayout->setContentsMargins(0, 8, 0, 0);
    footerLayout->setSpacing(2);
    addEntry("About", ":/icons/info.png", Tab::About, footerLayout);
    layout_->addWidget(footer);

    setActiveTab(Tab::Start);
}

QWidget* Sidebar::buildBrandLockup() {
    auto* lockup = new QWidget(this);
    auto* row = new QHBoxLayout(lockup);
    row->setContentsMargins(6, 5, 6, 10);
    row->setSpacing(9);

    auto* mark = new QLabel(lockup);
    mark->setPixmap(QIcon(":/branding/tuxblox_logo.png").pixmap(22, 22));
    mark->setFixedSize(22, 22);
    row->addWidget(mark);

    auto* name = new QLabel("TuxBlox", lockup);
    name->setObjectName("sidebarBrand");
    name->setFont(theme::displayFont(15, QFont::DemiBold));
    row->addWidget(name);
    row->addStretch(1);

    return lockup;
}

QPushButton* Sidebar::addEntry(const QString& label, const QString& iconResourcePath, Tab tab,
                                QVBoxLayout* target) {
    auto* button = new QPushButton(paddedIcon(iconResourcePath, 16, 8), label, this);
    button->setObjectName("sidebarItem");
    button->setCheckable(true);
    button->setCursor(Qt::PointingHandCursor);
    button->setIconSize(iconSizeWithGap(16, 8));
    group_->addButton(button);
    target->addWidget(button);
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
