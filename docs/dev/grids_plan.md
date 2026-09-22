# Grids drum generator: menu-first Deluge plan

Status: proposed; no Grids implementation in this task.
Updated: 2026-09-22.

## Goal

Add a playable three-part drum generator to complement TB3PO basslines. Start with OLED menus and the Select
encoder, sending MIDI notes to an external drum instrument such as Battalion through the user's iPad setup.
Later, route the same musical output to internal Deluge kit rows.

The performance is about moving between grooves, thinning or thickening individual parts, adding controlled
variation, and freezing a good result into ordinary notes. A custom pad view is optional future work.

This plan complements [the TB3PO plan](../../PLAN.md); it does not replace it. The current TB3PO implementation is
still in progress. Recheck its interfaces before starting Grids, and keep changes independently reviewable.

## Source and musical scope

References:

- [Original Grids manual](https://pichenettes.github.io/mutable-instruments-documentation/modules/grids/manual/).
- [Pattern generator header](https://github.com/pichenettes/eurorack/blob/master/grids/pattern_generator.h).
- [Pattern generator implementation](https://github.com/pichenettes/eurorack/blob/master/grids/pattern_generator.cc).
- [Resource declarations](https://github.com/pichenettes/eurorack/blob/master/grids/resources.h) and
  [resource data](https://github.com/pichenettes/eurorack/blob/master/grids/resources.cc).

Pin an upstream commit and record source hashes before adapting files. The inspected Grids generator and resource
headers carry Emilie Gillet's copyright and GPL-3.0-or-later notices; preserve those notices. Do not assume the MIT
license used by some other Mutable Instruments projects applies here. Use a distinct public feature name if needed
under the repository's derivative-work guidance; `Grids` is the working name in this plan.

Preserve the drum-map algorithm, interpolation, density thresholds, and accent decisions. Replace AVR flash reads,
EEPROM, hardware I/O, static mutable state, and global randomness with ordinary constant data and clip-owned state.
Only bring across resources needed for drum generation. Keep upstream arithmetic behavior in reference tests.

The initial engine has three roles: kick, snare, and hi-hat. These are assignable destinations, not built-in sounds.
X/Y selects a location in a map of related rhythms; density selects which events survive in each part. Chaos perturbs
the map-derived levels. It is not an independent random hit probability on every step.

Retain the source's 32-position pattern at its normal rate: eight positions per quarter note, or one 4/4 bar of
thirty-second-note subdivisions. Do not reuse TB3PO's sixteenth-note step duration. Half/double speed, arbitrary
lengths, extra pattern banks, Euclidean mode, ratchets, and additional parts are later extensions.

## First playable version

One generator instance belongs to one melodic MIDI clip. Its three parts send configurable, distinct note numbers
on that clip's MIDI channel, using the existing instrument output route. The arpeggiator is off and playback direction
is Forward initially. Show unsupported combinations clearly instead of silently playing a different pattern.

Use `Generator > Grids` in the appropriate clip menu. Proposed controls:

| Control | Initial behavior |
| --- | --- |
| Enabled | Off by default; replaces ordinary note playback for this clip while enabled |
| Map X / Map Y | 0–255, preserving the source parameter resolution |
| Kick / Snare / Hat density | Three independent 0–255 values; zero silences that part |
| Chaos | 0–255; zero gives a stable map-derived rhythm |
| Seed | Deluge-specific reproducible random sequence; default fixed |
| New seed | Explicit action; never an automatic consequence of opening a menu |
| Output notes | Three MIDI note numbers, initially 36 / 38 / 42 |

The note defaults are convenient placeholders, not a claim about Battalion's mapping. Configure the app/AUM route
and actual receiving notes during the integration test. Note events trigger drums; CC mapping for generator controls
is a separate later feature. No custom iPad profile or MIDI loopback is required for this prototype.

Start with normal/accent velocities of 80/120 and a short explicit note duration, provisionally half a subdivision.
Add editable velocities and gate duration after the basic path works. Do not copy the hardware's 1 ms trigger pulse
directly into MIDI behavior. Keep note numbers distinct initially to avoid ambiguous ownership of repeated notes.

OLED menus must show values, enabled/paused state, and pending structural edits. Keep 7-segment builds compiling and
define a readable fallback before broader use. The generator continues with its menu closed.

## Live editing and randomness

Map X/Y, densities, Chaos, and velocities should affect the next subdivision, without resetting pattern position.
Use one coherent settings snapshot per subdivision so simultaneous parts see the same state. Density zero prevents
new hits immediately at that boundary; any already-sounding note still receives its scheduled note-off.

For reproducible Chaos, choose a local seeded RNG and draw a fixed set of per-part random values at each pattern
start. Scale those held values by the current Chaos amount. This lets Chaos respond on the next subdivision without
redrawing randomness whenever a knob moves. Document this as a Deluge adaptation of the source's cycle-level
perturbation behavior. Identical seed numbers need not reproduce the original module's random sequence.

A seed change applies at the next pattern boundary; the newest pending seed wins. Restart begins the deterministic
sequence again. While stopped, apply settings immediately. Seeking establishes a documented fresh cycle/RNG state;
do not replay an unbounded number of missed cycles in a timing callback. Save the recipe initially, not a promise to
resume at the exact point of an evolving random performance.

Route/note changes must release notes through the old destination before adopting the new destination. Stop, mute,
disable, deletion, and output replacement release all generator-owned notes without a global MIDI panic.

## Integration with the current TB3PO work

The current [playback notes](tb3po_playback.md) describe synth-only TB3PO with in-place Session-clip Freeze and undo.
MIDI output and generator recipe persistence are not yet provided there. Reuse proven ownership and lifecycle
patterns, but treat MIDI integration as work to do rather than an existing capability.

Relevant areas to inspect before implementation:

- `src/deluge/model/generator/`: TB3PO core, runtime, history, and freeze conversion.
- `src/deluge/model/clip/instrument_clip.{h,cpp}`: clip ownership, scheduling, cloning, and ordinary playback handoff.
- `src/deluge/model/output.cpp`: output lifecycle and note cleanup.
- `src/deluge/gui/menu_item/generator/` and `src/deluge/gui/ui/menus.cpp`: menu selection and editing.
- `src/deluge/model/instrument/midi_instrument.cpp`: normal MIDI note routing and instrument behavior.
- `tests/unit/tb3po_*`: regression coverage to retain during shared changes.

Use three modest components: a pure three-part musical core, a clip-owned scheduler, and a destination adapter.
The core returns hit/accent decisions, with no current-song or UI dependencies. Keep state and buffers bounded;
perform no allocation or storage access in the playback callback.

TB3PO's current event batch holds two events, and its runtime represents one melodic voice. Grids can produce three
simultaneous onsets plus due note-offs. Size and test the new batch for the actual worst case, with offs before
same-pitch retriggers. Share a small event or scheduling helper only where the second implementation proves useful;
do not force drums through TB3PO's slide logic or introduce a general plugin framework.

At most one generator is active per clip. If both types become available for an output, use an explicit selection
such as Off / TB3PO / Grids instead of independent booleans that could both play. Switching releases old owned notes.
TB3PO and Grids must also work concurrently on separate clips with independent random state.

Use Deluge's transport, swing, launch, mute, solo, and clip lifecycle. Disable Grids' own clock/swing implementation.
Define the 32-position generator cycle independently of source clip length; a normally launched clip starts at
position zero and cycles continuously. Verify shorter/longer clip lengths, Arrangement entry, seeks, and restarts.
Preserve piano-roll notes while enabled and restore ordinary playback at the correct current clip position.

## Freeze and internal kits

After MIDI playback works, add `Freeze last bar` following TB3PO's current interaction: create a separate inactive
ordinary Session clip on the same output, leave the source playing, and let normal clip launch switch to the result.
Preserve the source's recipe and piano-roll notes. The frozen clip must have generation disabled.

Capture actual emitted pitches, velocities, onsets, and note-offs, including live edits and Chaos. Never reconstruct
the bar from the final menu settings. Use the last completed transport-aligned 4/4 bar; partial startup bars are not
eligible. Three parts across 32 subdivisions allow 96 onsets per bar, plus any carried notes at the boundary. Derive
the buffer bound explicitly; TB3PO's 32-note captured-bar buffer is insufficient. Overflow invalidates the capture
with a clear message rather than silently dropping notes.

Build the destination fully before publishing it to the song. Handle allocation failure and full Session sections
without changing the source. Capture silence as an empty clip. Document boundary clipping and any unsupported
automation copying. Recipe persistence and frozen ordinary notes are separate deliverables.

Then add a kit destination adapter, assigning each of the three roles to an existing row. Start with SoundDrum rows;
follow with MIDI and gate rows after their normal output semantics are verified. Kits are not sample-only: respect
each row's instrument type, pitch conventions, note-off behavior, choke groups, mute state, and arpeggiator settings.
Use stable row identities and handle row deletion/reordering. Never bypass normal kit voice/choke handling.

For the first kit version, choose whole-clip generator playback and preserve ordinary notes as for MIDI. Mixing
generated rows with independently sequenced rows can follow once ownership is explicitly designed. Kit Freeze writes
to corresponding ordinary kit rows and needs its own validation.

## Milestones and acceptance checks

### 1. Adapt and test the musical core

- [ ] Pin source, retain notices, extract drum-map data and integer interpolation helpers.
- [ ] Implement per-instance state and deterministic Chaos; document deliberate source differences.
- [ ] Test reference patterns at Chaos zero, map edges/intermediate positions, density thresholds, accents, wrap,
  reproducibility, and interleaved instances. Check that raising density only adds hits for fixed map/perturbation.

Exit: meaningful three-part patterns with no firmware-global dependencies and a measured memory footprint.

### 2. Menu and MIDI playback

- [ ] Add clip-owned settings, supported-output checks, and the minimal menu.
- [ ] Integrate thirty-second-note timing, simultaneous events, gate deadlines, live edits, and lifecycle cleanup.
- [ ] Establish emulator MIDI/event capture; use hardware MIDI capture if the emulator cannot expose output.
- [ ] Configure Battalion's actual notes/channel and listen while changing map, densities, and Chaos.
- [ ] Verify no doubled hits or hanging notes on repeated pitches, stop/mute/solo/disable, clip switches, seeks, and
  Session/Arrangement transitions. Run TB3PO alongside it and confirm no cross-instance interference.

Exit: a three-part groove follows Deluge transport, responds to menu edits, and keeps playing with the menu closed.

### 3. Preserve useful performances

- [ ] Save/load validated settings, routing, seed, and algorithm version; old songs default to generator off.
- [ ] Copy the recipe on clip clone with independent runtime state; define recipe reset on conversion to unsupported
  outputs. Audit every new field's copy, load, and cleanup path.
- [ ] Implement emitted-event history and inactive-clip Freeze with dense, silent, edited, and boundary-crossing bars.

Exit: reload a recipe and save/edit a frozen MIDI performance without damaging existing notes.

### 4. Internal kits and performance mapping

- [ ] Add SoundDrum routing and kit Freeze, then evaluate MIDI/gate rows and explicit unsupported combinations.
- [ ] Give a small set of performance parameters native mapping/automation support, separate from one-shot actions.
- [ ] Test concurrent automation/manual edits and record the resulting settings-application rules.
- [ ] Decide through use whether gold knobs or a dedicated pad view justify additional UI work.

Run focused core/scheduler/history tests and existing TB3PO regressions as each implementation milestone lands.
Use `DBT_NO_SYNC=1 ./dbt test` and `DBT_NO_SYNC=1 ./dbt build release` when local dependencies allow, following the
repository's build workflow. Verify menus in the emulator and timing/output on a real MIDI route. An emulator screen
test alone cannot establish Battalion articulation or hardware timing.

Success means a reliable, enjoyable drum partner for TB3PO, with a few controls that are useful while music plays.
