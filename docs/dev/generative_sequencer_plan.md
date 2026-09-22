# Generative Music and Sequencer Plan

Status: planning

Initial scope: personal experimental firmware

Last updated: 2026-08-07

## Goal

Improve the Deluge with musical, controllable generative sequencing. The user should be able to create a rhythm,
bassline, melody, chord part, or arpeggio from the current scale and optionally the song's chords, then edit the
result like any other Deluge sequence.

Development must proceed in small, testable increments. Each milestone should leave the firmware working and add
one independently useful capability. Static generation comes first. Automatic pattern evolution during playback is
a later feature, after note creation, undo, musical behavior, and performance are proven.

## Product principles

- Generate ordinary Deluge notes first. Generated material remains visible, editable, copyable, saveable, and
  playable without a separate engine.
- Make every result reproducible with a seed.
- Keep musical rules soft and adjustable. Scale membership and valid note positions are hard constraints; chord
  adherence, contour, repetition, and genre are preferences.
- Separate genre from musical role. `House + Bass` and `House + Lead` require different behavior.
- Reuse existing Deluge timing, scales, note rows, probability, iterance, undo, and playback.
- Keep the generation core independent of the UI and global firmware state so it can be unit tested on a computer.
- Avoid song-file format changes until the controls and behavior have stabilized.
- Do not make the first implementation depend on the proposed
  [Lanes](https://github.com/SynthstromAudible/DelugeFirmware/pull/4353) or
  [Sequencer Modes](https://github.com/SynthstromAudible/DelugeFirmware/pull/4143) pull requests. Re-evaluate those
  integrations after the generator works on the normal sequencer.
- Prefer bounded storage, integer or fixed-point scoring, and predictable execution time.

## Initial non-goals

- Running Python, JavaScript, a neural network, or an external music-theory library on the Deluge.
- Generating audio or choosing synth presets.
- Replacing the existing sequencer or playback scheduler.
- Training a model from MIDI files on the Deluge.
- Shipping a large MIDI corpus inside the firmware.
- Simultaneously implementing a MIDI browser, live generative playback, chord recognition, and genre learning.

## Proposed architecture

The generator should have a pure planning layer and a Deluge-specific application layer.

```text
GeneratorConfig + GeneratorContext + Seed
                    |
                    v
             GenerationCore
                    |
                    v
          fixed list of GeneratedNote
                    |
                    v
             ClipNoteWriter
                    |
                    v
     normal NoteRows + one undoable Action
```

Suggested data boundary:

```cpp
struct GeneratorConfig {
	uint32_t seed;
	uint8_t bars;
	uint8_t stepsPerBar;
	uint8_t density;
	uint8_t minDegree;
	uint8_t maxDegree;
};

struct GeneratorContext {
	MusicalKey key;
	int16_t rootNote;
	int32_t clipLength;
};

struct GeneratedNote {
	int32_t position;
	int32_t length;
	int16_t noteCode;
	uint8_t velocity;
};
```

These types are illustrative. Their final names and locations should follow nearby firmware conventions. The
important boundary is that the core returns a note plan without touching the current song, UI, action logger, or
playback engine.

## Milestone plan

### 0. Establish guardrails and fixtures

Purpose: agree on observable behavior before introducing a new UI or changing clips.

Implementation:

- Choose the initial target: melodic instrument clips only, 4/4, one to four bars.
- Define a maximum generated-note count and return a clear error instead of allocating without a bound.
- Identify the supported note grid and conversion from generator steps to Deluge ticks.
- Create representative test fixtures for major, natural minor, Dorian, Phrygian, and pentatonic scales.
- Record a short hardware listening checklist covering bass, lead, and chord use cases.
- Decide how generation treats existing notes. Initially support only an empty clip or explicit `REPLACE`.

Tests:

- Fixture construction works in host unit tests.
- Step-to-tick conversion is exact at supported clip lengths.
- Invalid lengths, empty scales, and excessive event counts fail safely.

Exit criteria:

- The generation contract, limits, and first test cases are reviewed before clip-writing code is added.

### 1. Build a deterministic headless generator

Purpose: prove the smallest complete generation pipeline without firmware UI or model mutation.

Implementation:

- Add a small deterministic pseudo-random number generator owned by the generation context.
- Generate a one-bar monophonic sequence on a fixed grid.
- Select pitches only from the supplied scale and configured register.
- Return `GeneratedNote` values; do not write to a clip yet.
- Start with simple even spacing and a small random walk of scale degrees.

Controls represented in `GeneratorConfig`:

- Seed
- Steps per bar
- Density or pulse count
- Lowest and highest scale degree/octave
- Gate length
- Velocity

Tests:

- The same configuration and seed always produce identical notes.
- Different seeds produce at least two distinct results over a defined sample.
- Every note belongs to the selected scale and register.
- Positions are ordered, lengths are positive, and events do not exceed the generation span.
- Maximum-density and zero-density cases are safe.

Exit criteria:

- The core has no dependency on the GUI, current song, playback, or dynamic string-based music representation.

### 2. Write generated notes into a clip

Purpose: turn the headless output into normal Deluge sequence data.

Implementation:

- Add a `ClipNoteWriter`-style adapter that maps each pitch to a note row and calls the normal note-add path.
- Write all generated notes as one undoable action.
- Support melodic clips only.
- Require an empty target clip at first, or display a confirmation/error when notes already exist.
- Refresh the clip view after a successful write.
- Never emit notes directly to the playback engine from the generator.

Tests:

- Applying a known generated plan creates the expected rows and notes.
- Undo removes the complete generation in one action; redo restores it.
- Failure partway through application does not leave a partially generated clip.
- Notes remain correct after the existing song save/load path.
- Generation during playback does not corrupt note vectors or leave stuck notes.

Hardware check:

- Generate into an empty synth, MIDI, and CV melodic clip where supported by the common note-row path.
- Confirm playback, editing, copy/paste, save/load, undo, and redo.

Exit criteria:

- A generated clip behaves exactly like a manually entered clip.

### 3. Add a minimal user interface

Purpose: make the first version usable without committing to a large menu design.

Initial menu:

```text
GENERATOR
  LENGTH
  RATE
  DENSITY
  RANGE
  GATE
  SEED
  GENERATE
```

Implementation:

- Add one discoverable entry from the melodic clip context/menu.
- Support OLED and 7-segment displays using existing menu conventions.
- Keep settings session-local initially; do not serialize them into the song.
- `GENERATE` applies the plan. Changing a parameter does not modify notes until `GENERATE` is selected.
- Add `NEW SEED` or encoder interaction for quick rerolling.

Tests:

- Each menu item clamps and displays its valid range.
- Cancel leaves the clip unchanged.
- Generate followed by undo returns to the exact previous state.
- OLED and 7-segment paths expose the same behavior.

Exit criteria:

- A user can generate, audition, undo, change the seed, and generate again without hidden shortcuts.

### 4. Improve rhythm generation

Purpose: establish reusable rhythmic behavior before adding more pitch intelligence.

Add one rhythm operation at a time:

1. Even pulse distribution using the existing Deluge Euclidean behavior where practical.
2. Rotation.
3. Explicit 16-step onset masks.
4. Rest/density probability.
5. Accent and gate patterns.
6. Rhythmic bias: downbeat, offbeat, sixteenth subdivisions, before beat, and after beat.

Keep each operation independently testable. A rhythm stage should output onset positions, lengths, and accents; the
pitch stage should not decide where notes occur.

Tests:

- Known pulse/step combinations match expected patterns.
- Rotation wraps exactly.
- Density zero and full density behave correctly.
- Accent and gate values remain in MIDI/Deluge ranges.
- Seeded probability produces stable results.

Hardware check:

- Test one-bar and odd-length clips.
- Confirm generated events align with the grid at several zoom levels.

Exit criteria:

- Rhythm generation can be used independently of the pitch generator.

### 5. Add musical pitch scoring

Purpose: replace the basic random walk with a controllable musical decision function.

For every allowed candidate pitch, calculate a score from independent factors:

```text
score = degree preference
      x interval/proximity preference
      x melodic expectation
      x register gravity
      x recent-note diversity
      x contour preference
```

Initial factors:

- Prefer small steps while allowing occasional leaps.
- After a large leap, prefer a smaller movement in the opposite direction.
- After a small movement, mildly prefer continuing in the same direction.
- Pull notes back toward the configured register centre.
- Penalize immediate and recent repetitions by an adjustable amount.
- Support flat, ascending, descending, arch, and valley contours.
- Retain only the previous four pitches as melodic state.

Implementation notes:

- Use scale-degree or MIDI-integer candidates, not note-name strings.
- Use fixed-point or small integer weights and bounded candidate lists.
- Keep each factor switchable in tests.
- A first-order transition table can be added later, but should not be required for musical output.

Tests:

- Candidate scoring is deterministic and does not overflow.
- A leap-reversal fixture favors the expected direction.
- Register gravity never selects notes outside the range.
- Contour preference measurably shifts generated pitch height over many fixed seeds.
- Disabling all soft factors leaves a valid uniform scale choice.

Exit criteria:

- Listening tests produce coherent one- and four-bar lines without genre presets.

### 6. Add motif and phrase development

Purpose: make generated music repeat and develop instead of producing unrelated notes continuously.

Operations, added in this order:

1. `REPEAT`: copy the source motif exactly.
2. `VARY END`: retain rhythm and most pitches, changing one or two ending notes.
3. `TRANSPOSE`: move a motif by scale degree while staying in range.
4. `INVERT`: reverse melodic direction around a pivot where possible.
5. `REROLL BAR`: regenerate interior pitches while preserving the first and last notes.
6. Phrase plans: `AAAA`, `AABB`, `ABAB`, and `AABA`.
7. Call-and-response: preserve the call rhythm and produce a related answer.

Implementation:

- Represent a motif as generated events plus its seed and length.
- Generate the complete phrase into normal clip notes; do not create a new runtime pattern type.
- Preserve boundaries during partial rerolls to avoid audible melodic discontinuities.

Tests:

- Repetition is bit-for-bit identical.
- Variation changes only the requested notes.
- Transposition remains in scale and range.
- Phrase plans place the expected motif variant in every bar.
- Rerolling one bar does not alter other bars.

Exit criteria:

- Four- and eight-bar results contain recognizable repetition and controlled variation.

### 7. Add chord following

Purpose: make basslines, melodies, arpeggios, and chord parts respond to song harmony.

First implementation:

- Add `HARMONY: SCALE / SONG CHORDS`.
- Read filled song chord-memory slots in order and distribute them evenly across the generation span.
- Treat chord memory notes initially as a set of allowed or preferred pitch classes; do not require root detection.
- Add `ADHERENCE` from 0 to 100.
- At high adherence, strongly favor chord tones on strong beats while allowing scale passing notes on weak beats.
- If no chord-memory slots are filled, fall back visibly and safely to scale mode.

Second implementation:

- Infer or let the user select the chord root when bass generation needs it.
- Add chord-tone indices for root, third, fifth, seventh, and extensions.
- Add closest-inversion voice leading for chord parts.
- Allow a generated progression to populate or reference chord-memory slots.

Tests:

- At 100% adherence, designated strong-beat notes belong to the active chord.
- At 0% adherence, chord context does not affect a fixed-seed result.
- Chord changes occur at exact generation-span boundaries.
- Empty and partially filled chord memories are handled correctly.
- Voice leading chooses the tested lowest-movement inversion.

Hardware check:

- Generate bass and lead clips against the same chord slots and verify complementary but harmonically consistent
  results.

Exit criteria:

- A generated line follows a four-chord sequence without requiring manual correction of obvious clashes.

### 8. Add roles and genre presets

Purpose: make useful musical results accessible through a small number of high-level choices.

Roles:

- Bass
- Lead
- Chords
- Arpeggio

First genres:

- House
- Techno
- Acid
- Trance

A preset is data, not a separate generator. It supplies defaults for rhythm masks/bias, scale-degree weights,
register, gate, accent, repetition, mutation, contour, and chord adherence. The user can override the defaults.

Example starting behavior:

| Preset | Rhythm | Pitch | Harmony | Development |
| --- | --- | --- | --- | --- |
| House Bass | Offbeat and syncopated eighth/sixteenth patterns | Root, fifth, octave, passing tones | High adherence | High repetition, low mutation |
| House Chords | Offbeat stabs | Triads, sevenths, inversions | Song chords | Stable with occasional inversion change |
| Techno Bass | Dense or Euclidean sixteenths | Two- to four-note pool | Scale/root biased | Periodic mutation |
| Techno Lead | Sparse repeated motif | Narrow movement with rare octave motion | Medium adherence | AABB or call-and-response |
| Acid Bass | Sixteenth grid with rests and accents | Root, fifth, octave, chromatic approach option | Root/scale | Medium mutation |

Tests:

- Presets only change defaults; explicit user overrides win.
- Every preset stays inside its declared role range.
- Snapshot fixtures make accidental preset changes visible in review.
- Hardware listening tests cover several seeds per preset rather than approving a single lucky result.

Exit criteria:

- House Bass and Techno Bass reliably produce recognizable starting points over multiple seeds.
- At least one lead or arpeggio preset works with chord following.

### 9. Add deliberate evolution commands

Purpose: approach hardware generative sequencers without introducing continuous background mutation yet.

Commands:

- `LOCK`: retain the current seed and result.
- `NEW`: create a new seed and regenerate the selected scope.
- `VARY`: preserve the motif and alter a controlled number of attributes.
- `REROLL BAR`: regenerate only the selected bar.
- `MUTATE RHYTHM` and `MUTATE NOTES`: change one dimension independently.

Scope choices can be introduced progressively: current bar, selected region, current clip, and later multiple clips.

Tests:

- Every operation is one undoable action.
- Note-only mutation leaves positions, gates, and velocities unchanged.
- Rhythm-only mutation preserves pitch order where events can be mapped.
- Locked seeds reproduce the same result after reopening the menu.

Exit criteria:

- The user can explore variations quickly without losing the last good version or regenerating unrelated material.

### 10. Prototype live evolving playback

Purpose: add optional cycle-by-cycle evolution only after static generation is reliable.

Start with the safest behavior:

1. Regenerate only at a clip loop boundary.
2. Generate the next cycle into a bounded staging buffer.
3. Swap or apply the complete result atomically.
4. Preserve a locked base seed and a separate cycle counter.
5. Provide an immediate `FREEZE` action that commits the audible result as ordinary notes.

Open design decision:

- Mutating stored notes at boundaries keeps everything visible but interacts with undo and live editing.
- A temporary playback overlay avoids modifying notes but creates a second sequencing path.

Do not choose between these architectures until milestones 1–9 provide profiling data and real hardware experience.
This is also the appropriate point to re-evaluate the Lanes and Sequencer Modes pull requests.

Tests:

- No event is emitted twice or skipped at loop boundaries.
- Freezing produces exactly the currently audible notes.
- Editing, stopping, changing clips, saving, and loading cannot leave a staging buffer active.
- CPU and memory remain within measured budgets while audio and MIDI are busy.
- Failure to generate the next cycle repeats the current valid cycle rather than producing silence or corruption.

Exit criteria:

- Live evolution is explicitly opt-in, glitch-free, recoverable, and freezeable.

### 11. Add user-owned MIDI analysis and preset import

Purpose: let personal musical material inform the generator without parsing or training large datasets on the
Deluge.

Desktop tool workflow:

```text
owned MIDI files
    -> label by genre and role
    -> normalize notes to scale/chord degrees
    -> extract rhythm, interval, gate, velocity, and repetition statistics
    -> export a compact Deluge generator preset
```

Implementation order:

1. Define a versioned, compact preset schema.
2. Add firmware loading for one preset file from the SD card.
3. Write an offline analyzer for monophonic MIDI clips.
4. Add role and genre labels.
5. Extend analysis to chords and multitrack material only if the monophonic workflow is useful.

Keep MIDI clip browsing/import as an adjacent project. It can share SD-card browsing and MIDI parsing, but it should
not block the generator and should not be introduced in the same firmware change.

Tests:

- Malformed or unsupported preset files fail without affecting the song.
- Schema versions are checked explicitly.
- Exporting the same corpus and options is deterministic.
- Imported weights are clamped to safe bounds.
- Firmware presets continue working when no SD-card presets are present.

Exit criteria:

- A small collection of personal House or Techno MIDI clips can produce a compact preset that changes generation
  behavior in a repeatable and audible way.

## Testing strategy across all milestones

### Host unit tests

- Determinism: identical inputs and seed produce identical output.
- Invariants: scale, register, time bounds, ordering, gate length, velocity, and maximum event count.
- Golden fixtures for selected seeds, with intentional updates documented.
- Property-style loops over many seeds, scales, clip lengths, and densities.
- Individual scoring-factor tests rather than listening-only validation.

### Firmware integration tests

- Note-row creation and note insertion.
- One-action undo/redo.
- Replace and cancellation behavior.
- Save/load compatibility after generated notes are committed.
- Playback-safe generation and error handling.

### Hardware acceptance tests

- OLED and 7-segment navigation.
- Synth, MIDI, and CV melodic clips where applicable.
- Generation while stopped and while playing.
- Long clips and high-density limits.
- CPU, RAM, and generation latency measurements.
- Listening across multiple seeds and roles.

Musical quality cannot be established by one golden output. Each preset should have a small listening matrix: several
seeds, two scales, at least two chord progressions, and the intended role/register.

## Delivery gates

| Gate | Included milestones | Usable result |
| --- | --- | --- |
| Technical prototype | 0–2 | Deterministic notes can be generated, written, and undone |
| First playable version | 3–5 | Menu-driven musical rhythm and melody generation |
| Musical MVP | 6–7 | Motifs, phrases, and chord following |
| Genre release | 8–9 | House/Techno/Acid starting points and controlled variation |
| Experimental live mode | 10 | Stateful evolution at loop boundaries |
| Personalization | 11 | Presets derived from user-owned MIDI |

Do not begin a new gate until the previous gate's exit criteria pass on hardware. A milestone may be split further if
it changes both the generation core and the UI substantially.

## External projects and provenance

### Subsequence

[Subsequence](https://github.com/simonholliday/subsequence) is the strongest architectural reference. Useful areas
include seeded randomness, persistent melodic state, candidate scoring, motif development, chord graphs, voice
leading, rhythmic bias, and per-cycle rebuilding. It is licensed AGPL-3.0-or-later.

### Melody Mate

[Melody Mate](https://github.com/Fenrir200678/melody_mate) is useful for weighted rhythm ideas, simple scale-derived
training sequences, chord adherence, contours, and menu concepts. Its repository declares a non-commercial licence
allowing personal use and modification with attribution.

### Reuse policy for this personal project

- Record the upstream project, exact commit, source file, and licence when code or data is copied.
- Preserve required copyright and attribution notices.
- Prefer a small native C++ implementation that fits Deluge data structures over mechanically translating Python or
  TypeScript architecture.
- Keep Melody Mate-derived code out of any public firmware contribution unless compatible permission is obtained.
- Review AGPL obligations before distributing a build containing Subsequence-derived implementation.
- Independently authored algorithms and tests should still credit the musical or architectural inspiration where
  appropriate.

## Immediate next work

The next implementation task is milestone 0 only:

1. Choose the initial grid and maximum generated-note count.
2. Define the pure input/output types.
3. Add deterministic generator fixtures to the unit-test build.
4. Implement no menu and make no clip changes yet.

That creates the stable seam on which every later rhythm, pitch, chord, genre, and live-evolution feature can build.
