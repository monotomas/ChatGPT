# MIDI-only core validation,7 November2026

The dependency-free standalone core executable passes six suites with
2,140,697 assertions and10,000 realtime iterations with zero new allocations.
This is numeric MIDI-engine validation, not a Windows/VST3/Ableton result.
No plugin or installer is approved by this archive.

The implementation has31 scale definitions. Mathematical parent rotations
check all seven diatonic, jazz melodic-minor and harmonic-minor modes. The
three user-specified bebop formulas and other scale families are checked
explicitly. All50 chord menus retain every31 choice, put compatible scales
first, and preserve the requested family/mode order within both sections.
Transposition and all31 packed GUI snapshot values are exercised. Existing
exhaustive nearest-note/reference recognition, voice lifetime, pedals,
overflow and allocation checks remain intact.

The original nineteen internal IDs/indices and all50 numeric factory defaults
are retained. A separate post-run audit matches every31 name,interval and
family/mode entry to the original worksheet catalog and verifies that the
50-type chord table remains byte-identical to source commit
`d65f2bc5c95eca84dec7d7a0e297cf05e70143cc`. New factory choices await the user's
Excel file. Stable raw indices/IDs do NOT prove compatibility of old host
normalized automation when an AudioParameterChoice changes19→31 options.
Old project/preset import and the new component identity are processor/host
concerns; they are not tested here.

`command.json` retains the exact successful standalone command, omitted binary
hash and the compiler version observed afterward. `compile.txt` retains the
pre-existing warnings from the unchanged audited ChordNames.h reference;
there are no new MusicData/CoreTests warnings. `run.txt` is unedited stdout.
`source-inputs-after-run.json` records selected project input hashes afterward,
not an invented before-compile attestation or full system-header closure.
The runtime binary is intentionally excluded. Current tracked Source/Tests
provide the implementation; copies are not duplicated here.

`inventory.json` checks retained bytes, excluding itself. Verification does
not execute audio, load a plugin or establish practical Live12.4.6 behavior.
