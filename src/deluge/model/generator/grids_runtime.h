#pragma once
#include "model/generator/grids.h"
namespace deluge::model::generator::grids {
struct Event {
	bool on = false;
	uint8_t part = 0;
	uint8_t velocity = 0;
};
struct Events {
	// All three previous gates off, then all three new gates on.
	std::array<Event, 6> notes{};
	uint8_t count = 0;
	int32_t ticksToNext = 1;
};
class Runtime {
public:
	void queue(const Settings& settings);
	Events process(int64_t tick, int32_t ticksPerStep);
	Events stop();
	bool pending() const { return started_ && settings_.seed != seed_; }

private:
	void release(Events& events);
	Core core_;
	Settings settings_{};
	uint16_t seed_ = 1;
	bool started_ = false;
	std::array<bool, kParts> held_{};
	int32_t ticksPerStep_ = 1;
	int64_t lastTick_ = 0;
	int64_t nextStep_ = 0;
	int64_t offTick_ = 0;
};
} // namespace deluge::model::generator::grids
