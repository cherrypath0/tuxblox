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
#include "page.h"

#include <vector>

// Third Parties
#include <adwaita.h>

namespace tuxblox {

class FastFlagsPage : public Page {
public:
    explicit FastFlagsPage(App &app);
    GtkWidget *widget() const override;
    void update(const AppSnapshot &snap) override;

private:
    struct FlagRow {
        GtkWidget *pRow;
        GtkEntry *pName;
        GtkEntry *pValue;
    };

    // Player and Studio differ only in which half of FastFlagSet they edit
    struct Section {
        FastFlagsPage *pOwner;
        AdwPreferencesGroup *pGroup = nullptr;
        GtkWidget *pEmpty = nullptr;
        std::vector<FlagRow> rows;
    };

    // The import window while it is open, freed when it closes
    struct ImportDialog {
        FastFlagsPage *pOwner;
        Section *pSection;
        AdwDialog *pDialog = nullptr;
        GtkTextBuffer *pBuffer = nullptr;
        GtkWidget *pImportButton = nullptr;
        GtkWidget *pStatus = nullptr;
        std::vector<FastFlag> parsed;
    };

    struct RemoveAction {
        Section *pSection;
        GtkEntry *pName;
    };

    AdwPreferencesGroup *buildSection(Section &section, const char *pTitle);
    void addRow(Section &section, const FastFlag &flag);
    void rebuild(Section &section, const std::vector<FastFlag> &flags);
    void refreshEmpty(Section &section);
    void openImport(Section &section, GtkWidget *pParent);
    void importFlags(Section &section, const std::vector<FastFlag> &flags);
    std::vector<FastFlag> collect(const Section &section) const;
    void markDuplicates(Section &section);
    // Reads every row back out of the widgets and saves the result, dropping rows whose name is still empty
    void commit();

    static void onAdd(GtkButton *, gpointer data);
    static void onImportClicked(GtkButton *pButton, gpointer data);
    static void onImportTextChanged(GtkTextBuffer *, gpointer data);
    static void onImportConfirm(GtkButton *, gpointer data);
    static void onImportCancel(GtkButton *, gpointer data);
    static void onImportClosed(AdwDialog *, gpointer data);
    static void onActivate(GtkEntry *, gpointer data);
    static void onFocusLeft(GtkEventControllerFocus *, gpointer data);
    static void onRemove(GtkButton *, gpointer data);
    static void freeRemoveAction(gpointer data, GClosure *);

    App &app_;
    GtkWidget *pRoot_ = nullptr;
    Section player_{this};
    Section studio_{this};
    // What the rows were last built from or saved as, so a poll tick that changes nothing leaves the entries and the cursor alone
    FastFlagSet rendered_;
    bool renderedOnce_ = false;
    bool rebuilding_ = false;
};

} // namespace tuxblox
