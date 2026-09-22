# TB3PO playable synth prototype

Build from the repository root:

```sh
DBT_NO_SYNC=1 ./dbt build release
```

Close the previous emulator, then launch this build explicitly:

```sh
/Applications/DelugEmu.app/Contents/MacOS/DelugEmu \
  "$PWD/build/Release/deluge.elf" \
  --sd "$HOME/Library/Application Support/DelugEmu/sdcard_rw"
```

1. Open a synth clip. Keep its arpeggiator off and sequence direction Forward.
2. Press Select (Return), then open `Generator > TB3PO`.
3. Click Enabled so its box is checked, then press Play (Space).
4. Back out of the menu with Backspace. The generated sequence keeps playing; the piano-roll contents are preserved.
5. Reopen TB3PO and change Seed, Density, or Length while playing. `Next cycle` means the edit is queued. The newest
   recipe takes over at the next generator-cycle boundary. Stop applies pending edits immediately for the next start.
6. Disable TB3PO to return to ordinary notes at the clip's current grid position. Independent NoteRows realign there.

In installed DelugEmu v0.5.7 with this checkout, Select's mouse-wheel direction is reversed: scroll **down** over
Select to move down menus or increase a value; scroll up to move up or decrease it.

| Control | Behavior |
| --- | --- |
| Enabled | Off by default; selects generated notes instead of piano-roll playback |
| Seed | 0–9999; default 771; reproducible with the same other settings and song scale/root |
| New pattern | Explicitly chooses a different seed and displays its number |
| Freeze last bar | Replaces the current clip’s notes with the last complete captured 4/4 bar; supports undo/redo |
| Density | -7 to +7; zero sparse, either extreme fully gated; negative values favor repeated scale degrees |
| Length | 1–32 sixteenth-note steps; default 16 |
| Register | 0–8; default 3 means tonic MIDI note 36 plus the song root pitch class |
| Slides | Hold into the next gated step; same-pitch slides tie |

The scheduler follows Deluge's swung ticks. Non-slide gates last half a step. Normal velocity is 80; accents use
120. To hear connected synth articulation, use the synth's Legato voice mode and adjust portamento as desired.
The synth patch determines how velocity affects the sound; a continuing legato voice may retain its original
velocity. This is not a dedicated 303 accent circuit or CV glide implementation.

Settings are **not saved into songs yet**. Clip clones copy the recipe with fresh runtime state. MIDI, kit, and CV
outputs are not supported. Turning on arp or changing to reverse/ping-pong pauses an enabled generator and displays
`TB3PO paused`; turn arp off and restore Forward to resume. Undo for individual generator parameter edits and mapping
are later work; Freeze itself supports undo/redo.

## Prove Freeze works

1. Enable TB3PO and play for at least one complete bar. If enabled partway through a bar, wait through the following
   full bar too. Capture follows transport bars, even when Length is not 16.
2. Select **Freeze last bar**. `Play a full bar` means no complete capture is available yet. `Frozen in clip` means
   the current clip now contains the captured notes, its loop is one 4/4 bar, and TB3PO is disabled.
   The pads refresh immediately beneath the open menu, including colours for the updated vertical scroll.
3. Back out of the menu to the piano roll. The same clip continues playing ordinary editable notes. No new clip or
   track is created; its name, output, section, and active/muted state stay unchanged.
4. From the clip view, press **Back/Undo** to restore the previous notes, original loop length, and generator recipe
   and enable state. **Shift + Back** redoes Freeze. Subsequent note edits have their normal undo steps first.
5. Save the song to keep the frozen notes through normal song storage. Generator recipe persistence remains separate.

Freeze can also use the last completed capture after stopping, muting, or disabling. Restarting/seeking clears the
capture and requires another full bar. Freeze is available for Session synth clips while recording is off.
Freeze and undo/redo release outgoing notes, then align the replacement loop to the current transport position.

The capture stores **emitted events**, so a bar spanning a live seed/density change contains what actually played.
Silent bars replace the sequence with an empty one-bar loop. Held notes are split at the bar edges; frozen loops do
not preserve a glide across the final-to-first boundary. Same-pitch ties within the bar remain one long note.
Cross-pitch slides use one tick of overlap so normal NoteRow playback produces legato; that is a timing approximation.

Clip-level synth parameters and automation stay in place. Freeze captures notes, not sound/effects or historical
synth automation. Previous NoteRows (including their individual lengths, directions, and expression data) are kept
in undo storage. The frozen rows contain captured notes and velocities, without new MPE lanes.

Capture uses fixed arrays and allocates nothing during playback. Replacement rows and undo storage are built before
changing the clip. If allocation fails, the current sequence is preserved. Freeze, undo, and redo swap whole NoteRow
sets without allocating during the handover. The previous state remains available while its undo action is retained.

## Verification

`DBT_NO_SYNC=1 ./dbt test` runs the host suite. The 13 core tests cover deterministic patterns; 16 runtime tests cover
note-off deadlines, overlapping slide order, ties, rests, pending edits, stop/restart, seeks, and invalid patterns.
The 13 history/conversion tests cover complete versus partial bars, mid-bar enable, boundary splits, slide overlap,
same-pitch ties/retriggers, a live recipe change within a five-step cycle, silent bars, stop/restart, overflow, and
clock changes. Run just these with `build/tests/unit/Debug/UnitTests -g TB3POHistory -v` after building tests.

Current-clip Freeze verification (2026-09-22): started with an existing note in a two-bar clip. Freeze replaced it
with the six captured notes and a one-bar loop; undo restored the exact original note, two-bar length, and enabled
generator; redo restored all six captured notes. The clip count remained one, and clip/output/parameter pointers
were unchanged throughout. Frozen playback produced nonzero audio (peak 8233), with zero output after transport stop.
Screenshot: `build/emulator/freeze-inplace.png`. Release build, all seven host test targets, new-file formatting, and
`git diff --check` passed. Song save/reload and hardware slide articulation remain hands-on checks.

Pad-refresh regression check: after Freeze, all four note pads visible at the default scroll position updated while
the TB3PO menu remained open, and remained visible after closing it. No Session/clip view switch was used.
Screenshot: `build/emulator/freeze-redraw-menu.png`. The release build and formatting/diff checks passed.

Emulator smoke checks used a release ELF with WAV capture and confirmed nonzero synth output, continued playback
after closing menus, a queued seed edit becoming active, transport restart, and zero output after stop and mute.
Disabling returned to a test piano-roll note; removing that note left silence. Debugger inspection confirmed the
disabled runtime had no owned note and was stopped. Screenshots are under `build/emulator/tb3po-*.png`.
The final rebuilt ELF also passed an empty-clip enable/disable check: captured peak 8178 while enabled and zero
after disabling. Release build, all seven CTest targets, new-file formatting checks, and `git diff --check` passed.

Still check by hand: rapid edits across cycles, clip switches/clones, Session solo, Arrangement recording/playback,
independent NoteRow lengths/directions when returning to ordinary notes, and slide/accent sound with suitable patches.
Hardware timing and external MIDI behavior have not been validated by this prototype.
