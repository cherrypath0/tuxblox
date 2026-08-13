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
#include <algorithm>
#include <stdexcept>

namespace tuxblox {

Progress::Progress(std::vector<ProgressPhase> phases) : phases_(std::move(phases)) {
    if (phases_.empty()) {
        throw std::invalid_argument("Progress: phases must not be empty");
    }
}

void Progress::beginPhase(size_t index) {
    currentIndex_ = index;
    phaseFraction_ = 0.0;
}

void Progress::setPhaseFraction(double fraction) {
    phaseFraction_ = std::clamp(fraction, 0.0, 1.0);
}

const std::string& Progress::currentLabel() const {
    return phases_[currentIndex_].label;
}

double Progress::overallPercent() const {
    double completed = 0.0;
    for (size_t i = 0; i < currentIndex_; ++i) {
        completed += phases_[i].weight;
    }
    return completed + phases_[currentIndex_].weight * phaseFraction_;
}

} // namespace tuxblox
