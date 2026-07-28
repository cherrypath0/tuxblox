#pragma once
#include <array>
#include <cstddef>

namespace tuxblox {

// The INSTALLING steps and their percentage weights, in order.
// Weights must sum to 100.
enum class Step : size_t {
    CreatingDirectory = 0,
    DownloadingProton,
    VerifyingProton,
    ExtractingProton,
    DownloadingLauncher,
    VerifyingLauncher,
    DownloadingInstaller,
    VerifyingInstaller,
    // Pre-warms the Roblox Player/Studio installer cache (same URLs/cache
    // paths launch.sh and the launcher's own lazy-download fallback
    // already use) so the first "Launch Player"/"Launch Studio" click
    // doesn't need to download it -- these installers are never *run*
    // here, only fetched; running one happens later, on first launch.
    DownloadingRobloxPlayer,
    DownloadingRobloxStudio,
    MovingExecutables,
    Count
};

constexpr std::array<int, static_cast<size_t>(Step::Count)> kStepWeights = {
    2, 39, 3, 22, 9, 2, 9, 2, 5, 5, 2
};

// `isUpgrade` selects between the fresh-install wording ("Downloading
// Proton", "Downloading Launcher", ...) and the upgrade wording an
// existing install sees ("Upgrading Proton" / "Upgrading TuxBlox",
// collapsing the Proton-artifact steps and the launcher/installer-artifact
// steps into those two user-facing labels respectively).
const char* stepLabel(Step step, bool isUpgrade);

// Tracks overall install progress (0.0-100.0) across the weighted steps.
class Progress {
public:
    // Marks `step` as the current step, with 0 fractional progress within it.
    void beginStep(Step step);

    // Sets fractional completion (0.0-1.0) within the current step.
    void setStepFraction(double fraction);

    // Current step, for display.
    Step currentStep() const { return currentStep_; }

    // Overall progress across all steps, 0.0-100.0.
    double overallPercent() const;

private:
    Step currentStep_ = Step::CreatingDirectory;
    double stepFraction_ = 0.0;
};

} // namespace tuxblox
