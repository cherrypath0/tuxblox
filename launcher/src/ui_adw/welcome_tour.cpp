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

#include "welcome_tour.h"
#include "adw_look.h"
#include "app.h"
#include "open_url.h"
#include "tuxblox_logo_png.h"
#include "welcome_tour_state.h"
#include "widgets.h"

#include <memory>
#include <string>
#include <vector>

// Third Parties
#include <adwaita.h>

namespace tuxblox {

namespace {

struct ThemeChoice {
    const char *pId;
    const char *pLabel;
    const char *pSwatchClass;
};

// Same four the Settings page offers, in the same order.
const ThemeChoice Themes[] = {
    {"system", "System", "theme-swatch-system"},
    {"dark", "Dark", "theme-swatch-dark"},
    {"grey", "Grey", "theme-swatch-grey"},
    {"light", "Light", "theme-swatch-light"},
};

struct SwitchChoice {
    const char *pTitle;
    const char *pSubtitle;
    bool Settings::*pField;
};

// Titles and descriptions are the Settings page's own, so the two screens cannot disagree.
const SwitchChoice Switches[] = {
    {"Automatic updates", "Install TuxBlox updates without asking.", &Settings::autoUpdate},
    {"Always send crash reports",
     "Send the session log and your versions when Roblox crashes. See tuxblox.net/privacy",
     &Settings::sendCrashReports},
    {"Enable Discord Rich Presence", "", &Settings::discordRpc},
    {"Notify server location",
     "Sends a notification about the server's location whenever you join a game server or a Team Create server",
     &Settings::serverRegionNotify},
};

struct Tour {
    App *pApp = nullptr;
    AdwDialog *pDialog = nullptr;
    AdwCarousel *pCarousel = nullptr;
    GtkWidget *pBack = nullptr;
    GtkWidget *pSkip = nullptr;
    GtkButton *pNext = nullptr;
    GtkLabel *pStudioMeta = nullptr;
    GtkButton *pStudioButton = nullptr;
    GtkWidget *pStudioBar = nullptr;
    std::vector<GtkWidget *> pThemeTiles;
    std::vector<AdwSwitchRow *> pSwitchRows;
    std::string appliedStudioMeta;
    guint tickId = 0;
    // Set while a toggle is being written back, so the poll below cannot fight the user.
    bool writing = false;
    std::function<void()> onClosed;
};

struct SwitchBinding {
    Tour *pTour = nullptr;
    bool Settings::*pField = nullptr;
};

struct ThemeBinding {
    Tour *pTour = nullptr;
    const char *pId = nullptr;
};

GtkWidget *caption(const std::string &text) {
    GtkWidget *pLabel = gtk_label_new(text.c_str());
    gtk_label_set_xalign(GTK_LABEL(pLabel), 0.0f);
    gtk_widget_add_css_class(pLabel, "caption-heading");
    gtk_widget_add_css_class(pLabel, "dim-label");
    return pLabel;
}

GtkWidget *bodyLabel(const std::string &text, const char *pStyleClass) {
    GtkWidget *pLabel = gtk_label_new(text.c_str());
    gtk_label_set_xalign(GTK_LABEL(pLabel), 0.0f);
    gtk_label_set_wrap(GTK_LABEL(pLabel), TRUE);
    if (pStyleClass != nullptr) gtk_widget_add_css_class(pLabel, pStyleClass);
    return pLabel;
}

GtkWidget *tourLogo(int size) {
    GdkPaintable *pLogo = logoPaintable(kTuxbloxLogoPng, kTuxbloxLogoPngLen, size);
    GtkWidget *pImage;
    if (pLogo != nullptr) {
        pImage = gtk_image_new_from_paintable(pLogo);
        g_object_unref(pLogo);
    } else {
        // A machine that cannot decode the logo gets the desktop's copy rather than a hole.
        pImage = gtk_image_new_from_icon_name("tuxblox");
    }
    gtk_image_set_pixel_size(GTK_IMAGE(pImage), size);
    return pImage;
}

GtkWidget *pageBox() {
    GtkWidget *pBox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_widget_set_margin_start(pBox, 24);
    gtk_widget_set_margin_end(pBox, 24);
    gtk_widget_set_margin_top(pBox, 26);
    gtk_widget_set_margin_bottom(pBox, 8);
    return pBox;
}

void writeSettings(Tour &tour, const std::function<void(Settings &)> &change) {
    Settings settings = tour.pApp->snapshot().settings;
    change(settings);
    tour.writing = true;
    tour.pApp->updateSettings(settings);
    tour.writing = false;
}

void onThemePicked(GtkToggleButton *pButton, gpointer data) {
    if (!gtk_toggle_button_get_active(pButton)) return;
    auto *pBinding = static_cast<ThemeBinding *>(data);
    const std::string id = pBinding->pId;
    writeSettings(*pBinding->pTour, [&](Settings &settings) { settings.theme = id; });
}

void onSwitchToggled(AdwSwitchRow *pRow, GParamSpec *, gpointer data) {
    auto *pBinding = static_cast<SwitchBinding *>(data);
    if (pBinding->pTour->writing) return;
    const bool on = adw_switch_row_get_active(pRow);
    bool Settings::*pField = pBinding->pField;
    writeSettings(*pBinding->pTour, [&](Settings &settings) { settings.*pField = on; });
}

void onLinkPressed(GtkButton *pButton, gpointer) {
    openUrl(static_cast<const char *>(g_object_get_data(G_OBJECT(pButton), "tuxblox-url")));
}

gboolean onPolicyLink(GtkLabel *, const char *pUri, gpointer) {
    openUrl(pUri);
    // Handled here, so GTK does not also hand it to its own default opener.
    return TRUE;
}

void onDownloadStudio(GtkButton *, gpointer data) {
    auto *pTour = static_cast<Tour *>(data);
    pTour->pApp->requestInstallVersion(LaunchTarget::Studio, VersionSelectMode::Latest, "live", "");
}

// Page 1: who TuxBlox is, the launcher's colours, and an offer to fetch Studio.
GtkWidget *buildWelcomePage(Tour &tour) {
    GtkWidget *pBox = pageBox();

    GtkWidget *pHeading = gtk_label_new("Welcome to TuxBlox");
    gtk_label_set_xalign(GTK_LABEL(pHeading), 0.0f);
    gtk_widget_add_css_class(pHeading, "title-2");

    GtkWidget *pText = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4);
    gtk_widget_set_hexpand(pText, TRUE);
    gtk_widget_set_valign(pText, GTK_ALIGN_CENTER);
    gtk_box_append(GTK_BOX(pText), pHeading);
    gtk_box_append(GTK_BOX(pText),
                   bodyLabel("TuxBlox is an open-source launcher and compatibility layer for Roblox.", "dim-label"));

    GtkWidget *pHeader = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 15);
    gtk_box_append(GTK_BOX(pHeader), tourLogo(54));
    gtk_box_append(GTK_BOX(pHeader), pText);
    gtk_box_append(GTK_BOX(pBox), pHeader);

    GtkWidget *pAppearance = caption("Appearance");
    gtk_widget_set_margin_top(pAppearance, 22);
    gtk_widget_set_margin_bottom(pAppearance, 9);
    gtk_box_append(GTK_BOX(pBox), pAppearance);

    const std::string current = tour.pApp->snapshot().settings.theme;
    GtkWidget *pTiles = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 9);
    gtk_box_set_homogeneous(GTK_BOX(pTiles), TRUE);
    GtkToggleButton *pFirst = nullptr;
    for (const ThemeChoice &choice : Themes) {
        GtkWidget *pSwatch = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
        gtk_widget_add_css_class(pSwatch, "theme-swatch");
        gtk_widget_add_css_class(pSwatch, choice.pSwatchClass);

        GtkWidget *pLabel = gtk_label_new(choice.pLabel);
        gtk_widget_add_css_class(pLabel, "caption");

        GtkWidget *pInner = gtk_box_new(GTK_ORIENTATION_VERTICAL, 6);
        gtk_box_append(GTK_BOX(pInner), pSwatch);
        gtk_box_append(GTK_BOX(pInner), pLabel);

        GtkWidget *pTile = gtk_toggle_button_new();
        gtk_button_set_child(GTK_BUTTON(pTile), pInner);
        gtk_widget_add_css_class(pTile, "flat");
        gtk_widget_add_css_class(pTile, "theme-tile");
        if (pFirst == nullptr) {
            pFirst = GTK_TOGGLE_BUTTON(pTile);
        } else {
            gtk_toggle_button_set_group(GTK_TOGGLE_BUTTON(pTile), pFirst);
        }
        if (current == choice.pId) gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(pTile), TRUE);
        auto *pBinding = new ThemeBinding{&tour, choice.pId};
        g_signal_connect_data(pTile, "toggled", G_CALLBACK(onThemePicked), pBinding,
                              [](gpointer data, GClosure *) { delete static_cast<ThemeBinding *>(data); },
                              static_cast<GConnectFlags>(0));
        tour.pThemeTiles.push_back(pTile);
        gtk_box_append(GTK_BOX(pTiles), pTile);
    }
    gtk_box_append(GTK_BOX(pBox), pTiles);

    GtkWidget *pStudioCaption = caption("Roblox Studio");
    gtk_widget_set_margin_top(pStudioCaption, 22);
    gtk_widget_set_margin_bottom(pStudioCaption, 9);
    gtk_box_append(GTK_BOX(pBox), pStudioCaption);

    GtkWidget *pName = gtk_label_new("Roblox Studio");
    gtk_label_set_xalign(GTK_LABEL(pName), 0.0f);
    GtkWidget *pMeta = bodyLabel("", "caption");
    gtk_widget_add_css_class(pMeta, "dim-label");
    tour.pStudioMeta = GTK_LABEL(pMeta);

    tour.pStudioBar = gtk_progress_bar_new();
    gtk_widget_set_margin_top(tour.pStudioBar, 6);
    gtk_widget_set_visible(tour.pStudioBar, FALSE);

    GtkWidget *pStudioText = gtk_box_new(GTK_ORIENTATION_VERTICAL, 2);
    gtk_widget_set_hexpand(pStudioText, TRUE);
    gtk_box_append(GTK_BOX(pStudioText), pName);
    gtk_box_append(GTK_BOX(pStudioText), pMeta);
    gtk_box_append(GTK_BOX(pStudioText), tour.pStudioBar);

    GtkWidget *pDownload = gtk_button_new_with_label("Download");
    gtk_widget_add_css_class(pDownload, "suggested-action");
    gtk_widget_set_valign(pDownload, GTK_ALIGN_CENTER);
    tour.pStudioButton = GTK_BUTTON(pDownload);
    g_signal_connect(pDownload, "clicked", G_CALLBACK(onDownloadStudio), &tour);

    GtkWidget *pStudioRow = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 12);
    gtk_widget_set_margin_start(pStudioRow, 14);
    gtk_widget_set_margin_end(pStudioRow, 14);
    gtk_widget_set_margin_top(pStudioRow, 12);
    gtk_widget_set_margin_bottom(pStudioRow, 12);
    gtk_box_append(GTK_BOX(pStudioRow), pStudioText);
    gtk_box_append(GTK_BOX(pStudioRow), pDownload);

    GtkWidget *pCard = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_widget_add_css_class(pCard, "card");
    gtk_box_append(GTK_BOX(pCard), pStudioRow);
    gtk_box_append(GTK_BOX(pBox), pCard);

    GtkWidget *pHelp = bodyLabel("Optional, you can install it later from the Home page instead.", "caption");
    gtk_widget_add_css_class(pHelp, "dim-label");
    gtk_widget_set_margin_top(pHelp, 10);
    gtk_box_append(GTK_BOX(pBox), pHelp);
    return pBox;
}

// Page 2: the four settings that send something outside this machine.
GtkWidget *buildOptionalPage(Tour &tour) {
    GtkWidget *pBox = pageBox();

    GtkWidget *pHeading = gtk_label_new("Optional extras");
    gtk_label_set_xalign(GTK_LABEL(pHeading), 0.0f);
    gtk_widget_add_css_class(pHeading, "title-2");
    gtk_box_append(GTK_BOX(pBox), pHeading);

    GtkWidget *pBody = bodyLabel("All four are off until you turn them on.", "dim-label");
    gtk_widget_set_margin_top(pBody, 4);
    gtk_widget_set_margin_bottom(pBody, 16);
    gtk_box_append(GTK_BOX(pBox), pBody);

    const Settings settings = tour.pApp->snapshot().settings;
    GtkWidget *pList = gtk_list_box_new();
    gtk_list_box_set_selection_mode(GTK_LIST_BOX(pList), GTK_SELECTION_NONE);
    gtk_widget_add_css_class(pList, "boxed-list");
    for (const SwitchChoice &choice : Switches) {
        GtkWidget *pRow = plainSwitchRow(choice.pTitle, choice.pSubtitle);
        adw_switch_row_set_active(ADW_SWITCH_ROW(pRow), settings.*choice.pField);
        auto *pBinding = new SwitchBinding{&tour, choice.pField};
        g_signal_connect_data(pRow, "notify::active", G_CALLBACK(onSwitchToggled), pBinding,
                              [](gpointer data, GClosure *) { delete static_cast<SwitchBinding *>(data); },
                              static_cast<GConnectFlags>(0));
        tour.pSwitchRows.push_back(ADW_SWITCH_ROW(pRow));
        gtk_list_box_append(GTK_LIST_BOX(pList), pRow);
    }
    gtk_box_append(GTK_BOX(pBox), pList);

    GtkWidget *pPolicy = gtk_label_new(
        "TuxBlox's <a href=\"https://tuxblox.net/privacy\">Privacy Policy</a> and "
        "<a href=\"https://tuxblox.net/terms\">Terms of Use</a> apply to these features.");
    gtk_label_set_use_markup(GTK_LABEL(pPolicy), TRUE);
    gtk_label_set_xalign(GTK_LABEL(pPolicy), 0.0f);
    gtk_label_set_wrap(GTK_LABEL(pPolicy), TRUE);
    gtk_widget_add_css_class(pPolicy, "caption");
    gtk_widget_add_css_class(pPolicy, "dim-label");
    gtk_widget_set_margin_top(pPolicy, 13);
    g_signal_connect(pPolicy, "activate-link", G_CALLBACK(onPolicyLink), nullptr);
    gtk_box_append(GTK_BOX(pBox), pPolicy);
    return pBox;
}

// Page 3: the same links the About page carries.
GtkWidget *buildFinishPage(Tour &) {
    GtkWidget *pBox = pageBox();

    GtkWidget *pLogo = tourLogo(46);
    gtk_widget_set_halign(pLogo, GTK_ALIGN_CENTER);
    gtk_box_append(GTK_BOX(pBox), pLogo);

    GtkWidget *pHeading = gtk_label_new("You are all set");
    gtk_widget_add_css_class(pHeading, "title-2");
    gtk_widget_set_margin_top(pHeading, 10);
    gtk_box_append(GTK_BOX(pBox), pHeading);

    GtkWidget *pBody = gtk_label_new("Everything here is in the About tab too.");
    gtk_label_set_wrap(GTK_LABEL(pBody), TRUE);
    gtk_label_set_justify(GTK_LABEL(pBody), GTK_JUSTIFY_CENTER);
    gtk_widget_add_css_class(pBody, "dim-label");
    gtk_widget_set_margin_top(pBody, 4);
    gtk_widget_set_margin_bottom(pBody, 18);
    gtk_box_append(GTK_BOX(pBox), pBody);

    GtkWidget *pGrid = gtk_grid_new();
    gtk_grid_set_column_spacing(GTK_GRID(pGrid), 9);
    gtk_grid_set_row_spacing(GTK_GRID(pGrid), 9);
    gtk_grid_set_column_homogeneous(GTK_GRID(pGrid), TRUE);
    int at = 0;
    for (const TourLink &link : tourLinks()) {
        GtkWidget *pButton = gtk_button_new_with_label(link.pLabel);
        gtk_widget_set_hexpand(pButton, TRUE);
        g_object_set_data(G_OBJECT(pButton), "tuxblox-url", const_cast<char *>(link.pUrl));
        g_signal_connect(pButton, "clicked", G_CALLBACK(onLinkPressed), nullptr);
        gtk_grid_attach(GTK_GRID(pGrid), pButton, at % 2, at / 2, 1, 1);
        ++at;
    }
    gtk_box_append(GTK_BOX(pBox), pGrid);
    return pBox;
}

int currentPage(const Tour &tour) {
    return static_cast<int>(adw_carousel_get_position(tour.pCarousel) + 0.5);
}

void scrollTo(Tour &tour, int index) {
    GtkWidget *pPage = adw_carousel_get_nth_page(tour.pCarousel, static_cast<guint>(index));
    if (pPage != nullptr) adw_carousel_scroll_to(tour.pCarousel, pPage, TRUE);
}

void applyFooter(Tour &tour) {
    const int at = currentPage(tour);
    const bool last = at >= tourSkipTarget();
    gtk_widget_set_visible(tour.pBack, at > 0);
    gtk_widget_set_visible(tour.pSkip, !last);
    setButtonLabel(tour.pNext, last ? "Get started" : "Next");
}

void onCarouselMoved(AdwCarousel *, GParamSpec *, gpointer data) {
    applyFooter(*static_cast<Tour *>(data));
}

void onBack(GtkButton *, gpointer data) {
    auto *pTour = static_cast<Tour *>(data);
    scrollTo(*pTour, currentPage(*pTour) - 1);
}

void onSkip(GtkButton *, gpointer data) {
    auto *pTour = static_cast<Tour *>(data);
    scrollTo(*pTour, tourSkipTarget());
}

void onNext(GtkButton *, gpointer data) {
    auto *pTour = static_cast<Tour *>(data);
    if (currentPage(*pTour) >= tourSkipTarget()) {
        adw_dialog_force_close(pTour->pDialog);
        return;
    }
    scrollTo(*pTour, currentPage(*pTour) + 1);
}

// The Studio row is the one part that moves on its own, so it is polled while the tour is open.
gboolean onTourTick(gpointer data) {
    auto *pTour = static_cast<Tour *>(data);
    const AppSnapshot snap = pTour->pApp->snapshot();
    const bool installed = !snap.versions.studio.activeHash.empty();
    const bool busy = snap.versionInstall.target == LaunchTarget::Studio &&
                      (snap.versionInstall.phase == VersionInstallPhase::ResolvingVersion ||
                       snap.versionInstall.phase == VersionInstallPhase::DownloadingManifest ||
                       snap.versionInstall.phase == VersionInstallPhase::DownloadingPackages ||
                       snap.versionInstall.phase == VersionInstallPhase::Extracting);

    std::string meta = "Latest \xC2\xB7 LIVE";
    if (busy) {
        meta = "Downloading\xE2\x80\xA6";
    } else if (installed) {
        meta = "Installed";
        for (const InstalledVersion &version : snap.versions.studio.installed) {
            if (version.hash != snap.versions.studio.activeHash) continue;
            if (!version.versionString.empty()) meta = "Installed \xC2\xB7 " + version.versionString;
            break;
        }
    }
    if (meta != pTour->appliedStudioMeta) {
        pTour->appliedStudioMeta = meta;
        setLabelText(pTour->pStudioMeta, meta);
    }

    gtk_widget_set_visible(pTour->pStudioBar, busy);
    if (busy) gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(pTour->pStudioBar), snap.versionInstall.fraction);

    const char *pLabel = busy ? "Downloading\xE2\x80\xA6" : (installed ? "Installed" : "Download");
    setButtonLabel(pTour->pStudioButton, pLabel);
    gtk_widget_set_sensitive(GTK_WIDGET(pTour->pStudioButton), !busy && !installed);
    return G_SOURCE_CONTINUE;
}

void onTourClosed(AdwDialog *, gpointer data) {
    auto *pTour = static_cast<Tour *>(data);
    if (pTour->tickId != 0) {
        g_source_remove(pTour->tickId);
        pTour->tickId = 0;
    }
    if (pTour->onClosed) pTour->onClosed();
    delete pTour;
}

} // namespace

void presentWelcomeTour(App &app, GtkWidget *pParent, std::function<void()> onClosed) {
    auto *pTour = new Tour;
    pTour->pApp = &app;
    pTour->onClosed = std::move(onClosed);
    pTour->pDialog = adw_dialog_new();

    GtkWidget *pCarousel = adw_carousel_new();
    pTour->pCarousel = ADW_CAROUSEL(pCarousel);
    // Off, or a scroll over the switch list would change the page under the pointer.
    adw_carousel_set_allow_scroll_wheel(pTour->pCarousel, FALSE);
    adw_carousel_append(pTour->pCarousel, buildWelcomePage(*pTour));
    adw_carousel_append(pTour->pCarousel, buildOptionalPage(*pTour));
    adw_carousel_append(pTour->pCarousel, buildFinishPage(*pTour));
    gtk_widget_set_vexpand(pCarousel, TRUE);
    g_signal_connect(pCarousel, "notify::position", G_CALLBACK(onCarouselMoved), pTour);

    GtkWidget *pDots = adw_carousel_indicator_dots_new();
    adw_carousel_indicator_dots_set_carousel(ADW_CAROUSEL_INDICATOR_DOTS(pDots), pTour->pCarousel);

    pTour->pBack = gtk_button_new_with_label("Back");
    g_signal_connect(pTour->pBack, "clicked", G_CALLBACK(onBack), pTour);
    pTour->pSkip = gtk_button_new_with_label("Skip");
    gtk_widget_add_css_class(pTour->pSkip, "flat");
    g_signal_connect(pTour->pSkip, "clicked", G_CALLBACK(onSkip), pTour);
    GtkWidget *pNext = gtk_button_new_with_label("Next");
    gtk_widget_add_css_class(pNext, "suggested-action");
    pTour->pNext = GTK_BUTTON(pNext);
    g_signal_connect(pNext, "clicked", G_CALLBACK(onNext), pTour);

    GtkWidget *pSpacer = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    gtk_widget_set_hexpand(pSpacer, TRUE);

    GtkWidget *pFooter = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    gtk_widget_set_margin_start(pFooter, 20);
    gtk_widget_set_margin_end(pFooter, 20);
    gtk_widget_set_margin_top(pFooter, 12);
    gtk_widget_set_margin_bottom(pFooter, 16);
    gtk_box_append(GTK_BOX(pFooter), pDots);
    gtk_box_append(GTK_BOX(pFooter), pSpacer);
    gtk_box_append(GTK_BOX(pFooter), pTour->pSkip);
    gtk_box_append(GTK_BOX(pFooter), pTour->pBack);
    gtk_box_append(GTK_BOX(pFooter), pNext);

    GtkWidget *pOuter = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_box_append(GTK_BOX(pOuter), pCarousel);
    gtk_box_append(GTK_BOX(pOuter), pFooter);

    // No title bar when presented parentless, so the whole surface drags the window.
    GtkWidget *pHandle = gtk_window_handle_new();
    gtk_window_handle_set_child(GTK_WINDOW_HANDLE(pHandle), pOuter);

    adw_dialog_set_title(pTour->pDialog, "Welcome to TuxBlox");
    // The installer's width, which is what every other TuxBlox dialog uses.
    adw_dialog_set_content_width(pTour->pDialog, 520);
    adw_dialog_set_child(pTour->pDialog, pHandle);
    adw_dialog_set_default_widget(pTour->pDialog, pNext);
    adw_dialog_set_focus(pTour->pDialog, pNext);
    g_signal_connect(pTour->pDialog, "closed", G_CALLBACK(onTourClosed), pTour);

    applyFooter(*pTour);
    onTourTick(pTour);
    pTour->tickId = g_timeout_add(250, onTourTick, pTour);
    adw_dialog_present(pTour->pDialog, pParent);
}

} // namespace tuxblox
