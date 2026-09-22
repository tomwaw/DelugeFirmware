#pragma once

#include "model/generator/tb3po.h"
#include <array>
#include <cstdint>

namespace deluge::model::generator::tb3po {

struct NoteEvent {
	bool on = false;
	uint8_t note = 0;
	uint8_t velocity = 0;
};

struct Events {
	std::array<NoteEvent, 2> notes{};
	uint8_t count = 0;
	int32_t ticksToNext = 1;
};

// Clip-owned, allocation-free clock adapter. Time is a forward clip-relative tick position including repeats.
// The caller sends returned events in order and calls stop() before giving up ownership of the output.
class Runtime {
public:
	void queue(const Pattern& pattern);
	[[nodiscard]] Events process(int64_t tick, int32_t ticksPerStep);
	[[nodiscard]] Events stop();
	bool pending() const { return pending_; }
	int16_t playingNote() const { return note_; }

private:
	void release(Events& events);
	void applyPending();
	void playStep(Events& events, int64_t tick);
	Pattern active_{};
	Pattern replacement_{};
	bool pending_ = false;
	bool started_ = false;
	uint8_t step_ = 0;
	int16_t note_ = -1;
	int32_t ticksPerStep_ = 1;
	int64_t lastTick_ = 0;
	int64_t nextStepTick_ = 0;
	int64_t offTick_ = 0;
};

} // namespace deluge::model::generator::tb3po
