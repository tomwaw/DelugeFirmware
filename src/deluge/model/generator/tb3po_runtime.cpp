#include "model/generator/tb3po_runtime.h"
#include <algorithm>
#include <limits>

namespace deluge::model::generator::tb3po {

void Runtime::queue(const Pattern& pattern) {
	if (pattern.length == 0 || pattern.length > kMaxSteps) {
		return;
	}
	if (!started_) {
		active_ = pattern;
		pending_ = false;
	}
	else {
		replacement_ = pattern;
		pending_ = replacement_ != active_;
	}
}

void Runtime::release(Events& events) {
	if (note_ >= 0) {
		events.notes[events.count++] = {false, static_cast<uint8_t>(note_), 64};
		note_ = -1;
	}
}

void Runtime::applyPending() {
	if (pending_) {
		active_ = replacement_;
		pending_ = false;
	}
}

Events Runtime::stop() {
	Events events;
	release(events);
	started_ = false;
	applyPending();
	return events;
}

void Runtime::playStep(Events& events, int64_t tick) {
	const Step& step = active_.steps[step_];
	if (!step.gate) {
		release(events);
		return;
	}
	if (note_ != step.note) {
		// New note before old note-off gives the synth's LEGATO mode an overlapping input pair.
		events.notes[events.count++] = {true, step.note, static_cast<uint8_t>(step.accent ? 120 : 80)};
		release(events);
		note_ = step.note;
	}
	// Same-pitch slides are ties; they never send an off which would kill the continuing voice.
	offTick_ = step.slide ? std::numeric_limits<int64_t>::max() : tick + std::max<int32_t>(1, ticksPerStep_ / 2);
}

Events Runtime::process(int64_t tick, int32_t ticksPerStep) {
	if (tick < 0 || active_.length == 0 || active_.length > kMaxSteps) {
		return stop();
	}
	Events events;
	ticksPerStep = std::max<int32_t>(1, ticksPerStep);
	bool boundary = false;
	// Seeks, resolution changes and missed deadlines release the previous note without replaying a backlog.
	if (!started_ || ticksPerStep != ticksPerStep_ || tick < lastTick_ || tick > nextStepTick_) {
		release(events);
		applyPending();
		ticksPerStep_ = ticksPerStep;
		step_ = (tick / ticksPerStep_) % active_.length;
		nextStepTick_ = tick + ticksPerStep_ - tick % ticksPerStep_;
		started_ = true;
		boundary = tick % ticksPerStep_ == 0;
	}
	else if (tick == nextStepTick_) {
		step_ = (step_ + 1) % active_.length;
		if (step_ == 0 && pending_) {
			release(events);
			applyPending();
		}
		nextStepTick_ += ticksPerStep_;
		boundary = true;
	}
	lastTick_ = tick;
	if (note_ >= 0 && tick >= offTick_) {
		release(events);
	}
	if (boundary) {
		playStep(events, tick);
	}
	const int64_t next = note_ >= 0 ? std::min(nextStepTick_, offTick_) : nextStepTick_;
	events.ticksToNext = std::clamp<int64_t>(next - tick, 1, std::numeric_limits<int32_t>::max());
	return events;
}

} // namespace deluge::model::generator::tb3po
