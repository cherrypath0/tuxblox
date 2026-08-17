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

#include "console_ui.h"
#include <atomic>
#include <chrono>
#include <csignal>
#include <cstdio>
#include <string>
#include <thread>
#include <unistd.h>

namespace tuxblox {

namespace {

constexpr int kBarWidth = 28;
constexpr int kLabelWidth = 32;
constexpr auto kPollInterval = std::chrono::milliseconds(100);

// Set from the SIGINT handler, which may do nothing else -- only
// sig_atomic-safe stores are legal there, so the actual cancel happens back
// in the poll loop.
std::atomic<bool> gInterrupted{false};

void onSigint(int) {
    gInterrupted.store(true);
}

std::string phaseLabel(const AppSnapshot& snapshot) {
    switch (snapshot.phase) {
        case AppPhase::Init:
            return "Preparing";
        case AppPhase::FetchingManifest:
            return "Fetching release manifest";
        case AppPhase::Installing:
            // Already resolved to fresh-install vs "Upgrading ..." wording
            // by installer_steps.cpp -- print it as-is.
            return snapshot.currentStepLabel;
        case AppPhase::Done:
            return "Done";
        case AppPhase::Error:
            return "Failed";
    }
    return snapshot.currentStepLabel;
}

int clampPercent(double percent) {
    if (percent < 0.0) return 0;
    if (percent > 100.0) return 100;
    return static_cast<int>(percent + 0.5);
}

// Everything the progress display needs to know about where it left off.
// On a TTY the bar occupies the current line and has to be terminated with
// a newline before any other output is written; when redirected, output is
// already line-oriented and there is nothing to terminate.
struct Renderer {
    bool tty;
    bool barPending = false;
    std::string lastLabel;
    int lastPercent = -1;

    void draw(const std::string& label, int percent) {
        if (tty) {
            std::string bar(static_cast<size_t>(percent * kBarWidth / 100), '#');
            bar.append(static_cast<size_t>(kBarWidth) - bar.size(), '-');
            // Fixed-width label: pads short labels and truncates long ones,
            // so a shorter step can't leave the previous one's tail behind.
            printf("\r%-*.*s [%s] %3d%%", kLabelWidth, kLabelWidth, label.c_str(), bar.c_str(),
                   percent);
            fflush(stdout);
            barPending = true;
        } else if (label != lastLabel || percent - lastPercent >= 1) {
            printf("[%3d%%] %s\n", percent, label.c_str());
            fflush(stdout);
        }
        lastLabel = label;
        lastPercent = percent;
    }

    // Ends the in-place bar so a standalone message doesn't land on top of it.
    void endBar() {
        if (barPending) {
            printf("\n");
            fflush(stdout);
            barPending = false;
        }
    }
};

} // namespace

bool runConsoleInstall(App& app) {
    Renderer renderer{isatty(STDOUT_FILENO) != 0};

    struct sigaction handler {};
    handler.sa_handler = onSigint;
    sigemptyset(&handler.sa_mask);
    struct sigaction previous {};
    sigaction(SIGINT, &handler, &previous);

    bool succeeded = false;
    bool upgradeAnnounced = false;

    for (;;) {
        if (gInterrupted.load()) {
            app.cancel();
            renderer.endBar();
            fprintf(stderr, "Cancelled. Cleaning up...\n");
            // Returning is enough: ~App joins the install thread, which stops
            // at its next checkpoint and removes whatever it created.
            break;
        }

        const AppSnapshot snapshot = app.snapshot();

        if (snapshot.isUpgrade && !upgradeAnnounced) {
            upgradeAnnounced = true;
            renderer.endBar();
            // printf("Existing install found -- upgrading in place.\n");
            fflush(stdout);
        }

        if (snapshot.phase == AppPhase::Error) {
            renderer.endBar();
            fprintf(stderr, "Error: %s\n", snapshot.errorMessage.c_str());
            break;
        }

        // readyToLaunch() is set just before the phase flips to Done, so
        // check both rather than racing on which one this poll observes.
        if (snapshot.phase == AppPhase::Done || app.readyToLaunch()) {
            renderer.draw("Done", 100);
            renderer.endBar();
            printf(snapshot.isUpgrade ? "TuxBlox upgraded successfully.\n"
                                      : "TuxBlox installed successfully.\n");
            fflush(stdout);
            succeeded = true;
            break;
        }

        renderer.draw(phaseLabel(snapshot), clampPercent(snapshot.overallPercent));
        std::this_thread::sleep_for(kPollInterval);
    }

    sigaction(SIGINT, &previous, nullptr);
    return succeeded;
}

} // namespace tuxblox
