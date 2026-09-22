// Copyright 2012 Emilie Gillet.
//
// Author: Emilie Gillet (emilie.o.gillet@gmail.com)
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <http://www.gnu.org/licenses/>.
//
// Adapted from Grids EvaluateDrums / ReadDrumMap. Source pins: docs/dev/grids_core.md.
#include "model/generator/grids.h"
#include "model/generator/grids_data.h"
#include <algorithm>
namespace deluge::model::generator::grids {
namespace {
uint8_t mix(uint8_t a, uint8_t b, uint8_t weight) {
	return (static_cast<uint16_t>(a) * (255 - weight) + static_cast<uint16_t>(b) * weight) >> 8;
}
} // namespace
uint8_t level(uint8_t step, uint8_t part, uint8_t x, uint8_t y) {
	if (part >= kParts)
		return 0;
	const auto i = x >> 6, j = y >> 6;
	const auto offset = part * kSteps + (step % kSteps);
	return mix(mix(data::map[i][j][offset], data::map[i + 1][j][offset], static_cast<uint8_t>(x << 2)),
	           mix(data::map[i][j + 1][offset], data::map[i + 1][j + 1][offset], static_cast<uint8_t>(x << 2)),
	           static_cast<uint8_t>(y << 2));
}
void Core::reset(uint16_t seed) {
	random_ = static_cast<uint32_t>(seed) + 1;
	perturbation_.fill(0);
}
void Core::beginCycle() {
	for (auto& value : perturbation_) {
		random_ ^= random_ << 13;
		random_ ^= random_ >> 17;
		random_ ^= random_ << 5;
		value = static_cast<uint8_t>(random_ >> 24);
	}
}
Hits Core::evaluate(uint8_t step, const Settings& settings) const {
	Hits hits{};
	for (uint8_t part = 0; part < kParts; ++part) {
		const auto perturbation = (perturbation_[part] * (settings.chaos >> 2)) >> 8;
		const auto value = std::min(255, level(step, part, settings.x, settings.y) + perturbation);
		hits[part] = {value > 255 - settings.density[part], value > 192};
	}
	return hits;
}
} // namespace deluge::model::generator::grids
