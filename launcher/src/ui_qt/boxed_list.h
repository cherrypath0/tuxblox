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
#include <QWidget>

class QHBoxLayout;
class QLabel;
class QPushButton;
class QVBoxLayout;

namespace tuxblox {

// One line in a settings group: a name, an optional sentence explaining
// what it does, and the control that changes it pinned to the right.
//
// Grouping controls this way, rather than stacking a label and its widget
// with a hint floating underneath, is the one idea worth borrowing from
// Bottles and GNOME's settings apps -- it keeps every option's explanation
// attached to the option itself.
class BoxedRow : public QWidget {
    Q_OBJECT
public:
    explicit BoxedRow(const QString& title, const QString& description = QString(),
                       QWidget* parent = nullptr);

    // A small brand-tinted badge after the title, e.g. "Active".
    void setPill(const QString& text);

    // Renders the description in the system's fixed-width font, for version
    // hashes and other machine-readable values.
    void useMonospaceDescription();

    // Controls are added left to right, so the last one added sits furthest
    // right. The row takes ownership.
    void addControl(QWidget* control);

private:
    QHBoxLayout* layout_ = nullptr;
    QHBoxLayout* titleRow_ = nullptr;
    QLabel* title_ = nullptr;
    QLabel* description_ = nullptr;
    QLabel* pill_ = nullptr;
};

// A rounded, bordered container holding BoxedRows separated by hairlines.
// Rows are plain widgets, so anything can go in one.
class BoxedGroup : public QFrame {
    Q_OBJECT
public:
    explicit BoxedGroup(QWidget* parent = nullptr);

    void addRow(QWidget* row);

    // Deletes every row and separator. Used by the Versions tab, which
    // rebuilds its list whenever the installed versions change.
    void clearRows();

private:
    QVBoxLayout* layout_ = nullptr;
    int rowCount_ = 0;
};

// The small uppercase heading that introduces a group of rows.
QLabel* makeSectionLabel(const QString& text, QWidget* parent = nullptr);

// The same heading with a small button pushed to the right of it, for a
// section that can be added to. `outButton` receives the button so the
// caller can connect to it.
QWidget* makeSectionHeader(const QString& text, const QString& buttonText,
                            QPushButton** outButton, QWidget* parent = nullptr);

} // namespace tuxblox
