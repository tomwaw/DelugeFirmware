# TB3PO generator: menu-first Deluge experiment

Status: playable synth-only TB3PO prototype implemented and built; emulator audio and menu smoke checks passed.
Generator settings are in memory only. Freeze last bar replaces the current clip’s notes, with undo/redo.
MIDI, recipe persistence, and broader Session/Arrangement validation remain open.
Updated: 2026-09-22.

Branch: `feature/tb3po-generator`.
Starting point: upstream `main` at `d735e7a65bd97e5af3c8947499a4d145fd5551af`.

## Goal and direction

Make the Deluge more musical, generative, and playable for personal use. Start with one enjoyable acid-pattern
generator inspired by TB3PO, controlled through the OLED, existing menus, and the Select encoder. The user already
has a running Deluge emulator; use it for early development and UI feedback.

The intended generator plays live as an alternative note source for an instrument clip, using Deluge's transport
and instrument outputs. It does not continually rewrite piano-roll notes. A later Freeze action captures what
actually played into an ordinary editable clip. Opening or closing the menu must not affect generation.

This document sets the direction for this branch. The existing `docs/dev/generative_sequencer_plan.md` remains
background material; its static-generation-first implementation order is superseded here.

Work in small, demonstrable increments. First prove a menu-controlled bassline can play and stop reliably. Do not
build a general sequencer framework, custom pad interface, or complete modulation system before that works.

## Reference and adaptation

- [TB3PO source](https://raw.githubusercontent.com/djphazer/O_C-Phazerville/refs/heads/wiki-docs/software/src/applets/TB3PO.h):
  retain the MIT notice and credit Logarhythm and djphazer when adapting code. Pin an upstream commit before copying.
- [Sequencer Modes PR #4143](https://github.com/SynthstromAudible/DelugeFirmware/pull/4143): architectural reference
  for alternative clip playback, not a dependency to merge wholesale. Recheck its implementation before reuse.
- Extract musical decisions from the O&C applet: seeded patterns, gates/rests, pitch degrees, octave changes,
  accents, slides, and the characteristic bipolar density control. Keep the initial maximum at 32 steps.
- Replace O&C clocks, CV, quantizer, UI, and global random-number calls with Deluge adapters and a per-instance RNG.
  Reproducibility on Deluge is required; identical numeric seeds need not match O&C unless explicitly implemented.
- Keep extensions such as independent accent probability, slide probability, mutation, or pitch-range controls
  separate from the initial adaptation. Do not turn every possible extension into an MVP requirement.

## First playable prototype

Following the confirmed synth-only menu test, target an internal synth clip first, with its arpeggiator off, so the
first audible prototype uses the same emulator workflow. Add a melodic MIDI output path afterward, giving an
observable note-event output and a route to an iPad synth. Support both melodic MIDI and internal synth clips before
calling the feature a usable first release; verify emulator MIDI routing during the integration spike.

Use an existing instrument-clip menu entry, provisionally `GENERATOR > TB3PO`. The smallest useful controls are:

| Control | Initial behavior |
| --- | --- |
| Enabled | Choose ordinary clip notes or TB3PO playback; default off |
| Seed | Explicit, reproducible pattern seed |
| New pattern | One-shot action selecting a new seed |
| Density | TB3PO-style bipolar control, displayed as -7 through +7 |
| Length | 1–32 steps; default 16 |
| Register / transpose | Place scale-constrained notes in a useful bass register |
| Slides | On/off |

Start with sixteenth-note steps, song scale/root, and fixed normal/accent velocities. Add editable rate and velocities
once the basic path works. A seed stays unchanged until explicitly edited or regenerated; transport restart alone
does not pick a new pattern. Automatic reseeding and a separate lock control can follow if useful.

Display the effective seed and values clearly. Reuse normal menu navigation and Back behavior. No custom pad UI,
gold-encoder assignment, or MIDI CC mapping is required for this prototype. OLED is the initial target; keep
7-segment builds compiling and explicitly define their fallback before release.

## Playback and ownership

Use three small boundaries, with names and placement chosen to fit the current source:

1. A UI-independent TB3PO core takes settings, musical context, and a seed, and returns a bounded step pattern.
2. Clip-owned runtime state advances that pattern on Deluge's existing musical clock and schedules note on/off.
3. An output adapter uses existing melodic-instrument paths for MIDI and internal synths.

The menu edits clip-owned settings. It does not own the generator or drive playback with UI timers. Avoid a broad
plugin API until a second generator demonstrates what should actually be shared.

- Generator playback and normal piano-roll playback are mutually exclusive for a clip initially. Preserve original
  notes when enabling the generator; disabling it restores ordinary playback at a defined safe boundary.
- Use existing tempo, swing, clip launch, mute, solo, and stop behavior. Account for Session and Arrangement playback,
  clip-relative position, transport seeks/restarts, and loops; do not create a separate transport.
- Schedule real note-off deadlines. A note-output API argument such as `sampleSyncLength` is not a gate scheduler.
- Explicitly handle repeated pitches, rests, slide chains, loop boundaries, and pending edits. Stop owned notes on
  disable, stop, mute, clip/output replacement, deletion, and song load. Never send a global panic as normal cleanup.
- Keep generation and event storage bounded. No allocation, file access, or unbounded work in timing callbacks.
- Clip cloning, save/load, output conversion, and undo need deliberate treatment; copying a UI setting is insufficient.

### Changing controls during playback

For the first version, seed, density, length, scale/root, and register changes prepare a replacement pattern that
becomes active at the next generator-cycle boundary. The latest pending settings win. While stopped, apply edits
immediately. Show when a change is pending so a delayed audible response is understandable.

Apply slide-mode changes at the same boundary initially. Finish or release outgoing notes explicitly during the
handover. Stop and disabling the generator must not wait indefinitely for a pattern boundary to release notes.
Later, selected performance controls can apply on the next step after their musical behavior is tested.

### Accents and slides

Accents initially select between normal and accented note velocities. Their sound depends on the receiving patch;
velocity alone does not reproduce a 303's accent circuit.

For MIDI, express slide intent using controlled legato note overlap, with note-on/off ordering and repeated-note
behavior tested. The receiving synth must support and be configured for legato/portamento. Device-specific CCs are
an optional later profile, not a universal slide command. Do not port O&C's CV smoothing constants into MIDI timing.

For internal synths, verify the existing mono/legato/portamento path and how it handles accents during a slide chain.
A legato pitch change may not update velocity like a fresh note. Document the initial limitation or add a narrowly
scoped adaptation after testing; do not promise identical behavior for every patch.

## Freeze to the piano roll

After live playback is reliable, add `Freeze last bar`. Capture a bounded history of actual emitted note events,
including velocity, onset, duration, and overlap. Do not regenerate from the current seed: settings may have changed
during the captured interval. Capture notes, not the resulting audio or synth/effect randomness.

The initial target is the last completed transport-aligned 4/4 bar, even when the generator cycle has another length.
Disable the action with an explanation until enough history exists. Later options may include the last cycle or
multiple bars.

The user-selected destination is the current clip. Replace its NoteRows and loop length, disable the generator,
and continue ordinary playback on the same output at the current transport position. Preserve the old NoteRows,
length, generator recipe, and enable state in a dedicated undo consequence. Keep clip identity, name, section,
launch status, and clip-level synth parameters/automation unchanged. Undo/redo must release outgoing notes.

Specify clipping/splitting of notes that cross capture boundaries, final slides, and same-pitch overlaps. Verify that
Deluge's note storage and ordinary playback can reproduce the captured articulation, and document any unavoidable
conversion. Prepare changes outside the timing callback and handle allocation failure without a partial capture.

## Parameters, mapping, and persistence

Give settings clear identities and one authoritative clip owner from the start. This should make later MIDI learn,
gold-encoder assignment, and automation possible without coupling the core to menus.

Mapping is a later milestone, not an automatic consequence of adding a menu item. Inspect the different internal-synth
and MIDI parameter paths before choosing storage. Use native parameter mechanisms for internal controls rather than
routing MIDI back into Deluge. Distinguish continuous controls from structural settings and one-shot actions such as
New pattern and Freeze; an automation lane must not trigger those actions continuously.

Save generator settings and the effective seed per clip once the prototype stabilizes. Define defaults for old songs,
validate loaded values, and preserve behavior when cloning clips. Version the algorithm if future changes would alter
saved patterns. Evolving generators would require more state than a seed; that is outside the first version.

## Milestones and checks

### 0. Establish a working development loop

- [x] Fetch upstream main, fast-forward local main, and create a dedicated branch.
- [x] Record this plan without modifying the previous planning document.
- [x] Locate the installed emulator and establish an explicit firmware path for this checkout (see below).
- [x] Build the updated baseline; load the changed build and confirm OLED and encoder behavior in the emulator.
- [ ] Establish how to inspect MIDI events in the emulator.
- [x] Trace menu ownership, clip scheduling, note output, and stop handling on this revision. Choose the smallest
  integration path and record it here before adding playback code.

Exit: a repeatable edit/build/run loop and a concrete route from a menu value to a clip-owned note source.

#### Menu smoke test / local development loop (2026-09-22)

- Added `Sound > Generator > TB3PO test` only to `soundEditorRootMenu`, the synth clip root.
  Selecting the test action displays `TB3PO test OK`; it does not edit clip state or play notes.
  The 7-segment labels are `GEN > TEST`, with an `OK` popup.
- Installed emulator: `/Applications/DelugEmu.app`, version v0.5.7. A separate portable bundle also exists at
  `/Users/tom/Sites/DelugEmu-macos-arm64`. Pass the firmware explicitly to avoid loading a downloaded release.
- Build from this checkout: `DBT_NO_SYNC=1 ./dbt build release` (existing dependencies are already present).
  Outputs: `build/Release/deluge.elf` and `build/Release/deluge.bin`.
- Launch from Terminal, in this checkout:

  ```sh
  /Applications/DelugEmu.app/Contents/MacOS/DelugEmu \
    "$PWD/build/Release/deluge.elf" \
    --sd "$HOME/Library/Application Support/DelugEmu/sdcard_rw"
  ```

- Open a synth clip, press Select, turn to `Generator`, press Select, then press Select on `TB3PO test`.
  Back returns normally. Other clip types have no Generator entry.
- Validation: release build succeeded; `DBT_NO_SYNC=1 ./dbt test` passed all 7 CTest targets on macOS.
  Test dependency configuration required network access outside the sandbox. The new header and string enum passed
  Xcode clang-format checks; the bundled formatter has an incompatible/missing Intel zstd dependency.
- Booted the built ELF in DelugEmu and visually confirmed the synth root includes `Generator`.
  Screenshot: `build/emulator/synth-menu.png`. User confirmed the interactive test works.
- Playback ownership, scheduling, and MIDI event inspection remain to be traced before implementing generation.

### 1. Deterministic musical core

- [x] Pin and adapt the TB3PO source into a small, host-testable core.
- [x] Test repeatability, instance independence, density behavior, valid pitches, and lengths 1, 16, and 32.
- [x] Inspect representative sparse, repetitive, and busy event patterns.
- [ ] Listen to those patterns once playback is connected; determinism alone is not musical quality.

Exit: bounded, reproducible step patterns with gate, accent, and slide intent, independent of firmware globals.

Implementation: `src/deluge/model/generator/tb3po.{h,cpp}`, with 13 focused tests in `tests/unit/tb3po_tests.cpp`.
Pinned upstream: `8f0ac08d92005821a1b1e05bc1bf664036c2f97a`; MIT notice and Logarhythm/djphazer attribution retained.
See [core contract, adaptation choices, and inspected patterns](docs/dev/tb3po_core.md).
Validation: release firmware build passed, all 7 CTest targets passed (156 unit tests, including the 13 new TB3PO
tests), and formatting/diff checks passed for the new code.
The core uses a local seeded RNG, fixed arrays, scale-relative pitches, and explicit outgoing slide connections.
It now feeds the clip-owned scheduler and internal-synth adapter described below.

### 2. Menu-controlled live playback

Integration route (2026-09-22): store settings and a bounded scheduler on `InstrumentClip`. Run it from
`InstrumentClip::processCurrentPos` after `Clip::processCurrentPos`, retaining clip automation and loop/launch handling
but bypassing NoteRow playback while enabled. Use `Song::getSixteenthNoteLength()` and the existing swung-event
deadline. Send individual notes through `MelodicInstrument::sendNote` with the clip's parameter manager and zero MPE
values. Stop generator-owned notes through the clip stop, seek, output detach, and active-clip replacement paths.
Clones copy settings but start with fresh runtime state. The first adapter supports forward synth clips with arp off;
other directions/arp combinations must be visibly unavailable. Pending recipes swap at generator-cycle boundaries.
Persistence and Freeze remain separate milestones.

- [x] Add the minimal menu and per-clip state; connect one output path first.
- [x] Integrate Deluge timing, note-off scheduling, and safe pending-pattern swaps.
- [ ] Verify menu closure, rapid edits, transport restart, mute, disable, and clip switching without stuck notes.
- [ ] Complete MIDI and internal-synth support, checking accents and slides against actual output behavior.
- [ ] Verify Session and Arrangement behavior; keep unsupported clip/output types unavailable in the menu.

Exit: an acid bassline can play while the menu is closed, and edits take effect at the documented boundary.

#### Playable prototype (2026-09-22)

`Sound > Generator > TB3PO` replaces the earlier test popup. Controls are Enabled, Seed, New pattern, Density,
Length, Register, and Slides. Defaults: off, seed 771, density +4, 16 steps, register 3, slides on.
Use a forward synth clip with its arp off. Press Play after enabling; generation continues with the menu closed.
See [launch instructions, controls, and verification](docs/dev/tb3po_playback.md).

The bounded runtime lives in `model/generator/tb3po_runtime.{h,cpp}` with 16 focused host tests, alongside the 13 core
tests. DelugEmu checks confirmed actual nonzero audio, live seed edits showing `Next cycle` then clearing, playback
with the menu closed, restart, and silence after stop/mute. Disabling released the runtime note and restored a test
piano-roll note; removing that note left silence. Independent NoteRows are realigned to the clip grid on disable.
Rapid edits, clip handovers, Arrangement, unusual NoteRow lengths/directions, and audible slide/accent quality still
need broader hands-on checks. The checkbox covering all those integration cases remains open.

### 3. Make it safe to keep and edit musical results

- [ ] Add validated save/load and clip-copy behavior, including old-song defaults.
- [x] Add event history and Freeze, with an explicit destination, boundary policy, and reversible note conversion.
- [ ] Test saved/reloaded settings, capture after live edits, and the transition back to ordinary clip playback.

Exit: keep a useful performance as normal editable notes without losing the source clip or leaving notes sounding.

Freeze implementation: `tb3po_history.{h,cpp}` records actual emitted note events into bounded transport-aligned
4/4 bars. `tb3po_freeze.cpp` snapshots the completed bar and builds replacement NoteRows before modifying the current
clip. A dedicated undo consequence swaps full NoteRow sets, loop lengths, generator settings/state, direction, and
arp mode. Clip-level parameters remain in place. Freeze disables TB3PO and continues ordinary playback; undo restores
the previous sequence and generator state. Bar-edge notes are split, same-pitch ties remain sustained, and internal
slides use one tick of overlap. See the [Freeze test procedure](docs/dev/tb3po_playback.md).
Song save/reload and hardware articulation remain unverified, so the broader save/capture/transition milestone check
stays open.
Current-clip verification: release build and all seven host test targets passed. Emulator checks confirmed one clip
throughout, identical clip/output/parameter identities, a six-note freeze, undo restoring an existing note and two-bar
length with TB3PO enabled, redo restoring the frozen notes, and silence on stop.

### 4. Performance controls, only after the core is useful

- [ ] Expose a small set of musically useful controls to native mapping/automation where supported.
- [ ] Test automation and manual editing together, including pending boundary changes.
- [ ] Decide from hands-on use whether gold knobs, extra menu controls, or a dedicated pad view add enough value.

For implementation milestones, use focused host tests for core/scheduler behavior, the emulator for UI and integration,
and hardware or MIDI capture for real timing and articulation. Run the relevant firmware build and `./dbt test` when
dependencies allow. An emulator pass alone does not establish hardware timing or external-synth slide behavior.

## Deliberately later

Kit clips can contain sound, MIDI, and gate rows; they are not inherently incompatible with generation. Their row
routing, pitch conventions, choke behavior, and arpeggiators need their own adapter. Defer kits, CV, MPE, and generator
plus arpeggiator combinations until the melodic path works.

Future experiments include drum generation inspired by Grids, remembered randomness inspired by Marbles, Euclidean
rhythms, motifs, fills, and other musical algorithms. Evaluate each source's license and hardware dependencies
individually. A MIDI macro/controller overlay for iPad apps such as Battalion remains a separate future project.

The measure of success is a reliable instrument that is fun to play, not the number of generator types or controls.
