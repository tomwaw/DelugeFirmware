#include "model/generator/grids_history.h"
#include <algorithm>
namespace deluge::model::generator::grids {
void History::reset() {
	collecting_ = false;
	valid_ = false;
	currentOverflow_ = completedOverflow_ = false;
	current_.count = completed_.count = 0;
	current_.length = completed_.length = 0;
	held_ = {};
}
void History::finish(uint8_t part, int64_t end) {
	const auto& held = held_[part];
	if (!held.on || end <= held.start)
		return;
	if (current_.count == current_.notes.size()) {
		valid_ = false;
		currentOverflow_ = true;
		return;
	}
	current_.notes[current_.count++] = {static_cast<int32_t>(held.start - barStart_),
	                                    static_cast<int32_t>(end - held.start), part, held.velocity};
}
void History::advance(int64_t tick, int32_t barLength, const Destinations& destinations) {
	if (tick < 0 || barLength <= 0) {
		reset();
		return;
	}
	if (collecting_
	    && (tick < lastTick_ || current_.length != barLength || current_.destinations != destinations
	        || tick >= barStart_ + 2 * static_cast<int64_t>(barLength)))
		reset();
	if (!collecting_) {
		// A changed destination invalidates a completed capture even after stopping.
		if (completed_.destinations != destinations) {
			completed_.length = 0;
			completedOverflow_ = false;
		}
		barStart_ = tick - tick % barLength;
		current_.count = 0;
		current_.length = barLength;
		current_.destinations = destinations;
		currentOverflow_ = false;
		valid_ = tick == barStart_;
		collecting_ = true;
	}
	else if (tick >= barStart_ + barLength) {
		const auto boundary = barStart_ + barLength;
		for (uint8_t part = 0; part < kParts; ++part) {
			finish(part, boundary);
			held_[part].start = boundary;
		}
		completedOverflow_ = currentOverflow_;
		if (valid_) {
			std::sort(current_.notes.begin(), current_.notes.begin() + current_.count,
			          [](const CapturedNote& a, const CapturedNote& b) {
				          return a.pos < b.pos || (a.pos == b.pos && a.part < b.part);
			          });
			completed_ = current_;
		}
		else {
			completed_.length = 0;
			completed_.count = 0;
		}
		barStart_ = boundary;
		current_.count = 0;
		valid_ = true;
		currentOverflow_ = false;
	}
	lastTick_ = tick;
}
void History::record(int64_t tick, int32_t barLength, const Destinations& destinations, const Events& emitted) {
	advance(tick, barLength, destinations);
	if (!collecting_)
		return;
	for (uint8_t i = 0; i < emitted.count; ++i) {
		const auto& event = emitted.notes[i];
		if (event.part >= kParts) {
			valid_ = false;
			continue;
		}
		auto& held = held_[event.part];
		if (event.on) {
			if (held.on || !destinations[event.part]) {
				valid_ = false;
				continue;
			}
			held = {tick, event.velocity, true};
		}
		else {
			finish(event.part, tick);
			held.on = false;
		}
	}
}
void History::stop(int64_t tick) {
	if (!collecting_)
		return;
	const auto destinations = current_.destinations;
	advance(tick, current_.length, destinations);
	collecting_ = false;
	held_ = {};
}
} // namespace deluge::model::generator::grids
