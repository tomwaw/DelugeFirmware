#include "CppUTest/TestHarness.h"
#include "model/generator/grids_history.h"
using namespace deluge::model::generator::grids;
TEST_GROUP(GridsHistory) {
	History history;
	Destinations routes{100, 200, 300};
	void record(int64_t tick, std::initializer_list<Event> notes = {}, int32_t length = 96) {
		Events events;
		for (auto note : notes)
			events.notes[events.count++] = note;
		history.record(tick, length, routes, events);
	}
};
TEST(GridsHistory, RequiresCompleteBar) {
	record(0, {{true, 0, 120}});
	record(1, {{false, 0, 64}});
	record(95);
	CHECK_FALSE(history.ready());
	record(96);
	CHECK(history.ready());
	const auto& bar = history.completed();
	CHECK_EQUAL(96, bar.length);
	CHECK_EQUAL(1, bar.count);
	CHECK_EQUAL(0, bar.notes[0].pos);
	CHECK_EQUAL(1, bar.notes[0].length);
	CHECK_EQUAL(120, bar.notes[0].velocity);
	CHECK_EQUAL(100, bar.destinations[0]);
}
TEST(GridsHistory, MidBarEnableWaitsForFollowingFullBar) {
	record(12, {{true, 0, 80}});
	record(13, {{false, 0, 64}});
	record(96);
	CHECK_FALSE(history.ready());
	record(99, {{true, 2, 120}});
	record(100, {{false, 2, 64}});
	record(192);
	CHECK(history.ready());
	CHECK_EQUAL(1, history.completed().count);
	CHECK_EQUAL(3, history.completed().notes[0].pos);
}
TEST(GridsHistory, CapturesAllNinetySixOnsets) {
	for (int step = 0; step < 32; ++step) {
		record(step * 3, {{true, 0, 80}, {true, 1, 120}, {true, 2, 80}});
		record(step * 3 + 1, {{false, 0, 64}, {false, 1, 64}, {false, 2, 64}});
	}
	record(96);
	CHECK(history.ready());
	CHECK_EQUAL(96, history.completed().count);
	for (int i = 0; i < 96; ++i) {
		const auto& note = history.completed().notes[i];
		CHECK_EQUAL(i / 3 * 3, note.pos);
		CHECK_EQUAL(i % 3, note.part);
		CHECK_EQUAL(1, note.length);
	}
}
TEST(GridsHistory, BoundIncludesThreeCarriedGates) {
	record(0);
	record(95, {{true, 0, 80}, {true, 1, 80}, {true, 2, 80}});
	record(96);
	record(97, {{false, 0, 64}, {false, 1, 64}, {false, 2, 64}});
	for (int step = 0; step < 32; ++step) {
		record(98 + step * 2, {{true, 0, 120}, {true, 1, 120}, {true, 2, 120}});
		record(99 + step * 2, {{false, 0, 64}, {false, 1, 64}, {false, 2, 64}});
	}
	record(192);
	CHECK(history.ready());
	CHECK_EQUAL(99, history.completed().count);
	CHECK_EQUAL(0, history.completed().notes[0].pos);
	CHECK_EQUAL(1, history.completed().notes[0].length);
}
TEST(GridsHistory, ClipsHeldGateAtBothBarEdges) {
	record(0);
	record(94, {{true, 2, 120}});
	record(96);
	CHECK_EQUAL(94, history.completed().notes[0].pos);
	CHECK_EQUAL(2, history.completed().notes[0].length);
	record(100, {{false, 2, 64}});
	record(192);
	CHECK_EQUAL(0, history.completed().notes[0].pos);
	CHECK_EQUAL(4, history.completed().notes[0].length);
}
TEST(GridsHistory, SilentBarIsValid) {
	record(0);
	record(96);
	CHECK(history.ready());
	CHECK_EQUAL(0, history.completed().count);
}
TEST(GridsHistory, OnlyEmittedPartsAreCaptured) {
	// Caller omits hits suppressed by row mute, missing destinations or unsupported output.
	record(0, {{true, 1, 80}});
	record(1, {{false, 1, 64}});
	record(96);
	CHECK_EQUAL(1, history.completed().count);
	CHECK_EQUAL(1, history.completed().notes[0].part);
}
TEST(GridsHistory, PreservesLiveVelocityAndDensityChanges) {
	record(0, {{true, 0, 80}, {true, 1, 120}});
	record(1, {{false, 0, 64}, {false, 1, 64}});
	record(12, {{true, 0, 120}});
	record(13, {{false, 0, 64}});
	record(96);
	CHECK_EQUAL(3, history.completed().count);
	CHECK_EQUAL(80, history.completed().notes[0].velocity);
	CHECK_EQUAL(120, history.completed().notes[2].velocity);
	CHECK_EQUAL(12, history.completed().notes[2].pos);
}
TEST(GridsHistory, DestinationChangeInvalidatesOldBar) {
	record(0);
	record(96);
	CHECK(history.ready());
	routes[0] = 400;
	record(100, {{true, 0, 120}});
	record(101, {{false, 0, 64}});
	CHECK_FALSE(history.ready());
	record(192);
	CHECK_FALSE(history.ready());
	record(288);
	CHECK(history.ready());
	CHECK_EQUAL(400, history.completed().destinations[0]);
}
TEST(GridsHistory, StopKeepsCompletedCapture) {
	record(0, {{true, 0, 80}});
	record(1, {{false, 0, 64}});
	record(96);
	history.stop(100);
	CHECK(history.ready());
	CHECK_EQUAL(1, history.completed().count);
	history.stop(150);
	CHECK_EQUAL(1, history.completed().count);
}
TEST(GridsHistory, StopExactlyAtBoundaryCompletesBar) {
	record(0);
	record(93, {{true, 1, 80}});
	history.stop(96);
	CHECK(history.ready());
	CHECK_EQUAL(3, history.completed().notes[0].length);
}
TEST(GridsHistory, ResetClearsCaptureAndHeldNotes) {
	record(0);
	record(96, {{true, 1, 120}});
	history.reset();
	CHECK_FALSE(history.ready());
	record(192);
	record(288);
	CHECK(history.ready());
	CHECK_EQUAL(0, history.completed().count);
}
TEST(GridsHistory, OverflowRejectsWholeBarAndCanRecover) {
	record(0);
	for (int step = 0; step < 34; ++step) {
		record(step * 2, {{true, 0, 80}, {true, 1, 80}, {true, 2, 80}});
		record(step * 2 + 1, {{false, 0, 64}, {false, 1, 64}, {false, 2, 64}});
	}
	record(96);
	CHECK_FALSE(history.ready());
	CHECK(history.overflowed());
	record(192);
	CHECK(history.ready());
	CHECK_FALSE(history.overflowed());
	CHECK_EQUAL(0, history.completed().count);
}
TEST(GridsHistory, SeekOrResolutionChangeDoesNotInventBars) {
	record(0);
	record(96);
	record(50);
	CHECK_FALSE(history.ready());
	record(96);
	record(192);
	CHECK(history.ready());
	record(193, {}, 192);
	CHECK_FALSE(history.ready());
	record(1000, {}, 192);
	CHECK_FALSE(history.ready());
}
TEST(GridsHistory, SamePartRetriggersRemainSeparate) {
	record(0, {{true, 0, 80}});
	record(3, {{false, 0, 64}, {true, 0, 120}});
	record(4, {{false, 0, 64}});
	record(96);
	CHECK_EQUAL(2, history.completed().count);
	CHECK_EQUAL(3, history.completed().notes[0].length);
	CHECK_EQUAL(3, history.completed().notes[1].pos);
	CHECK_EQUAL(1, history.completed().notes[1].length);
}
