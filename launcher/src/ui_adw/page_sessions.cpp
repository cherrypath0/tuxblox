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

#include "page_sessions.h"

#include "asset_chevron_down.h"
#include "asset_chevron_up.h"
#include "asset_close.h"
#include "asset_copy.h"
#include "asset_folder.h"
#include "asset_roblox_player.h"
#include "asset_roblox_studio.h"
#include "asset_save.h"
#include "asset_search.h"
#include "asset_stop.h"
#include "open_url.h"
#include "widgets.h"

#include <algorithm>
#include <fstream>

namespace tuxblox {

namespace {

// The log is read as columns, so it is pinned to the face TuxBlox ships rather than to whatever the machine calls monospace -- on some that is a font which draws a 7 as a bare slash.
void useBundledMonospace() {
    static bool done = false;
    if (done) return;
    done = true;
    GtkCssProvider *pProvider = gtk_css_provider_new();
    gtk_css_provider_load_from_string(
        pProvider, "textview.sessionlog, textview.sessionlog text { font-family: \"Adwaita Mono\", monospace; font-size: 11px; }\n");
    gtk_style_context_add_provider_for_display(gdk_display_get_default(), GTK_STYLE_PROVIDER(pProvider),
                                               GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
    g_object_unref(pProvider);
}

GtkWidget *sectionLabel(const char *pText) {
    GtkWidget *pLabel = gtk_label_new(pText);
    gtk_label_set_xalign(GTK_LABEL(pLabel), 0.0f);
    gtk_widget_add_css_class(pLabel, "caption-heading");
    gtk_widget_add_css_class(pLabel, "dim-label");
    gtk_widget_set_margin_start(pLabel, 10);
    gtk_widget_set_margin_top(pLabel, 10);
    gtk_widget_set_margin_bottom(pLabel, 2);
    return pLabel;
}

GtkWidget *iconButton(const unsigned char *pIcon, size_t length, const char *pTooltip) {
    GtkWidget *pButton = gtk_button_new();
    gtk_button_set_child(GTK_BUTTON(pButton), iconImage(pIcon, length, 16));
    gtk_widget_add_css_class(pButton, "flat");
    gtk_widget_set_tooltip_text(pButton, pTooltip);
    return pButton;
}

} // namespace

SessionsPage::SessionsPage(App &app) : app_(app) {
    useBundledMonospace();

    pRoot_ = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    gtk_box_append(GTK_BOX(pRoot_), buildList());

    GtkWidget *pSeparator = gtk_separator_new(GTK_ORIENTATION_VERTICAL);
    gtk_box_append(GTK_BOX(pRoot_), pSeparator);
    gtk_box_append(GTK_BOX(pRoot_), buildLogPane());
}

GtkWidget *SessionsPage::widget() const {
    return pRoot_;
}

GtkWidget *SessionsPage::buildList() {
    GtkWidget *pColumn = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    // A floor, not a fixed width: the window can be taken down to 640 across and the list has to give way rather than hold it open.
    gtk_widget_set_size_request(pColumn, 168, -1);

    pSearch_ = gtk_search_entry_new();
    gtk_widget_set_margin_start(pSearch_, 8);
    gtk_widget_set_margin_end(pSearch_, 8);
    gtk_widget_set_margin_top(pSearch_, 8);
    gtk_widget_set_margin_bottom(pSearch_, 4);
    gtk_editable_set_text(GTK_EDITABLE(pSearch_), "");
    g_signal_connect(pSearch_, "changed", G_CALLBACK(onSearchChanged), this);
    gtk_box_append(GTK_BOX(pColumn), pSearch_);

    pListBox_ = gtk_list_box_new();
    gtk_widget_add_css_class(pListBox_, "navigation-sidebar");
    gtk_list_box_set_selection_mode(GTK_LIST_BOX(pListBox_), GTK_SELECTION_SINGLE);
    g_signal_connect(pListBox_, "row-activated", G_CALLBACK(onRowActivated), this);

    pEmpty_ = gtk_label_new("");
    gtk_label_set_wrap(GTK_LABEL(pEmpty_), TRUE);
    gtk_label_set_justify(GTK_LABEL(pEmpty_), GTK_JUSTIFY_CENTER);
    gtk_widget_add_css_class(pEmpty_, "dim-label");
    gtk_widget_set_margin_start(pEmpty_, 14);
    gtk_widget_set_margin_end(pEmpty_, 14);
    gtk_widget_set_margin_top(pEmpty_, 24);
    gtk_widget_set_visible(pEmpty_, FALSE);

    GtkWidget *pInner = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_box_append(GTK_BOX(pInner), pEmpty_);
    gtk_box_append(GTK_BOX(pInner), pListBox_);

    GtkWidget *pScroll = gtk_scrolled_window_new();
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(pScroll), GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(pScroll), pInner);
    gtk_widget_set_vexpand(pScroll, TRUE);
    gtk_box_append(GTK_BOX(pColumn), pScroll);
    return pColumn;
}

GtkWidget *SessionsPage::toggleButton(Toggle &toggle, const char *pLabel, bool on) {
    toggle.pButton = gtk_toggle_button_new_with_label(pLabel);
    toggle.on = on;
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(toggle.pButton), on);
    gtk_widget_add_css_class(toggle.pButton, "flat");
    g_object_set_data(G_OBJECT(toggle.pButton), "tuxblox-toggle", &toggle);
    g_signal_connect(toggle.pButton, "clicked", G_CALLBACK(onToggle), this);
    return toggle.pButton;
}

GtkWidget *SessionsPage::buildControls() {
    GtkWidget *pStrip = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 4);
    gtk_widget_set_margin_start(pStrip, 12);
    gtk_widget_set_margin_end(pStrip, 8);
    gtk_widget_set_margin_top(pStrip, 4);
    gtk_widget_set_margin_bottom(pStrip, 4);

    gtk_box_append(GTK_BOX(pStrip), toggleButton(follow_, "Follow", true));
    gtk_box_append(GTK_BOX(pStrip), toggleButton(wrap_, "Wrap", true));
    gtk_box_append(GTK_BOX(pStrip), toggleButton(tidy_, "Tidy", true));
    gtk_box_append(GTK_BOX(pStrip), toggleButton(errorsOnly_, "Errors", false));

    GtkWidget *pSpacer = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    gtk_widget_set_hexpand(pSpacer, TRUE);
    gtk_box_append(GTK_BOX(pStrip), pSpacer);

    GtkWidget *pCopy = iconButton(kAssetCopy, kAssetCopyLen, "Copy the log");
    g_signal_connect(pCopy, "clicked", G_CALLBACK(onCopyClicked), this);
    gtk_box_append(GTK_BOX(pStrip), pCopy);

    GtkWidget *pSave = iconButton(kAssetSave, kAssetSaveLen, "Save the log to a file");
    g_signal_connect(pSave, "clicked", G_CALLBACK(onSaveClicked), this);
    gtk_box_append(GTK_BOX(pStrip), pSave);

    GtkWidget *pFolder = iconButton(kAssetFolder, kAssetFolderLen, "Open the folder the logs are in");
    g_signal_connect(pFolder, "clicked", G_CALLBACK(onFolderClicked), this);
    gtk_box_append(GTK_BOX(pStrip), pFolder);

    // Scrolled sideways rather than widening the window: at the narrowest the window goes to, the strip is wider than the pane it sits in.
    GtkWidget *pScroll = gtk_scrolled_window_new();
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(pScroll), GTK_POLICY_AUTOMATIC, GTK_POLICY_NEVER);
    gtk_scrolled_window_set_propagate_natural_width(GTK_SCROLLED_WINDOW(pScroll), FALSE);
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(pScroll), pStrip);
    gtk_widget_set_size_request(pScroll, 120, -1);
    return pScroll;
}

GtkWidget *SessionsPage::buildFindBar() {
    GtkWidget *pBar = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
    gtk_widget_set_margin_start(pBar, 10);
    gtk_widget_set_margin_end(pBar, 8);
    gtk_widget_set_margin_top(pBar, 5);
    gtk_widget_set_margin_bottom(pBar, 6);

    pFind_ = gtk_search_entry_new();
    gtk_widget_set_hexpand(pFind_, TRUE);
    g_signal_connect(pFind_, "changed", G_CALLBACK(onFindChanged), this);
    gtk_box_append(GTK_BOX(pBar), pFind_);

    GtkWidget *pCount = gtk_label_new("");
    pFindCount_ = GTK_LABEL(pCount);
    gtk_widget_add_css_class(pCount, "dim-label");
    gtk_widget_add_css_class(pCount, "caption");
    gtk_box_append(GTK_BOX(pBar), pCount);

    GtkWidget *pPrevious = iconButton(kAssetChevronUp, kAssetChevronUpLen, "Previous match");
    g_object_set_data(G_OBJECT(pPrevious), "tuxblox-step", GINT_TO_POINTER(-1));
    g_signal_connect(pPrevious, "clicked", G_CALLBACK(onFindStep), this);
    gtk_box_append(GTK_BOX(pBar), pPrevious);

    GtkWidget *pNext = iconButton(kAssetChevronDown, kAssetChevronDownLen, "Next match");
    g_object_set_data(G_OBJECT(pNext), "tuxblox-step", GINT_TO_POINTER(1));
    g_signal_connect(pNext, "clicked", G_CALLBACK(onFindStep), this);
    gtk_box_append(GTK_BOX(pBar), pNext);
    return pBar;
}

GtkWidget *SessionsPage::buildLogPane() {
    GtkWidget *pPane = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_widget_set_hexpand(pPane, TRUE);

    GtkWidget *pHeader = gtk_box_new(GTK_ORIENTATION_VERTICAL, 2);
    gtk_widget_set_margin_start(pHeader, 12);
    gtk_widget_set_margin_end(pHeader, 8);
    gtk_widget_set_margin_top(pHeader, 8);
    gtk_widget_set_margin_bottom(pHeader, 6);

    GtkWidget *pTitleRow = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    GtkWidget *pName = gtk_label_new("No session selected");
    pName_ = GTK_LABEL(pName);
    gtk_label_set_xalign(GTK_LABEL(pName), 0.0f);
    gtk_widget_add_css_class(pName, "heading");
    gtk_box_append(GTK_BOX(pTitleRow), pName);

    GtkWidget *pState = gtk_label_new("");
    pState_ = GTK_LABEL(pState);
    gtk_widget_add_css_class(pState, "caption-heading");
    gtk_widget_set_hexpand(pState, TRUE);
    gtk_label_set_xalign(GTK_LABEL(pState), 0.0f);
    gtk_box_append(GTK_BOX(pTitleRow), pState);

    pStop_ = gtk_button_new_with_label("Stop");
    gtk_widget_add_css_class(pStop_, "destructive-action");
    gtk_widget_set_visible(pStop_, FALSE);
    g_signal_connect(pStop_, "clicked", G_CALLBACK(onStopClicked), this);
    gtk_box_append(GTK_BOX(pTitleRow), pStop_);
    gtk_box_append(GTK_BOX(pHeader), pTitleRow);

    GtkWidget *pMeta = gtk_label_new("");
    pMeta_ = GTK_LABEL(pMeta);
    gtk_label_set_xalign(GTK_LABEL(pMeta), 0.0f);
    gtk_widget_add_css_class(pMeta, "caption");
    gtk_widget_add_css_class(pMeta, "dim-label");
    gtk_label_set_ellipsize(GTK_LABEL(pMeta), PANGO_ELLIPSIZE_END);
    gtk_box_append(GTK_BOX(pHeader), pMeta);

    // Naming the file makes a session matched to the wrong log visible rather than quietly misleading.
    GtkWidget *pSources = gtk_label_new("");
    pSources_ = GTK_LABEL(pSources);
    gtk_label_set_xalign(GTK_LABEL(pSources), 0.0f);
    gtk_widget_add_css_class(pSources, "caption");
    gtk_widget_add_css_class(pSources, "dim-label");
    gtk_label_set_ellipsize(GTK_LABEL(pSources), PANGO_ELLIPSIZE_MIDDLE);
    gtk_box_append(GTK_BOX(pHeader), pSources);
    gtk_box_append(GTK_BOX(pPane), pHeader);

    gtk_box_append(GTK_BOX(pPane), gtk_separator_new(GTK_ORIENTATION_HORIZONTAL));
    gtk_box_append(GTK_BOX(pPane), buildControls());
    gtk_box_append(GTK_BOX(pPane), gtk_separator_new(GTK_ORIENTATION_HORIZONTAL));

    GtkWidget *pView = gtk_text_view_new();
    pLog_ = GTK_TEXT_VIEW(pView);
    gtk_widget_add_css_class(pView, "sessionlog");
    gtk_text_view_set_editable(pLog_, FALSE);
    gtk_text_view_set_cursor_visible(pLog_, FALSE);
    gtk_text_view_set_wrap_mode(pLog_, GTK_WRAP_WORD_CHAR);
    gtk_text_view_set_left_margin(pLog_, 10);
    gtk_text_view_set_right_margin(pLog_, 10);
    gtk_text_view_set_top_margin(pLog_, 6);
    gtk_text_view_set_bottom_margin(pLog_, 6);
    pBuffer_ = gtk_text_view_get_buffer(pLog_);

    gtk_text_buffer_create_tag(pBuffer_, "time", "foreground", "#6b7687", nullptr);
    gtk_text_buffer_create_tag(pBuffer_, "warning", "foreground", "#f5b73d", nullptr);
    gtk_text_buffer_create_tag(pBuffer_, "error", "foreground", "#f87171", nullptr);
    gtk_text_buffer_create_tag(pBuffer_, "ours", "foreground", "#7fd1a8", nullptr);
    gtk_text_buffer_create_tag(pBuffer_, "match", "background", "#f59e0b", "foreground", "#0a0c11", nullptr);

    pLogScroll_ = gtk_scrolled_window_new();
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(pLogScroll_), pView);
    gtk_widget_set_vexpand(pLogScroll_, TRUE);
    gtk_box_append(GTK_BOX(pPane), pLogScroll_);

    gtk_box_append(GTK_BOX(pPane), gtk_separator_new(GTK_ORIENTATION_HORIZONTAL));
    gtk_box_append(GTK_BOX(pPane), buildFindBar());
    return pPane;
}

void SessionsPage::rebuildList(const SessionsView &view) {
    for (GtkWidget *pRow : rows_) gtk_list_box_remove(GTK_LIST_BOX(pListBox_), pRow);
    rows_.clear();

    auto addGroup = [&](const char *pTitle, const std::vector<SessionRow> &group) {
        if (group.empty()) return;
        GtkWidget *pHeaderRow = gtk_list_box_row_new();
        gtk_list_box_row_set_selectable(GTK_LIST_BOX_ROW(pHeaderRow), FALSE);
        gtk_list_box_row_set_activatable(GTK_LIST_BOX_ROW(pHeaderRow), FALSE);
        gtk_list_box_row_set_child(GTK_LIST_BOX_ROW(pHeaderRow), sectionLabel(pTitle));
        gtk_list_box_append(GTK_LIST_BOX(pListBox_), pHeaderRow);
        rows_.push_back(pHeaderRow);

        for (const SessionRow &entry : group) {
            const bool player = entry.name == "Roblox Player";
            GtkWidget *pIcon = iconImage(player ? kAssetRobloxPlayer : kAssetRobloxStudio,
                                         player ? kAssetRobloxPlayerLen : kAssetRobloxStudioLen, 18);
            gtk_widget_set_valign(pIcon, GTK_ALIGN_CENTER);
            if (!entry.running) gtk_widget_set_opacity(pIcon, 0.6);

            GtkWidget *pName = gtk_label_new(entry.name.c_str());
            gtk_label_set_xalign(GTK_LABEL(pName), 0.0f);
            GtkWidget *pMeta = gtk_label_new(entry.meta.c_str());
            gtk_label_set_xalign(GTK_LABEL(pMeta), 0.0f);
            gtk_label_set_ellipsize(GTK_LABEL(pMeta), PANGO_ELLIPSIZE_END);
            gtk_widget_add_css_class(pMeta, "caption");
            gtk_widget_add_css_class(pMeta, entry.crashed ? "error" : "dim-label");

            GtkWidget *pText = gtk_box_new(GTK_ORIENTATION_VERTICAL, 1);
            gtk_widget_set_hexpand(pText, TRUE);
            gtk_box_append(GTK_BOX(pText), pName);
            gtk_box_append(GTK_BOX(pText), pMeta);

            GtkWidget *pBox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 9);
            gtk_box_append(GTK_BOX(pBox), pIcon);
            gtk_box_append(GTK_BOX(pBox), pText);

            if (entry.running) {
                GtkWidget *pStop = iconButton(kAssetStop, kAssetStopLen, "Stop this session");
                gtk_widget_set_valign(pStop, GTK_ALIGN_CENTER);
                g_object_set_data(G_OBJECT(pStop), "tuxblox-pid", GINT_TO_POINTER(entry.pid));
                g_signal_connect(pStop, "clicked", G_CALLBACK(onRowStopClicked), this);
                gtk_box_append(GTK_BOX(pBox), pStop);
            }

            GtkWidget *pRow = gtk_list_box_row_new();
            gtk_list_box_row_set_child(GTK_LIST_BOX_ROW(pRow), pBox);
            g_object_set_data(G_OBJECT(pRow), "tuxblox-pid", GINT_TO_POINTER(entry.pid));
            gtk_list_box_append(GTK_LIST_BOX(pListBox_), pRow);
            rows_.push_back(pRow);
            if (entry.pid == selectedPid_) gtk_list_box_select_row(GTK_LIST_BOX(pListBox_), GTK_LIST_BOX_ROW(pRow));
        }
    };

    addGroup("RUNNING", view.running);
    addGroup("ENDED THIS RUN", view.ended);

    gtk_widget_set_visible(pEmpty_, !view.emptyMessage.empty());
    if (!view.emptyMessage.empty()) gtk_label_set_text(GTK_LABEL(pEmpty_), view.emptyMessage.c_str());
}

void SessionsPage::selectSession(int pid) {
    if (pid == selectedPid_) return;
    selectedPid_ = pid;
    entries_.clear();
    matches_.clear();
    currentMatch_ = 0;
    pReader_.reset();
    gtk_text_buffer_set_text(pBuffer_, "", 0);

    for (const Session &session : app_.snapshot().sessionList) {
        if (session.pid != pid) continue;
        pReader_ = std::make_unique<SessionLogReader>(session.capturedLogPath, session.robloxLogPath);
        break;
    }
    if (pReader_ != nullptr) {
        gtk_label_set_text(pSources_, ("Reading " + pReader_->describeSources()).c_str());
    } else {
        gtk_label_set_text(pSources_, "");
    }
}

void SessionsPage::appendEntry(const LogEntry &entry) {
    if (errorsOnly_.on && entry.level == LogLevel::Plain) return;

    GtkTextIter end;
    gtk_text_buffer_get_end_iter(pBuffer_, &end);
    if (!entry.time.empty()) {
        gtk_text_buffer_insert_with_tags_by_name(pBuffer_, &end, (entry.time + "  ").c_str(), -1, "time", nullptr);
        gtk_text_buffer_get_end_iter(pBuffer_, &end);
    }

    const char *pTag = nullptr;
    if (entry.level == LogLevel::Error) {
        pTag = "error";
    } else if (entry.level == LogLevel::Warning) {
        pTag = "warning";
    } else if (entry.source == LogSource::TuxBlox) {
        pTag = "ours";
    }

    std::string body = entry.text;
    // The lines that continue a message are indented under it, so a traceback reads as one thing.
    for (const std::string &more : entry.continuation) body += "\n    " + more;
    body += "\n";

    if (pTag != nullptr) {
        gtk_text_buffer_insert_with_tags_by_name(pBuffer_, &end, body.c_str(), -1, pTag, nullptr);
    } else {
        gtk_text_buffer_insert(pBuffer_, &end, body.c_str(), -1);
    }
}

void SessionsPage::showEntries() {
    gtk_text_buffer_set_text(pBuffer_, "", 0);
    for (const LogEntry &entry : entries_) appendEntry(entry);
    applyFind();
}

void SessionsPage::applyFind() {
    GtkTextIter start;
    GtkTextIter end;
    gtk_text_buffer_get_bounds(pBuffer_, &start, &end);
    gtk_text_buffer_remove_tag_by_name(pBuffer_, "match", &start, &end);

    const std::string query = gtk_editable_get_text(GTK_EDITABLE(pFind_));
    matches_.clear();
    if (query.empty()) {
        gtk_label_set_text(pFindCount_, "");
        return;
    }

    int found = 0;
    GtkTextIter from = start;
    for (;;) {
        GtkTextIter hitStart;
        GtkTextIter hitEnd;
        if (!gtk_text_iter_forward_search(&from, query.c_str(), GTK_TEXT_SEARCH_CASE_INSENSITIVE,
                                          &hitStart, &hitEnd, nullptr)) {
            break;
        }
        gtk_text_buffer_apply_tag_by_name(pBuffer_, "match", &hitStart, &hitEnd);
        if (found == static_cast<int>(currentMatch_)) {
            gtk_text_view_scroll_to_iter(pLog_, &hitStart, 0.1, FALSE, 0.0, 0.0);
        }
        ++found;
        from = hitEnd;
    }

    if (found == 0) {
        gtk_label_set_text(pFindCount_, "no matches");
        return;
    }
    if (currentMatch_ >= static_cast<size_t>(found)) currentMatch_ = 0;
    gtk_label_set_text(pFindCount_,
                       (std::to_string(currentMatch_ + 1) + " of " + std::to_string(found)).c_str());
}

void SessionsPage::refreshHeader(const AppSnapshot &snap) {
    for (const Session &session : snap.sessionList) {
        if (session.pid != selectedPid_) continue;
        gtk_label_set_text(pName_, session.target == LaunchTarget::Player ? "Roblox Player" : "Roblox Studio");
        gtk_label_set_text(pState_, session.running ? "running" : "ended");
        gtk_widget_set_visible(pStop_, session.running);

        const SessionsView view = sessionsView({session}, std::time(nullptr));
        const std::vector<SessionRow> &group = session.running ? view.running : view.ended;
        if (!group.empty()) gtk_label_set_text(pMeta_, group.front().meta.c_str());

        if (pReader_ != nullptr && !pReader_->robloxLogAppeared() && session.robloxLogPath.empty()) {
            gtk_label_set_text(pSources_, "Roblox has not written a log for this session yet.");
        }
        return;
    }
    gtk_label_set_text(pName_, "No session selected");
    gtk_label_set_text(pState_, "");
    gtk_label_set_text(pMeta_, "");
    gtk_widget_set_visible(pStop_, FALSE);
}

void SessionsPage::update(const AppSnapshot &snap) {
    const SessionsView all = sessionsView(snap.sessionList, std::time(nullptr));
    const SessionsView shown = filterSessions(all, gtk_editable_get_text(GTK_EDITABLE(pSearch_)));
    rebuildList(shown);

    // Nothing is selected yet, so the newest running session is a better first thing to look at than an empty pane.
    if (selectedPid_ == 0 && !shown.running.empty()) selectSession(shown.running.back().pid);
    refreshHeader(snap);

    // Reading two files is only worth doing while somebody is looking at them; this runs on the window's own tick, which fires for every page whether it is on screen or not.
    if (snap.activeTab != Tab::Sessions || pReader_ == nullptr) return;

    std::vector<LogEntry> fresh = pReader_->pump();
    if (fresh.empty()) return;

    const bool atEnd = follow_.on;
    for (const LogEntry &entry : fresh) {
        entries_.push_back(entry);
        appendEntry(entry);
    }
    if (!renderedQuery_.empty()) applyFind();
    if (atEnd) {
        GtkTextIter end;
        gtk_text_buffer_get_end_iter(pBuffer_, &end);
        gtk_text_view_scroll_to_iter(pLog_, &end, 0.0, FALSE, 0.0, 0.0);
    }
}

void SessionsPage::onSearchChanged(GtkEditable *, gpointer data) {
    auto *pSelf = static_cast<SessionsPage *>(data);
    pSelf->update(pSelf->app_.snapshot());
}

void SessionsPage::onRowActivated(GtkListBox *, GtkListBoxRow *pRow, gpointer data) {
    auto *pSelf = static_cast<SessionsPage *>(data);
    const int pid = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(pRow), "tuxblox-pid"));
    if (pid != 0) pSelf->selectSession(pid);
}

void SessionsPage::onToggle(GtkButton *pButton, gpointer data) {
    auto *pSelf = static_cast<SessionsPage *>(data);
    auto *pToggle = static_cast<Toggle *>(g_object_get_data(G_OBJECT(pButton), "tuxblox-toggle"));
    pToggle->on = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(pButton));

    gtk_text_view_set_wrap_mode(pSelf->pLog_, pSelf->wrap_.on ? GTK_WRAP_WORD_CHAR : GTK_WRAP_NONE);
    if (pToggle == &pSelf->errorsOnly_ || pToggle == &pSelf->tidy_) pSelf->showEntries();
}

void SessionsPage::onStopClicked(GtkButton *, gpointer data) {
    auto *pSelf = static_cast<SessionsPage *>(data);
    pSelf->app_.requestStopSession(pSelf->selectedPid_);
}

void SessionsPage::onRowStopClicked(GtkButton *pButton, gpointer data) {
    auto *pSelf = static_cast<SessionsPage *>(data);
    pSelf->app_.requestStopSession(GPOINTER_TO_INT(g_object_get_data(G_OBJECT(pButton), "tuxblox-pid")));
}

void SessionsPage::onCopyClicked(GtkButton *, gpointer data) {
    auto *pSelf = static_cast<SessionsPage *>(data);
    GtkTextIter start;
    GtkTextIter end;
    gtk_text_buffer_get_bounds(pSelf->pBuffer_, &start, &end);
    char *pText = gtk_text_buffer_get_text(pSelf->pBuffer_, &start, &end, FALSE);
    gdk_clipboard_set_text(gtk_widget_get_clipboard(GTK_WIDGET(pSelf->pLog_)), pText);
    g_free(pText);
}

void SessionsPage::onSaveClicked(GtkButton *, gpointer data) {
    auto *pSelf = static_cast<SessionsPage *>(data);
    GtkTextIter start;
    GtkTextIter end;
    gtk_text_buffer_get_bounds(pSelf->pBuffer_, &start, &end);
    char *pText = gtk_text_buffer_get_text(pSelf->pBuffer_, &start, &end, FALSE);

    const std::string target = std::string(g_get_home_dir()) + "/TuxBlox-session-" +
                               std::to_string(pSelf->selectedPid_) + ".log";
    std::ofstream(target) << pText;
    g_free(pText);
    gtk_label_set_text(pSelf->pSources_, ("Saved to " + target).c_str());
}

void SessionsPage::onFolderClicked(GtkButton *, gpointer data) {
    auto *pSelf = static_cast<SessionsPage *>(data);
    for (const Session &session : pSelf->app_.snapshot().sessionList) {
        if (session.pid != pSelf->selectedPid_ || session.capturedLogPath.empty()) continue;
        const size_t slash = session.capturedLogPath.rfind('/');
        if (slash == std::string::npos) return;
        openUrl(("file://" + session.capturedLogPath.substr(0, slash)).c_str());
        return;
    }
}

void SessionsPage::onFindChanged(GtkEditable *pEditable, gpointer data) {
    auto *pSelf = static_cast<SessionsPage *>(data);
    pSelf->renderedQuery_ = gtk_editable_get_text(pEditable);
    pSelf->currentMatch_ = 0;
    pSelf->applyFind();
}

void SessionsPage::onFindStep(GtkButton *pButton, gpointer data) {
    auto *pSelf = static_cast<SessionsPage *>(data);
    const int step = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(pButton), "tuxblox-step"));
    if (step < 0 && pSelf->currentMatch_ == 0) {
        pSelf->currentMatch_ = 0;
    } else {
        pSelf->currentMatch_ = static_cast<size_t>(static_cast<int>(pSelf->currentMatch_) + step);
    }
    pSelf->applyFind();
}

} // namespace tuxblox
