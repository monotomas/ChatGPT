# First blind holdout audit — frozen detector iteration 14

This is the **first evaluation** of the sealed independent holdout. The detector
and fixtures were not tuned during this run. It is a measurement report, not a
passing product-acceptance statement. Original CSV/summary/manifest files are
preserved unchanged; their hashes are recorded in `scoring-audit.json`.

The fixture manifest remains sealed at
`816faff8b414f579e2b107057113c0c34df8685abfc768d26c978197e6e7d692`.
The detector executable SHA256 is
`ff698709dce8c281ef619a54998f113a53e51addfa6fb152545c391596faec77`.
`measurement-manifest.json` lists every source/helper/header hash and reports
`source_changed_during_measurement: false`. Compiler: GCC 14.2.0, C++17, `-O2`.

## Results

| Mode | Unique reference notes per configuration | Matched | Missed | Extra Note Ons | Octave errors | Wrong notes before first correct | Premature releases |
|---|---:|---:|---:|---:|---:|---:|---:|
| Mono | 30 | 29 | 1 | 26 | 4 | 1 | 6 |
| Poly | 69 | 57 | 12 | 14 | 6 | 6 | 5 |

These musical outcome counts are identical for all nine rate/block settings.
Totals over the full matrix are mono 261/270 matched with 234 extras, and poly
513/621 matched with 126 extras. The repeated configurations are **not additional
independent recordings**. There were no hanging voices or output-buffer overflow.

| Mode | First useful correct response median | P95 | P99 | Maximum |
|---|---:|---:|---:|---:|
| Mono | 48.646ms | 69.979ms | 77.846ms | 77.846ms |
| Poly | 59.320ms | 88.646ms | 109.979ms | 109.990ms |

These sample-based detector measurements exclude drivers, Live, receiver attack
and audio output. They do not meet or certify the requested physical 5/10ms
objectives. Earlier wrong notes and missed notes are counted separately and
cannot turn into passing low-latency events.

Processing-time measurements retained all scheduler stalls. Across the combined
matrix, mono callback median/P95/P99/max were 0.801/35.072/51.686/3328.576µs;
poly were 0.571/360.519/705.335/14550.485µs. There were 8 mono and 3491 poly
callback deadline misses in this shared VM. Most poly misses occurred at the
smallest 96kHz callbacks. These observations do not prove a dedicated Windows
workstation's capacity, but must not be hidden or described as a realtime guarantee.

## Dominant error mechanisms

Five electric isolated-note recordings produce 22 same-pitch extra Note Ons
per configuration, or 198/234 mono extras (84.6%). Every extra follows a Note Off
of the same pitch at the identical sample while the recorded tone continues.
Independent source decoding reproduces every derived PCM sample exactly; the
reference waveform has a stable pitch family, decaying amplitude and no obvious
renewed broadband attacks. The evidence supports false retriggering from
sustained changing timbre. See `independent-recording-audit.md` for the numerical
checks, source limitations and event times. It does not claim human ground truth
for every physical gesture.

The other four mono extras per configuration are octave errors in two synthetic
weak-fundamental cases. One starts with the correct pitch then changes an octave;
the other never emits its annotated fundamental. Their frequencies and partials
are defined by construction. No reference pitch was changed to accommodate the
algorithm's stronger-harmonic interpretation.

Polyphonic failures include missing voices in several multi-note voicings,
missing an octave voice, missing a weak higher note, and invented pitches from
ordinary harmonic/body-resonance energy. All twelve recorded single-note pitches
are eventually recalled in poly mode, but three recordings produce extra voices
(one electric and two nylon cases). Synthetic mixtures account for most missing
voices and some invented notes. No recorded mixture is represented as a natural
live multi-string performance.

## All failing cases

Counts below are **per rate/block configuration**; the same counts recur in all
nine settings. Raw events for every configuration are in `audio-events.csv`.

| Case | Mode | Matched/expected | Missed | Extra | Octave | Wrong before correct | Early Off |
|---|---|---:|---:|---:|---:|---:|---:|
| `hidden_modal_07_mono` | mono | 1/1 | 0 | 3 | 3 | 0 | 1 |
| `hidden_modal_07_poly` | poly | 1/1 | 0 | 2 | 1 | 0 | 1 |
| `hidden_modal_14_mono` | mono | 0/1 | 1 | 1 | 1 | 1 | 0 |
| `hidden_modal_14_poly` | poly | 0/1 | 1 | 1 | 1 | 1 | 0 |
| `hidden_modal_16_poly` | poly | 1/1 | 0 | 1 | 1 | 0 | 0 |
| `hidden_poly_01_poly` | poly | 5/6 | 1 | 0 | 0 | 0 | 0 |
| `hidden_poly_02_poly` | poly | 3/4 | 1 | 0 | 0 | 0 | 0 |
| `hidden_poly_03_poly` | poly | 3/6 | 3 | 3 | 0 | 3 | 1 |
| `hidden_poly_04_poly` | poly | 4/6 | 2 | 0 | 0 | 0 | 1 |
| `hidden_poly_05_poly` | poly | 4/5 | 1 | 0 | 0 | 0 | 1 |
| `hidden_poly_06_poly` | poly | 4/5 | 1 | 0 | 0 | 0 | 0 |
| `hidden_poly_08_poly` | poly | 2/3 | 1 | 0 | 0 | 0 | 0 |
| `hidden_poly_09_poly` | poly | 1/2 | 1 | 1 | 1 | 1 | 1 |
| `hidden_recorded_01_mono` | mono | 1/1 | 0 | 5 | 0 | 0 | 1 |
| `hidden_recorded_02_mono` | mono | 1/1 | 0 | 6 | 0 | 0 | 1 |
| `hidden_recorded_03_mono` | mono | 1/1 | 0 | 6 | 0 | 0 | 1 |
| `hidden_recorded_04_mono` | mono | 1/1 | 0 | 3 | 0 | 0 | 1 |
| `hidden_recorded_05_mono` | mono | 1/1 | 0 | 2 | 0 | 0 | 1 |
| `hidden_recorded_06_poly` | poly | 1/1 | 0 | 2 | 0 | 0 | 0 |
| `hidden_recorded_11_poly` | poly | 1/1 | 0 | 3 | 2 | 1 | 0 |
| `hidden_recorded_12_poly` | poly | 1/1 | 0 | 1 | 0 | 0 | 0 |

## Independent scoring and invariance checks

`Tools/audio_validation_audit.py` independently reconstructs note lifecycles and
reference matching in Python from the raw events and sealed annotations. All
621 rows' counts and latency/release quantiles match the C++ report exactly:
**zero scoring discrepancies**. The source-to-PCM audit independently confirms
the single-note annotations' source assumptions and controlled cutoff timings.

At each sample rate, changing callback size between 32/64/128 samples produces
identical note order, velocity and absolute sample positions for every case.
Across 44.1/48/96kHz, recall identities and all musical error counts are identical.
Some event times and velocities vary: the largest correct-response spread is
2.668ms; an incorrect octave ghost varies by 15.990ms. One polyphonic voicing
emits a correct voice one analysis frame earlier at 96kHz, changing its order
relative to another simultaneous voice. Thus rate invariance of outcome counts
does not mean bit-identical event timing at all rates. `scoring-audit.json`
records these differences explicitly.

## Interpretation

The selected 30-case basic quality gate and mechanical/host tests can pass while
this independent audit still exposes musical errors. These findings are not
acceptable evidence of broad, accurate polyphonic transcription or universal
live-guitar readiness. The original first-blind results remain part of the
record. Any subsequent change using these findings is holdout-informed regression
work, and must not call the same data unseen again.

The corpus is small and controlled. The edited isolated samples have no human
pick-gesture or physical-release annotations. Synthetic Note On/Off timing and
our controlled recorded cutoffs are known, but physical end-to-end latency and
natural live-guitar transcription are not measured here.
