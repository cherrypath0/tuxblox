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
#include <memory>
#include <string>
#include <vector>

#include "app.h"
#include "log_entry.h"
#include "page.h"
#include "session_log_reader.h"
#include "sessions_page_state.h"

// Third Parties
#include <adwaita.h>

namespace tuxblox {

class SessionsPage : public Page {
public:
    explicit SessionsPage(App &app);
    GtkWidget *widget() const override;
    void update(const AppSnapshot &snap) override;

private:
    struct Toggle {
        GtkWidget *pButton = nullptr;
        bool on = false;
    };

    GtkWidget *buildList();
    GtkWidget *buildLogPane();
    GtkWidget *buildControls();
    GtkWidget *buildFindBar();
    GtkWidget *toggleButton(Toggle &toggle, const char *pLabel, bool on);

    // What the list is showing now, so a census that says the same thing does not throw the rows away -- rebuilding them ten times a second destroys the row under the pointer before a click can land on it.
    struct RenderedRow {
        int pid = 0;
        bool running = false;
        bool crashed = false;
    };
    static std::vector<RenderedRow> shapeOf(const SessionsView &view);

    void rebuildList(const SessionsView &view);
    void updateMetaInPlace(const SessionsView &view);
    void selectSession(int pid);
    // Keeps the reader pointed at what the session actually has now: Roblox writes its log a moment after its process exists, and when a session ends TuxBlox folds that log into its own, so the files a reader was built with go stale underneath it.
    void reconcileReader(const AppSnapshot &snap);
    void showEntries();
    void appendEntry(const LogEntry &entry);
    void applyFind();
    void scrollToTail();
    static gboolean scrollToTailWhenDrawn(gpointer data);
    void refreshHeader(const AppSnapshot &snap);

    static void onSearchChanged(GtkEditable *pEditable, gpointer data);
    static void onRowActivated(GtkListBox *pList, GtkListBoxRow *pRow, gpointer data);
    static void onToggle(GtkButton *pButton, gpointer data);
    static void onStopClicked(GtkButton *pButton, gpointer data);
    static void onRowStopClicked(GtkButton *pButton, gpointer data);
    static void onCopyClicked(GtkButton *pButton, gpointer data);
    static void onSaveClicked(GtkButton *pButton, gpointer data);
    static void onFolderClicked(GtkButton *pButton, gpointer data);
    static void onFindChanged(GtkEditable *pEditable, gpointer data);
    static void onFindStep(GtkButton *pButton, gpointer data);
    static void onJumpToEnd(GtkButton *pButton, gpointer data);
    static gboolean onLogScrolled(GtkEventControllerScroll *pController, double dx, double dy, gpointer data);

    App &app_;
    GtkWidget *pRoot_ = nullptr;

    GtkWidget *pSearch_ = nullptr;
    GtkWidget *pListBox_ = nullptr;
    GtkWidget *pEmpty_ = nullptr;
    std::vector<GtkWidget *> rows_;
    std::vector<RenderedRow> renderedShape_;
    std::vector<GtkLabel *> renderedMetas_;
    std::string renderedEmpty_;

    GtkLabel *pName_ = nullptr;
    GtkLabel *pState_ = nullptr;
    GtkLabel *pMeta_ = nullptr;
    GtkLabel *pSources_ = nullptr;
    GtkLabel *pSize_ = nullptr;
    GtkLabel *pFollowState_ = nullptr;
    GtkWidget *pJumpToEnd_ = nullptr;
    GtkWidget *pStop_ = nullptr;

    GtkTextView *pLog_ = nullptr;
    GtkTextBuffer *pBuffer_ = nullptr;
    GtkWidget *pLogScroll_ = nullptr;
    GtkWidget *pFind_ = nullptr;
    GtkLabel *pFindCount_ = nullptr;

    Toggle follow_;
    Toggle wrap_;
    Toggle tidy_;
    Toggle errorsOnly_;

    std::unique_ptr<SessionLogReader> pReader_;
    std::string readerCaptured_;
    std::string readerRoblox_;
    bool readerSessionRunning_ = true;
    std::vector<LogEntry> entries_;
    int selectedPid_ = 0;
    // Set only when the reader actually scrolls. Working it out from the scrollbar instead latches: the scroll the page asks for is carried out later, so the next tick reads a stale position, decides the reader moved away, and then never scrolls again.
    bool readerScrolledAway_ = false;
    std::string renderedQuery_;
    size_t currentMatch_ = 0;
    std::vector<LogMatch> matches_;
};

} // namespace tuxblox
