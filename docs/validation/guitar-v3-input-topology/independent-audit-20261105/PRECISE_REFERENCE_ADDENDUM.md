# Precise-reference addendum

This is a separately identified later result. The original `REPORT.md`,
21-miss raw-reference finding, disabled-baseline run and all earlier raw files
remain unchanged. The precise-reference snapshot restores the already existing
bounded quarter-tone short-lag refinement before its raw estimate replaces a
fresh processed estimate. This audit does not introduce or edit that algorithm.

All17 bound source/driver files verify against the final snapshot manifest.
Its PitchDetector.cpp SHA-256 is
`89e0ab388e9050cbba016ef91c75e0efd4eb1e1a5cb5ed562855416182ccff06`.
Both controlled and recorded-noise `events.csv` files are **byte-identical**
to shared protection. Therefore the original duration/release audit applies
without alteration, including the documented late-tail limitations. The
precision correction does not erase those acoustic failures.

## Detuning: the six new high-edge misses are fixed

The retained final876-row CSV is compared one-to-one with the earlier fully
bound dynamics-disabled baseline. Both now have861 strict passes and the same
15 preexisting near-mains misses. Final output has zero new misses, wrong
note-ons or active voices after supplied silence. The baseline and final share
exactly the same configuration keys; no failed row is omitted.

All six recovered100+40cent cases(pure/bright at three rates) now emit the
correct On at the **same sample as the disabled baseline**:

| Rate | On sample, both spectra | Decision time |
|---|---:|---:|
|44.1kHz|470|10.6576ms|
|48kHz|511|10.6458ms|
|96kHz|1023|10.6563ms|

Matched timing now includes all861 configurations acquired by both paths:
817 are identical,21 slower and23 faster. Changes span−2.68 to+53.33ms.
The long added delays still occur at low notes28–30; most faster decisions
are about one analysis hop(2.67ms). These are conditional causal detector
times; the15 shared misses remain explicitly listed. The earlier result of
45 slower configurations and six new misses belongs to the preceding
raw-reference implementation and is preserved in the original report.

## Repicks: all six configurations preserve onset pitch and release

The final mono repick trace covers44.1/48/96kHz ×bend ranges2/12. Each of the
six configurations has exactly three correct57 Note Ons and three matching
Offs, with no overlap or unclosed note in the raw stream. Every On is immediately
preceded by a same-sample pitch event for anchor57. The pitch is the observed
absolute pitch rather than a centered57 reset; maximum onset deviation from
the source57.3 is11.91cents(first attacks), and later attacks are much closer.
All raw On/Off/P values are retained in `precise-reference-repick.csv`.

These are detector pitch events, not proof of MIDI-wheel serialization through
the full processor or Ableton. Separate processor/VST3 tests provide that layer.

## Source and proof boundaries

The final CSVs are retained existing captures, independently inspected here;
this addendum does not pretend they were newly rerun. Source, driver, executable
and raw hashes are visible in `precise-reference-addendum.json` and the copied
source manifest. Complete per-case detuning timing and all six repick event
checks are retained there. New raw copies preserve the original bytes.

**The live main source includes additional lifecycle/helper changes not present
in this frozen snapshot.** At this audit, main differs in PitchDetector.cpp,
PitchDetector.h and InputDynamics.h. This evidence therefore validates the
frozen precision/signal-path behavior, not those added lifecycle/calibration
paths or an exact full-product build. Main native/integration validation remains
separate. No source files were edited by this audit agent, and no Windows,
Ableton or physical-interface acceptance is claimed.

`manifest-before-precise-reference.json` preserves the prior audit manifest
byte-for-byte. The refreshed `manifest.json` binds both the unchanged original
evidence and these additional records, keeping the negative and corrected
snapshots distinct.
