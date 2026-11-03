# Reviewed 0.2.1 audio evidence

This folder records a fresh isolated build of the reviewed detector snapshot
whose `PitchDetector.cpp` SHA256 is
`e70cc37f1b344c8dac325364a33e545e7393f3b0d305d526f6d8bb6e3b332a34`.
`snapshot-manifest.json` binds all six DSP files, native evaluator/test sources,
compiler/flags, executable hashes and the frozen guitar-fixture seal. These
were measured before the final repository commit, so `source_commit` is
explicitly null; content hashes identify the measured source. Source stability
and agreement with the live working tree were checked after all measurements.
This is native Linux detector evidence, not a Windows/Live/hardware measurement.

The public/holdout/confirmation folders contain 196 known-data regression rows
at 48 kHz / 64 samples. All 107 mono reference notes are correct without counted
errors; broad poly remains 163/180 matched with 17 misses, 20 additional NoteOns,
10 octave extras and three early releases. Independent Python reconstruction
from raw MIDI agrees with all native counts/timing scores.

`source19-reference-audits.json` and `source19-per-reference.csv` independently
reconstruct the original delivered source19 reports without changing them.
`mono-onsets-107.csv/json` pairs every old mono reference: median/P95/maximum
45.979/74.229/96.604 ms are unchanged; two notes improve 2.667/5.333 ms, one quieter
re-pick is 2.667 ms slower and 104 are unchanged. Case medians alone had hidden a
large per-note delay in a rejected speculative latency path; that path is not
in the reviewed source.

`guitar-full` contains 55 new mode cases × 3 sample rates × 3 block sizes = 495
rows. Of these, 459 are fixed-note rows, all 459 references matched with no
false/missing/octave notes, premature releases, hanging voices or output
overflow. At 48 kHz / 64 samples, compared with the preserved source19 baseline:

| New controlled subset | Source 19 | Reviewed 0.2.1 |
| --- | ---: | ---: |
| Mono false octave NoteOns | 7 | 0 |
| Mono premature releases | 17 | 0 |
| Poly false octave NoteOns | 7 | 0 |
| Poly premature releases | 11 | 0 |

The original new-control octave failures are UP an octave. The user's exact
DOWN-octave failure has not been reproduced, and must not be claimed fixed
from these inputs. The eight extended recorded tails reuse four known guitar
recordings and already passed source19; the improvement above concerns the
synthetic decay/timbre/inharmonic controls. All new inputs were disclosed after
their first source19 baseline and are now seen regressions, not new blind
generalization evidence.

The 36 default-bend-disabled evaluator rows for four continuous curves are
retained as **legacy fixed-note diagnostics**. They are not an enabled-bend
accuracy score. `guitar-quality.txt` separately enables true mono bend on those
four 20-harmonic, phase-continuous guitar-like curves with 2/12-semitone ranges
and 64/257-sample blocks. It passes 93,927 checks: one anchored note, exact
block-invariant note/pitch decisions, complete curve coverage, P95 ≤ 35 cents,
dynamic maximum ≤ 75 cents, settled maximum ≤ 20 cents and bounded post-damping pitch
before NoteOff. Actual worst fast-bend P95/maximum are 18.535/25.828 cents;
settled maxima are below 1.52 cents. These are synthesized curves, not recorded
guitar bends. Separate mandatory processor/actual-VST3 tests exercise emitted
MIDI pitchwheel/RPN/channel output; this folder's direct-detector score does
not substitute for that delivery proof.

`preautomation-equivalence.json` compares all 691 raw MIDI streams and every
non-CPU metric with `../audio-update21-preautomation`: all are identical. The
reviewed change makes irrelevant bend automation stop resetting poly voices;
the separate automation regression exercises that state transition. CPU
timings differ between runs and retain VM/scheduler outliers. No E2-under-10-ms,
E3-under-5-ms, universal guitar accuracy or physical realtime guarantee is made.
