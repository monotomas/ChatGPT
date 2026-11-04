# Rejected polyphonic mains admission veto

This bounded research change is **rejected and unadopted**. It removes four pure-hum false note-ons, but also removes three previously detected wanted reference notes. Product `Source`, CMake, routing, fixtures, thresholds, and release packaging were not changed by this experiment. This archive contains frozen source snapshots and raw cloud CPU captures, not a release candidate or evidence of live musical performance.

## Question and implementation

The main polyphonic path conditions the input and uses an RMS floor, but its candidate admission was `strength[n] > 0`. A cold mains-only YIN hint could consequently bootstrap a phantom voice and prevent idle noise learning. Could the existing `NoiseConditioner::assess` positive mains-family evidence close this specific gap?

The candidate calls `assess` once per eligible analysis frame, using the actual valid YIN hint's continuous frequency and confidence. A cheap dispatch check uses the component's existing ±0.35 Hz per harmonic search neighborhood around the learned 50/60 Hz families. Proximity alone never rejects a candidate; `assess(...).mainsOnly` supplies the rejection evidence. Invalid YIN or unpitched audio does not reject polyphonic candidates.

When that shared decision says mains-only and no voice is held, the candidate skips new prediction/admission and clears pending pitch protection, allowing subsequent idle conditioning to learn. With held voices, existing voices continue and only new candidate-ons are vetoed. Automatic-input-off behavior and all numerical thresholds remain unchanged. See [candidate.patch](candidate.patch).

There is at most one **added** assessment per eligible hop, rather than one per candidate or voice. The pre-existing cold high-frequency physical-series protection can perform its own assessment, so the combined cold path can execute two assessments. No claim of one total assessment across the whole detector is made.

## Frozen baseline and measurement scope

`baseline/` is the exact main DSP and capture-driver snapshot used in this comparison; `candidate/` differs only by the archived patch to `PitchDetector.cpp`. All 18 baseline source and driver files (16 DSP CPP/H files and two capture drivers) match commit `ddceefbf9a49f2f265cb692863ab43ad4c2389a9` byte-for-byte. The per-file source manifests and build manifests additionally bind the exact measured and compiled inputs. Both isolated builds and all ten corpus captures report no source changes during measurement.

The key baseline file hashes are:

| File | SHA-256 |
| --- | --- |
| `PitchDetector.cpp` | `fb8fc9df50d5ebadcbc6027e9f94f085a5e52bad47577c8ccf5471a28bebd9b9` |
| `PolyphonicTracker.cpp` | `3d6536ffce7199f9a42d6bd5da2baec8f9bbd09d77028604d0ec16a262283983` |
| `NoiseConditioner.cpp` | `5f68d01a8cd4202dab066aac99b8d6067d055caa5e7bd9fb08766d5cde7862a1` |

The five previously seen regression sets contain 396 case configurations: 72 controlled readiness cases, 196 legacy cases (81 public, 69 previously named holdout, 46 confirmation), and 128 recorded-noise mixtures. The legacy names do not mean these are new blind evaluations. Every corpus capture completed successfully; readiness scoring exited 1 because acceptance checks failed. Corpus fixture manifest hashes and seals are in [fixture-provenance.json](fixture-provenance.json). No fixture or acceptance-policy thresholds were changed. The separate 16 recorded-hammer cases were not rerun for this rejected poly admission trial.

## Controlled cases: targeted benefit and wanted-note loss

| Metric over 72 configurations | Baseline | Candidate |
| --- | ---: | ---: |
| Expected reference notes | 167 | 167 |
| Matched | 120 | 119 |
| Missed | 47 | 48 |
| Extra note-ons | 98 | 78 |
| Note-ons in noise regions | 25 | 16 |
| Octave errors | 15 | 22 |
| Wrong notes before correct note | 9 | 0 |
| Early note-offs | 4 | 6 |
| Late note-offs | 40 | 31 |
| Hung / overflow | 0 / 0 | 0 / 0 |
| Cases passing the existing policy | 31 / 72 | 35 / 72 |

All four pure-hum poly configurations (`hum50_450`, `hum50_3600`, `hum60_450`, `hum60_3600`) lose their single false note-on. Including the corresponding mono cases, the stationary-mains family improves from 4/8 to 8/8 passing. Controlled mono raw event rows are identical (4,846 rows in either run).

The aggregate pass increase cannot justify adoption. Three previously matched intended references disappear:

| Case | Reference | Baseline correct note-on | Candidate |
| --- | --- | --- | --- |
| `music_handling_snr24_gain1_poly` | A3 / 57, starts 0.450 s | 57 at 0.4853125 s | No output notes |
| `music_handling_snr24_gain4_poly` | A3 / 57, starts 0.450 s | 57 at 0.4853125 s | No output notes |
| `quiet_new_note_after_decay_noise_poly` | Second note A3 / 57, starts 1.020 s | 57 at 1.0613125 s | First A2 / 45 detected; second note absent |

Two other reference notes are recovered, which is why the net matched count falls by only one. Several surviving cases also acquire additional octave errors, and both `music_handling_snr6` gain variants gain an early note-off. [comparison.json](comparison.json) records every changed controlled case. [evidence-summary.json](evidence-summary.json) contains the three lost references and complete note-on/off traces for them.

This demonstrates that the current mains-only component decision, when promoted to a global polyphonic admission veto with changed idle-learning history, is insufficiently selective. It does not establish which individual classifier feature or state transition causes every loss. Removing phantom voices can change later noise-learning and candidate history; a lower false-on total alone is not evidence of safe admission.

## Unchanged seen legacy and recorded-noise events

For all 196 legacy configurations and all 128 recorded-noise configurations, each baseline/candidate raw event file is byte-for-byte identical. Hash equality is recorded in `evidence-summary.json` and independently checked by `verify_archive.py`.

| Corpus | Configurations | Expected | Matched | Missed | False / extra ons | Octave errors |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| Legacy public | 81 | 117 | 114 | 3 | 2 | 1 |
| Legacy holdout, already seen | 69 | 99 | 88 | 11 | 12 | 3 |
| Legacy confirmation, already seen | 46 | 71 | 68 | 3 | 10 | 7 |
| Recorded-noise mixtures | 128 | 96 | 96 | 0 | 219 | 24 |

The recorded-noise set still has 67 noise-region ons, 48 early offs, 18 late offs, and only 37/128 passing configurations in either run. The unchanged result does not solve the user's noise and articulation concerns. The legacy totals retain 12 wrong-before-correct events and four early offs. No corpus has hung notes or overflow in these runs.

## Near-mains and additional-note positive controls

The deterministic control drivers contain source pitches, levels, and timings fixed before either variant is measured. The first sweep has 13 signals × two modes; the extended sweep has 19 signals × two modes, all at 48 kHz / 64 samples. The sweeps overlap, so they are 26 and 38 captures, not 64 independent musical scenarios. Full raw events, including pitch updates and offs, are byte-identical between variants for both sweeps.

The controls preserve legitimate G1, Bb1, B1, G2, F4, E5, and F5 acquisition. A held G1 moves from 48.9994 to 50 Hz between 0.25 and 0.45 s and remains held with identical pitch updates and release. Three polyphonic additions are actually acquired in both variants:

| Held source when A4 begins at 0.600 s | New A4 note-on, either variant |
| --- | ---: |
| F4 at 349.228 Hz | 0.6399792 s |
| F4 moving to 350 Hz | 0.6239792 s |
| E5 at 659.255 Hz | 0.6533125 s |

These controls provide specific positive evidence that the held-voice branch can preserve a near-mains voice and admit some new voices. They do not override the wanted-note losses in the controlled corpus.

The G1→50 Hz plus D2 controls expose an existing coverage failure: neither variant acquires the added D2 at any of eight amplitudes from 0.012 through 0.20, even though D2 begins at 0.600 s. These cases cannot support a claim that weak or strong additions beside G1 are safely preserved. The mono results for simultaneous high notes also contain wrong note choices and are retained without being counted as successful polyphonic acquisitions.

## CPU and performance limits

The frozen DSP was compiled with Debian g++ 14.2, C++17, `-O2 -DNDEBUG`, independently of the shared CMake build. Timing comes from this cloud Linux process, not Ableton, Windows, a live audio device, or the user's hardware. The extended controls record `std::clock` process CPU time as well as wall time around each process call. Across the 19 poly configurations, the median of their per-case CPU P99 values is 517 µs baseline and 722 µs candidate. This is a small non-randomized comparison and is not a causal microbenchmark of the new assessment alone.

At a 64/48000 ≈ 1,333 µs block deadline, observed wall-time misses total 10 baseline and 13 candidate over those poly captures. Maximum observed wall times are 4,383.67 and 2,455.72 µs respectively; process CPU maxima are 4,383 and 2,455 µs. Scheduling, concurrent cloud load, and existing detector costs limit interpretation. These data do not certify realtime safety or host latency. No broader rate/block matrix was attempted after the concrete musical regression rejected this guard.

## Archive and reproduction

- `baseline/` and `candidate/`: exact measured DSP and existing capture drivers.
- `candidate.patch`: complete experimental code delta.
- `build.py`, `run.py`: original isolated-build and corpus-run scripts. They use the original absolute repository path and create fresh report directories; copy the archive to scratch before rebuilding and preserve this archive unchanged.
- `positive_controls.cpp`, `positive_controls_v2.cpp`: complete deterministic signal generators and capture code.
- `build/*/build-manifest.json`: compiler, command lines, source hashes, and executable hashes. Executables and object files are deliberately excluded.
- `positive-capture-provenance.json`: post-capture reconciliation of the control driver, executable, and frozen source hashes. It is explicitly not presented as a contemporaneous capture manifest.
- `reports/`: both variants' complete raw events, timings, scoring cases, summaries, and measurement manifests.
- `archive-manifest.json`, `verify_archive.py`: integrity and key result checks.

From this directory, `python3 verify_archive.py` verifies all archived hashes, frozen build provenance, the exact unchanged raw files, positive-control raw equality, and the three newly missed reference outcomes. The recorded fixtures remain in the repository and are referenced by their manifest hashes; no external audio, environments, compiled binaries, or downloaded datasets are duplicated here.

The next defensible step is to preserve this negative result. No additional threshold tuning or variant was adopted to make this experiment pass.
