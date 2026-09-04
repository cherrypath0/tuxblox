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

#include "boxed_list.h"
#include "theme.h"
#include <QFontDatabase>
#include <QHBoxLayout>
#include <QLabel>
#include <QLayoutItem>
#include <QPushButton>
#include <QVBoxLayout>

namespace tuxblox {

BoxedRow::BoxedRow(const QString& title, const QString& description, QWidget* parent)
    : QWidget(parent) {
    setObjectName("boxedRow");

    layout_ = new QHBoxLayout(this);
    layout_->setContentsMargins(14, 10, 14, 10);
    layout_->setSpacing(10);

    auto* textColumn = new QVBoxLayout();
    textColumn->setContentsMargins(0, 0, 0, 0);
    textColumn->setSpacing(2);

    titleRow_ = new QHBoxLayout();
    titleRow_->setContentsMargins(0, 0, 0, 0);
    titleRow_->setSpacing(6);

    title_ = new QLabel(title, this);
    title_->setObjectName("rowTitle");
    titleRow_->addWidget(title_);
    titleRow_->addStretch(1);
    textColumn->addLayout(titleRow_);

    description_ = new QLabel(description, this);
    description_->setObjectName("rowDesc");
    description_->setWordWrap(true);
    description_->setVisible(!description.isEmpty());
    textColumn->addWidget(description_);

    layout_->addLayout(textColumn, 1);
}

void BoxedRow::setPill(const QString& text) {
    if (!pill_) {
        pill_ = new QLabel(this);
        pill_->setObjectName("pill");
        // Inserted at index 1, immediately after the title and before the
        // stretch that keeps both pushed left.
        titleRow_->insertWidget(1, pill_);
    }
    pill_->setText(text);
}

void BoxedRow::useMonospaceDescription() {
    QFont mono = QFontDatabase::systemFont(QFontDatabase::FixedFont);
    mono.setPixelSize(11);
    description_->setFont(mono);
}

void BoxedRow::addControl(QWidget* control) {
    control->setParent(this);
    layout_->addWidget(control);
}

BoxedGroup::BoxedGroup(QWidget* parent) : QFrame(parent) {
    setObjectName("boxedGroup");

    layout_ = new QVBoxLayout(this);
    layout_->setContentsMargins(0, 0, 0, 0);
    layout_->setSpacing(0);
}

void BoxedGroup::addRow(QWidget* row) {
    if (rowCount_ > 0) {
        auto* separator = new QFrame(this);
        separator->setObjectName("rowSeparator");
        separator->setFixedHeight(1);
        layout_->addWidget(separator);
    }
    row->setParent(this);
    layout_->addWidget(row);
    ++rowCount_;
}

void BoxedGroup::clearRows() {
    QLayoutItem* item = nullptr;
    while ((item = layout_->takeAt(0)) != nullptr) {
        delete item->widget();
        delete item;
    }
    rowCount_ = 0;
}

QLabel* makeSectionLabel(const QString& text, QWidget* parent) {
    auto* label = new QLabel(text, parent);
    label->setObjectName("sectionLabel");
    label->setFont(theme::sectionLabelFont());
    return label;
}

QWidget* makeSectionHeader(const QString& text, const QString& buttonText,
                            QPushButton** outButton, QWidget* parent) {
    auto* header = new QWidget(parent);
    auto* row = new QHBoxLayout(header);
    row->setContentsMargins(0, 0, 0, 0);
    row->setSpacing(8);

    row->addWidget(makeSectionLabel(text, header));
    row->addStretch(1);

    auto* button = new QPushButton(buttonText, header);
    button->setObjectName("toolbarButton");
    button->setCursor(Qt::PointingHandCursor);
    row->addWidget(button);

    if (outButton) *outButton = button;
    return header;
}

} // namespace tuxblox
