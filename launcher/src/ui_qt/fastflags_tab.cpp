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

#include "fastflags_tab.h"
#include "boxed_list.h"
#include "theme.h"
#include <algorithm>
#include <QFont>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QScrollArea>
#include <QSet>
#include <QStyle>
#include <QVBoxLayout>

namespace tuxblox {

namespace {

bool flagsEqual(const std::vector<FastFlag>& a, const std::vector<FastFlag>& b) {
    if (a.size() != b.size()) return false;
    for (size_t i = 0; i < a.size(); ++i) {
        if (a[i].name != b[i].name || a[i].value != b[i].value) return false;
    }
    return true;
}

void restyle(QWidget* widget) {
    widget->style()->unpolish(widget);
    widget->style()->polish(widget);
}

} // namespace

FastFlagsTab::FastFlagsTab(App& app, QWidget* parent) : QWidget(parent), app_(app) {
    auto* outer = new QVBoxLayout(this);
    outer->setContentsMargins(0, 0, 0, 0);

    auto* scrollArea = new QScrollArea(this);
    scrollArea->setWidgetResizable(true);
    scrollArea->setFrameShape(QFrame::NoFrame);
    scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    outer->addWidget(scrollArea);

    auto* content = new QWidget(scrollArea);
    scrollArea->setWidget(content);

    auto* layout = new QVBoxLayout(content);
    layout->setContentsMargins(24, 22, 24, 22);
    layout->setSpacing(0);

    auto* title = new QLabel("FastFlags", content);
    title->setObjectName("pageTitle");
    title->setFont(theme::displayFont(21, QFont::Bold));
    layout->addWidget(title);

    auto* subtitle = new QLabel(
        "FastFlags are internal Roblox settings that you can configure. "
        "Note that Roblox has a FastFlag allowlist, and Roblox may ban or take action against your account for using this.",
        content);
    subtitle->setObjectName("pageSubtitle");
    subtitle->setWordWrap(true);
    layout->addSpacing(4);
    layout->addWidget(subtitle);

    layout->addSpacing(18);
    layout->addWidget(buildSection(player_, "Player", content));
    layout->addSpacing(18);
    layout->addWidget(buildSection(studio_, "Studio", content));
    layout->addStretch(1);
}

QWidget* FastFlagsTab::buildSection(Section& section, const QString& title, QWidget* parent) {
    auto* container = new QWidget(parent);
    auto* layout = new QVBoxLayout(container);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(8);

    QPushButton* addButton = nullptr;
    layout->addWidget(makeSectionHeader(title, "+ Add flag", &addButton, container));
    connect(addButton, &QPushButton::clicked, this, [this, &section] {
        // Not committed yet: an empty row has no name, so there is nothing to
        // save until the user types one.
        addRow(section, FastFlag{});
        section.rows.back().name->setFocus();
    });

    section.group = new BoxedGroup(container);
    layout->addWidget(section.group);

    section.emptyNotice = new QLabel("No flags set.", container);
    section.emptyNotice->setObjectName("rowDesc");
    layout->addWidget(section.emptyNotice);

    return container;
}

void FastFlagsTab::addRow(Section& section, const FastFlag& flag) {
    auto* row = new QWidget();
    row->setObjectName("boxedRow");
    auto* layout = new QHBoxLayout(row);
    layout->setContentsMargins(14, 8, 14, 8);
    layout->setSpacing(8);

    auto* name = new QLineEdit(QString::fromStdString(flag.name), row);
    name->setPlaceholderText("FlagName");
    layout->addWidget(name, 3);

    auto* value = new QLineEdit(QString::fromStdString(flag.value), row);
    value->setPlaceholderText("value");
    layout->addWidget(value, 2);

    auto* remove = new QPushButton("\xE2\x88\x92", row); // minus sign, not a hyphen
    remove->setObjectName("dangerButton");
    remove->setFixedWidth(32);
    remove->setToolTip("Remove this flag");
    remove->setCursor(Qt::PointingHandCursor);
    layout->addWidget(remove);

    section.rows.push_back({name, value});
    section.group->addRow(row);
    refreshSectionVisibility(section);

    connect(name, &QLineEdit::editingFinished, this, [this] { commitFromRows(); });
    connect(value, &QLineEdit::editingFinished, this, [this] { commitFromRows(); });
    connect(remove, &QPushButton::clicked, this, [this, &section, name] {
        // Found by identity rather than index: rows above this one may have
        // been removed since this handler was connected.
        auto it = std::find_if(section.rows.begin(), section.rows.end(),
                                [name](const FlagRow& r) { return r.name == name; });
        if (it != section.rows.end()) section.rows.erase(it);
        commitFromRows();
        // The removed row's widgets are still in the group's layout, so the
        // sections have to be rebuilt from what was just saved to dispose of
        // them. commitFromRows() already recorded that as rendered_.
        rebuildSection(player_, rendered_.player);
        rebuildSection(studio_, rendered_.studio);
    });
}

std::vector<FastFlag> FastFlagsTab::collectRows(const Section& section) const {
    std::vector<FastFlag> flags;
    for (const auto& row : section.rows) {
        FastFlag flag{row.name->text().trimmed().toStdString(),
                       row.value->text().trimmed().toStdString()};
        if (flag.name.empty()) continue; // a row the user has not named yet
        flags.push_back(std::move(flag));
    }
    return flags;
}

void FastFlagsTab::markDuplicates(Section& section) {
    QSet<QString> seen;
    for (auto& row : section.rows) {
        const QString name = row.name->text().trimmed();
        const bool duplicate = !name.isEmpty() && seen.contains(name);
        if (!name.isEmpty()) seen.insert(name);
        if (row.name->property("duplicate").toBool() == duplicate) continue;
        row.name->setProperty("duplicate", duplicate);
        row.name->setToolTip(duplicate ? "Another row already sets this flag. The last one wins."
                                        : QString());
        restyle(row.name);
    }
}

void FastFlagsTab::commitFromRows() {
    markDuplicates(player_);
    markDuplicates(studio_);

    Settings updated = app_.snapshot().settings;
    updated.fastFlags.player = collectRows(player_);
    updated.fastFlags.studio = collectRows(studio_);
    // Remember what was just saved, so the poll tick that follows sees the
    // widgets and the snapshot agreeing and leaves the rows alone.
    rendered_ = updated.fastFlags;
    renderedOnce_ = true;
    app_.updateSettings(updated);
}

void FastFlagsTab::rebuildSection(Section& section, const std::vector<FastFlag>& flags) {
    section.group->clearRows();
    section.rows.clear();
    for (const auto& flag : flags) addRow(section, flag);
    refreshSectionVisibility(section);
}

// The one place that decides whether a section shows its rows or its "no
// flags" line. Adding, removing and rebuilding all go through here, so an
// empty group can never be left visible or a populated one left hidden.
void FastFlagsTab::refreshSectionVisibility(Section& section) {
    const bool hasRows = !section.rows.empty();
    section.group->setVisible(hasRows);
    section.emptyNotice->setVisible(!hasRows);
}

void FastFlagsTab::updateFromSnapshot(const AppSnapshot& snap) {
    // Only rebuild when the stored flags actually differ from what the rows
    // are showing. This runs on MainWindow's 100ms poll -- rebuilding every
    // tick would destroy the QLineEdit the user is typing into.
    if (renderedOnce_ && flagsEqual(snap.settings.fastFlags.player, rendered_.player) &&
        flagsEqual(snap.settings.fastFlags.studio, rendered_.studio)) {
        return;
    }
    rebuildSection(player_, snap.settings.fastFlags.player);
    rebuildSection(studio_, snap.settings.fastFlags.studio);
    rendered_ = snap.settings.fastFlags;
    renderedOnce_ = true;
}

} // namespace tuxblox
