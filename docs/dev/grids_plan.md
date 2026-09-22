# Grids drum generator: menu-first Deluge plan

Status: implementation plan; Grids is not implemented yet. Kit and MIDI clips are both initial targets.
Updated: 2026-09-22.

Branch: `feature/grids-generator`, starting from TB3PO commit `a10dfcf7` on `feature/tb3po-generator`.
Keep the complete TB3PO implementation, tests, and documentation available; Grids is an additional generator.

## Goal

Add a playable three-part drum generator to complement TB3PO basslines. Start with OLED menus and the Select
encoder, playing internal kit rows or sending MIDI notes to an external drum instrument such as Battalion through
the user's iPad setup. Kit support belongs in the first playable version, alongside MIDI support.
For a kit, default kick, snare, and hi-hat to its first, second, and third rows respectively.

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

One generator instance belongs to one kit or melodic MIDI clip. On a kit, its three parts trigger assigned existing
rows through normal kit playback. On a MIDI clip, they send configurable, distinct note numbers on that clip's MIDI
channel through the existing instrument output route. The arpeggiator is off and playback direction is Forward
initially. Show unsupported combinations clearly instead of silently playing a different pattern.

Kit defaults use absolute clip row indices 0, 1, and 2, independent of pad scrolling: kick = first row, snare = second,
hat = third. These are role assignments, not automatic sound recognition; the user puts the desired sounds in those
rows or changes the assignments. Bind defaults when first configuring Grids for that kit, then retain the assigned
row/drum identities across reordering rather than silently assigning different sounds. Missing or deleted rows leave
their parts unassigned and silent, visibly marked in the menu; do not create drums or silently substitute other rows.
Allow each role to select an existing row, with distinct assignments initially. Clone mappings to the corresponding
rows in the cloned clip and reset or validate mappings when replacing a kit.

Use `Generator > Grids` in the appropriate clip menu. Proposed controls:

| Control | Initial behavior |
| --- | --- |
| Enabled | Off by default; replaces ordinary note playback for this clip while enabled |
| Map X / Map Y | 0–255, preserving the source parameter resolution |
| Kick / Snare / Hat density | Three independent 0–255 values; zero silences that part |
| Chaos | 0–255; zero gives a stable map-derived rhythm |
| Seed | Deluge-specific reproducible random sequence; default fixed |
| New seed | Explicit action; never an automatic consequence of opening a menu |
| Kit rows | Kick / snare / hat destinations, initially the first / second / third clip rows |
| MIDI output notes | Three MIDI note numbers, initially 36 / 38 / 42 |

Only show destination controls relevant to the current clip type. The MIDI note defaults are convenient placeholders,
not a claim about Battalion's mapping. Configure the app/AUM route
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

## Kit playback and Freeze

Implement the kit destination adapter alongside MIDI playback. Start with SoundDrum rows, including normal sample
and synth drum playback. Explicitly mark MIDI and gate rows unsupported until their normal output semantics have been
verified. Respect each row's instrument type, note-off behavior, choke groups, mute state, and arpeggiator settings.
Use stable row/drum identities and handle deletion/reordering. Never bypass normal kit voice/choke handling.

For the first version, choose whole-clip generator playback and preserve ordinary notes while enabled, for both MIDI
and kits. Mixing generated rows with independently sequenced rows can follow once ownership is explicitly designed.

After kit and MIDI playback work, add `Freeze last bar` following TB3PO's current interaction: replace notes in the
current Session clip, disable generation, and continue ordinary playback in that same clip. Do not create another
clip or output. A single undo restores the previous notes, clip length, generator settings, mappings, and enabled
state; redo restores the frozen performance. For kits, write captured events to their assigned existing rows and
preserve all drum assignments, sounds, row parameter managers, mute states, and kit identity. Unused rows remain
present; their previous notes belong to the undo snapshot because playback initially replaces the whole clip.

Capture actual emitted destinations, pitches, velocities, onsets, and note-offs, including live edits and Chaos.
Kit history identifies the actual destination row/drum, not just a role number that may later be reassigned.
Invalidate capture on row deletion or incompatible destination changes if the original destination cannot be retained
safely. Never reconstruct
the bar from the final menu settings. Use the last completed transport-aligned 4/4 bar; partial startup bars are not
eligible. Three parts across 32 subdivisions allow 96 onsets per bar, plus any carried notes at the boundary. Derive
the buffer bound explicitly; TB3PO's 32-note captured-bar buffer is insufficient. Overflow invalidates the capture
with a clear message rather than silently dropping notes.

Build replacement notes and undo storage fully before mutating the clip. Allocation failure leaves the source
unchanged. Capture silence as an empty sequence in the same clip. Preserve existing clip and kit-row automation;
document boundary clipping and any unsupported expression conversion. Refresh pads immediately after Freeze.
Recipe persistence and frozen ordinary notes are separate deliverables.

## Milestones and acceptance checks

### 1. Adapt and test the musical core

- [ ] Pin source, retain notices, extract drum-map data and integer interpolation helpers.
- [ ] Implement per-instance state and deterministic Chaos; document deliberate source differences.
- [ ] Test reference patterns at Chaos zero, map edges/intermediate positions, density thresholds, accents, wrap,
  reproducibility, and interleaved instances. Check that raising density only adds hits for fixed map/perturbation.

Exit: meaningful three-part patterns with no firmware-global dependencies and a measured memory footprint.

### 2. Menu, kit, and MIDI playback

- [ ] Add clip-owned settings, supported-output checks, and the minimal menu.
- [ ] Add kit row selectors with first-three-row defaults, stable assignments, and visible unassigned states.
- [ ] Route kit events through normal SoundDrum handling; verify row mute, choke, and unsupported arpeggiator cases.
- [ ] Integrate thirty-second-note timing, simultaneous events, gate deadlines, live edits, and lifecycle cleanup.
- [ ] Verify the default mapping on a kit with kick, snare, and hat in its first three rows; scroll, reorder, delete,
  reassign, and clone rows/clips without wrong sounds or hanging notes. Test kits with fewer than three rows.
- [ ] Establish emulator MIDI/event capture; use hardware MIDI capture if the emulator cannot expose output.
- [ ] Configure Battalion's actual notes/channel and listen while changing map, densities, and Chaos.
- [ ] Verify no doubled hits or hanging notes on repeated pitches, stop/mute/solo/disable, clip switches, seeks, and
  Session/Arrangement transitions. Run TB3PO alongside it and confirm no cross-instance interference.

Exit: a three-part groove works on both an internal kit and a MIDI clip, follows Deluge transport, responds to menu
edits, and keeps playing with the menu closed. TB3PO still runs independently on a synth clip.

### 3. Preserve useful performances

- [ ] Save/load validated settings, routing, seed, and algorithm version; old songs default to generator off.
- [ ] Copy the recipe on clip clone with independent runtime state; define recipe reset on conversion to unsupported
  outputs. Audit every new field's copy, load, and cleanup path.
- [ ] Implement emitted-event history and in-place Freeze/undo/redo for MIDI and kit clips, with dense, silent, edited,
  and boundary-crossing bars. Verify kit sounds, routing, automation, clip identity, and immediate pad refresh.

Exit: reload a recipe and save/edit a frozen kit or MIDI performance in its original clip; undo restores prior notes
and generator playback.

### 4. Extended destinations and performance mapping

- [ ] Extend kit support to MIDI/gate rows after verifying their output semantics and Freeze behavior.
- [ ] Give a small set of performance parameters native mapping/automation support, separate from one-shot actions.
- [ ] Test concurrent automation/manual edits and record the resulting settings-application rules.
- [ ] Decide through use whether gold knobs or a dedicated pad view justify additional UI work.

Run focused core/scheduler/history tests and existing TB3PO regressions as each implementation milestone lands.
Use `DBT_NO_SYNC=1 ./dbt test` and `DBT_NO_SYNC=1 ./dbt build release` when local dependencies allow, following the
repository's build workflow. Verify menus in the emulator and timing/output on a real MIDI route. An emulator screen
test alone cannot establish Battalion articulation or hardware timing.

Success means a reliable, enjoyable drum partner for TB3PO, with a few controls that are useful while music plays.
