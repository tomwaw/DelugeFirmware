#include "model/generator/grids_runtime.h"
#include <algorithm>
#include <limits>
namespace deluge::model::generator::grids {
void Runtime::queue(const Settings& settings) {
	settings_ = settings;
}
void Runtime::release(Events& events) {
	for (uint8_t part = 0; part < kParts; ++part) {
		if (held_[part])
			events.notes[events.count++] = {false, part, 64};
		held_[part] = false;
	}
}
Events Runtime::stop() {
	Events events;
	release(events);
	started_ = false;
	return events;
}
Events Runtime::process(int64_t tick, int32_t ticksPerStep) {
	if (tick < 0)
		return stop();
	Events events;
	ticksPerStep = std::max<int32_t>(1, ticksPerStep);
	bool boundary = false;
	const bool restart = !started_ || tick < lastTick_ || ticksPerStep != ticksPerStep_ || tick > nextStep_;
	if (restart) {
		release(events);
		seed_ = settings_.seed;
		core_.reset(seed_);
		core_.beginCycle();
		ticksPerStep_ = ticksPerStep;
		nextStep_ = tick + ticksPerStep_ - tick % ticksPerStep_;
		started_ = true;
		boundary = tick % ticksPerStep_ == 0;
	}
	else if (tick == nextStep_) {
		nextStep_ += ticksPerStep_;
		boundary = true;
	}
	lastTick_ = tick;
	if (tick >= offTick_)
		release(events);
	const auto step = static_cast<uint8_t>((tick / ticksPerStep_) % kSteps);
	if (boundary) {
		if (step == 0 && !restart) {
			if (seed_ != settings_.seed) {
				seed_ = settings_.seed;
				core_.reset(seed_);
			}
			core_.beginCycle();
		}
		const auto hits = core_.evaluate(step, settings_);
		for (uint8_t part = 0; part < kParts; ++part) {
			if (hits[part].on) {
				events.notes[events.count++] = {true, part, static_cast<uint8_t>(hits[part].accent ? 120 : 80)};
				held_[part] = true;
			}
		}
		offTick_ = tick + std::max<int32_t>(1, ticksPerStep_ / 2);
	}
	const bool held = held_[0] || held_[1] || held_[2];
	const auto next = held ? std::min(nextStep_, offTick_) : nextStep_;
	events.ticksToNext = std::clamp<int64_t>(next - tick, 1, std::numeric_limits<int32_t>::max());
	return events;
}
} // namespace deluge::model::generator::grids
