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

#include "update_popup.h"
#include <QHBoxLayout>
#include <QLabel>
#include <QMouseEvent>
#include <QPushButton>

namespace tuxblox {

UpdatePopup::UpdatePopup(QWidget* parent) : QWidget(parent) {
    setObjectName("updatePopup");
    setCursor(Qt::PointingHandCursor);
    // A plain QWidget does not paint its QSS "background"/"border"
    // properties by default (that's the styled-widget behavior QFrame/
    // QPushButton/etc. get automatically) -- without this flag,
    // #updatePopup's background/border/border-radius rule in theme.cpp
    // compiles and matches but never actually paints anything, leaving the
    // popup visually transparent against whatever's behind it. Observed
    // for real: the toast rendered as bare text floating over the page.
    setAttribute(Qt::WA_StyledBackground, true);

    auto* layout = new QHBoxLayout(this);
    layout->setContentsMargins(12, 10, 8, 10);
    layout->setSpacing(10);

    iconLabel_ = new QLabel(this);
    iconLabel_->setPixmap(QPixmap(":/icons/download.png").scaled(20, 20, Qt::KeepAspectRatio, Qt::SmoothTransformation));
    layout->addWidget(iconLabel_);
    // A bit more room between the icon and the message than layout's base
    // 10px spacing gives every widget in this row -- keeps the
    // text-to-dismiss-button gap unchanged.
    layout->addSpacing(6);

    textLabel_ = new QLabel(this);
    textLabel_->setObjectName("updatePopupText");
    layout->addWidget(textLabel_, 1);

    dismissButton_ = new QPushButton("×", this); // "×"
    dismissButton_->setObjectName("updatePopupDismiss");
    dismissButton_->setFixedSize(20, 20);
    layout->addWidget(dismissButton_);
    connect(dismissButton_, &QPushButton::clicked, this, &UpdatePopup::dismissRequested);

    hide();
}

void UpdatePopup::setAvailableVersion(const std::optional<std::string>& version) {
    if (!version.has_value()) {
        hide();
        return;
    }
    textLabel_->setText(QString("Update available: v%1").arg(QString::fromStdString(*version)));
    adjustSize();
    show();
    raise();
}

void UpdatePopup::mousePressEvent(QMouseEvent* event) {
    // dismissButton_ is a child widget positioned within this one -- Qt
    // routes a press landing on it to the button itself (handled by the
    // connect() above), so this handler only ever fires for clicks on the
    // rest of the popup body.
    if (event->button() == Qt::LeftButton) {
        emit updateRequested();
    }
    QWidget::mousePressEvent(event);
}

} // namespace tuxblox
