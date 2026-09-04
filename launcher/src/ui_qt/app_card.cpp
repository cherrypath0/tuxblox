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

#include "app_card.h"
#include "theme.h"
#include <QFontDatabase>
#include <QHBoxLayout>
#include <QIcon>
#include <QLabel>
#include <QPushButton>
#include <QStyle>
#include <QVBoxLayout>

namespace tuxblox {

namespace {

// Re-runs the stylesheet against a widget after one of the properties its
// selectors test has changed. Qt does not do this on its own.
void restyle(QWidget* widget) {
    widget->style()->unpolish(widget);
    widget->style()->polish(widget);
}

} // namespace

AppCard::AppCard(const QString& title, const QString& iconResourcePath,
                  const QString& launchLabel, const QString& installLabel, QWidget* parent)
    : QFrame(parent), launchLabel_(launchLabel), installLabel_(installLabel) {
    setObjectName("appCard");

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(16, 15, 16, 15);
    layout->setSpacing(0);

    auto* header = new QHBoxLayout();
    header->setContentsMargins(0, 0, 0, 0);
    header->setSpacing(11);

    auto* icon = new QLabel(this);
    icon->setPixmap(QIcon(iconResourcePath).pixmap(34, 34));
    icon->setFixedSize(34, 34);
    header->addWidget(icon, 0, Qt::AlignTop);

    auto* textColumn = new QVBoxLayout();
    textColumn->setContentsMargins(0, 0, 0, 0);
    textColumn->setSpacing(2);

    auto* name = new QLabel(title, this);
    name->setObjectName("cardTitle");
    name->setFont(theme::displayFont(15, QFont::DemiBold));
    textColumn->addWidget(name);

    meta_ = new QLabel(this);
    meta_->setObjectName("cardMeta");
    QFont mono = QFontDatabase::systemFont(QFontDatabase::FixedFont);
    mono.setPixelSize(11);
    meta_->setFont(mono);
    textColumn->addWidget(meta_);

    header->addLayout(textColumn, 1);
    layout->addLayout(header);
    layout->addStretch(1);

    button_ = new QPushButton(this);
    button_->setObjectName("primaryButton");
    button_->setMinimumHeight(34);
    button_->setCursor(Qt::PointingHandCursor);
    connect(button_, &QPushButton::clicked, this, &AppCard::launchRequested);
    layout->addSpacing(12);
    layout->addWidget(button_);

    setState(false, QString());
}

void AppCard::setState(bool installed, const QString& versionLabel) {
    const QString metaText = installed ? versionLabel : "Not installed yet";
    if (meta_->text() != metaText) {
        meta_->setText(metaText);
    }

    if (stateApplied_ && installed == installed_) return;
    installed_ = installed;
    stateApplied_ = true;

    // The install path deliberately looks secondary: it is a download, not
    // a launch, even though requestLaunch() handles both.
    button_->setText(installed ? launchLabel_ : installLabel_);
    button_->setObjectName(installed ? "primaryButton" : "secondaryButton");

    // A version hash deserves the fixed-width face; "Not installed yet" is
    // prose and should not be monospaced.
    QFont metaFont = installed ? QFontDatabase::systemFont(QFontDatabase::FixedFont)
                                : QFont(theme::sansFamily());
    metaFont.setPixelSize(11);
    meta_->setFont(metaFont);

    meta_->setProperty("missing", !installed);
    setProperty("installed", installed);
    restyle(meta_);
    restyle(button_);
    restyle(this);
}

} // namespace tuxblox
