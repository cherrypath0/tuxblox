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
#include <QWidget>
#include <vector>

class QLabel;
class QLineEdit;

namespace tuxblox {

class BoxedGroup;

// Editor for the FastFlags written into Roblox's ClientSettings folder at
// every launch. One group of rows per app, since Player and Studio read
// their own files.
//
// The rows are the source of truth while the tab is open: edits are read
// back out of the widgets and saved on every change, and the widgets are
// only rebuilt from settings.json when the underlying data actually differs
// from what is on screen. Rebuilding on every poll tick would fight the user
// for the cursor in whichever field they are typing in.
class FastFlagsTab : public QWidget {
    Q_OBJECT
public:
    explicit FastFlagsTab(App& app, QWidget* parent = nullptr);

    void updateFromSnapshot(const AppSnapshot& snap);

private:
    // One editable row: the two fields and the button that removes it.
    struct FlagRow {
        QLineEdit* name = nullptr;
        QLineEdit* value = nullptr;
    };

    // Player and Studio differ only in which half of FastFlagSet they edit,
    // so both are built from this one description rather than duplicated.
    struct Section {
        LaunchTarget target;
        BoxedGroup* group = nullptr;
        QLabel* emptyNotice = nullptr;
        std::vector<FlagRow> rows;
    };

    QWidget* buildSection(Section& section, const QString& title, QWidget* parent);
    void rebuildSection(Section& section, const std::vector<FastFlag>& flags);
    void addRow(Section& section, const FastFlag& flag);
    void refreshSectionVisibility(Section& section);

    // Reads every row back out of the widgets and writes the result to
    // settings.json, dropping rows whose name is still empty.
    void commitFromRows();

    // Marks name fields that repeat within their own section. Duplicates are
    // still saved -- the user is most likely mid-edit, and refusing to save
    // would throw away what they just typed.
    void markDuplicates(Section& section);

    std::vector<FastFlag> collectRows(const Section& section) const;

    App& app_;
    Section player_{LaunchTarget::Player};
    Section studio_{LaunchTarget::Studio};

    // What the rows were last built from, so a poll tick that changes
    // nothing leaves the widgets (and the user's cursor) alone.
    FastFlagSet rendered_;
    bool renderedOnce_ = false;
};

} // namespace tuxblox
