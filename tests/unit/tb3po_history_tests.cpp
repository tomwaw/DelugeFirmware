#include "CppUTest/TestHarness.h"
#include "model/generator/tb3po_history.h"

using namespace deluge::model::generator::tb3po;

namespace {
Events on(int note, int velocity = 80) {
	Events e;
	e.notes[e.count++] = {true, static_cast<uint8_t>(note), static_cast<uint8_t>(velocity)};
	return e;
}
Events off(int note) {
	Events e;
	e.notes[e.count++] = {false, static_cast<uint8_t>(note), 64};
	return e;
}
TEST_GROUP(TB3POHistory) {
	History history;
};
} // namespace

TEST(TB3POHistory, WaitsForCompleteBarAndExcludesCurrentBar) {
	history.record(0, 96, on(36, 120));
	history.record(3, 96, off(36));
	history.record(95, 96, {});
	CHECK_FALSE(history.ready());
	history.record(96, 96, on(48));
	CHECK(history.ready());
	const auto& bar = history.completed();
	CHECK_EQUAL(96, bar.length);
	CHECK_EQUAL(1, bar.count);
	CHECK_EQUAL(36, bar.notes[0].note);
	CHECK_EQUAL(120, bar.notes[0].velocity);
	CHECK_EQUAL(0, bar.notes[0].pos);
	CHECK_EQUAL(3, bar.notes[0].length);
}

TEST(TB3POHistory, MidBarEnableRequiresAFullTransportAlignedBar) {
	history.record(12, 96, on(36));
	history.record(15, 96, off(36));
	history.record(96, 96, on(43));
	CHECK_FALSE(history.ready());
	history.record(99, 96, off(43));
	history.record(192, 96, {});
	CHECK(history.ready());
	CHECK_EQUAL(1, history.completed().count);
	CHECK_EQUAL(43, history.completed().notes[0].note);
	CHECK_EQUAL(0, history.completed().notes[0].pos);
}

TEST(TB3POHistory, HeldNoteIsSplitAtBarBoundariesWithOriginalVelocity) {
	history.record(0, 96, {});
	history.record(90, 96, on(36, 120));
	history.record(96, 96, {});
	CHECK_EQUAL(90, history.completed().notes[0].pos);
	CHECK_EQUAL(6, history.completed().notes[0].length);
	history.record(105, 96, off(36));
	history.record(192, 96, {});
	CHECK_EQUAL(0, history.completed().notes[0].pos);
	CHECK_EQUAL(9, history.completed().notes[0].length);
	CHECK_EQUAL(120, history.completed().notes[0].velocity);
}

TEST(TB3POHistory, CrossPitchSlidesBecomeOneTickOverlap) {
	history.record(0, 96, on(36));
	Events slide = on(43, 120);
	slide.notes[slide.count++] = {false, 36, 64};
	history.record(6, 96, slide);
	history.record(9, 96, off(43));
	history.record(96, 96, {});
	const auto& bar = history.completed();
	CHECK_EQUAL(2, bar.count);
	CHECK(bar.notes[0].slide);
	CHECK_EQUAL(6, bar.notes[0].length);
	CHECK_EQUAL(7, frozenNoteLength(bar, 0));
	CHECK_EQUAL(6, bar.notes[1].pos);
	CHECK_EQUAL(3, frozenNoteLength(bar, 1));
}

TEST(TB3POHistory, OrdinaryOffBeforeOnDoesNotCreateSlide) {
	history.record(0, 96, on(36));
	Events retrigger = off(36);
	retrigger.notes[retrigger.count++] = {true, 36, 120};
	history.record(6, 96, retrigger);
	history.record(9, 96, off(36));
	history.record(96, 96, {});
	CHECK_EQUAL(2, history.completed().count);
	CHECK_FALSE(history.completed().notes[0].slide);
	CHECK_EQUAL(6, frozenNoteLength(history.completed(), 0));
}

TEST(TB3POHistory, RuntimeSamePitchTiesProduceOneLongNote) {
	Pattern p;
	p.length = 1;
	p.steps[0] = {36, true, true, true};
	Runtime runtime;
	runtime.queue(p);
	for (int tick = 0; tick <= 192; tick += 6) {
		history.record(tick, 96, runtime.process(tick, 6));
	}
	CHECK_EQUAL(1, history.completed().count);
	CHECK_EQUAL(0, history.completed().notes[0].pos);
	CHECK_EQUAL(96, history.completed().notes[0].length);
	CHECK_EQUAL(120, history.completed().notes[0].velocity);
}

TEST(TB3POHistory, CapturesActualMixedRecipesWithFiveStepCycle) {
	Pattern p;
	p.length = 5;
	for (int i = 0; i < 5; ++i) {
		p.steps[i] = {36, true, false, false};
	}
	Runtime runtime;
	runtime.queue(p);
	for (int tick = 0; tick <= 96; ++tick) {
		if (tick == 20) {
			for (auto& step : p.steps) {
				step.note = 48;
			}
			runtime.queue(p); // Applied at tick 30, not the edit time or the transport bar boundary.
		}
		history.record(tick, 96, runtime.process(tick, 6));
	}
	const auto& bar = history.completed();
	CHECK_EQUAL(16, bar.count);
	for (int i = 0; i < 16; ++i) {
		CHECK_EQUAL(i < 5 ? 36 : 48, bar.notes[i].note);
		CHECK_EQUAL(i * 6, bar.notes[i].pos);
		CHECK_EQUAL(3, bar.notes[i].length);
	}
}

TEST(TB3POHistory, StopRetainsCompletedBarAndDropsIncompleteBar) {
	history.record(0, 96, on(36));
	history.record(3, 96, off(36));
	history.record(96, 96, on(48));
	history.stop(100);
	CHECK(history.ready());
	CHECK_EQUAL(36, history.completed().notes[0].note);
	history.record(150, 96, on(60));
	history.record(153, 96, off(60));
	history.record(192, 96, {});
	CHECK_FALSE(history.ready()); // The muted gap is not a captured bar.
}

TEST(TB3POHistory, StopAtBoundaryMakesFinishedBarAvailable) {
	history.record(0, 96, on(36));
	history.stop(96);
	CHECK(history.ready());
	CHECK_EQUAL(96, history.completed().notes[0].length);
}

TEST(TB3POHistory, SilentBarIsValid) {
	history.record(0, 96, {});
	history.record(96, 96, {});
	CHECK(history.ready());
	CHECK_EQUAL(0, history.completed().count);
}

TEST(TB3POHistory, SeekClockChangeAndResetInvalidateOldCapture) {
	history.record(0, 96, {});
	history.record(96, 96, {});
	history.record(0, 96, {});
	CHECK_FALSE(history.ready());
	history.record(96, 96, {});
	history.record(100, 192, {});
	CHECK_FALSE(history.ready());
	history.record(192, 192, {});
	history.record(384, 192, {});
	CHECK(history.ready());
	history.reset();
	CHECK_FALSE(history.ready());
}

TEST(TB3POHistory, OverflowRefusesPartialResultAndRecoversNextBar) {
	for (int i = 0; i < 33; ++i) {
		history.record(i * 2, 96, on(36));
		history.record(i * 2 + 1, 96, off(36));
	}
	history.record(96, 96, {});
	CHECK_FALSE(history.ready());
	history.record(192, 96, {});
	CHECK(history.ready());
}

TEST(TB3POHistory, OverlapCannotExceedBarOrNextSamePitchOnset) {
	CapturedBar bar;
	bar.length = 96;
	bar.count = 3;
	bar.notes[0] = {0, 6, 36, 80, true};
	bar.notes[1] = {6, 3, 36, 80, false};
	bar.notes[2] = {90, 6, 48, 80, true};
	CHECK_EQUAL(6, frozenNoteLength(bar, 0));
	CHECK_EQUAL(6, frozenNoteLength(bar, 2));
}
