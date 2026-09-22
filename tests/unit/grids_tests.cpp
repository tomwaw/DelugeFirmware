#include "CppUTest/TestHarness.h"
#include "model/generator/grids.h"
#include "model/generator/grids_runtime.h"
using namespace deluge::model::generator::grids;
TEST_GROUP(Grids){};
// Golden values computed independently from the pinned upstream map and AVR U8Mix arithmetic.
TEST(Grids, UpstreamReferencePatterns) {
	struct Reference {
		uint8_t x, y;
		uint32_t hits[3];
		uint32_t accents[3];
	};
	const Reference reference[] = {
	    {0, 0, {0x10041001u, 0x11510u, 0x11010111u}, {0x41000u, 0x11000u, 0x10001u}},
	    {64, 64, {0x10111001u, 0x11101100u, 0x1111111u}, {0x11001u, 0x1000100u, 0x10111u}},
	    {128, 128, {0x111001u, 0x15100100u, 0x11101110u}, {0x10001u, 0x5000100u, 0x101010u}},
	    {255, 255, {0x1014101u, 0x1044101u, 0x10441010u}, {0x1004101u, 0x4101u, 0x10001010u}},
	    {17, 193, {0x14101041u, 0x41104100u, 0x11111151u}, {0x101001u, 0x1004100u, 0x10111010u}},
	    {127, 63, {0x110001u, 0x51001100u, 0x110011u}, {0x10001u, 0x50000100u, 0x10001u}},
	    {200, 150, {0x1010111u, 0x5001100u, 0x10151010u}, {0x10001u, 0x100u, 0x10101010u}},
	};
	for (const auto& r : reference) {
		Core core;
		core.reset(1);
		core.beginCycle();
		Settings settings;
		settings.x = r.x;
		settings.y = r.y;
		for (uint8_t s = 0; s < 32; ++s) {
			auto hits = core.evaluate(s, settings);
			for (uint8_t p = 0; p < 3; ++p) {
				CHECK_EQUAL((r.hits[p] >> s) & 1, hits[p].on);
				CHECK_EQUAL((r.accents[p] >> s) & 1, hits[p].accent);
			}
		}
	}
}
TEST(Grids, ZeroDensityAlwaysSilent) {
	Core core;
	core.reset(5);
	core.beginCycle();
	Settings s;
	s.chaos = 255;
	s.density.fill(0);
	for (int step = 0; step < 32; ++step)
		for (auto hit : core.evaluate(step, s))
			CHECK_FALSE(hit.on);
}
TEST(Grids, IncreasingDensityNeverRemovesHits) {
	Core core;
	core.reset(65535);
	core.beginCycle();
	Settings s;
	s.chaos = 255;
	for (int step = 0; step < 32; ++step) {
		Hits previous{};
		for (int density = 0; density < 256; ++density) {
			s.density.fill(density);
			const auto hits = core.evaluate(step, s);
			for (int p = 0; p < 3; ++p)
				CHECK(!previous[p].on || hits[p].on);
			previous = hits;
		}
	}
}
TEST(Grids, DeterministicIndependentInstances) {
	Core a, b, other;
	a.reset(42);
	b.reset(42);
	other.reset(321);
	Settings s;
	s.chaos = 255;
	for (int cycle = 0; cycle < 10; ++cycle) {
		a.beginCycle();
		other.beginCycle();
		b.beginCycle();
		for (int step = 0; step < 32; ++step) {
			auto x = a.evaluate(step, s);
			other.evaluate(step, s);
			auto y = b.evaluate(step, s);
			for (int p = 0; p < 3; ++p) {
				CHECK_EQUAL(x[p].on, y[p].on);
				CHECK_EQUAL(x[p].accent, y[p].accent);
			}
		}
	}
}
TEST(Grids, MapWrapsAtThirtyTwo) {
	for (int p = 0; p < 3; ++p)
		CHECK_EQUAL(level(0, p, 255, 255), level(32, p, 255, 255));
	CHECK_EQUAL(0, level(0, 3, 0, 0));
}
TEST_GROUP(GridsRuntime){};
static Settings dense() {
	Settings s;
	s.density.fill(255);
	s.chaos = 255;
	return s;
}
TEST(GridsRuntime, ThreeOnsetsAndScheduledOffs) {
	Runtime r;
	r.queue(dense());
	auto e = r.process(0, 3);
	CHECK_EQUAL(3, e.count);
	CHECK_EQUAL(1, e.ticksToNext);
	for (int p = 0; p < 3; ++p) {
		CHECK(e.notes[p].on);
		CHECK_EQUAL(p, e.notes[p].part);
	}
	e = r.process(1, 3);
	CHECK_EQUAL(3, e.count);
	CHECK_EQUAL(2, e.ticksToNext);
	for (int p = 0; p < 3; ++p)
		CHECK_FALSE(e.notes[p].on);
	CHECK_EQUAL(0, r.stop().count);
}
TEST(GridsRuntime, WorstCaseOffsBeforeRetriggers) {
	Runtime r;
	r.queue(dense());
	r.process(0, 1);
	auto e = r.process(1, 1);
	CHECK(e.count <= 6);
	bool seenOn = false;
	for (int i = 0; i < e.count; ++i) {
		if (e.notes[i].on)
			seenOn = true;
		else
			CHECK_FALSE(seenOn);
	}
}
TEST(GridsRuntime, StopReleasesEveryVoiceAndIsIdempotent) {
	Runtime r;
	r.queue(dense());
	r.process(0, 3);
	auto e = r.stop();
	CHECK_EQUAL(3, e.count);
	for (int i = 0; i < 3; ++i)
		CHECK_FALSE(e.notes[i].on);
	CHECK_EQUAL(0, r.stop().count);
}
TEST(GridsRuntime, LiveDensityAppliesNextStepWithoutCancellingOffs) {
	Runtime r;
	auto s = dense();
	r.queue(s);
	r.process(0, 3);
	s.density.fill(0);
	r.queue(s);
	CHECK_EQUAL(3, r.process(1, 3).count);
	CHECK_EQUAL(0, r.process(3, 3).count);
}
TEST(GridsRuntime, SeedWaitsForCycleAndLatestWins) {
	Runtime r;
	auto s = dense();
	r.queue(s);
	r.process(0, 3);
	s.seed = 5;
	r.queue(s);
	CHECK(r.pending());
	s.seed = 9;
	r.queue(s);
	for (int tick = 1; tick < 96; ++tick)
		r.process(tick, 3);
	CHECK(r.pending());
	r.process(96, 3);
	CHECK_FALSE(r.pending());
}
TEST(GridsRuntime, MidStepSeekReleasesAndDoesNotInventHit) {
	Runtime r;
	r.queue(dense());
	r.process(0, 3);
	auto e = r.process(40, 3);
	CHECK_EQUAL(3, e.count);
	for (int i = 0; i < 3; ++i)
		CHECK_FALSE(e.notes[i].on);
	CHECK_EQUAL(2, e.ticksToNext);
	CHECK_EQUAL(0, r.process(40, 3).count);
}
TEST(GridsRuntime, RestartReproducesChaos) {
	Runtime r;
	r.queue(dense());
	std::array<Events, 192> first{};
	for (int t = 0; t < 192; ++t)
		first[t] = r.process(t, 3);
	r.stop();
	for (int t = 0; t < 192; ++t) {
		auto e = r.process(t, 3);
		CHECK_EQUAL(first[t].count, e.count);
		for (int i = 0; i < e.count; ++i) {
			CHECK_EQUAL(first[t].notes[i].part, e.notes[i].part);
			CHECK_EQUAL(first[t].notes[i].on, e.notes[i].on);
			CHECK_EQUAL(first[t].notes[i].velocity, e.notes[i].velocity);
		}
	}
}
TEST(GridsRuntime, NegativeTickStopsPlayback) {
	Runtime r;
	r.queue(dense());
	r.process(0, 3);
	CHECK_EQUAL(3, r.process(-1, 3).count);
}
