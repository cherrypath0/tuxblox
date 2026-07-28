#include "progress.h"
#include <cassert>
#include <cmath>
#include <cstdio>
#include <cstring>

int main() {
    using namespace tuxblox;

    int sum = 0;
    for (int w : kStepWeights) sum += w;
    assert(sum == 100);

    // Fresh-install wording.
    assert(std::strcmp(stepLabel(Step::CreatingDirectory, false), "Creating TuxBlox directory") == 0);
    assert(std::strcmp(stepLabel(Step::DownloadingProton, false), "Downloading Proton") == 0);
    assert(std::strcmp(stepLabel(Step::VerifyingProton, false), "Verifying Proton") == 0);
    assert(std::strcmp(stepLabel(Step::ExtractingProton, false), "Extracting Proton") == 0);
    assert(std::strcmp(stepLabel(Step::DownloadingLauncher, false), "Downloading Launcher") == 0);
    assert(std::strcmp(stepLabel(Step::VerifyingLauncher, false), "Verifying Launcher") == 0);
    assert(std::strcmp(stepLabel(Step::DownloadingInstaller, false), "Downloading Installer") == 0);
    assert(std::strcmp(stepLabel(Step::VerifyingInstaller, false), "Verifying Installer") == 0);
    assert(std::strcmp(stepLabel(Step::DownloadingRobloxPlayer, false), "Downloading Roblox Player") == 0);
    assert(std::strcmp(stepLabel(Step::DownloadingRobloxStudio, false), "Downloading Roblox Studio") == 0);
    assert(std::strcmp(stepLabel(Step::MovingExecutables, false), "Moving Executables") == 0);

    // Upgrade wording: the three Proton-artifact steps collapse to one
    // label, the four launcher/installer-artifact steps collapse to
    // another, and the two Roblox pre-fetch steps keep their own label
    // regardless of upgrade mode (pre-warming Roblox's own installer cache
    // isn't a "TuxBlox upgrade" concern).
    assert(std::strcmp(stepLabel(Step::DownloadingProton, true), "Upgrading Proton") == 0);
    assert(std::strcmp(stepLabel(Step::VerifyingProton, true), "Upgrading Proton") == 0);
    assert(std::strcmp(stepLabel(Step::ExtractingProton, true), "Upgrading Proton") == 0);
    assert(std::strcmp(stepLabel(Step::DownloadingLauncher, true), "Upgrading TuxBlox") == 0);
    assert(std::strcmp(stepLabel(Step::VerifyingLauncher, true), "Upgrading TuxBlox") == 0);
    assert(std::strcmp(stepLabel(Step::DownloadingInstaller, true), "Upgrading TuxBlox") == 0);
    assert(std::strcmp(stepLabel(Step::VerifyingInstaller, true), "Upgrading TuxBlox") == 0);
    assert(std::strcmp(stepLabel(Step::MovingExecutables, true), "Upgrading TuxBlox") == 0);
    assert(std::strcmp(stepLabel(Step::DownloadingRobloxPlayer, true), "Downloading Roblox Player") == 0);
    assert(std::strcmp(stepLabel(Step::DownloadingRobloxStudio, true), "Downloading Roblox Studio") == 0);

    Progress p;
    p.beginStep(Step::CreatingDirectory);
    assert(std::abs(p.overallPercent() - 0.0) < 1e-9);

    p.beginStep(Step::DownloadingProton);
    p.setStepFraction(0.5);
    // weight before DownloadingProton = 2 (CreatingDirectory), weight of step = 39
    assert(std::abs(p.overallPercent() - (2.0 + 39.0 * 0.5)) < 1e-9);

    p.beginStep(Step::MovingExecutables);
    p.setStepFraction(1.0);
    // sum of weights before MovingExecutables = 2+39+3+22+9+2+9+2+5+5 = 98, + 2*1.0 = 100
    assert(std::abs(p.overallPercent() - 100.0) < 1e-9);

    // Out-of-range fractions clamp.
    p.setStepFraction(1.5);
    assert(std::abs(p.overallPercent() - 100.0) < 1e-9);
    p.setStepFraction(-0.5);
    assert(std::abs(p.overallPercent() - 98.0) < 1e-9);

    printf("progress: all tests passed\n");
    return 0;
}
