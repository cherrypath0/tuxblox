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

#include "page_fastflags.h"
#include "fastflag_import.h"
#include "widgets.h"

#include <algorithm>
#include <set>
#include <string>

namespace tuxblox {

namespace {

bool sameFlags(const std::vector<FastFlag> &a, const std::vector<FastFlag> &b) {
    if (a.size() != b.size()) return false;
    for (size_t i = 0; i < a.size(); ++i) {
        if (a[i].name != b[i].name || a[i].value != b[i].value) return false;
    }
    return true;
}

std::string trimmedText(GtkEntry *pEntry) {
    const std::string text = gtk_editable_get_text(GTK_EDITABLE(pEntry));
    const size_t first = text.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) return "";
    const size_t last = text.find_last_not_of(" \t\r\n");
    return text.substr(first, last - first + 1);
}

} // namespace

FastFlagsPage::FastFlagsPage(App &app) : app_(app) {
    pRoot_ = adw_preferences_page_new();
    adw_preferences_page_set_description(
        ADW_PREFERENCES_PAGE(pRoot_),
        "FastFlags are hidden Roblox settings you can change yourself. Only some of them are meant to be changed, and "
        "what happens to an account that changes the rest is Roblox's decision alone.");
    adw_preferences_page_add(ADW_PREFERENCES_PAGE(pRoot_), buildSection(player_, "Player"));
    adw_preferences_page_add(ADW_PREFERENCES_PAGE(pRoot_), buildSection(studio_, "Studio"));
}

GtkWidget *FastFlagsPage::widget() const {
    return pRoot_;
}

AdwPreferencesGroup *FastFlagsPage::buildSection(Section &section, const char *pTitle) {
    GtkWidget *pGroup = adw_preferences_group_new();
    adw_preferences_group_set_title(ADW_PREFERENCES_GROUP(pGroup), pTitle);
    section.pGroup = ADW_PREFERENCES_GROUP(pGroup);

    GtkWidget *pImport = gtk_button_new_with_label("Import JSON");
    g_signal_connect(pImport, "clicked", G_CALLBACK(onImportClicked), &section);
    GtkWidget *pAdd = gtk_button_new_with_label("Add flag");
    g_signal_connect(pAdd, "clicked", G_CALLBACK(onAdd), &section);
    GtkWidget *pButtons = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
    gtk_widget_set_valign(pButtons, GTK_ALIGN_CENTER);
    gtk_box_append(GTK_BOX(pButtons), pImport);
    gtk_box_append(GTK_BOX(pButtons), pAdd);
    adw_preferences_group_set_header_suffix(section.pGroup, pButtons);

    section.pEmpty = plainActionRow("No flags set.", "");
    adw_preferences_group_add(section.pGroup, section.pEmpty);
    return section.pGroup;
}

void FastFlagsPage::addRow(Section &section, const FastFlag &flag) {
    GtkWidget *pName = gtk_entry_new();
    gtk_entry_set_placeholder_text(GTK_ENTRY(pName), "FlagName");
    gtk_editable_set_text(GTK_EDITABLE(pName), flag.name.c_str());
    gtk_widget_set_hexpand(pName, TRUE);

    GtkWidget *pValue = gtk_entry_new();
    gtk_entry_set_placeholder_text(GTK_ENTRY(pValue), "value");
    gtk_editable_set_text(GTK_EDITABLE(pValue), flag.value.c_str());
    gtk_editable_set_width_chars(GTK_EDITABLE(pValue), 12);

    GtkWidget *pRemove = gtk_button_new_with_label("\xE2\x88\x92");
    gtk_widget_add_css_class(pRemove, "destructive-action");
    gtk_widget_set_tooltip_text(pRemove, "Remove this flag");

    for (GtkWidget *pEntry : {pName, pValue}) {
        g_signal_connect(pEntry, "activate", G_CALLBACK(onActivate), this);
        GtkEventController *pFocus = gtk_event_controller_focus_new();
        g_signal_connect(pFocus, "leave", G_CALLBACK(onFocusLeft), this);
        gtk_widget_add_controller(pEntry, pFocus);
    }
    g_signal_connect_data(pRemove, "clicked", G_CALLBACK(onRemove), new RemoveAction{&section, GTK_ENTRY(pName)},
                          freeRemoveAction, static_cast<GConnectFlags>(0));

    GtkWidget *pBox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    gtk_widget_set_margin_start(pBox, 12);
    gtk_widget_set_margin_end(pBox, 12);
    gtk_widget_set_margin_top(pBox, 8);
    gtk_widget_set_margin_bottom(pBox, 8);
    gtk_box_append(GTK_BOX(pBox), pName);
    gtk_box_append(GTK_BOX(pBox), pValue);
    gtk_box_append(GTK_BOX(pBox), pRemove);

    GtkWidget *pRow = gtk_list_box_row_new();
    gtk_list_box_row_set_activatable(GTK_LIST_BOX_ROW(pRow), FALSE);
    gtk_list_box_row_set_child(GTK_LIST_BOX_ROW(pRow), pBox);
    adw_preferences_group_add(section.pGroup, pRow);

    section.rows.push_back({pRow, GTK_ENTRY(pName), GTK_ENTRY(pValue)});
    refreshEmpty(section);
}

void FastFlagsPage::refreshEmpty(Section &section) {
    gtk_widget_set_visible(section.pEmpty, section.rows.empty());
}

void FastFlagsPage::rebuild(Section &section, const std::vector<FastFlag> &flags) {
    // Removing a focused entry fires its focus-leave, which must not save a half-emptied list
    rebuilding_ = true;
    for (const FlagRow &row : section.rows) adw_preferences_group_remove(section.pGroup, row.pRow);
    section.rows.clear();
    for (const FastFlag &flag : flags) addRow(section, flag);
    refreshEmpty(section);
    rebuilding_ = false;
}

std::vector<FastFlag> FastFlagsPage::collect(const Section &section) const {
    std::vector<FastFlag> flags;
    for (const FlagRow &row : section.rows) {
        FastFlag flag{trimmedText(row.pName), trimmedText(row.pValue)};
        if (flag.name.empty()) continue;
        flags.push_back(std::move(flag));
    }
    return flags;
}

void FastFlagsPage::markDuplicates(Section &section) {
    std::set<std::string> seen;
    for (const FlagRow &row : section.rows) {
        const std::string name = trimmedText(row.pName);
        const bool duplicate = !name.empty() && seen.count(name) > 0;
        if (!name.empty()) seen.insert(name);
        if (duplicate) {
            gtk_widget_add_css_class(GTK_WIDGET(row.pName), "error");
            gtk_widget_set_tooltip_text(GTK_WIDGET(row.pName), "Another row already sets this flag. The last one wins.");
        } else {
            gtk_widget_remove_css_class(GTK_WIDGET(row.pName), "error");
            gtk_widget_set_tooltip_text(GTK_WIDGET(row.pName), nullptr);
        }
    }
}

void FastFlagsPage::commit() {
    if (rebuilding_) return;
    markDuplicates(player_);
    markDuplicates(studio_);

    Settings updated = app_.snapshot().settings;
    updated.fastFlags.player = collect(player_);
    updated.fastFlags.studio = collect(studio_);
    rendered_ = updated.fastFlags;
    renderedOnce_ = true;
    app_.updateSettings(updated);
}

void FastFlagsPage::onAdd(GtkButton *, gpointer data) {
    auto *pSection = static_cast<Section *>(data);
    // Not saved yet: an empty row has no name, so there is nothing to save until one is typed
    pSection->pOwner->addRow(*pSection, FastFlag{});
    gtk_widget_grab_focus(GTK_WIDGET(pSection->rows.back().pName));
}

void FastFlagsPage::importFlags(Section &section, const std::vector<FastFlag> &flags) {
    rebuild(section, mergeFastFlags(collect(section), flags));
    commit();
}

void FastFlagsPage::onImportClicked(GtkButton *pButton, gpointer data) {
    auto *pSection = static_cast<Section *>(data);
    pSection->pOwner->openImport(*pSection, GTK_WIDGET(pButton));
}

void FastFlagsPage::openImport(Section &section, GtkWidget *pParent) {
    auto *pImport = new ImportDialog{this, &section};

    GtkWidget *pText = gtk_text_view_new();
    gtk_text_view_set_monospace(GTK_TEXT_VIEW(pText), TRUE);
    gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(pText), GTK_WRAP_WORD_CHAR);
    gtk_text_view_set_top_margin(GTK_TEXT_VIEW(pText), 8);
    gtk_text_view_set_bottom_margin(GTK_TEXT_VIEW(pText), 8);
    gtk_text_view_set_left_margin(GTK_TEXT_VIEW(pText), 8);
    gtk_text_view_set_right_margin(GTK_TEXT_VIEW(pText), 8);
    pImport->pBuffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(pText));

    GtkWidget *pScroll = gtk_scrolled_window_new();
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(pScroll), pText);
    gtk_widget_set_vexpand(pScroll, TRUE);
    gtk_widget_add_css_class(pScroll, "card");

    pImport->pStatus = gtk_label_new("Paste the flags as JSON, like {\"FlagName\": \"true\"}.");
    gtk_label_set_wrap(GTK_LABEL(pImport->pStatus), TRUE);
    gtk_label_set_wrap_mode(GTK_LABEL(pImport->pStatus), PANGO_WRAP_WORD_CHAR);
    gtk_label_set_xalign(GTK_LABEL(pImport->pStatus), 0.0f);
    gtk_widget_add_css_class(pImport->pStatus, "dim-label");

    GtkWidget *pBody = gtk_box_new(GTK_ORIENTATION_VERTICAL, 12);
    gtk_widget_set_margin_start(pBody, 12);
    gtk_widget_set_margin_end(pBody, 12);
    gtk_widget_set_margin_top(pBody, 6);
    gtk_widget_set_margin_bottom(pBody, 12);
    gtk_box_append(GTK_BOX(pBody), pScroll);
    gtk_box_append(GTK_BOX(pBody), pImport->pStatus);

    GtkWidget *pCancel = gtk_button_new_with_label("Cancel");
    g_signal_connect(pCancel, "clicked", G_CALLBACK(onImportCancel), pImport);
    pImport->pImportButton = gtk_button_new_with_label("Import");
    gtk_widget_add_css_class(pImport->pImportButton, "suggested-action");
    gtk_widget_set_sensitive(pImport->pImportButton, FALSE);
    g_signal_connect(pImport->pImportButton, "clicked", G_CALLBACK(onImportConfirm), pImport);

    GtkWidget *pHeader = adw_header_bar_new();
    adw_header_bar_set_show_start_title_buttons(ADW_HEADER_BAR(pHeader), FALSE);
    adw_header_bar_set_show_end_title_buttons(ADW_HEADER_BAR(pHeader), FALSE);
    adw_header_bar_pack_start(ADW_HEADER_BAR(pHeader), pCancel);
    adw_header_bar_pack_end(ADW_HEADER_BAR(pHeader), pImport->pImportButton);

    GtkWidget *pView = adw_toolbar_view_new();
    adw_toolbar_view_add_top_bar(ADW_TOOLBAR_VIEW(pView), pHeader);
    adw_toolbar_view_set_content(ADW_TOOLBAR_VIEW(pView), pBody);

    AdwDialog *pDialog = adw_dialog_new();
    pImport->pDialog = pDialog;
    adw_dialog_set_title(pDialog, &section == &player_ ? "Import JSON into Player" : "Import JSON into Studio");
    adw_dialog_set_content_width(pDialog, 560);
    adw_dialog_set_content_height(pDialog, 420);
    adw_dialog_set_child(pDialog, pView);
    adw_dialog_set_focus(pDialog, pText);
    g_signal_connect(pImport->pBuffer, "changed", G_CALLBACK(onImportTextChanged), pImport);
    g_signal_connect(pDialog, "closed", G_CALLBACK(onImportClosed), pImport);
    adw_dialog_present(pDialog, pParent);
}

void FastFlagsPage::onImportTextChanged(GtkTextBuffer *pBuffer, gpointer data) {
    auto *pImport = static_cast<ImportDialog *>(data);
    GtkTextIter start, end;
    gtk_text_buffer_get_bounds(pBuffer, &start, &end);
    char *pText = gtk_text_buffer_get_text(pBuffer, &start, &end, FALSE);
    const FastFlagImport result = parseFastFlagJson(pText);
    const bool empty = std::string(pText).find_first_not_of(" \t\r\n") == std::string::npos;
    g_free(pText);

    pImport->parsed = result.flags;
    gtk_widget_set_sensitive(pImport->pImportButton, result.ok);

    GtkWidget *pStatus = pImport->pStatus;
    gtk_widget_remove_css_class(pStatus, "error");
    gtk_widget_remove_css_class(pStatus, "success");
    gtk_widget_remove_css_class(pStatus, "dim-label");
    if (result.ok) {
        gtk_widget_add_css_class(pStatus, "success");
        const std::string count = std::to_string(result.flags.size());
        gtk_label_set_text(GTK_LABEL(pStatus), (count + (result.flags.size() == 1 ? " flag is" : " flags are") + " ready to import.").c_str());
    } else if (empty) {
        gtk_widget_add_css_class(pStatus, "dim-label");
        gtk_label_set_text(GTK_LABEL(pStatus), "Paste the flags as JSON, like {\"FlagName\": \"true\"}.");
    } else {
        gtk_widget_add_css_class(pStatus, "error");
        gtk_label_set_text(GTK_LABEL(pStatus), result.error.c_str());
    }
}

void FastFlagsPage::onImportConfirm(GtkButton *, gpointer data) {
    auto *pImport = static_cast<ImportDialog *>(data);
    pImport->pOwner->importFlags(*pImport->pSection, pImport->parsed);
    adw_dialog_close(pImport->pDialog);
}

void FastFlagsPage::onImportCancel(GtkButton *, gpointer data) {
    adw_dialog_close(static_cast<ImportDialog *>(data)->pDialog);
}

void FastFlagsPage::onImportClosed(AdwDialog *, gpointer data) {
    delete static_cast<ImportDialog *>(data);
}

void FastFlagsPage::onActivate(GtkEntry *, gpointer data) {
    static_cast<FastFlagsPage *>(data)->commit();
}

void FastFlagsPage::onFocusLeft(GtkEventControllerFocus *, gpointer data) {
    static_cast<FastFlagsPage *>(data)->commit();
}

void FastFlagsPage::onRemove(GtkButton *, gpointer data) {
    auto *pAction = static_cast<RemoveAction *>(data);
    Section &section = *pAction->pSection;
    FastFlagsPage &self = *section.pOwner;
    // Found by identity rather than position, since rows above it may have been removed since it was built
    const auto found = std::find_if(section.rows.begin(), section.rows.end(),
                                    [pAction](const FlagRow &row) { return row.pName == pAction->pName; });
    if (found == section.rows.end()) return;
    GtkWidget *pRow = found->pRow;
    section.rows.erase(found);
    self.rebuilding_ = true;
    adw_preferences_group_remove(section.pGroup, pRow);
    self.rebuilding_ = false;
    self.refreshEmpty(section);
    self.commit();
}

void FastFlagsPage::freeRemoveAction(gpointer data, GClosure *) {
    delete static_cast<RemoveAction *>(data);
}

void FastFlagsPage::update(const AppSnapshot &snap) {
    if (renderedOnce_ && sameFlags(snap.settings.fastFlags.player, rendered_.player) &&
        sameFlags(snap.settings.fastFlags.studio, rendered_.studio)) {
        return;
    }
    rebuild(player_, snap.settings.fastFlags.player);
    rebuild(studio_, snap.settings.fastFlags.studio);
    rendered_ = snap.settings.fastFlags;
    renderedOnce_ = true;
}

} // namespace tuxblox
