#pragma once
#include "model/generator/grids_runtime.h"
#include <cstdint>

namespace deluge::model::generator::grids {
// Opaque destination identities; the firmware supplies drum pointers. Host tests use small integers.
using Destinations = std::array<uintptr_t, kParts>;
struct CapturedNote {
	int32_t pos = 0;
	int32_t length = 0;
	uint8_t part = 0;
	uint8_t velocity = 0;
};
struct CapturedBar {
	// 32 onsets per part plus one carried gate per part at the transport boundary.
	std::array<CapturedNote, kParts*(kSteps + 1)> notes{};
	Destinations destinations{};
	int32_t length = 0;
	uint8_t count = 0;
};
class History {
public:
	void record(int64_t tick, int32_t barLength, const Destinations& destinations, const Events& emitted);
	void stop(int64_t tick);
	void reset();
	bool ready() const { return completed_.length > 0; }
	bool overflowed() const { return completedOverflow_; }
	const CapturedBar& completed() const { return completed_; }

private:
	struct HeldNote {
		int64_t start = 0;
		uint8_t velocity = 0;
		bool on = false;
	};
	void advance(int64_t tick, int32_t barLength, const Destinations& destinations);
	void finish(uint8_t part, int64_t end);
	CapturedBar current_{};
	CapturedBar completed_{};
	std::array<HeldNote, kParts> held_{};
	int64_t barStart_ = 0;
	int64_t lastTick_ = 0;
	bool collecting_ = false;
	bool valid_ = false;
	bool currentOverflow_ = false;
	bool completedOverflow_ = false;
};
} // namespace deluge::model::generator::grids
