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

#pragma once
#include <string>
#include <vector>

namespace tuxblox {

// One weighted phase of the install pipeline. `label` is already fully
// resolved (fresh-install vs "Upgrading ..." wording decided by the
// caller, not looked up here) since it's built per-artifact from whatever
// the manifest happens to list -- there's no fixed, compile-time set of
// phases to hang a lookup table off of.
struct ProgressPhase {
    std::string label;
    double weight; // percentage points; every phase's weight should sum to 100 across the whole sequence
};

// Tracks overall install progress (0.0-100.0) across a caller-supplied,
// dynamically-sized sequence of weighted phases. The phase list isn't
// known at compile time -- it's built at runtime from however many
// artifacts a given manifest lists (see installer_steps.cpp).
class Progress {
public:
    explicit Progress(std::vector<ProgressPhase> phases);

    // Marks phase `index` as current, with 0 fractional progress within it.
    void beginPhase(size_t index);

    // Sets fractional completion (0.0-1.0) within the current phase.
    void setPhaseFraction(double fraction);

    // Current phase's label, for display.
    const std::string& currentLabel() const;

    // Overall progress across all phases, 0.0-100.0.
    double overallPercent() const;

private:
    std::vector<ProgressPhase> phases_;
    size_t currentIndex_ = 0;
    double phaseFraction_ = 0.0;
};

} // namespace tuxblox
