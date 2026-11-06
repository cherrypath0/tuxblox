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

#include "boot_overlay.h"
#include "boot_animation.h"
#include "tuxblox_logo_png.h"

#include <algorithm>
#include <functional>
#include <utility>

// Third Parties
#include <gdk-pixbuf/gdk-pixbuf.h>
#include <gtk/gtk.h>
#include <pango/pangocairo.h>

namespace tuxblox {

// The header only forward-declares this, so it has to live in the namespace
// proper rather than in the anonymous one with the helpers.
struct BootOverlay {
    GtkWidget *pWidget = nullptr;
    GdkPixbuf *pLogo = nullptr;
    guint tickId = 0;
    guint finishId = 0;
    gint64 startTimeUs = 0;
    double elapsedMs = 0.0;
    bool started = false;
    bool finishScheduled = false;
    std::function<void()> onFinished;
};

namespace {

// The lockup scales with the window instead of being pinned to the little
// 16:9 prototype. 124px is the floor, which keeps the small prototype size;
// a normal 988x549 launcher lands around 165px for the mark and 80px for the
// name.
constexpr double kMinLogoSize = 124.0;
constexpr double kMaxLogoSize = 190.0;
constexpr double kLogoHeightRatio = 0.30;
constexpr double kTextToLogoRatio = 60.0 / 124.0;
constexpr double kGapToLogoRatio = 22.0 / 124.0;
constexpr double kPi = 3.14159265358979323846;

// A short fade at the end reveals the content behind the overlay instead of
// snapping from the finished lockup to the window in one frame.
constexpr double kOverlayFadeMs = 180.0;

bool animationsEnabled() {
    GtkSettings *pSettings = gtk_settings_get_default();
    if (pSettings == nullptr) return true;
    gboolean enabled = TRUE;
    g_object_get(pSettings, "gtk-enable-animations", &enabled, nullptr);
    return enabled != FALSE;
}

void paintBackground(cairo_t *cr, int width, int height) {
    // The same near-black the launcher uses, with a blue-to-orange halo that
    // sits still behind the moving lockup.
    cairo_set_source_rgb(cr, 8.0 / 255.0, 10.0 / 255.0, 14.0 / 255.0);
    cairo_paint(cr);

    const double centreX = width / 2.0;
    const double centreY = height / 2.0;

    cairo_pattern_t *pBlue = cairo_pattern_create_radial(centreX, centreY, 0.0, centreX, centreY,
                                                         std::max(width, height) * 0.62);
    cairo_pattern_add_color_stop_rgba(pBlue, 0.0, 10.0 / 255.0, 161.0 / 255.0, 249.0 / 255.0, 0.085);
    cairo_pattern_add_color_stop_rgba(pBlue, 1.0, 10.0 / 255.0, 161.0 / 255.0, 249.0 / 255.0, 0.0);
    cairo_set_source(cr, pBlue);
    cairo_paint(cr);
    cairo_pattern_destroy(pBlue);

    cairo_pattern_t *pOrange = cairo_pattern_create_radial(
        width * 0.58, height * 0.68, 0.0, width * 0.58, height * 0.68, width * 0.42);
    cairo_pattern_add_color_stop_rgba(pOrange, 0.0, 252.0 / 255.0, 161.0 / 255.0, 28.0 / 255.0, 0.055);
    cairo_pattern_add_color_stop_rgba(pOrange, 1.0, 252.0 / 255.0, 161.0 / 255.0, 28.0 / 255.0, 0.0);
    cairo_set_source(cr, pOrange);
    cairo_paint(cr);
    cairo_pattern_destroy(pOrange);
}

void paintLogoShadow(cairo_t *cr, double centreX, double centreY, double logoSize, double opacity) {
    cairo_save(cr);
    cairo_translate(cr, centreX, centreY + logoSize * 0.40);
    cairo_scale(cr, 1.0, 0.30);

    cairo_pattern_t *pShadow = cairo_pattern_create_radial(0.0, 0.0, 0.0, 0.0, 0.0, logoSize * 0.62);
    cairo_pattern_add_color_stop_rgba(pShadow, 0.0, 0.0, 0.0, 0.0, 0.42 * opacity);
    cairo_pattern_add_color_stop_rgba(pShadow, 1.0, 0.0, 0.0, 0.0, 0.0);
    cairo_set_source(cr, pShadow);
    cairo_arc(cr, 0.0, 0.0, logoSize * 0.62, 0.0, 2.0 * kPi);
    cairo_fill(cr);
    cairo_pattern_destroy(pShadow);

    cairo_restore(cr);
}

void paintLogo(cairo_t *cr, GdkPixbuf *pLogo, const BootAnimationFrame &frame,
               double centreX, double centreY, double logoSize) {
    if (pLogo == nullptr || frame.logoOpacity <= 0.0) return;

    paintLogoShadow(cr, centreX, centreY, logoSize, frame.logoOpacity);

    const int logoWidth = gdk_pixbuf_get_width(pLogo);
    const int logoHeight = gdk_pixbuf_get_height(pLogo);
    if (logoWidth <= 0 || logoHeight <= 0) return;

    // The asset is already tilted; the animation starts at the angle that
    // cancels that tilt and settles back to zero over the move. The pixbuf is
    // the full-size PNG, so the renderer has enough pixels above 1x.
    const double drawnSize = logoSize * frame.logoScale;
    cairo_save(cr);
    cairo_translate(cr, centreX, centreY);
    cairo_rotate(cr, frame.logoAngleDegrees * kPi / 180.0);
    cairo_scale(cr, drawnSize / logoWidth, drawnSize / logoHeight);
    cairo_translate(cr, -logoWidth / 2.0, -logoHeight / 2.0);
    gdk_cairo_set_source_pixbuf(cr, pLogo, 0.0, 0.0);
    cairo_pattern_set_filter(cairo_get_source(cr), CAIRO_FILTER_BEST);
    cairo_paint_with_alpha(cr, frame.logoOpacity);
    cairo_restore(cr);
}

struct WordmarkMetrics {
    int width = 0;
    int height = 0;
    int tuxWidth = 0;
};

// The wordmark is drawn in two pieces so "Blox" can carry the gradient. Both
// pieces use the same layout, so only the widths need measuring once a frame.
WordmarkMetrics measureWordmark(PangoLayout *pLayout) {
    WordmarkMetrics metrics;
    pango_layout_set_text(pLayout, "TuxBlox", -1);
    pango_layout_get_pixel_size(pLayout, &metrics.width, &metrics.height);

    int ignoredHeight = 0;
    pango_layout_set_text(pLayout, "Tux", -1);
    pango_layout_get_pixel_size(pLayout, &metrics.tuxWidth, &ignoredHeight);
    return metrics;
}

void paintWordmark(cairo_t *cr, PangoLayout *pLayout, const BootAnimationFrame &frame,
                   const WordmarkMetrics &metrics, double textX, double textY) {
    if (frame.textOpacity <= 0.0 || frame.textReveal <= 0.0) return;

    const double visibleWidth = metrics.width * frame.textReveal;
    if (visibleWidth <= 0.0) return;

    // The name is revealed from its left edge, which starts under the logo.
    // The vertical padding keeps ascenders and descenders out of the clip.
    cairo_save(cr);
    cairo_rectangle(cr, textX, textY - 8.0, visibleWidth, metrics.height + 16.0);
    cairo_clip(cr);
    cairo_push_group(cr);

    pango_layout_set_text(pLayout, "Tux", -1);
    cairo_set_source_rgb(cr, 0.957, 0.969, 0.984);
    cairo_move_to(cr, textX, textY);
    pango_cairo_show_layout(cr, pLayout);

    pango_layout_set_text(pLayout, "Blox", -1);
    cairo_pattern_t *pGradient = cairo_pattern_create_linear(
        textX + metrics.tuxWidth, 0.0, textX + metrics.width, 0.0);
    cairo_pattern_add_color_stop_rgb(pGradient, 0.0, 10.0 / 255.0, 161.0 / 255.0, 249.0 / 255.0);
    cairo_pattern_add_color_stop_rgb(pGradient, 0.5, 125.0 / 255.0, 208.0 / 255.0, 1.0);
    cairo_pattern_add_color_stop_rgb(pGradient, 1.0, 252.0 / 255.0, 161.0 / 255.0, 28.0 / 255.0);
    cairo_set_source(cr, pGradient);
    cairo_move_to(cr, textX + metrics.tuxWidth, textY);
    pango_cairo_show_layout(cr, pLayout);
    cairo_pattern_destroy(pGradient);

    cairo_pop_group_to_source(cr);
    cairo_paint_with_alpha(cr, frame.textOpacity);
    cairo_restore(cr);
}

void drawBootOverlay(GtkDrawingArea *, cairo_t *cr, int width, int height, gpointer data) {
    auto *pOverlay = static_cast<BootOverlay *>(data);
    paintBackground(cr, width, height);

    const double logoSize = std::clamp(height * kLogoHeightRatio, kMinLogoSize, kMaxLogoSize);
    const double textSize = logoSize * kTextToLogoRatio;
    const double gap = logoSize * kGapToLogoRatio;

    PangoLayout *pLayout = pango_cairo_create_layout(cr);
    PangoFontDescription *pFont = pango_font_description_new();
    pango_font_description_set_family(pFont, "Adwaita Sans");
    pango_font_description_set_weight(pFont, PANGO_WEIGHT_BOLD);
    pango_font_description_set_absolute_size(pFont, textSize * PANGO_SCALE);
    pango_layout_set_font_description(pLayout, pFont);
    pango_font_description_free(pFont);
    // Absolute size is in points; 72 DPI makes one point one logical pixel, so
    // the ratio above lands on the same pixel sizes the prototype used.
    pango_cairo_context_set_resolution(pango_layout_get_context(pLayout), 72.0);

    const WordmarkMetrics metrics = measureWordmark(pLayout);
    const BootAnimationFrame frame = bootAnimationAt(pOverlay->elapsedMs);
    const BootLockupLayout layout = bootLockupLayout(width, logoSize, metrics.width, gap);

    const double logoX = layout.startLogoX + (layout.finalLogoX - layout.startLogoX) * frame.slideProgress;
    const double textX = layout.startTextX + (layout.finalTextX - layout.startTextX) * frame.slideProgress;
    const double centreY = height / 2.0;
    const double textY = centreY - metrics.height / 2.0;

    // Text first: the logo is meant to sit on top of it while the two part.
    paintWordmark(cr, pLayout, frame, metrics, textX, textY);
    paintLogo(cr, pOverlay->pLogo, frame, logoX + logoSize / 2.0, centreY, logoSize);

    g_object_unref(pLayout);
}

gboolean onFinishedIdle(gpointer data) {
    auto *pOverlay = static_cast<BootOverlay *>(data);
    pOverlay->finishId = 0;

    // Move the callback out before calling it: it is allowed to destroy the
    // overlay, and then this function must not touch pOverlay again.
    std::function<void()> onFinished = std::move(pOverlay->onFinished);
    pOverlay->onFinished = nullptr;
    if (onFinished) onFinished();
    return G_SOURCE_REMOVE;
}

void finishBootOverlay(BootOverlay *pOverlay) {
    if (pOverlay == nullptr || pOverlay->finishScheduled) return;
    pOverlay->finishScheduled = true;
    // The tick that called this is removed by returning G_SOURCE_REMOVE.
    pOverlay->tickId = 0;
    pOverlay->finishId = g_idle_add(onFinishedIdle, pOverlay);
}

gboolean onTick(GtkWidget *pWidget, GdkFrameClock *pFrameClock, gpointer data) {
    auto *pOverlay = static_cast<BootOverlay *>(data);
    const gint64 now = gdk_frame_clock_get_frame_time(pFrameClock);

    if (!pOverlay->started) {
        pOverlay->started = true;
        pOverlay->startTimeUs = now;
    }
    pOverlay->elapsedMs = static_cast<double>(now - pOverlay->startTimeUs) / 1000.0;

    if (pOverlay->elapsedMs >= kBootAnimationDurationMs) {
        const double fade = std::clamp((pOverlay->elapsedMs - kBootAnimationDurationMs) / kOverlayFadeMs,
                                       0.0, 1.0);
        gtk_widget_set_opacity(pWidget, 1.0 - fade);
        if (fade >= 1.0) {
            finishBootOverlay(pOverlay);
            return G_SOURCE_REMOVE;
        }
    }

    gtk_widget_queue_draw(pWidget);
    return G_SOURCE_CONTINUE;
}

void onWidgetDestroy(GtkWidget *, gpointer data) {
    // The window can go away before the entrance ends. The caller still owns
    // the BootOverlay struct and will free it; this only makes sure a pending
    // finish does not run against a window that is no longer there.
    auto *pOverlay = static_cast<BootOverlay *>(data);
    pOverlay->pWidget = nullptr;
    pOverlay->tickId = 0;
    if (pOverlay->finishId != 0) {
        g_source_remove(pOverlay->finishId);
        pOverlay->finishId = 0;
    }
    pOverlay->onFinished = nullptr;
}

} // namespace

BootOverlay *bootOverlayCreate(std::function<void()> onFinished) {
    if (!animationsEnabled()) return nullptr;

    auto *pOverlay = new BootOverlay();
    pOverlay->onFinished = std::move(onFinished);

    // The full-size PNG, not a prescaled copy: the Cairo transform below draws
    // it at the size the window needs, and the extra pixels keep it sharp on a
    // HiDPI display.
    GError *pError = nullptr;
    GInputStream *pStream = g_memory_input_stream_new_from_data(kTuxbloxLogoPng, kTuxbloxLogoPngLen, nullptr);
    pOverlay->pLogo = gdk_pixbuf_new_from_stream(pStream, nullptr, &pError);
    g_object_unref(pStream);
    if (pError != nullptr) g_error_free(pError);
    if (pOverlay->pLogo == nullptr) {
        delete pOverlay;
        return nullptr;
    }

    pOverlay->pWidget = gtk_drawing_area_new();
    gtk_widget_set_hexpand(pOverlay->pWidget, TRUE);
    gtk_widget_set_vexpand(pOverlay->pWidget, TRUE);
    gtk_widget_set_halign(pOverlay->pWidget, GTK_ALIGN_FILL);
    gtk_widget_set_valign(pOverlay->pWidget, GTK_ALIGN_FILL);
    gtk_drawing_area_set_draw_func(GTK_DRAWING_AREA(pOverlay->pWidget), drawBootOverlay, pOverlay, nullptr);
    g_signal_connect(pOverlay->pWidget, "destroy", G_CALLBACK(onWidgetDestroy), pOverlay);
    pOverlay->tickId = gtk_widget_add_tick_callback(pOverlay->pWidget, onTick, pOverlay, nullptr);
    return pOverlay;
}

GtkWidget *bootOverlayWidget(BootOverlay *pOverlay) {
    return pOverlay != nullptr ? pOverlay->pWidget : nullptr;
}

void bootOverlayDestroy(BootOverlay *pOverlay) {
    if (pOverlay == nullptr) return;

    if (pOverlay->finishId != 0) {
        g_source_remove(pOverlay->finishId);
        pOverlay->finishId = 0;
    }
    if (pOverlay->tickId != 0 && pOverlay->pWidget != nullptr) {
        gtk_widget_remove_tick_callback(pOverlay->pWidget, pOverlay->tickId);
        pOverlay->tickId = 0;
    }
    if (pOverlay->pWidget != nullptr) {
        if (gtk_widget_get_parent(pOverlay->pWidget) != nullptr) {
            gtk_widget_unparent(pOverlay->pWidget);
        } else {
            // The widget was created but never added to the window's overlay;
            // drop the floating reference that it still holds.
            g_object_ref_sink(pOverlay->pWidget);
            g_object_unref(pOverlay->pWidget);
        }
        pOverlay->pWidget = nullptr;
    }
    if (pOverlay->pLogo != nullptr) {
        g_object_unref(pOverlay->pLogo);
        pOverlay->pLogo = nullptr;
    }
    delete pOverlay;
}

} // namespace tuxblox
