# TB3PO pattern core

The first implementation lives in `src/deluge/model/generator/tb3po.{h,cpp}`. It generates a complete pattern from
settings and musical context, without consulting the UI, transport, current song, or global random state. It does
not itself play notes. The [playback adapter](tb3po_playback.md) connects it to `Sound > Generator > TB3PO`.

## Source and attribution

Adapted from TB-3PO by Logarhythm, extensively modified in Phazerville by djphazer:

- Repository: [djphazer/O_C-Phazerville](https://github.com/djphazer/O_C-Phazerville).
- Pinned commit: `8f0ac08d92005821a1b1e05bc1bf664036c2f97a`.
- [Pinned source, `software/src/applets/TB3PO.h`](https://github.com/djphazer/O_C-Phazerville/blob/8f0ac08d92005821a1b1e05bc1bf664036c2f97a/software/src/applets/TB3PO.h).
- Source SHA-256: `c0bbca46367ae08e26fa5a72ba304f40e0c972b37bcda7ca81faa75f403d9ab7`.

The original MIT copyright and permission notice is preserved in both core files. The adaptation follows
`regenerate_pitches()`, `apply_density()`, and their density helpers. No O&C clock, CV, quantizer, display, or
reseed-on-reset code is included.

## Contract

`tb3po::generate(settings, context)` returns a fixed-capacity `Pattern` with an active length of 1–32. Each step has
a MIDI note, gate/rest flag, accent flag, and outgoing slide flag. Inactive array entries are zeroed. No heap storage
is used; all work is bounded by 12 scale tones and 32 steps.

Settings contain a 16-bit seed (including zero), signed density from -7 to +7, length (default 16), and a slide switch.
Density and length are clamped. There is no implicit reseeding; the New pattern action explicitly provides
a new seed. Generation starts afresh from the supplied seed on each call, so concurrent clip instances cannot
interfere with one another's random state.

Context supplies a 12-bit scale mask relative to an absolute MIDI tonic. The mask can come from Deluge's
`NoteSet::toBits()`; the adapter combines song root and bass register to choose the tonic. Defaults are natural minor
and MIDI note 36. Mask bits above 11 are ignored, and an empty mask falls back to a tonic-only scale. Tonic is clamped
to 0–127. Notes beyond the MIDI range are folded by octaves, preserving scale membership rather than clipping to an
arbitrary chromatic boundary note.

The bipolar density behavior is:

- Gate probability is `min(100, 10 + 14 * abs(density))` percent. Zero is sparse; either extreme gates every step.
- The negative side increasingly restricts the scale-degree vocabulary and encourages repeats. At -7 all degrees
  are the tonic, with octave movement still possible. At -6 the tonic and next scale tone are available.
- Positive densities +1 through +7 share their pitches and differ in gate density. Separate random streams keep
  pitch changes and scale changes from shuffling the rhythm. Increasing density magnitude only adds gates for a
  fixed seed, and equal positive/negative magnitudes have the same rhythm.
- Octave selection has a 20% down / 60% unchanged / 20% up distribution.
- Raw slide probability is 18%, reduced to 10% after a raw slide; raw accent probability is 16%, reduced to 7% after
  a raw accent. These thresholds come from the source. Accents on rests are suppressed; slides also require an
  immediately following gated step. Consequently sparse patterns have fewer effective accents and slides.

`slide` connects the current step to the immediately following step, including the last-to-first connection. It
never skips rests or creates hidden gates. Same-pitch connections, including a one-step loop, are permitted as tie
intent; the playback adapter implements ties and releases held notes on stop or disable.
Disabling slides changes no other pattern data. Changing length retains the common pitch/gate/accent prefix and
re-evaluates the new final slide against the first step.

## Deliberate differences from the applet

- Local xorshift32 streams replace platform/global `random()` and `randomSeed()`. Range mapping uses explicit unsigned
  multiply-high arithmetic, with fixed seeds, salts, warm-up, and draw ordering. Numeric seeds do not reproduce O&C
  patterns. The fixed expected pattern in the tests protects Deluge's reproducibility contract.
- Steps are generated in playback order. Octave decisions are stored per step, independent of forced degree repeats;
  the applet's reverse bit accumulation and conditional octave-mask shifts are not reproduced.
- A slide into a rest is suppressed. The CV applet can keep its gate high into an otherwise ungated step. Here rests
  remain rests, which gives the eventual MIDI and synth adapters the same explicit note-source contract.
- Short/empty scales are handled explicitly, including the one-tone case where upstream's degree bounds can cross.
- Sparse patterns may be completely silent, especially at length 1. No compulsory first note is inserted. For example,
  seed `0x0303`, density 0, length 16 produces all rests; raising density adds gates deterministically.
- The core introduces no CV glide curve, velocities, gate durations, scheduler, persistence, or menu-owned state.
  Velocities, gate durations, and scheduling are supplied by the playback adapter; settings belong to the clip.

## Inspected examples

Seed `0x002A`, natural minor, tonic 36, length 16. Numbers are MIDI notes; `-` is a rest, `!` an accent, and `~` an
outgoing slide. These were inspected as event patterns; audible judgment remains for playback integration.

```text
density 0: sparse, with a same-pitch tie on steps 9–10
notes    - 26  -  -  -  -  -  - 36 36  -  -  -  -  -  -
accent   .  .  .  .  .  .  .  .  .  .  .  .  .  .  .  .
slide    .  .  .  .  .  .  .  .  ~  .  .  .  .  .  .  .

density -7: a full rhythm of tonic notes and octave jumps
notes   24 24 36 36 36 36 36 48 36 36 36 36 36 48 36 36
accent   !  .  .  .  !  .  .  .  .  .  !  .  .  .  .  .
slide    ~  .  .  .  .  .  .  .  ~  .  .  .  ~  .  .  .

density +7: the same rhythm and articulation, with the full scale available
notes   29 26 36 36 41 39 38 53 36 44 44 44 43 58 39 44
accent   !  .  .  .  !  .  .  .  .  .  !  .  .  .  .  .
slide    ~  .  .  .  .  .  .  .  ~  .  .  .  ~  .  .  .
```

## Validation

`tests/unit/tb3po_tests.cpp` covers a fixed expected pattern, zero and maximum seeds, interleaved instances,
lengths 1/16/32 and intermediate loop boundaries, invalid inputs, sparse and dense rhythm behavior, negative-side
repetition, seven scale shapes, MIDI-range edges, transposition, and independent scale/slide edits.

Run the host suite with `DBT_NO_SYNC=1 ./dbt test`, or just this group after building with
`build/tests/unit/Debug/UnitTests -g TB3PO -v`. Build firmware with `DBT_NO_SYNC=1 ./dbt build release`.
The firmware source glob includes the core and scheduler automatically; the synth clip adapter calls them.
