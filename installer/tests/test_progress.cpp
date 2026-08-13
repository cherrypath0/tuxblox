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

#include "progress.h"
#include <cassert>
#include <cmath>
#include <cstdio>
#include <stdexcept>

int main() {
    using namespace tuxblox;

    // An empty phase list is a programming error, not a runtime one to
    // tolerate silently -- overallPercent()/currentLabel() would have
    // nothing to index into.
    {
        bool threw = false;
        try {
            Progress empty({});
        } catch (const std::invalid_argument&) {
            threw = true;
        }
        assert(threw);
    }

    // A simple 3-phase sequence (mirrors what installer_steps.cpp builds:
    // one fixed "Creating directory" phase plus two artifact phases).
    Progress p({
        {"Creating TuxBlox directory", 2.0},
        {"Downloading Proton", 78.0},
        {"Downloading Launcher", 20.0},
    });

    p.beginPhase(0);
    assert(p.currentLabel() == "Creating TuxBlox directory");
    assert(std::abs(p.overallPercent() - 0.0) < 1e-9);

    p.beginPhase(1);
    p.setPhaseFraction(0.5);
    assert(p.currentLabel() == "Downloading Proton");
    // weight before phase 1 = 2.0 (phase 0), weight of phase 1 = 78.0
    assert(std::abs(p.overallPercent() - (2.0 + 78.0 * 0.5)) < 1e-9);

    p.beginPhase(2);
    p.setPhaseFraction(1.0);
    assert(p.currentLabel() == "Downloading Launcher");
    // sum of weights before phase 2 = 2.0 + 78.0 = 80.0, + 20.0*1.0 = 100.0
    assert(std::abs(p.overallPercent() - 100.0) < 1e-9);

    // Out-of-range fractions clamp.
    p.setPhaseFraction(1.5);
    assert(std::abs(p.overallPercent() - 100.0) < 1e-9);
    p.setPhaseFraction(-0.5);
    assert(std::abs(p.overallPercent() - 80.0) < 1e-9);

    // beginPhase resets the fraction back to 0 for the new phase.
    p.beginPhase(1);
    assert(std::abs(p.overallPercent() - 2.0) < 1e-9);

    // A single-phase sequence works too (e.g. a manifest with exactly one
    // artifact plus the fixed directory-creation phase collapsed away in a
    // hypothetical minimal caller).
    Progress single({{"Only phase", 100.0}});
    single.beginPhase(0);
    single.setPhaseFraction(0.25);
    assert(std::abs(single.overallPercent() - 25.0) < 1e-9);

    printf("progress: all tests passed\n");
    return 0;
}
