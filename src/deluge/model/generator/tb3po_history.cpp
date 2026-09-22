#include "model/generator/tb3po_history.h"
#include <algorithm>

namespace deluge::model::generator::tb3po {

void History::reset() {
	*this = History{};
}

void History::finish(const HeldNote& held, int64_t end, bool slide) {
	if (held.note < 0 || end <= held.start) {
		return;
	}
	if (current_.count == current_.notes.size()) {
		valid_ = false;
		return;
	}
	current_.notes[current_.count++] = {static_cast<int32_t>(held.start - barStart_),
	                                    static_cast<int32_t>(end - held.start), static_cast<uint8_t>(held.note),
	                                    held.velocity, slide};
}

void History::advance(int64_t tick, int32_t barLength) {
	if (barLength <= 0 || tick < 0) {
		reset();
		return;
	}
	if (collecting_
	    && (tick < lastTick_ || barLength != current_.length
	        || tick >= barStart_ + 2 * static_cast<int64_t>(barLength))) {
		reset(); // Discontinuous clock: don't invent unobserved bars.
	}
	if (!collecting_) {
		barStart_ = tick - tick % barLength;
		current_ = {};
		current_.length = barLength;
		valid_ = tick == barStart_; // Mid-bar enable must wait for the next full bar.
		collecting_ = true;
	}
	else if (tick >= barStart_ + barLength) {
		const int64_t boundary = barStart_ + barLength;
		for (auto& held : held_) {
			finish(held, boundary, false);
			held.start = boundary;
		}
		if (valid_) {
			std::sort(current_.notes.begin(), current_.notes.begin() + current_.count,
			          [](const CapturedNote& a, const CapturedNote& b) { return a.pos < b.pos; });
			completed_ = current_;
		}
		else {
			completed_ = {};
		}
		barStart_ = boundary;
		current_ = {};
		current_.length = barLength;
		valid_ = true;
	}
	lastTick_ = tick;
}

void History::record(int64_t tick, int32_t barLength, const Events& events) {
	advance(tick, barLength);
	if (!collecting_) {
		return;
	}
	for (uint8_t i = 0; i < events.count; ++i) {
		const auto& event = events.notes[i];
		if (event.on) {
			auto slot = std::find_if(held_.begin(), held_.end(), [](const HeldNote& held) { return held.note < 0; });
			if (slot == held_.end()) {
				valid_ = false;
			}
			else {
				*slot = {tick, event.note, event.velocity};
			}
		}
		else {
			for (auto& held : held_) {
				if (held.note == event.note) {
					bool slide = i > 0 && events.notes[i - 1].on && events.notes[i - 1].note != event.note;
					finish(held, tick, slide);
					held.note = -1;
					break;
				}
			}
		}
	}
}

void History::stop(int64_t tick) {
	if (!collecting_) {
		return;
	}
	advance(tick, current_.length);
	collecting_ = false;
	held_ = {};
}

int32_t frozenNoteLength(const CapturedBar& bar, std::size_t index) {
	const auto& note = bar.notes[index];
	int32_t end = std::min(bar.length, note.pos + note.length + (note.slide ? 1 : 0));
	for (uint8_t i = 0; i < bar.count; ++i) {
		if (bar.notes[i].note == note.note && bar.notes[i].pos > note.pos) {
			end = std::min(end, bar.notes[i].pos);
		}
	}
	return end - note.pos;
}

} // namespace deluge::model::generator::tb3po
