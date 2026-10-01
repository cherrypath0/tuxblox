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

#include "launcher_window.h"
#include "adw_look.h"
#include "asset_fastflags.h"
#include "asset_home.h"
#include "asset_info.h"
#include "asset_roblox_rdd.h"
#include "asset_settings.h"
#include "desktop_integration.h"
#include "desktop_notify.h"
#include "page.h"
#include "page_about.h"
#include "page_fastflags.h"
#include "page_home.h"
#include "page_settings.h"
#include "page_versions.h"
#include "widgets.h"

#include <cstdio>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

// Third Parties
#include <adwaita.h>

namespace tuxblox {

namespace {

struct PageEntry {
    Tab tab;
    const char *pTitle;
    bool footer;
    std::unique_ptr<Page> page;
    GtkListBoxRow *pRow = nullptr;
};

struct WindowUi {
    App *pApp = nullptr;
    std::string exePath;
    std::string installDir;
    GtkWindow *pWindow = nullptr;
    AdwNavigationPage *pContentPage = nullptr;
    GtkStack *pStack = nullptr;
    GtkListBox *pMainList = nullptr;
    GtkListBox *pFooterList = nullptr;
    std::vector<PageEntry> pages;
    std::optional<Tab> shownTab;
    std::string notifiedVersion;
    bool containerWarningShown = false;
    bool backgroundWorkStarted = false;
    guint tickId = 0;
    bool activated = false;
};

std::string tabName(Tab tab) {
    return std::to_string(static_cast<int>(tab));
}

void showTab(WindowUi &ui, Tab tab) {
    if (ui.shownTab == tab) return;
    for (const PageEntry &entry : ui.pages) {
        if (entry.tab != tab) continue;
        gtk_stack_set_visible_child_name(ui.pStack, tabName(tab).c_str());
        adw_navigation_page_set_title(ui.pContentPage, entry.pTitle);
        gtk_list_box_unselect_all(entry.footer ? ui.pMainList : ui.pFooterList);
        gtk_list_box_select_row(entry.footer ? ui.pFooterList : ui.pMainList, entry.pRow);
        ui.shownTab = tab;
        return;
    }
}

void onRowActivated(GtkListBox *, GtkListBoxRow *pRow, gpointer data) {
    auto *pUi = static_cast<WindowUi *>(data);
    const Tab tab = static_cast<Tab>(GPOINTER_TO_INT(g_object_get_data(G_OBJECT(pRow), "tuxblox-tab")));
    pUi->pApp->setActiveTab(tab);
    showTab(*pUi, tab);
}

void addPage(WindowUi &ui, Tab tab, const char *pTitle, const unsigned char *pIcon, size_t iconLength, bool footer,
             std::unique_ptr<Page> page) {
    GtkWidget *pBox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 12);
    gtk_box_append(GTK_BOX(pBox), iconImage(pIcon, iconLength, 16));
    GtkWidget *pLabel = gtk_label_new(pTitle);
    gtk_label_set_xalign(GTK_LABEL(pLabel), 0.0f);
    gtk_box_append(GTK_BOX(pBox), pLabel);

    GtkWidget *pRow = gtk_list_box_row_new();
    gtk_list_box_row_set_child(GTK_LIST_BOX_ROW(pRow), pBox);
    g_object_set_data(G_OBJECT(pRow), "tuxblox-tab", GINT_TO_POINTER(static_cast<int>(tab)));
    gtk_list_box_append(footer ? ui.pFooterList : ui.pMainList, pRow);

    gtk_stack_add_named(ui.pStack, page->widget(), tabName(tab).c_str());
    ui.pages.push_back({tab, pTitle, footer, std::move(page), GTK_LIST_BOX_ROW(pRow)});
}

// Once per offered version, so the notice does not repeat on every poll; the Update button on the Home page is what acts on it
void notifyUpdate(WindowUi &ui, const AppSnapshot &snap) {
    if (!snap.updateAvailableVersion || *snap.updateAvailableVersion == ui.notifiedVersion) return;
    ui.notifiedVersion = *snap.updateAvailableVersion;
    showDesktopNotification("TuxBlox", "TuxBlox " + ui.notifiedVersion + " is available", "tuxblox");
}

gboolean onTick(gpointer data) {
    auto *pUi = static_cast<WindowUi *>(data);
    App &app = *pUi->pApp;
    const AppSnapshot snap = app.snapshot();

    showTab(*pUi, snap.activeTab);
    for (PageEntry &entry : pUi->pages) entry.page->update(snap);
    notifyUpdate(*pUi, snap);

    if (!snap.containerWarning.empty() && !pUi->containerWarningShown) {
        pUi->containerWarningShown = true;
        showNotice(GTK_WIDGET(pUi->pWindow), "Distrobox GPU Passthrough", snap.containerWarning);
    }

    // A launch hands over to the watcher process, and both handoffs exec the installer once this window is gone
    if (app.shouldQuit() || app.needsInstallerHandoff() || app.needsUninstallHandoff()) {
        pUi->tickId = 0;
        gtk_window_close(pUi->pWindow);
        return G_SOURCE_REMOVE;
    }
    return G_SOURCE_CONTINUE;
}

gboolean onCloseRequest(GtkWindow *, gpointer data) {
    auto *pUi = static_cast<WindowUi *>(data);
    if (pUi->tickId != 0) {
        g_source_remove(pUi->tickId);
        pUi->tickId = 0;
    }
    return FALSE;
}

// Desktop integration can block for seconds on xdg-mime, so it waits until the window has been drawn once
gboolean startBackgroundWork(gpointer data) {
    auto *pUi = static_cast<WindowUi *>(data);
    if (pUi->backgroundWorkStarted) return G_SOURCE_REMOVE;
    pUi->backgroundWorkStarted = true;
    ensureDesktopIntegration(pUi->exePath, pUi->installDir);
    pUi->pApp->startUpdateCheck();
    return G_SOURCE_REMOVE;
}

void onAfterPaint(GdkFrameClock *pClock, gpointer data) {
    g_signal_handlers_disconnect_by_func(pClock, reinterpret_cast<gpointer>(onAfterPaint), data);
    g_idle_add(startBackgroundWork, data);
}

void onRealize(GtkWidget *pWindow, gpointer data) {
    GdkFrameClock *pClock = gdk_surface_get_frame_clock(gtk_native_get_surface(GTK_NATIVE(pWindow)));
    g_signal_connect(pClock, "after-paint", G_CALLBACK(onAfterPaint), data);
}

GtkWidget *listBox(WindowUi &ui) {
    GtkWidget *pList = gtk_list_box_new();
    gtk_list_box_set_selection_mode(GTK_LIST_BOX(pList), GTK_SELECTION_SINGLE);
    gtk_widget_add_css_class(pList, "navigation-sidebar");
    g_signal_connect(pList, "row-activated", G_CALLBACK(onRowActivated), &ui);
    return pList;
}

GtkWidget *buildSidebar(WindowUi &ui) {
    GtkWidget *pMain = listBox(ui);
    ui.pMainList = GTK_LIST_BOX(pMain);
    GtkWidget *pFooter = listBox(ui);
    ui.pFooterList = GTK_LIST_BOX(pFooter);
    gtk_widget_set_valign(pFooter, GTK_ALIGN_END);
    gtk_widget_set_vexpand(pFooter, TRUE);

    GtkWidget *pLists = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_box_append(GTK_BOX(pLists), pMain);
    gtk_box_append(GTK_BOX(pLists), pFooter);

    GtkWidget *pName = gtk_label_new("TuxBlox");
    gtk_widget_add_css_class(pName, "heading");
    // The desktop's own window buttons already put the app icon in this header bar, so only the name goes here
    GtkWidget *pHeader = adw_header_bar_new();
    adw_header_bar_set_title_widget(ADW_HEADER_BAR(pHeader), pName);

    GtkWidget *pView = adw_toolbar_view_new();
    adw_toolbar_view_add_top_bar(ADW_TOOLBAR_VIEW(pView), pHeader);
    adw_toolbar_view_set_content(ADW_TOOLBAR_VIEW(pView), pLists);
    return GTK_WIDGET(adw_navigation_page_new(pView, "TuxBlox"));
}

GtkWidget *buildContent(WindowUi &ui) {
    GtkWidget *pStack = gtk_stack_new();
    ui.pStack = GTK_STACK(pStack);

    GtkWidget *pView = adw_toolbar_view_new();
    adw_toolbar_view_add_top_bar(ADW_TOOLBAR_VIEW(pView), adw_header_bar_new());
    adw_toolbar_view_set_content(ADW_TOOLBAR_VIEW(pView), pStack);

    AdwNavigationPage *pPage = adw_navigation_page_new(pView, "Home");
    ui.pContentPage = pPage;
    return GTK_WIDGET(pPage);
}

void onActivate(GtkApplication *pGtkApp, gpointer data) {
    auto *pUi = static_cast<WindowUi *>(data);
    pUi->activated = true;
    applyLook();
    installLauncherStyles();

    GtkWidget *pWindow = adw_application_window_new(pGtkApp);
    pUi->pWindow = GTK_WINDOW(pWindow);
    gtk_window_set_title(pUi->pWindow, "TuxBlox");
    gtk_window_set_default_size(pUi->pWindow, 760, 480);
    gtk_widget_set_size_request(pWindow, 640, 420);
    g_signal_connect(pWindow, "close-request", G_CALLBACK(onCloseRequest), pUi);
    g_signal_connect(pWindow, "realize", G_CALLBACK(onRealize), pUi);

    GtkWidget *pSplit = adw_navigation_split_view_new();
    adw_navigation_split_view_set_min_sidebar_width(ADW_NAVIGATION_SPLIT_VIEW(pSplit), 180);
    adw_navigation_split_view_set_max_sidebar_width(ADW_NAVIGATION_SPLIT_VIEW(pSplit), 220);
    adw_navigation_split_view_set_sidebar(ADW_NAVIGATION_SPLIT_VIEW(pSplit), ADW_NAVIGATION_PAGE(buildSidebar(*pUi)));
    adw_navigation_split_view_set_content(ADW_NAVIGATION_SPLIT_VIEW(pSplit), ADW_NAVIGATION_PAGE(buildContent(*pUi)));

    App &app = *pUi->pApp;
    addPage(*pUi, Tab::Start, "Home", kAssetHome, kAssetHomeLen, false, std::make_unique<HomePage>(app));
    addPage(*pUi, Tab::Versions, "Versions", kAssetRobloxRdd, kAssetRobloxRddLen, false, std::make_unique<VersionsPage>(app));
    addPage(*pUi, Tab::FastFlags, "FastFlags", kAssetFastflags, kAssetFastflagsLen, false, std::make_unique<FastFlagsPage>(app));
    addPage(*pUi, Tab::Settings, "Settings", kAssetSettings, kAssetSettingsLen, false, std::make_unique<SettingsPage>(app));
    addPage(*pUi, Tab::About, "About", kAssetInfo, kAssetInfoLen, true, std::make_unique<AboutPage>());

    adw_application_window_set_content(ADW_APPLICATION_WINDOW(pWindow), pSplit);

    onTick(pUi);
    pUi->tickId = g_timeout_add(100, onTick, pUi);
    // A window that never gets a frame, such as one opened on a hidden workspace, must still check for updates
    g_timeout_add_seconds(2, startBackgroundWork, pUi);
    gtk_window_present(pUi->pWindow);
}

} // namespace

int runLauncherWindow(App &app, const std::string &exePath, const std::string &installDir) {
    // GTK names the window after the program on both X11 and Wayland, and this is the name the .desktop entry declares
    g_set_prgname("tuxblox-launcher");
    if (!gtk_init_check()) {
        fprintf(stderr, "TuxBlox: there is no display to open the launcher window on\n");
        return 1;
    }
    gtk_window_set_default_icon_name("tuxblox");

    WindowUi ui;
    ui.pApp = &app;
    ui.exePath = exePath;
    ui.installDir = installDir;

    // No application id, so GTK uses the program name above as the Wayland app id; the launcher has its own single-instance lock
    AdwApplication *pGtkApp = adw_application_new(nullptr, G_APPLICATION_NON_UNIQUE);
    g_signal_connect(pGtkApp, "activate", G_CALLBACK(onActivate), &ui);
    const int status = g_application_run(G_APPLICATION(pGtkApp), 0, nullptr);
    g_object_unref(pGtkApp);
    return ui.activated ? status : 1;
}

} // namespace tuxblox
