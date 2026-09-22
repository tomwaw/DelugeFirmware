#pragma once

#include "model/generator/tb3po_runtime.h"
#include <cstddef>

namespace deluge::model::generator::tb3po {

struct CapturedNote {
	int32_t pos = 0;
	int32_t length = 0;
	uint8_t note = 0;
	uint8_t velocity = 0;
	bool slide = false; // The next pitch was sent before this note's off at the same tick.
};

struct CapturedBar {
	// Sixteen steps plus a carried note fit comfortably. Overflow invalidates the bar, never truncates it.
	std::array<CapturedNote, 32> notes{};
	int32_t length = 0;
	uint8_t count = 0;
};

// Records emitted events, never recipes. Two bounded bars and two temporarily overlapping notes; no allocation.
class History {
public:
	void record(int64_t tick, int32_t barLength, const Events& events);
	void stop(int64_t tick); // Keep the last complete bar available after transport stop/mute/disable.
	void reset();            // A seek or new transport invalidates history.
	bool ready() const { return completed_.length > 0; }
	const CapturedBar& completed() const { return completed_; }

private:
	struct HeldNote {
		int64_t start = 0;
		int16_t note = -1;
		uint8_t velocity = 0;
	};
	void advance(int64_t tick, int32_t barLength);
	void finish(const HeldNote& held, int64_t end, bool slide);
	CapturedBar current_{};
	CapturedBar completed_{};
	std::array<HeldNote, 2> held_{};
	int64_t barStart_ = 0;
	int64_t lastTick_ = 0;
	bool collecting_ = false;
	bool valid_ = false;
};

// Ordinary NoteRows send offs before queued ons. One tick of overlap preserves cross-pitch legato.
// Clip at the captured bar boundary and before any subsequent onset of the same pitch.
int32_t frozenNoteLength(const CapturedBar& bar, std::size_t index);

} // namespace deluge::model::generator::tb3po
