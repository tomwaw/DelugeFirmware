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

#pragma once

#include <array>
#include <cstdint>

namespace deluge::model::generator::tb3po {

constexpr uint8_t kMaxSteps = 32;

struct Settings {
	uint16_t seed = 0;
	int8_t density = 0;  // -7..+7; both extremes are dense, negative values reduce pitch variety.
	uint8_t length = 16; // 1..32 sixteenth-note steps. Timing is owned by the playback adapter.
	bool slides = true;
};

struct MusicalContext {
	// Semitone mask relative to tonicNote, compatible with NoteSet::toBits(). Default: natural minor.
	uint16_t scaleNotes = 0x05AD;
	int16_t tonicNote = 36; // Absolute MIDI tonic, including bass register; C2 by default.
};

struct Step {
	uint8_t note = 0; // MIDI pitch, also populated for rests.
	bool gate = false;
	bool accent = false; // Always false on a rest. The output adapter chooses velocities.
	bool slide =
	    false; // Connect to the immediately following step, only if both have gates; wrap at the loop boundary.

	bool operator==(const Step&) const = default;
};

struct Pattern {
	std::array<Step, kMaxSteps> steps{};
	uint8_t length = 0;

	bool operator==(const Pattern&) const = default;
};

// Pure, bounded generation with local RNG state: no allocation, clock, UI, or firmware globals.
// Clamp length/density/tonic to their valid ranges. Ignore scale bits above 11; an empty scale uses only the tonic.
// Fold out-of-range pitches by octaves to keep them in 0..127 without leaving the supplied scale.
// Unused steps are zero-initialized. Changing length preserves pitches/gates/accents in the shared prefix;
// its last slide is resolved against the new loop boundary.
[[nodiscard]] Pattern generate(const Settings& settings, const MusicalContext& context = {});

} // namespace deluge::model::generator::tb3po
