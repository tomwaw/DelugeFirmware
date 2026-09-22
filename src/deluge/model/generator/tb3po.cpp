// Copyright (c) 2020, Logarhythm
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

// Adapted from TB-3PO by Logarhythm, with modifications by djphazer.
// Source: O_C-Phazerville, commit 8f0ac08d92005821a1b1e05bc1bf664036c2f97a,
// software/src/applets/TB3PO.h. See docs/dev/tb3po_core.md for adaptation details.

#include "model/generator/tb3po.h"
#include <algorithm>

namespace deluge::model::generator::tb3po {
namespace {

// Fixed xorshift32 sequence and multiply-high range mapping, independent of libc and global random state.
// Nonzero salts with high bits set cannot be cancelled by the 16-bit seed. Warm up each stream identically.
class Random {
public:
	Random(uint16_t seed, uint32_t salt) : state_(seed ^ salt) {
		for (int i = 0; i < 8; ++i) {
			next();
		}
	}

	uint32_t below(uint32_t limit) { return (static_cast<uint64_t>(next()) * limit) >> 32; }
	bool chance(int probability) { return static_cast<int>(below(100)) < probability; }

private:
	uint32_t next() {
		state_ ^= state_ << 13;
		state_ ^= state_ >> 17;
		state_ ^= state_ << 5;
		return state_;
	}

	uint32_t state_;
};

// Upper inclusive scale degree, following TB3PO's bipolar density curve.
int highestDegree(int pitchDensity, int scaleSize) {
	if (scaleSize == 1) {
		return 0;
	}
	if (pitchDensity > 7) {
		return scaleSize - 1;
	}
	if (pitchDensity < 2) {
		return std::min(pitchDensity, scaleSize - 1);
	}
	const int range = std::max(scaleSize - 3, 4);
	return std::clamp(3 + (pitchDensity - 3) * range / 4, 1, scaleSize - 1);
}

uint8_t pitchInRange(int note) {
	// Sanitized tonic and a single octave shift put note in -12..150, so two folds suffice at most.
	if (note < 0) {
		note += 12;
	}
	if (note > 127) {
		note -= ((note - 128) / 12 + 1) * 12;
	}
	return static_cast<uint8_t>(note);
}

} // namespace

Pattern generate(const Settings& settings, const MusicalContext& context) {
	Pattern pattern{};
	pattern.length = std::clamp<int>(settings.length, 1, kMaxSteps);
	const int density = std::clamp<int>(settings.density, -7, 7);
	const int pitchDensity = std::min(density + 7, 8);
	const int gateProbability = 10 + (density < 0 ? -density : density) * 14;
	const int tonic = std::clamp<int>(context.tonicNote, 0, 127);

	std::array<uint8_t, 12> scale{};
	int scaleSize = 0;
	for (int semitone = 0; semitone < 12; ++semitone) {
		if (context.scaleNotes & (1u << semitone)) {
			scale[scaleSize++] = semitone;
		}
	}
	if (scaleSize == 0) {
		scaleSize = 1; // scale[0] is the tonic.
	}
	const int maxDegree = highestDegree(pitchDensity, scaleSize);

	Random pitches(settings.seed, 0x9E3779B9u);
	Random octaves(settings.seed, 0x243F6A88u);
	Random rhythm(settings.seed, 0xB7E15162u);
	int previousDegree = 0;
	bool previousSlide = false;
	bool previousAccent = false;

	for (int i = 0; i < pattern.length; ++i) {
		// Always consume the same number of draws, even for the first step or a forced repeat.
		const bool repeat = pitches.chance(50 - pitchDensity * 6);
		int degree = pitches.below(maxDegree + 1);
		if (i > 0 && repeat) {
			degree = previousDegree;
		}
		previousDegree = degree;

		const int coin = octaves.below(200);
		const int octave = coin < 80 ? ((coin & 1) ? 1 : -1) : 0;
		Step& step = pattern.steps[i];
		step.note = pitchInRange(tonic + scale[degree] + octave * 12);
		step.gate = rhythm.chance(gateProbability);
		previousSlide = rhythm.chance(previousSlide ? 10 : 18);
		previousAccent = rhythm.chance(previousAccent ? 7 : 16);
		step.slide = previousSlide;
		step.accent = step.gate && previousAccent;
	}

	// A slide is an outgoing connection, never a hidden note that turns a rest into a gate.
	for (int i = 0; i < pattern.length; ++i) {
		Step& step = pattern.steps[i];
		const Step& next = pattern.steps[(i + 1) % pattern.length];
		step.slide = settings.slides && step.slide && step.gate && next.gate;
	}
	return pattern;
}

} // namespace deluge::model::generator::tb3po
