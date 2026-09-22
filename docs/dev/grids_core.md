# Grids core and playback prototype

The Grids implementation extends the TB3PO branch. TB3PO remains synth-only; Grids supports kit and melodic MIDI
clips. Grids recipe persistence and Freeze are separate subsequent milestones, not part of this first playback build.

## Source

Adapted from Emilie Gillet's [Grids source](https://github.com/pichenettes/eurorack/tree/08460a69a7e1f7a81c5a2abcc7189c9a6b7208d4/grids),
pinned at `08460a69a7e1f7a81c5a2abcc7189c9a6b7208d4`. The adapted core and extracted map retain the GPL-3.0-or-later notices.
Only the 25 drum-map nodes are included; no AVR clock, EEPROM, UI, or Euclidean resources are compiled.
The map contains 2,400 bytes of constant levels plus its pointer lookup table.

Interpolation follows upstream `ReadDrumMap` and the pinned AVR helper `U8Mix`: multiply by weights summing to 255,
then shift right by eight, twice. Do not replace this with a rounded lerp or divide by 255: reference patterns change.
The AVR dependency is [avril](https://github.com/pichenettes/avril/blob/276b2887e4110ca913294fcbb313163dfb28a448/op.h)
at `276b2887e4110ca913294fcbb313163dfb28a448`.

SHA-256 of inspected original files:

- `resources.cc`: `02c4911d1ee940f921d50207c25a39d911c3f6af9ace87869e7e667954c63265`
- `pattern_generator.cc`: `33bea2b5233a6becf86a70b455dfba5ffa4a831dd7fdcd3a8ebaac61a722ddd5`
- `op.h`: `23f810d4673744c2a81ffa986d53e4005898543fe2829cf3bfa25a9f4c7cb275`

## Deluge adaptations

Each clip has independent settings, PRNG, and runtime. Local xorshift32 is initialized from seed + 1, including seed
zero, and draws three bytes at each cycle start. Chaos scales these held bytes live before the original saturating
level addition and density/accent thresholds. Chaos zero matches upstream map decisions. Numeric seeds do not claim
to match the original module. Map, density, and Chaos edits affect the next step; the latest seed edit takes effect
at the next cycle start. Stop/restart begins the deterministic sequence again. Seeks and destination changes restart
random state at the current grid position; missed events are not replayed.

The cycle is always 32 thirty-second-note steps, independent of clip length. Deluge supplies swung ticks; there is
no second clock. Gates are half a step (minimum one tick). A batch fits three offs followed by three ons; storage and
work are bounded and no generator allocation occurs in the playback callback. Normal/accent velocities are 80/120.

Kit defaults bind kick/snare/hat to absolute row indices 0/1/2 on first configuration. Row selectors display 1/2/3;
0 means unassigned. Encoder selection skips destinations assigned to another part. Assignments follow drum identity through reordering, and missing/deleted rows are silent.
Changing a destination releases old owned gates first; assigning one destination to two parts is rejected.
Kit replacement disables Grids and resets mappings. Clones copy settings and drum assignments on the shared kit,
with a fresh scheduler. SoundDrum rows use normal Kit note-on/off and row expression paths, including choke behavior.
Forward playback and arpeggiators off are required. Assigned MIDI/gate drum rows pause this initial version.
MIDI clips send distinct configurable notes (defaults 36/38/42) through their usual output, channel, and device route.

While enabled, Grids replaces whole-clip piano-roll playback without changing the notes. Disable rejoins ordinary
playback at the current clip position. Existing notes remain visible; live generated hits are not written onto pads.

## Try it

Build with `DBT_NO_SYNC=1 ./dbt build release`, close the previous emulator, then launch:

```sh
/Applications/DelugEmu.app/Contents/MacOS/DelugEmu \
  "$PWD/build/Release/deluge.elf" \
  --sd "$HOME/Library/Application Support/DelugEmu/sdcard_rw"
```

1. Load a kit and put the desired kick, snare, and hat in its first three rows, or edit the Grids row assignments.
2. Open the sound menu with Select, then `Generator > Grids`. It is also available from the kit's Affect Entire menu.
3. Enable Grids and press Play. Adjust Map X/Y, the three densities, and Chaos; back out and it keeps playing.
4. Density zero silences that part. Disable Grids to return to the clip's ordinary notes.
5. For MIDI, open a MIDI clip's `Generator > Grids` and configure the output notes for your receiving instrument.

The installed emulator uses Return for Select, Space for Play, Backspace for Back, and A for Affect Entire. Scroll
down over Select to increase values/move down menus. Grids settings currently live in memory only.

## Verification

Host tests include seven pinned reference map positions, monotonic density, zero-density silence, deterministic
independent instances, wrapping, simultaneous events, off-before-retrigger ordering, stop, live edits, queued seed,
seeks, and restart. Existing TB3PO core/runtime/history tests remain in the same suite.

Hardware MIDI timing, Battalion articulation, and broader Arrangement behavior require hands-on testing.

Verified 2026-09-23:

- Release ELF and BIN built successfully. All seven CTest targets passed, including the existing TB3PO regressions.
  The 13 Grids tests passed 29,223 checks. On ARM, the scheduler occupies 56 bytes, including its 8-byte core.
- In DelugEmu v0.5.7, factory kit `000 TR-808` bound the default parts to KICK, SNARE, and HATC, exactly its first
  three rows. All three piano-roll rows remained empty. Recorded output peaked at 22,756; playback continued with
  the menu closed and output became zero after transport stop.
- The final build also produced kit audio (peak 22,749) and became silent when Grids was disabled with transport
  still running. The row selector showed row 1/KICK and moved directly to row 4/HATO, skipping assigned rows 2/3.
- Final-build USB MIDI capture recorded 8 note-ons/offs for note 36, 9 for note 38, and 11 for note 42, with velocities
  80/120. Every off matched an active note; none remained held after stop. DIN serial capture was empty in this
  emulator run; USB capture was used to verify emitted bytes. This does not establish real hardware DIN timing.
- Screenshots and local captures live under `build/emulator/grids-*` and are excluded from Git.

Still verify by hand: simultaneous TB3PO and Grids clips, solo and clip switching, row deletion/reordering/clone,
kit replacement, fewer-than-three-row kits, choke and muted-row behavior, non-default clip lengths, and Arrangement.
The code includes these lifecycle paths, but the emulator smoke checks above are narrower than full acceptance.
