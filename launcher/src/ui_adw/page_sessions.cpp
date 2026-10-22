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
// Carried in the page's own provider rather than the palette's, because the palette is skipped entirely on the grey theme and these are structural rather than decorative.
void installPageStyles() {
    static bool done = false;
    if (done) return;
    done = true;
    GtkCssProvider *pProvider = gtk_css_provider_new();
    gtk_css_provider_load_from_string(pProvider,
        "textview.sessionlog, textview.sessionlog text { font-family: \"Adwaita Mono\", monospace; font-size: 12.5px; }\n"
        ".session-meta { font-size: 12px; opacity: 0.95; }\n"".log-count { font-size: 12px; opacity: 0.95; }\n"".session-stop { color: #f08b8b; }\n"".session-stop:hover { background: alpha(#dc2626, 0.22); color: #ffd7d7; }\n"
        ".session-tile { background: alpha(currentColor, 0.1); border-radius: 9px; min-width: 34px; min-height: 34px; }\n"
        ".session-dot { border-radius: 6px; min-width: 10px; min-height: 10px; border: 2px solid @sidebar_bg_color; }\n"
        ".session-dot.running { background: #34d399; }\n"
        ".session-dot.ended { background: #4b5563; }\n"
        ".session-dot.crashed { background: #b45252; }\n"
        ".session-chip { border-radius: 6px; padding: 2px 9px; font-size: 11px; font-weight: 700; }\n"
        ".session-chip.running { background: alpha(#34d399, 0.16); color: #34d399; }\n"
        ".session-chip.ended { background: alpha(currentColor, 0.1); }\n"
        ".log-toggle { font-size: 12px; padding: 3px 12px; min-height: 26px; box-shadow: inset 0 0 0 1px alpha(currentColor, 0.22); }\n"
        ".log-toggle:checked { background: alpha(#4aa8db, 0.28); color: #bfe3f7; font-weight: 700; box-shadow: inset 0 0 0 1px alpha(#4aa8db, 0.55); }\n"
        );
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
    gtk_button_set_child(GTK_BUTTON(pButton), iconImage(pIcon, length, 18));
    gtk_widget_add_css_class(pButton, "flat");
    gtk_widget_set_tooltip_text(pButton, pTooltip);
    return pButton;
}

} // namespace

SessionsPage::SessionsPage(App &app) : app_(app) {
    installPageStyles();

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
    gtk_widget_set_size_request(pColumn, 292, -1);

    pSearch_ = gtk_search_entry_new();
    gtk_widget_set_margin_start(pSearch_, 8);
    gtk_widget_set_margin_end(pSearch_, 8);
    gtk_widget_set_margin_top(pSearch_, 8);
    gtk_widget_set_margin_bottom(pSearch_, 4);
    gtk_search_entry_set_placeholder_text(GTK_SEARCH_ENTRY(pSearch_), "Search sessions");
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
    gtk_widget_add_css_class(toggle.pButton, "log-toggle");
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

    follow_.on = true;
    wrap_.on = true;
    tidy_.on = true;
    gtk_box_append(GTK_BOX(pStrip), toggleButton(errorsOnly_, "Errors only", false));

    GtkWidget *pSpacer = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    gtk_widget_set_hexpand(pSpacer, TRUE);
    gtk_box_append(GTK_BOX(pStrip), pSpacer);

    GtkWidget *pSize = gtk_label_new("");
    pSize_ = GTK_LABEL(pSize);
    gtk_widget_add_css_class(pSize, "log-count");
    gtk_widget_add_css_class(pSize, "dim-label");
    gtk_box_append(GTK_BOX(pStrip), pSize);

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
    gtk_widget_set_size_request(pScroll, 80, -1);
    return pScroll;
}

GtkWidget *SessionsPage::buildFindBar() {
    GtkWidget *pBar = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
    gtk_widget_set_margin_start(pBar, 10);
    gtk_widget_set_margin_end(pBar, 8);
    gtk_widget_set_margin_top(pBar, 5);
    gtk_widget_set_margin_bottom(pBar, 6);

    pFind_ = gtk_search_entry_new();
    gtk_search_entry_set_placeholder_text(GTK_SEARCH_ENTRY(pFind_), "Find in log");
    gtk_widget_set_size_request(pFind_, 220, -1);
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

    GtkWidget *pSpacer = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    gtk_widget_set_hexpand(pSpacer, TRUE);
    gtk_box_append(GTK_BOX(pBar), pSpacer);

    // Reading back through a log that is still being written should not keep yanking you to the bottom, so following stops and says so until you ask for it again.
    GtkWidget *pFollowState = gtk_label_new("");
    pFollowState_ = GTK_LABEL(pFollowState);
    gtk_widget_add_css_class(pFollowState, "caption");
    gtk_widget_add_css_class(pFollowState, "dim-label");
    gtk_widget_set_visible(pFollowState, FALSE);
    gtk_box_append(GTK_BOX(pBar), pFollowState);

    pJumpToEnd_ = gtk_button_new_with_label("Jump to the end");
    gtk_widget_add_css_class(pJumpToEnd_, "flat");
    gtk_widget_add_css_class(pJumpToEnd_, "log-toggle");
    gtk_widget_set_visible(pJumpToEnd_, FALSE);
    g_signal_connect(pJumpToEnd_, "clicked", G_CALLBACK(onJumpToEnd), this);
    gtk_box_append(GTK_BOX(pBar), pJumpToEnd_);
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
    gtk_widget_add_css_class(pState, "session-chip");
    gtk_widget_set_valign(pState, GTK_ALIGN_CENTER);
    gtk_box_append(GTK_BOX(pTitleRow), pState);

    GtkWidget *pMetaInline = gtk_label_new("");
    pMeta_ = GTK_LABEL(pMetaInline);
    gtk_label_set_xalign(GTK_LABEL(pMetaInline), 0.0f);
    gtk_widget_set_hexpand(pMetaInline, TRUE);
    gtk_widget_add_css_class(pMetaInline, "session-meta");
    gtk_widget_add_css_class(pMetaInline, "dim-label");
    gtk_label_set_ellipsize(GTK_LABEL(pMetaInline), PANGO_ELLIPSIZE_END);
    gtk_box_append(GTK_BOX(pTitleRow), pMetaInline);

    pStop_ = gtk_button_new_with_label("Stop session");
    gtk_widget_add_css_class(pStop_, "destructive-action");
    gtk_widget_set_visible(pStop_, FALSE);
    g_signal_connect(pStop_, "clicked", G_CALLBACK(onStopClicked), this);
    gtk_box_append(GTK_BOX(pTitleRow), pStop_);
    gtk_box_append(GTK_BOX(pHeader), pTitleRow);

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
    // A log read as columns needs air between the rows, or it reads as a block rather than as lines.
    gtk_text_view_set_pixels_above_lines(pLog_, 1);
    gtk_text_view_set_pixels_below_lines(pLog_, 2);
    gtk_text_view_set_bottom_margin(pLog_, 6);
    pBuffer_ = gtk_text_view_get_buffer(pLog_);

    gtk_text_buffer_create_tag(pBuffer_, "time", "foreground", "#6b7687", nullptr);
    gtk_text_buffer_create_tag(pBuffer_, "warning", "foreground", "#f5b73d", nullptr);
    gtk_text_buffer_create_tag(pBuffer_, "error", "foreground", "#f87171", nullptr);
    gtk_text_buffer_create_tag(pBuffer_, "ours", "foreground", "#7fd1a8", nullptr);
    gtk_text_buffer_create_tag(pBuffer_, "match", "background", "#f59e0b", "foreground", "#0a0c11", nullptr);

    GtkTextIter start;
    gtk_text_buffer_get_end_iter(pBuffer_, &start);
    // Left gravity off, so it stays at the end as text arrives rather than being pushed along in front of it.
    gtk_text_buffer_create_mark(pBuffer_, "tail", &start, FALSE);

    GtkEventController *pScrollWatch = gtk_event_controller_scroll_new(GTK_EVENT_CONTROLLER_SCROLL_VERTICAL);
    g_signal_connect(pScrollWatch, "scroll", G_CALLBACK(onLogScrolled), this);
    gtk_widget_add_controller(pView, pScrollWatch);

    pLogScroll_ = gtk_scrolled_window_new();
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(pLogScroll_), pView);
    gtk_widget_set_vexpand(pLogScroll_, TRUE);
    gtk_box_append(GTK_BOX(pPane), pLogScroll_);

    gtk_box_append(GTK_BOX(pPane), gtk_separator_new(GTK_ORIENTATION_HORIZONTAL));
    gtk_box_append(GTK_BOX(pPane), buildFindBar());
    return pPane;
}

std::vector<SessionsPage::RenderedRow> SessionsPage::shapeOf(const SessionsView &view) {
    std::vector<RenderedRow> shape;
    for (const SessionRow &row : view.running) shape.push_back({row.pid, true, row.crashed});
    for (const SessionRow &row : view.ended) shape.push_back({row.pid, false, row.crashed});
    return shape;
}

void SessionsPage::updateMetaInPlace(const SessionsView &view) {
    size_t at = 0;
    for (const std::vector<SessionRow> *group : {&view.running, &view.ended}) {
        for (const SessionRow &row : *group) {
            if (at >= renderedMetas_.size()) return;
            if (gtk_label_get_text(renderedMetas_[at]) != row.meta) {
                gtk_label_set_text(renderedMetas_[at], row.meta.c_str());
            }
            ++at;
        }
    }
}

void SessionsPage::rebuildList(const SessionsView &view) {
    for (GtkWidget *pRow : rows_) gtk_list_box_remove(GTK_LIST_BOX(pListBox_), pRow);
    rows_.clear();
    renderedMetas_.clear();

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
                                         player ? kAssetRobloxPlayerLen : kAssetRobloxStudioLen, 19);
            gtk_widget_set_halign(pIcon, GTK_ALIGN_CENTER);
            gtk_widget_set_valign(pIcon, GTK_ALIGN_CENTER);
            if (!entry.running) gtk_widget_set_opacity(pIcon, 0.6);

            // The state rides on the corner of the app's own mark, so one glance gives both.
            GtkWidget *pDot = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
            gtk_widget_add_css_class(pDot, "session-dot");
            gtk_widget_add_css_class(pDot, entry.running ? "running" : (entry.crashed ? "crashed" : "ended"));
            gtk_widget_set_size_request(pDot, 10, 10);
            gtk_widget_set_halign(pDot, GTK_ALIGN_END);
            gtk_widget_set_valign(pDot, GTK_ALIGN_END);

            GtkWidget *pTile = gtk_overlay_new();
            gtk_widget_add_css_class(pTile, "session-tile");
            gtk_widget_set_size_request(pTile, 34, 34);
            gtk_widget_set_valign(pTile, GTK_ALIGN_CENTER);
            gtk_overlay_set_child(GTK_OVERLAY(pTile), pIcon);
            gtk_overlay_add_overlay(GTK_OVERLAY(pTile), pDot);

            GtkWidget *pName = gtk_label_new(entry.name.c_str());
            gtk_label_set_xalign(GTK_LABEL(pName), 0.0f);
            if (!entry.running) gtk_widget_add_css_class(pName, "dim-label");

            GtkWidget *pMeta = gtk_label_new(entry.meta.c_str());
            gtk_label_set_xalign(GTK_LABEL(pMeta), 0.0f);
            gtk_label_set_ellipsize(GTK_LABEL(pMeta), PANGO_ELLIPSIZE_END);
            gtk_widget_add_css_class(pMeta, "session-meta");
            gtk_widget_add_css_class(pMeta, entry.crashed ? "error" : "dim-label");
            renderedMetas_.push_back(GTK_LABEL(pMeta));

            GtkWidget *pText = gtk_box_new(GTK_ORIENTATION_VERTICAL, 3);
            gtk_widget_set_hexpand(pText, TRUE);
            gtk_box_append(GTK_BOX(pText), pName);
            gtk_box_append(GTK_BOX(pText), pMeta);

            GtkWidget *pBox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
            // Room above and below, so a row reads as its own thing rather than as a line of a table.
            gtk_widget_set_margin_top(pBox, 7);
            gtk_widget_set_margin_bottom(pBox, 7);
            gtk_box_append(GTK_BOX(pBox), pTile);
            gtk_box_append(GTK_BOX(pBox), pText);

            if (entry.running) {
                GtkWidget *pStop = iconButton(kAssetStop, kAssetStopLen, "Stop this session");
                gtk_widget_add_css_class(pStop, "session-stop");
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

void SessionsPage::highlightSelectedRow() {
    for (GtkWidget *pRow : rows_) {
        if (GPOINTER_TO_INT(g_object_get_data(G_OBJECT(pRow), "tuxblox-pid")) != selectedPid_) continue;
        gtk_list_box_select_row(GTK_LIST_BOX(pListBox_), GTK_LIST_BOX_ROW(pRow));
        return;
    }
}

void SessionsPage::selectSession(int pid) {
    if (pid == selectedPid_) return;
    selectedPid_ = pid;
    highlightSelectedRow();
    entries_.clear();
    matches_.clear();
    currentMatch_ = 0;
    pReader_.reset();
    readerCaptured_.clear();
    readerRoblox_.clear();
    readerScrolledAway_ = false;
    gtk_text_buffer_set_text(pBuffer_, "", 0);
    gtk_label_set_text(pSources_, "");
    gtk_widget_set_visible(GTK_WIDGET(pSources_), FALSE);
    // Which file is being read is not something to spend a line on, but it is what somebody filing a bug report needs, so it stays within reach.
    gtk_widget_set_tooltip_text(GTK_WIDGET(pName_),
                                pReader_ == nullptr ? nullptr : pReader_->describeSources().c_str());
}

void SessionsPage::reconcileReader(const AppSnapshot &snap) {
    const Session *pSession = nullptr;
    for (const Session &session : snap.sessionList) {
        if (session.pid == selectedPid_) pSession = &session;
    }
    if (pSession == nullptr) return;

    // Once a session is over, TuxBlox appends Roblox's log onto the end of its own and deletes the original -- so the capture alone is the whole thing, and going on reading both would show Roblox's half a second time.
    const bool ended = !pSession->running;
    const std::string wantCaptured = pSession->capturedLogPath;
    const std::string wantRoblox = ended ? std::string() : pSession->robloxLogPath;

    const bool sameFiles = pReader_ != nullptr && wantCaptured == readerCaptured_ && wantRoblox == readerRoblox_;
    if (sameFiles && ended == !readerSessionRunning_) return;

    pReader_ = std::make_unique<SessionLogReader>(wantCaptured, wantRoblox);
    readerCaptured_ = wantCaptured;
    readerRoblox_ = wantRoblox;
    readerSessionRunning_ = !ended;
    entries_.clear();
    gtk_text_buffer_set_text(pBuffer_, "", 0);
}

void SessionsPage::appendEntry(const LogEntry &entry) {
    if (errorsOnly_.on && entry.level == LogLevel::Plain) return;

    GtkTextIter end;
    gtk_text_buffer_get_end_iter(pBuffer_, &end);
    // Raw shows the line as written, stamp and all, so there is no separate column to put the time in.
    if (tidy_.on && !entry.time.empty()) {
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

    std::string body = tidy_.on ? entry.text : entry.raw;
    // The lines that continue a message are indented under it, so a traceback reads as one thing.
    const std::vector<std::string> &more = tidy_.on ? entry.continuation : entry.rawContinuation;
    for (const std::string &line : more) body += "\n    " + line;
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
    // Redrawing puts the view back at the top, which is nowhere anybody wants to be. The scroll is
    // queued rather than done here, because the lines just inserted have not been laid out yet.
    readerScrolledAway_ = false;
    g_idle_add(scrollToTailWhenDrawn, this);
}

gboolean SessionsPage::scrollToTailWhenDrawn(gpointer data) {
    static_cast<SessionsPage *>(data)->scrollToTail();
    return G_SOURCE_REMOVE;
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
        gtk_label_set_text(pState_, session.running ? "RUNNING" : "ENDED");
        gtk_widget_remove_css_class(GTK_WIDGET(pState_), session.running ? "ended" : "running");
        gtk_widget_add_css_class(GTK_WIDGET(pState_), session.running ? "running" : "ended");
        gtk_widget_set_visible(pStop_, session.running);

        const SessionsView view = sessionsView({session}, std::time(nullptr));
        const std::vector<SessionRow> &group = session.running ? view.running : view.ended;
        if (!group.empty()) {
            std::string meta = group.front().meta;
            const AppVersions &versions =
                session.target == LaunchTarget::Player ? snap.versions.player : snap.versions.studio;
            for (const InstalledVersion &installed : versions.installed) {
                if (installed.hash != versions.activeHash || installed.versionString.empty()) continue;
                meta += " \xC2\xB7 " + installed.versionString;
                break;
            }
            gtk_label_set_text(pMeta_, meta.c_str());
        }

        const bool waitingOnRoblox = pReader_ != nullptr && !pReader_->robloxLogAppeared();
        gtk_widget_set_visible(GTK_WIDGET(pSources_), waitingOnRoblox);
        if (waitingOnRoblox) {
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

    // Only when the sessions themselves change. Their ages change every second, and rewriting one label is not a reason to throw every row away and take the selection with it.
    const std::vector<RenderedRow> shape = shapeOf(shown);
    const bool sameShape = shape.size() == renderedShape_.size() && shown.emptyMessage == renderedEmpty_ &&
                           std::equal(shape.begin(), shape.end(), renderedShape_.begin(),
                                      [](const RenderedRow &a, const RenderedRow &b) {
                                          return a.pid == b.pid && a.running == b.running && a.crashed == b.crashed;
                                      });
    if (sameShape) {
        updateMetaInPlace(shown);
    } else {
        rebuildList(shown);
        renderedShape_ = shape;
        renderedEmpty_ = shown.emptyMessage;
    }

    // Nothing is selected yet, so the newest running session is a better first thing to look at than an empty pane.
    if (selectedPid_ == 0 && !shown.running.empty()) selectSession(shown.running.back().pid);
    if (!sameShape) highlightSelectedRow();
    refreshHeader(snap);
    if (selectedPid_ != 0) reconcileReader(snap);

    // Reading two files is only worth doing while somebody is looking at them; this runs on the window's own tick, which fires for every page whether it is on screen or not.
    if (snap.activeTab != Tab::Sessions || pReader_ == nullptr) return;

    // Reading back through a log that is still growing should stay where it was put.
    gtk_widget_set_visible(GTK_WIDGET(pFollowState_), follow_.on && readerScrolledAway_);
    gtk_widget_set_visible(pJumpToEnd_, follow_.on && readerScrolledAway_);
    if (follow_.on && readerScrolledAway_) {
        gtk_label_set_text(pFollowState_, "Scrolled up \xE2\x80\x94 following paused");
    }

    std::vector<LogEntry> fresh = pReader_->pump();
    // A file that was cleared or replaced is about to hand back everything already on screen.
    if (pReader_->startedOver()) {
        entries_.clear();
        gtk_text_buffer_set_text(pBuffer_, "", 0);
    }
    if (fresh.empty()) return;

    for (const LogEntry &entry : fresh) {
        entries_.push_back(entry);
        appendEntry(entry);
    }
    if (!renderedQuery_.empty()) applyFind();

    gtk_label_set_text(pSize_, (std::to_string(entries_.size()) + " entries \xC2\xB7 " +
                                std::to_string(gtk_text_buffer_get_char_count(pBuffer_) / 1024) + " KB").c_str());

    if (follow_.on && !readerScrolledAway_) scrollToTail();
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

void SessionsPage::scrollToTail() {
    GtkTextIter end;
    gtk_text_buffer_get_end_iter(pBuffer_, &end);
    GtkTextMark *pTail = gtk_text_buffer_get_mark(pBuffer_, "tail");
    if (pTail == nullptr) return;
    gtk_text_buffer_move_mark(pBuffer_, pTail, &end);
    gtk_text_view_scroll_to_mark(pLog_, pTail, 0.0, FALSE, 0.0, 0.0);
}

gboolean SessionsPage::onLogScrolled(GtkEventControllerScroll *, double, double dy, gpointer data) {
    auto *pSelf = static_cast<SessionsPage *>(data);
    if (dy < 0.0) {
        pSelf->readerScrolledAway_ = true;
    } else if (dy > 0.0) {
        // Back at the bottom means they want to be carried along again.
        GtkAdjustment *pPosition =
            gtk_scrolled_window_get_vadjustment(GTK_SCROLLED_WINDOW(pSelf->pLogScroll_));
        const double remaining = gtk_adjustment_get_upper(pPosition) -
                                 gtk_adjustment_get_page_size(pPosition) -
                                 gtk_adjustment_get_value(pPosition);
        if (remaining < 48.0) pSelf->readerScrolledAway_ = false;
    }
    return GDK_EVENT_PROPAGATE;
}

void SessionsPage::onJumpToEnd(GtkButton *, gpointer data) {
    auto *pSelf = static_cast<SessionsPage *>(data);
    pSelf->readerScrolledAway_ = false;
    pSelf->follow_.on = true;
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(pSelf->follow_.pButton), TRUE);
    pSelf->scrollToTail();
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
