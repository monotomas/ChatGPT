# First blind second-confirmation audit — frozen iteration 16

This is the **first detector evaluation** of the second sealed confirmation
corpus. Its 25 waveforms / 46 case-mode combinations were prepared without
consulting detector outputs. The earlier iteration-14 blind audit remains
available separately; reusing its revealed cases is explicitly regression work.
Nothing in this report changes an annotation, fixture, or acceptance threshold.

The source was frozen before this run. `measurement-manifest.json` records
`evaluation_role: first-blind`, `source_changed_during_measurement: false`, and
evaluator completion exit 0. The native evaluator SHA256 is
`317bd153ec4ab00448f051378f2406dc172dc25e3a9ab37c198cdae7eddc8d7f`.
The sealed fixture-manifest SHA256 remains
`5baff69d15a05bac7500aef83cd9d2efcb73743aa2136ce041c8c56c5cfbbda0`.

## Observed outcomes

The full matrix covers 44.1/48/96 kHz and 32/64/128-sample callbacks: 414 runs.
The following musical counts are identical in all nine configurations.

| Mode | Matched/reference notes per configuration | Missed | Extra Note Ons | Octave extras | Wrong before correct | Early releases | Hung/overflow |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| Mono | 27/27 | 0 | 3 | 1 | 3 | 0 | 0/0 |
| Poly | 41/44 | 3 | 9 | 6 | 0 | 0 | 0/0 |

Across the whole matrix, mono matches 243/243 references but emits 27 extras;
poly matches 369/396 references with 27 misses and 81 extras. Matching every
reference in mono does not make its earlier wrong notes acceptable or invisible.

First correct useful response, measured from the fixed waveform annotation:

| Mode | Median | P95 | P99 / maximum |
| --- | ---: | ---: | ---: |
| Mono | 43.313 ms | 128.656 ms | 163.281 / 163.281 ms |
| Poly | 53.991 ms | 86.990 ms | 101.990 / 101.995 ms |

These are detector sample observations, not physical pick-contact or
driver/Ableton/receiver latency. They do not establish the user's E2-under-10-ms
or E3-under-5-ms objective.

## All musical failures

The event times below are fixture time at 48 kHz / 64 samples. Onset padding
and any original source silence remain part of the input; latency is measured
from the separate annotated acoustic onset, not from fixture time zero.

| Case | Expected MIDI | Observed error |
| --- | --- | --- |
| `confirm_recorded_03_mono`, acoustic C4 | 60 | Emits 32 at 130.646 ms, then correct 60 at 151.979 ms; useful response 85.396 ms |
| `confirm_recorded_08_mono`, nylon G#4 | 68 | Emits 56 at 141.313 ms, then correct 68 at 213.313 ms; useful response 163.271 ms |
| `confirm_recorded_09_mono`, nylon A4 | 69 | Emits 41 at 157.313 ms, then correct 69 at 178.646 ms; useful response 128.646 ms |
| `confirm_sustain_06_poly` | 81 | Adds octave 93 with the correct onset at 103.979 ms |
| `confirm_voicing_01_poly` | 40/47/52/56/62/64 | Omits 52 and 64; four other voices appear without extras |
| `confirm_voicing_03_poly` | 45/52/56/61/64 | Omits 64; four other voices appear without extras |
| `confirm_recorded_05_poly`, electric C4 | 60 | Adds 72 at 103.979 ms and 74 at 149.313 ms |
| `confirm_recorded_06_poly`, electric D#4 | 63 | Adds 69 with the correct onset at 103.979 ms |
| `confirm_recorded_08_poly`, nylon G#4 | 68 | Adds 56 at 122.646 ms and 44 at 125.313 ms |
| `confirm_recorded_09_poly`, nylon A4 | 69 | Adds 55/57 with the correct onset at 114.646 ms and 81 at 210.646 ms |

No sustained same-pitch false retrigger, early release, hanging voice, or output
overflow was observed in this set. The poly extras are additional pitches,
mostly octave-related, rather than repeated articulations of the correct pitch.
The two voicings that lose voices overlap in octave/harmonic structure.
These observations identify failure families; they are not a causal trace of
the detector's internal branch.

## Independent score and recording checks

`Tools/audio_validation_audit.py` independently reconstructed intervals and
reference matches from the raw emitted MIDI and frozen annotations. All 414
reported score/response/release rows agree with the C++ evaluator exactly.
`scoring-audit.json`, `errors-by-case.csv` and `false-onset-details.csv` preserve
the derived checks. Buffer size changes produce exactly identical sample
positions, note identities and velocities at each rate. Cross-rate counts,
matched identities and note ordering agree; the largest correct-response spread
is 0.023668 ms. Timing/velocity interpolation differences remain recorded.

All nine recorded WAVs were independently reconstructed from their retained
original Oggs using the declared transformations, with **zero differing PCM
samples** and identical onset annotations. Upstream source hashes and the corpus
seal also match. The three mono errors already have their true fundamental and
harmonic families in the early observed signal, alongside nonharmonic/body
components near 100–109 Hz. The false low notes are not the dominant resolved
fundamental. See `independent-recording-audit.md` for methods and limits.

The VM observed 19 mono and 1,890 poly callback deadline misses in this run;
CPU maxima were 18,492 and 26,981 microseconds respectively. Concurrent/shared-VM
scheduling affects these timings, and no outliers were removed. No general
realtime deadline guarantee follows from this measurement.

The frozen 30-case basic gate, the public development matrix, this blind audit,
and physical live latency are separate evidence. This audit is **not** a passing
product-acceptance result. Any later corrections informed by these cases must
be evaluated as seen-data regression, preserving this first result unchanged.
