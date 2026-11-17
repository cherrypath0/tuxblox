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

#include "snow.h"

#include <algorithm>
#include <random>
#include <vector>

namespace {

// A flake is kept as a fraction of the window rather than in pixels, so resizing moves the snow with the window instead of leaving a bare column down one side.
struct Flake {
    float x = 0.0f;
    float y = 0.0f;
    float radius = 1.0f;
    float fall = 0.0f;
    float drift = 0.0f;
    float alpha = 1.0f;
};

const int FlakeCount = 90;

// Long enough that a window coming back from being hidden does not drop every flake to the bottom at once
const float LongestStep = 0.1f;

} // namespace

G_BEGIN_DECLS
#define TUXBLOX_TYPE_SNOW (tuxblox_snow_get_type())
G_DECLARE_FINAL_TYPE(TuxbloxSnow, tuxblox_snow, TUXBLOX, SNOW, GtkWidget)
G_END_DECLS

struct _TuxbloxSnow {
    GtkWidget parent_instance;
    std::vector<Flake> *pFlakes;
    std::mt19937 *pRandom;
    gint64 lastFrame;
};

G_DEFINE_TYPE(TuxbloxSnow, tuxblox_snow, GTK_TYPE_WIDGET)

namespace {

float randomBetween(std::mt19937 &random, float low, float high) {
    std::uniform_real_distribution<float> spread(low, high);
    return spread(random);
}

void placeFlake(Flake &flake, std::mt19937 &random, bool anywhere) {
    flake.x = randomBetween(random, 0.0f, 1.0f);
    flake.y = anywhere ? randomBetween(random, 0.0f, 1.0f) : randomBetween(random, -0.08f, -0.01f);
    flake.radius = randomBetween(random, 1.0f, 2.6f);
    flake.fall = randomBetween(random, 0.015f, 0.055f);
    flake.drift = randomBetween(random, -0.012f, 0.012f);
    flake.alpha = randomBetween(random, 0.25f, 0.85f);
}

gboolean onTick(GtkWidget *pWidget, GdkFrameClock *pClock, gpointer) {
    auto *pSnow = TUXBLOX_SNOW(pWidget);
    const gint64 now = gdk_frame_clock_get_frame_time(pClock);
    if (pSnow->lastFrame == 0) pSnow->lastFrame = now;
    const float elapsed = std::min(static_cast<float>(now - pSnow->lastFrame) / 1000000.0f, LongestStep);
    pSnow->lastFrame = now;

    for (Flake &flake : *pSnow->pFlakes) {
        flake.y += flake.fall * elapsed;
        flake.x += flake.drift * elapsed;
        if (flake.y > 1.05f) placeFlake(flake, *pSnow->pRandom, false);
        if (flake.x < -0.05f) flake.x += 1.1f;
        if (flake.x > 1.05f) flake.x -= 1.1f;
    }

    gtk_widget_queue_draw(pWidget);
    return G_SOURCE_CONTINUE;
}

} // namespace

static void tuxblox_snow_snapshot(GtkWidget *pWidget, GtkSnapshot *pSnapshot) {
    auto *pSnow = TUXBLOX_SNOW(pWidget);
    const float width = static_cast<float>(gtk_widget_get_width(pWidget));
    const float height = static_cast<float>(gtk_widget_get_height(pWidget));
    if (width <= 0.0f || height <= 0.0f) return;

    for (const Flake &flake : *pSnow->pFlakes) {
        const float size = flake.radius * 2.0f;
        const graphene_rect_t box = GRAPHENE_RECT_INIT(flake.x * width - flake.radius,
                                                       flake.y * height - flake.radius, size, size);
        GskRoundedRect rounded;
        gsk_rounded_rect_init_from_rect(&rounded, &box, flake.radius);
        GdkRGBA colour = {1.0f, 1.0f, 1.0f, flake.alpha};
        gtk_snapshot_push_rounded_clip(pSnapshot, &rounded);
        gtk_snapshot_append_color(pSnapshot, &colour, &box);
        gtk_snapshot_pop(pSnapshot);
    }
}

static void tuxblox_snow_finalize(GObject *pObject) {
    auto *pSnow = TUXBLOX_SNOW(pObject);
    delete pSnow->pFlakes;
    delete pSnow->pRandom;
    pSnow->pFlakes = nullptr;
    pSnow->pRandom = nullptr;
    G_OBJECT_CLASS(tuxblox_snow_parent_class)->finalize(pObject);
}

static void tuxblox_snow_class_init(TuxbloxSnowClass *pClass) {
    G_OBJECT_CLASS(pClass)->finalize = tuxblox_snow_finalize;
    GTK_WIDGET_CLASS(pClass)->snapshot = tuxblox_snow_snapshot;
}

static void tuxblox_snow_init(TuxbloxSnow *pSnow) {
    pSnow->pFlakes = new std::vector<Flake>(FlakeCount);
    pSnow->pRandom = new std::mt19937(std::random_device{}());
    pSnow->lastFrame = 0;

    for (Flake &flake : *pSnow->pFlakes) placeFlake(flake, *pSnow->pRandom, true);

    gtk_widget_set_can_target(GTK_WIDGET(pSnow), FALSE);
    gtk_widget_add_tick_callback(GTK_WIDGET(pSnow), onTick, nullptr, nullptr);
}

namespace tuxblox {

GtkWidget *createSnowLayer() {
    return GTK_WIDGET(g_object_new(TUXBLOX_TYPE_SNOW, nullptr));
}

} // namespace tuxblox
