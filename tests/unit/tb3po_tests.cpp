#include "CppUTest/TestHarness.h"
#include "model/generator/tb3po.h"

#include <array>
#include <cstdint>

using namespace deluge::model::generator::tb3po;

namespace {

int gateCount(const Pattern& pattern) {
	int count = 0;
	for (int i = 0; i < pattern.length; ++i) {
		count += pattern.steps[i].gate;
	}
	return count;
}

void checkPitchesAndArticulation(const Pattern& pattern, const MusicalContext& context) {
	for (int i = 0; i < pattern.length; ++i) {
		const Step& step = pattern.steps[i];
		CHECK(step.note <= 127);
		const int interval = (step.note - context.tonicNote % 12 + 12) % 12;
		CHECK(context.scaleNotes & (1u << interval));
		if (!step.gate) {
			CHECK_FALSE(step.accent);
			CHECK_FALSE(step.slide);
		}
		if (step.slide) {
			CHECK(pattern.steps[(i + 1) % pattern.length].gate);
		}
	}
	for (int i = pattern.length; i < kMaxSteps; ++i) {
		CHECK(pattern.steps[i] == Step{});
	}
}

TEST_GROUP(TB3PO){};

TEST(TB3PO, KnownSeedKeepsItsMusicalRecipe) {
	// Reviewed C-minor recipe: fixes the RNG sequence, draw order, octave mapping, and articulation together.
	// Same-pitch slides are intentional (e.g. step 9 into step 10).
	const std::array<Step, 16> expected = {{
	    {36, true, false, false},
	    {46, true, false, false},
	    {38, true, false, false},
	    {31, true, false, false},
	    {39, true, true, false},
	    {46, true, true, false},
	    {58, true, false, false},
	    {38, true, true, false},
	    {41, true, false, true},
	    {41, true, false, false},
	    {53, true, false, true},
	    {31, true, false, false},
	    {44, true, true, false},
	    {24, true, false, false},
	    {44, true, false, true},
	    {38, true, false, false},
	}};
	const Pattern pattern = generate({0x303, 7, 16, true});
	CHECK_EQUAL(16, pattern.length);
	for (int i = 0; i < pattern.length; ++i) {
		CHECK(pattern.steps[i] == expected[i]);
	}
}

TEST(TB3PO, RepeatableAcrossInterleavedInstancesAndSeedZero) {
	for (uint16_t seed : {0, 1, 0x303, 0xFFFF}) {
		const Settings settings{.seed = seed, .density = 4, .length = 32};
		const Pattern expected = generate(settings);
		for (int i = 0; i < 20; ++i) {
			const Pattern unrelated = generate({.seed = static_cast<uint16_t>(i + 700), .density = -4});
			CHECK_EQUAL(16, unrelated.length);
			CHECK(generate(settings) == expected);
		}
	}
	CHECK_FALSE(generate({.seed = 0, .density = 7}) == generate({.seed = 1, .density = 7}));
}

TEST(TB3PO, ValidPitchesAndArticulationAcrossScalesAndLengths) {
	for (uint16_t mask : {0x001, 0x081, 0x295, 0x5AD, 0xAB5, 0xFFF, 0x800}) {
		for (int16_t tonic : {0, 1, 36, 47, 116, 127}) {
			const MusicalContext context{mask, tonic};
			for (uint8_t length : {1, 16, 32}) {
				for (int density = -7; density <= 7; ++density) {
					for (uint16_t seed : {0, 1, 0x303, 0xFFFF}) {
						const Pattern pattern = generate({seed, static_cast<int8_t>(density), length, true}, context);
						CHECK_EQUAL(length, pattern.length);
						checkPitchesAndArticulation(pattern, context);
					}
				}
			}
		}
	}
}

TEST(TB3PO, SanitizesInvalidSettingsAndEmptyScale) {
	CHECK(generate({42, -128, 0, true}, {0xFFFF, -32768}) == generate({42, -7, 1, true}, {0xFFF, 0}));
	CHECK(generate({42, 127, 255, true}, {0xFFFF, 32767}) == generate({42, 7, 32, true}, {0xFFF, 127}));
	for (uint16_t mask : {0, 0xF000}) {
		const Pattern pattern = generate({0x303, 7, 32, true}, {mask, 38});
		CHECK(pattern == generate({0x303, 7, 32, true}, {1, 38}));
		checkPitchesAndArticulation(pattern, {1, 38});
	}
}

TEST(TB3PO, DensityMagnitudeAddsGatesSymmetricallyWithoutRemovingExistingOnes) {
	for (uint16_t seed = 0; seed < 128; ++seed) {
		Pattern previous = generate({seed, 0, 32, true});
		for (int magnitude = 1; magnitude <= 7; ++magnitude) {
			const Pattern positive = generate({seed, static_cast<int8_t>(magnitude), 32, true});
			const Pattern negative = generate({seed, static_cast<int8_t>(-magnitude), 32, true});
			for (int i = 0; i < kMaxSteps; ++i) {
				CHECK_EQUAL(positive.steps[i].gate, negative.steps[i].gate);
				CHECK_EQUAL(positive.steps[i].accent, negative.steps[i].accent);
				CHECK_EQUAL(positive.steps[i].slide, negative.steps[i].slide);
				if (previous.steps[i].gate) {
					CHECK(positive.steps[i].gate);
				}
			}
			previous = positive;
		}
		CHECK_EQUAL(32, gateCount(previous));
	}
}

TEST(TB3PO, CenterIsSparseAndFullDensityHasAccentsAndSlides) {
	int sparseGates = 0;
	int accents = 0;
	int slides = 0;
	for (uint16_t seed = 0; seed < 128; ++seed) {
		sparseGates += gateCount(generate({seed, 0, 32, true}));
		const Pattern dense = generate({seed, 7, 32, true});
		for (const Step& step : dense.steps) {
			accents += step.accent;
			slides += step.slide;
		}
	}
	// Broad statistical guards across a fixed seed set: catch stuck RNGs and inverted probability curves.
	CHECK(sparseGates > 250 && sparseGates < 600); // Expected about 10% of 4096 steps.
	CHECK(accents > 350 && accents < 900);
	CHECK(slides > 400 && slides < 1000);
}

TEST(TB3PO, NegativeExtremeRepeatsTonicWithOctaveMovement) {
	const MusicalContext context{0x5AD, 38};
	bool below = false;
	bool above = false;
	for (uint16_t seed = 0; seed < 32; ++seed) {
		const Pattern pattern = generate({seed, -7, 32, true}, context);
		CHECK_EQUAL(32, gateCount(pattern));
		for (const Step& step : pattern.steps) {
			CHECK_EQUAL(2, step.note % 12);
			below |= step.note < context.tonicNote;
			above |= step.note > context.tonicNote;
		}
	}
	CHECK(below);
	CHECK(above);
}

TEST(TB3PO, NegativeSideNarrowsPitchVocabularyAndRepeatsMore) {
	int narrowChanges = 0;
	int wideChanges = 0;
	bool wideUsesOtherDegrees = false;
	for (uint16_t seed = 0; seed < 128; ++seed) {
		const Pattern narrow = generate({seed, -6, 32, true});
		const Pattern wide = generate({seed, 6, 32, true});
		for (int i = 0; i < kMaxSteps; ++i) {
			const int narrowPitch = narrow.steps[i].note % 12;
			CHECK(narrowPitch == 0 || narrowPitch == 2);
			wideUsesOtherDegrees |= wide.steps[i].note % 12 > 2;
			if (i > 0) {
				narrowChanges += narrowPitch != narrow.steps[i - 1].note % 12;
				wideChanges += wide.steps[i].note % 12 != wide.steps[i - 1].note % 12;
			}
		}
	}
	CHECK(wideUsesOtherDegrees);
	CHECK(narrowChanges < wideChanges / 2);
}

TEST(TB3PO, PositiveDensityChangesOnlyRhythm) {
	for (uint16_t seed = 0; seed < 32; ++seed) {
		const Pattern low = generate({seed, 1, 32, true});
		for (int density = 2; density <= 7; ++density) {
			const Pattern high = generate({seed, static_cast<int8_t>(density), 32, true});
			for (int i = 0; i < kMaxSteps; ++i) {
				CHECK_EQUAL(low.steps[i].note, high.steps[i].note);
			}
		}
	}
}

TEST(TB3PO, ScaleChangesPreserveRhythm) {
	const Settings settings{0x303, 3, 32, true};
	const Pattern original = generate(settings);
	for (uint16_t mask : {0, 1, 0x81, 0xAB5, 0xFFF}) {
		const Pattern changed = generate(settings, {mask, 36});
		for (int i = 0; i < kMaxSteps; ++i) {
			CHECK_EQUAL(original.steps[i].gate, changed.steps[i].gate);
			CHECK_EQUAL(original.steps[i].accent, changed.steps[i].accent);
			CHECK_EQUAL(original.steps[i].slide, changed.steps[i].slide);
		}
	}
}

TEST(TB3PO, DisablingSlidesDoesNotChangeNotesGatesOrAccents) {
	const Pattern on = generate({0x303, 7, 32, true});
	const Pattern off = generate({0x303, 7, 32, false});
	bool hasSlide = false;
	for (int i = 0; i < kMaxSteps; ++i) {
		CHECK_EQUAL(on.steps[i].note, off.steps[i].note);
		CHECK_EQUAL(on.steps[i].gate, off.steps[i].gate);
		CHECK_EQUAL(on.steps[i].accent, off.steps[i].accent);
		CHECK_FALSE(off.steps[i].slide);
		hasSlide |= on.steps[i].slide;
	}
	CHECK(hasSlide);
}

TEST(TB3PO, LengthEditsKeepPrefixAndResolveOutgoingSlideAtNewBoundary) {
	bool observedBoundaryChange = false;
	bool observedSingleStepSlide = false;
	for (uint16_t seed = 0; seed < 128; ++seed) {
		const Pattern full = generate({seed, 4, 32, true});
		for (uint8_t length = 1; length < kMaxSteps; ++length) {
			const Pattern shorter = generate({seed, 4, length, true});
			for (int i = 0; i < length; ++i) {
				CHECK_EQUAL(full.steps[i].note, shorter.steps[i].note);
				CHECK_EQUAL(full.steps[i].gate, shorter.steps[i].gate);
				CHECK_EQUAL(full.steps[i].accent, shorter.steps[i].accent);
				if (i + 1 < length) {
					CHECK_EQUAL(full.steps[i].slide, shorter.steps[i].slide);
				}
			}
			if (shorter.steps[length - 1].slide) {
				CHECK(shorter.steps[0].gate);
			}
			observedBoundaryChange |= shorter.steps[length - 1].slide != full.steps[length - 1].slide;
			observedSingleStepSlide |= length == 1 && shorter.steps[0].slide;
		}
	}
	CHECK(observedBoundaryChange);
	CHECK(observedSingleStepSlide); // A one-step loop can request a same-pitch tie, handled later by playback.
}

TEST(TB3PO, RootAndRegisterTransposeWithoutChangingRhythm) {
	const Pattern original = generate({0x303, 4, 32, true}, {0x5AD, 36});
	for (int16_t tonic : {38, 48}) {
		const Pattern transposed = generate({0x303, 4, 32, true}, {0x5AD, tonic});
		for (int i = 0; i < kMaxSteps; ++i) {
			CHECK_EQUAL(original.steps[i].note + tonic - 36, transposed.steps[i].note);
			CHECK_EQUAL(original.steps[i].gate, transposed.steps[i].gate);
			CHECK_EQUAL(original.steps[i].accent, transposed.steps[i].accent);
			CHECK_EQUAL(original.steps[i].slide, transposed.steps[i].slide);
		}
	}
}

} // namespace
