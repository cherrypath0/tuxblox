#include "progress.h"
#include <algorithm>

namespace tuxblox {

const char* stepLabel(Step step, bool isUpgrade) {
    if (isUpgrade) {
        switch (step) {
            case Step::CreatingDirectory:    return "Preparing";
            case Step::DownloadingProton:
            case Step::VerifyingProton:
            case Step::ExtractingProton:     return "Upgrading Proton";
            case Step::DownloadingLauncher:
            case Step::VerifyingLauncher:
            case Step::DownloadingInstaller:
            case Step::VerifyingInstaller:
            case Step::MovingExecutables:    return "Upgrading TuxBlox";
            // Not part of the "Upgrading ..." collapse -- pre-warming the
            // Roblox installer cache isn't a TuxBlox upgrade concern, so it
            // keeps the same label regardless of fresh-install/upgrade mode.
            case Step::DownloadingRobloxPlayer: return "Downloading Roblox Player";
            case Step::DownloadingRobloxStudio: return "Downloading Roblox Studio";
            default:                         return "";
        }
    }

    switch (step) {
        case Step::CreatingDirectory:       return "Creating TuxBlox directory";
        case Step::DownloadingProton:       return "Downloading Proton";
        case Step::VerifyingProton:         return "Verifying Proton";
        case Step::ExtractingProton:        return "Extracting Proton";
        case Step::DownloadingLauncher:     return "Downloading Launcher";
        case Step::VerifyingLauncher:       return "Verifying Launcher";
        case Step::DownloadingInstaller:    return "Downloading Installer";
        case Step::VerifyingInstaller:      return "Verifying Installer";
        case Step::DownloadingRobloxPlayer: return "Downloading Roblox Player";
        case Step::DownloadingRobloxStudio: return "Downloading Roblox Studio";
        case Step::MovingExecutables:       return "Moving Executables";
        default:                            return "";
    }
}

void Progress::beginStep(Step step) {
    currentStep_ = step;
    stepFraction_ = 0.0;
}

void Progress::setStepFraction(double fraction) {
    stepFraction_ = std::clamp(fraction, 0.0, 1.0);
}

double Progress::overallPercent() const {
    double completed = 0.0;
    for (size_t i = 0; i < static_cast<size_t>(currentStep_); ++i) {
        completed += kStepWeights[i];
    }
    double currentWeight = kStepWeights[static_cast<size_t>(currentStep_)];
    return completed + currentWeight * stepFraction_;
}

} // namespace tuxblox
