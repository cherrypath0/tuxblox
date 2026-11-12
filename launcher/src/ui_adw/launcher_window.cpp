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
#include "asset_download.h"
#include "asset_fastflags.h"
#include "asset_home.h"
#include "asset_info.h"
#include "asset_roblox_rdd.h"
#include "asset_tuxblox_icon.h"
#include "asset_sessions.h"
#include "asset_settings.h"
#include "desktop_integration.h"
#include "desktop_notify.h"
#include "dialog.h"
#include "i18n.h"
#include "page.h"
#include "update_changes.h"
#include "page_about.h"
#include "page_fastflags.h"
#include "page_home.h"
#include "page_sessions.h"
#include "page_settings.h"
#include "page_versions.h"
#include "snow.h"
#include "welcome_tour.h"
#include "welcome_tour_state.h"
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
    ActionNotification updateNotice;
    std::string appliedTheme;
    GtkWidget *pSnow = nullptr;
    // How many sessions are open, shown on the Sessions row so it is answerable without going there.
    GtkLabel *pSessionCount = nullptr;
    int shownSessionCount = -1;
    bool containerWarningShown = false;
    bool backgroundWorkStarted = false;
    guint tickId = 0;
    bool activated = false;
    // True while the tour's own window is up, which holds back the dialogs that would stack on it.
    bool welcomeOpen = false;
    // In the header rather than on a page, so it is there on every tab for as long as an update is.
    GtkWidget *pUpdateButton = nullptr;
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
    gtk_box_append(GTK_BOX(pBox), iconImage(pIcon, iconLength, 18));
    GtkWidget *pLabel = gtk_label_new(pTitle);
    gtk_label_set_xalign(GTK_LABEL(pLabel), 0.0f);
    gtk_box_append(GTK_BOX(pBox), pLabel);

    if (tab == Tab::Sessions) {
        GtkWidget *pCount = gtk_label_new("");
        gtk_widget_add_css_class(pCount, "session-count");
        gtk_widget_set_halign(pCount, GTK_ALIGN_END);
        gtk_widget_set_valign(pCount, GTK_ALIGN_CENTER);
        gtk_widget_set_hexpand(pCount, TRUE);
        gtk_widget_set_visible(pCount, FALSE);
        gtk_box_append(GTK_BOX(pBox), pCount);
        ui.pSessionCount = GTK_LABEL(pCount);
    }

    GtkWidget *pRow = gtk_list_box_row_new();
    gtk_list_box_row_set_child(GTK_LIST_BOX_ROW(pRow), pBox);
    g_object_set_data(G_OBJECT(pRow), "tuxblox-tab", GINT_TO_POINTER(static_cast<int>(tab)));
    gtk_list_box_append(footer ? ui.pFooterList : ui.pMainList, pRow);

    gtk_stack_add_named(ui.pStack, page->widget(), tabName(tab).c_str());
    ui.pages.push_back({tab, pTitle, footer, std::move(page), GTK_LIST_BOX_ROW(pRow)});
}

gboolean onTick(gpointer data) {
    auto *pUi = static_cast<WindowUi *>(data);
    App &app = *pUi->pApp;
    const AppSnapshot snap = app.snapshot();

    showTab(*pUi, snap.activeTab);
    if (pUi->pUpdateButton != nullptr) {
        gtk_widget_set_visible(pUi->pUpdateButton, snap.updateAvailableVersion.has_value());
    }
    if (pUi->appliedTheme != launcherThemeFor(snap.settings.theme)) {
        pUi->appliedTheme = launcherThemeFor(snap.settings.theme);
        applyLauncherTheme(pUi->appliedTheme);
        if (pUi->pSnow != nullptr) gtk_widget_set_visible(pUi->pSnow, pUi->appliedTheme == "christmas");
    }
    if (pUi->pSessionCount != nullptr) {
        int running = 0;
        for (const Session &session : snap.sessionList) {
            if (session.running) ++running;
        }
        if (running != pUi->shownSessionCount) {
            pUi->shownSessionCount = running;
            gtk_label_set_text(pUi->pSessionCount, std::to_string(running).c_str());
            gtk_widget_set_visible(GTK_WIDGET(pUi->pSessionCount), running > 0);
        }
    }
    for (PageEntry &entry : pUi->pages) entry.page->update(snap);


    // Held back like the update dialog, or it stacks on the tour and is marked shown without being read.
    if (!pUi->welcomeOpen && !snap.containerWarning.empty() && !pUi->containerWarningShown) {
        pUi->containerWarningShown = true;
        showNotice(GTK_WIDGET(pUi->pWindow), i18n::tr("Distrobox GPU Passthrough"), snap.containerWarning);
    }

    // A launch hands over to the watcher process, and both handoffs exec the installer once this window is gone
    if (app.shouldQuit() || app.needsInstallerHandoff() || app.needsUninstallHandoff()) {
        pUi->tickId = 0;
        gtk_window_close(pUi->pWindow);
        return G_SOURCE_REMOVE;
    }
    return G_SOURCE_CONTINUE;
}

// The launcher window when the tour was abandoned: it was built but never shown, so it goes
// quietly. The poll stops first, since no close-request is raised for it to stop on.
gboolean closeUnopenedWindow(gpointer data) {
    auto *pUi = static_cast<WindowUi *>(data);
    if (pUi->tickId != 0) {
        g_source_remove(pUi->tickId);
        pUi->tickId = 0;
    }
    gtk_window_destroy(pUi->pWindow);
    return G_SOURCE_REMOVE;
}

gboolean onCloseRequest(GtkWindow *, gpointer data) {
    auto *pUi = static_cast<WindowUi *>(data);
    if (pUi->tickId != 0) {
        g_source_remove(pUi->tickId);
        pUi->tickId = 0;
    }
    return FALSE;
}

// Desktop integration can block for seconds on xdg-mime, so it waits until the window has been drawn
// once. The update check is deliberately not here: it starts as the window is built, so it is never
// held up behind this.
gboolean startBackgroundWork(gpointer data) {
    auto *pUi = static_cast<WindowUi *>(data);
    if (pUi->backgroundWorkStarted) return G_SOURCE_REMOVE;
    pUi->backgroundWorkStarted = true;
    ensureDesktopIntegration(pUi->exePath, pUi->installDir);
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
    // The desktop's window buttons would draw the app icon here, scaled from a much larger file; this copy is made for the size it is shown at
    GdkPaintable *pLogo = logoPaintable(kAssetTuxbloxIcon, kAssetTuxbloxIconLen, 24);
    GtkWidget *pLogoImage = gtk_image_new_from_paintable(pLogo);
    g_object_unref(pLogo);
    gtk_image_set_pixel_size(GTK_IMAGE(pLogoImage), 24);
    GtkWidget *pBrand = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    gtk_widget_set_margin_start(pBrand, 5);
    gtk_box_append(GTK_BOX(pBrand), pLogoImage);
    gtk_box_append(GTK_BOX(pBrand), pName);

    GtkWidget *pHeader = adw_header_bar_new();
    adw_header_bar_set_decoration_layout(ADW_HEADER_BAR(pHeader), ":");
    adw_header_bar_set_show_title(ADW_HEADER_BAR(pHeader), FALSE);
    adw_header_bar_pack_start(ADW_HEADER_BAR(pHeader), pBrand);

    GtkWidget *pView = adw_toolbar_view_new();
    adw_toolbar_view_add_top_bar(ADW_TOOLBAR_VIEW(pView), pHeader);
    adw_toolbar_view_set_content(ADW_TOOLBAR_VIEW(pView), pLists);
    return GTK_WIDGET(adw_navigation_page_new(pView, "TuxBlox"));
}

// The release being offered and the parts of it that move, under the body of the dialog.
GtkWidget *updateDetails(const std::string &release, const std::vector<ComponentChange> &changes) {
    GtkWidget *pBox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 2);
    gtk_widget_set_margin_top(pBox, 8);

    GtkWidget *pRelease = gtk_label_new(i18n::trf("Release {0}", {release}).c_str());
    gtk_label_set_xalign(GTK_LABEL(pRelease), 0.0);
    gtk_widget_add_css_class(pRelease, "heading");
    gtk_box_append(GTK_BOX(pBox), pRelease);

    for (const ComponentChange &change : changes) {
        GtkWidget *pLine = gtk_label_new(changeLine(change).c_str());
        gtk_label_set_xalign(GTK_LABEL(pLine), 0.0);
        gtk_label_set_wrap(GTK_LABEL(pLine), TRUE);
        gtk_widget_add_css_class(pLine, "dim-label");
        gtk_box_append(GTK_BOX(pBox), pLine);
    }

    GtkWidget *pQuestion = gtk_label_new(i18n::tr("Would you like to update?"));
    gtk_label_set_xalign(GTK_LABEL(pQuestion), 0.0);
    gtk_label_set_wrap(GTK_LABEL(pQuestion), TRUE);
    gtk_widget_set_margin_top(pQuestion, 10);
    gtk_box_append(GTK_BOX(pBox), pQuestion);
    return pBox;
}

// Nothing is offered until the check has found something, so the button is the whole prompt: it
// appears in the header on every tab and opens this when it is clicked.
void onUpdateClicked(GtkButton *, gpointer data) {
    auto *pUi = static_cast<WindowUi *>(data);
    const AppSnapshot snap = pUi->pApp->snapshot();
    if (!snap.updateAvailableVersion) return;
    App *pApp = pUi->pApp;
    AdwDialog *pDialog = buildDialog(i18n::tr("Update available"), i18n::tr("A new update is available"),
                                     {{i18n::tr("Not now"), nullptr, nullptr},
                                      {i18n::tr("Update"), "suggested-action", [pApp] { pApp->requestUpdateNow(); }, true}},
                                     updateDetails(*snap.updateAvailableVersion, snap.updateChanges));
    adw_dialog_present(pDialog, GTK_WIDGET(pUi->pStack));
}

GtkWidget *buildContent(WindowUi &ui) {
    GtkWidget *pStack = gtk_stack_new();
    ui.pStack = GTK_STACK(pStack);

    // Packed at the end, so it sits just inside the window's own buttons rather than beyond them,
    // and it is in the header rather than on one page because an update is waiting whatever tab
    // you are looking at.
    GtkWidget *pUpdate = gtk_button_new();
    gtk_button_set_child(GTK_BUTTON(pUpdate), iconImage(kAssetDownload, kAssetDownloadLen, 14));
    gtk_widget_add_css_class(pUpdate, "update-button");
    gtk_widget_set_valign(pUpdate, GTK_ALIGN_CENTER);
    gtk_widget_set_visible(pUpdate, FALSE);
    gtk_widget_set_tooltip_text(pUpdate, i18n::tr("Update available"));
    g_signal_connect(pUpdate, "clicked", G_CALLBACK(onUpdateClicked), &ui);
    ui.pUpdateButton = pUpdate;

    GtkWidget *pHeader = adw_header_bar_new();
    adw_header_bar_pack_end(ADW_HEADER_BAR(pHeader), pUpdate);

    GtkWidget *pView = adw_toolbar_view_new();
    adw_toolbar_view_add_top_bar(ADW_TOOLBAR_VIEW(pView), pHeader);
    adw_toolbar_view_set_content(ADW_TOOLBAR_VIEW(pView), pStack);

    AdwNavigationPage *pPage = adw_navigation_page_new(pView, i18n::tr("Home"));
    ui.pContentPage = pPage;
    return GTK_WIDGET(pPage);
}

void onActivate(GtkApplication *pGtkApp, gpointer data) {
    auto *pUi = static_cast<WindowUi *>(data);
    pUi->activated = true;
    applyLook();
    // Before the first applyLauncherTheme, which is the last moment the scheme is still the desktop's.
    captureSystemColourScheme();
    pUi->appliedTheme = launcherThemeFor(pUi->pApp->snapshot().settings.theme);
    applyLauncherTheme(pUi->appliedTheme);

    GtkWidget *pWindow = adw_application_window_new(pGtkApp);
    pUi->pWindow = GTK_WINDOW(pWindow);
    gtk_window_set_title(pUi->pWindow, "TuxBlox");
    // The size it opens at is also the smallest it goes: the Sessions tab's list and its log pane sit side by side, and narrower than this one of them stops being readable rather than merely getting tight.
    gtk_window_set_default_size(pUi->pWindow, 988, 549);
    gtk_widget_set_size_request(pWindow, 988, 549);
    g_signal_connect(pWindow, "close-request", G_CALLBACK(onCloseRequest), pUi);
    g_signal_connect(pWindow, "realize", G_CALLBACK(onRealize), pUi);

    GtkWidget *pSplit = adw_navigation_split_view_new();
    adw_navigation_split_view_set_min_sidebar_width(ADW_NAVIGATION_SPLIT_VIEW(pSplit), 180);
    adw_navigation_split_view_set_max_sidebar_width(ADW_NAVIGATION_SPLIT_VIEW(pSplit), 220);
    adw_navigation_split_view_set_sidebar(ADW_NAVIGATION_SPLIT_VIEW(pSplit), ADW_NAVIGATION_PAGE(buildSidebar(*pUi)));
    adw_navigation_split_view_set_content(ADW_NAVIGATION_SPLIT_VIEW(pSplit), ADW_NAVIGATION_PAGE(buildContent(*pUi)));

    App &app = *pUi->pApp;
    // First thing, so the check is already running while the pages are built and is never waiting behind
    // desktop integration. A no-op when main() already started it, which it does with Auto-Update on.
    app.startUpdateCheck();

    addPage(*pUi, Tab::Start, i18n::tr("Home"), kAssetHome, kAssetHomeLen, false, std::make_unique<HomePage>(app, pUi->installDir));
    addPage(*pUi, Tab::Sessions, i18n::tr("Sessions"), kAssetSessions, kAssetSessionsLen, false, std::make_unique<SessionsPage>(app));
    addPage(*pUi, Tab::Versions, i18n::tr("Versions"), kAssetRobloxRdd, kAssetRobloxRddLen, false, std::make_unique<VersionsPage>(app));
    addPage(*pUi, Tab::FastFlags, i18n::tr("FastFlags"), kAssetFastflags, kAssetFastflagsLen, false, std::make_unique<FastFlagsPage>(app));
    addPage(*pUi, Tab::Settings, i18n::tr("Settings"), kAssetSettings, kAssetSettingsLen, false, std::make_unique<SettingsPage>(app));
    addPage(*pUi, Tab::About, i18n::tr("About"), kAssetInfo, kAssetInfoLen, true, std::make_unique<AboutPage>(pUi->installDir));

    // The snow sits under the whole window rather than inside one page, so it keeps falling behind the sidebar too. The split view is what the overlay measures itself from, since the snow asks for no size of its own.
    GtkWidget *pOverlay = gtk_overlay_new();
    pUi->pSnow = createSnowLayer();
    gtk_overlay_set_child(GTK_OVERLAY(pOverlay), pUi->pSnow);
    gtk_overlay_add_overlay(GTK_OVERLAY(pOverlay), pSplit);
    gtk_overlay_set_measure_overlay(GTK_OVERLAY(pOverlay), pSplit, TRUE);
    gtk_widget_set_visible(pUi->pSnow, pUi->appliedTheme == "christmas");
    adw_application_window_set_content(ADW_APPLICATION_WINDOW(pWindow), pOverlay);

    onTick(pUi);
    pUi->tickId = g_timeout_add(100, onTick, pUi);
    // A window that never gets a frame, such as one opened on a hidden workspace, must still register itself with the desktop
    g_timeout_add_seconds(2, startBackgroundWork, pUi);

    // On a first run the tour comes up instead, and this window waits behind it for Get started.
    if (shouldShowWelcome(pUi->pApp->snapshot().settings)) {
        pUi->welcomeOpen = true;
        presentWelcomeTour(*pUi->pApp, pGtkApp, [pUi](bool finished) {
            pUi->welcomeOpen = false;
            if (!finished) {
                // Closed without finishing, so nothing was set up and the launcher does not open.
                // Deferred: this runs inside the tour window's own teardown, and tearing a second
                // window down from in there leaves GTK asking X about a drawable that has gone.
                g_idle_add(closeUnopenedWindow, pUi);
                return;
            }
            Settings settings = pUi->pApp->snapshot().settings;
            settings.welcomeSeen = true;
            pUi->pApp->updateSettings(settings);
            gtk_window_present(pUi->pWindow);
        });
        return;
    }
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
