#include "CppUTest/TestHarness.h"
#include "model/generator/tb3po_runtime.h"

using namespace deluge::model::generator::tb3po;

namespace {
Pattern pair(bool slide = false) {
	Pattern p;
	p.length = 2;
	p.steps[0] = {36, true, false, slide};
	p.steps[1] = {43, true, true, false};
	return p;
}
void event(const Events& events, int index, bool on, int note, int velocity = -1) {
	CHECK(index < events.count);
	CHECK_EQUAL(on, events.notes[index].on);
	CHECK_EQUAL(note, events.notes[index].note);
	if (velocity >= 0) {
		CHECK_EQUAL(velocity, events.notes[index].velocity);
	}
}
TEST_GROUP(TB3PORuntime) {
	Runtime runtime;
	void setup() override {
		runtime.queue(pair());
	}
};

TEST(TB3PORuntime, HalfStepGateSchedulesRealNoteOff) {
	auto e = runtime.process(0, 6);
	CHECK_EQUAL(1, e.count);
	event(e, 0, true, 36, 80);
	CHECK_EQUAL(3, e.ticksToNext);
	CHECK_EQUAL(0, runtime.process(2, 6).count);
	e = runtime.process(3, 6);
	CHECK_EQUAL(1, e.count);
	event(e, 0, false, 36);
	CHECK_EQUAL(3, e.ticksToNext);
	e = runtime.process(6, 6);
	event(e, 0, true, 43, 120);
}

TEST(TB3PORuntime, SlidesStartNewPitchBeforeReleasingOldPitch) {
	runtime.queue(pair(true));
	(void)runtime.process(0, 6);
	CHECK_EQUAL(0, runtime.process(3, 6).count);
	auto e = runtime.process(6, 6);
	CHECK_EQUAL(2, e.count);
	event(e, 0, true, 43, 120);
	event(e, 1, false, 36);
	CHECK_EQUAL(3, e.ticksToNext);
	e = runtime.process(9, 6);
	event(e, 0, false, 43);
}

TEST(TB3PORuntime, SamePitchSlideIsATieNotARetrigger) {
	auto p = pair(true);
	p.steps[1].note = 36;
	runtime.queue(p);
	(void)runtime.process(0, 6);
	CHECK_EQUAL(0, runtime.process(6, 6).count);
	const auto e = runtime.process(9, 6);
	CHECK_EQUAL(1, e.count);
	event(e, 0, false, 36);
}

TEST(TB3PORuntime, SamePitchWithoutSlideRetriggersAndWorksAtOneTickResolution) {
	auto p = pair();
	p.steps[1].note = 36;
	runtime.queue(p);
	(void)runtime.process(0, 1);
	const auto e = runtime.process(1, 1);
	CHECK_EQUAL(2, e.count);
	event(e, 0, false, 36);
	event(e, 1, true, 36, 120);
}

TEST(TB3PORuntime, RestTerminatesEvenMalformedSlideIntent) {
	auto p = pair(true);
	p.steps[1].gate = false;
	runtime.queue(p);
	(void)runtime.process(0, 6);
	const auto e = runtime.process(6, 6);
	CHECK_EQUAL(1, e.count);
	event(e, 0, false, 36);
	CHECK_EQUAL(-1, runtime.playingNote());
}

TEST(TB3PORuntime, PendingPatternWaitsForCycleAndLatestEditWins) {
	runtime.queue(pair(true));
	(void)runtime.process(0, 6);
	auto next = pair();
	next.steps[0].note = 40;
	runtime.queue(next);
	next.length = 1;
	next.steps[0].note = 48;
	runtime.queue(next);
	CHECK(runtime.pending());
	event(runtime.process(6, 6), 0, true, 43);
	CHECK(runtime.pending());
	(void)runtime.process(9, 6);
	event(runtime.process(12, 6), 0, true, 48);
	CHECK_FALSE(runtime.pending());
	(void)runtime.process(15, 6);
	event(runtime.process(18, 6), 0, true, 48);
}

TEST(TB3PORuntime, SwapBreaksOutgoingSlideEvenForSamePitch) {
	auto p = pair(true);
	p.length = 1;
	runtime.queue(p);
	(void)runtime.process(0, 6);
	p.steps[0].accent = true;
	runtime.queue(p);
	const auto e = runtime.process(6, 6);
	CHECK_EQUAL(2, e.count);
	event(e, 0, false, 36);
	event(e, 1, true, 36, 120);
}

TEST(TB3PORuntime, RevertingAnEditCancelsPendingRecipe) {
	(void)runtime.process(0, 6);
	auto p = pair();
	p.steps[0].note = 40;
	runtime.queue(p);
	CHECK(runtime.pending());
	runtime.queue(pair());
	CHECK_FALSE(runtime.pending());
}

TEST(TB3PORuntime, StopReleasesOwnedNoteImmediatelyAndAppliesPendingForRestart) {
	runtime.queue(pair(true));
	(void)runtime.process(0, 6);
	auto next = pair();
	next.steps[0].note = 40;
	runtime.queue(next);
	const auto e = runtime.stop();
	CHECK_EQUAL(1, e.count);
	event(e, 0, false, 36);
	CHECK_FALSE(runtime.pending());
	CHECK_EQUAL(0, runtime.stop().count);
	event(runtime.process(0, 6), 0, true, 40);
}

TEST(TB3PORuntime, EnablingMidStepWaitsForNextBoundary) {
	const auto e = runtime.process(4, 6);
	CHECK_EQUAL(0, e.count);
	CHECK_EQUAL(2, e.ticksToNext);
	event(runtime.process(6, 6), 0, true, 43);
}

TEST(TB3PORuntime, SeekAndMissedDeadlineReleaseWithoutBurstOfOldNotes) {
	runtime.queue(pair(true));
	(void)runtime.process(0, 6);
	auto e = runtime.process(100, 6);
	CHECK_EQUAL(1, e.count);
	event(e, 0, false, 36);
	CHECK_EQUAL(2, e.ticksToNext);
	event(runtime.process(102, 6), 0, true, 43);
	e = runtime.process(0, 6);
	CHECK_EQUAL(2, e.count);
	event(e, 0, false, 43);
	event(e, 1, true, 36);
}

TEST(TB3PORuntime, ResolutionChangesReleaseAndRealign) {
	runtime.queue(pair(true));
	(void)runtime.process(0, 6);
	auto e = runtime.process(6, 12);
	CHECK_EQUAL(1, e.count);
	event(e, 0, false, 36);
	CHECK_EQUAL(6, e.ticksToNext);
	event(runtime.process(12, 12), 0, true, 43);
}

TEST(TB3PORuntime, SameTickCannotDuplicateNoteOn) {
	(void)runtime.process(0, 6);
	CHECK_EQUAL(0, runtime.process(0, 6).count);
	(void)runtime.process(3, 6);
	CHECK_EQUAL(0, runtime.process(3, 6).count);
	(void)runtime.process(6, 6);
	CHECK_EQUAL(0, runtime.process(6, 6).count);
}

TEST(TB3PORuntime, OneStepSlideLoopsUntilExplicitStop) {
	auto p = pair(true);
	p.length = 1;
	runtime.queue(p);
	(void)runtime.process(0, 6);
	for (int tick = 6; tick <= 96; tick += 6) {
		CHECK_EQUAL(0, runtime.process(tick, 6).count);
		CHECK_EQUAL(36, runtime.playingNote());
	}
	event(runtime.stop(), 0, false, 36);
}

TEST(TB3PORuntime, GeneratedPatternsNeverLeakNotesAcrossEditsAndStops) {
	for (uint16_t seed = 0; seed < 32; ++seed) {
		Runtime player;
		player.queue(generate({seed, static_cast<int8_t>(seed % 15 - 7), static_cast<uint8_t>(seed + 1), true}));
		bool notes[128]{};
		auto send = [&](const Events& events) {
			CHECK(events.count <= 2);
			for (int i = 0; i < events.count; ++i) {
				const auto& e = events.notes[i];
				CHECK(notes[e.note] != e.on); // No duplicate note-on or unmatched note-off.
				notes[e.note] = e.on;
			}
		};
		for (int tick = 0; tick < 1200;) {
			const auto e = player.process(tick, 6);
			send(e);
			if (tick == 96) {
				player.queue(generate({static_cast<uint16_t>(seed + 10), 7, 1, true}));
			}
			CHECK(e.ticksToNext > 0);
			tick += e.ticksToNext;
		}
		send(player.stop());
		for (bool on : notes) {
			CHECK_FALSE(on);
		}
	}
}

TEST(TB3PORuntime, InvalidRecipesCannotReplaceTheActivePattern) {
	runtime.queue({});
	auto invalid = pair();
	invalid.length = 255;
	runtime.queue(invalid);
	event(runtime.process(0, 6), 0, true, 36);
}
} // namespace
