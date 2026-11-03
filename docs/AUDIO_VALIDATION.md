# Independent audio-to-MIDI evaluation

The measurement corpus and evaluator were developed separately from the detector.
They do not use chord correction: pitch errors cannot be hidden by the allowed
note mask. Reference annotations, source audio and matching criteria remain
unchanged while the detector is improved.

## Corpus and licensing

`Tests/AudioFixtures/README.md` contains full source attribution and limitations.
There are 46 PCM waveforms and 81 case/mode combinations, including eight actual
edited acoustic guitar recordings from a pinned CC-BY-3.0 collection, independent
modal and Karplus–Strong plucks, and deliberately difficult polyphonic mixtures.
Recorded-note mixtures are artificial mixtures, not natural multi-string guitar
performances. These tests do not establish accuracy for all guitar techniques,
amplifiers, pickups, acoustic environments or live performances.

The original recording files, upstream license/attribution, source commit and
SHA256 values are retained. All derived PCM/annotation hashes are verified with:

```sh
python Tools/audio_validation_verify.py
```

The checked-in PCM fixtures and native C++ evaluator require no audio decoder,
network connection or numpy at execution time. Regeneration alone needs numpy
and FFmpeg, via `Tools/audio_validation_fixtures.py`.

## Separate sealed holdout

The development corpus above is distinct from
`Tests/AudioFixtures/holdout`. The holdout was prepared on 3 November 2026 before
the detector passed its first fixed 30-case quality gate. It contains 39 PCM
waveforms and 69 case/mode combinations: 18 independently seeded synthetic
plucks, nine new synthetic polyphonic voicings, and twelve recorded isolated
notes from electric and nylon/classical guitar timbres. Each single-note
waveform is evaluated in both mono and poly modes; the nine voicings are poly
cases. Its full rate/block matrix contains 621 runs.

These recordings come from the same pinned tonejs-instruments collection,
commit `622c2f1c32c8cfce4158ddc3eb26e518ddef37e5`, but use additional
timbres rather than the eight acoustic guitar notes in the development corpus.
The sample license is CC-BY-3.0. The upstream collection credits Karoryfer
Samples for electric guitar and Freesound contributor quartertone's
"classicalguitar-multisampled" for nylon guitar; Nicholaus P. Brosowsky compiled
and edited the collection. Original source paths, hashes, license and attribution
files are retained under `holdout/tonejs`. See `holdout/README.md` for complete
attribution and declared modifications.

Recorded labels were checked independently against sustained harmonic peak
frequencies, including a check that supported harmonic indices have no common
divisor greater than one. Those frequency checks are retained in the manifest;
the detector under test did not supply reference labels. Derived recordings
retain the original file attack/leading silence, use a common gain of .35, a
650ms crop, 50ms initial padding, 250ms terminal padding and a 4ms terminal
taper. The fixed absolute-amplitude >=.001 (-60dBFS) onset rule is the same rule as
the development corpus. This is waveform-onset timing, not mechanical pick
contact.

Synthetic holdout signals vary pluck position, partial balance, attack, detuning,
phase and spectral holes using seeds not used by the development corpus. The
reference waveforms and annotations were sealed before evaluating a frozen
candidate. The sealed `holdout/fixtures.json` SHA256 is:

```text
816faff8b414f579e2b107057113c0c34df8685abfc768d26c978197e6e7d692
```

Run integrity verification without decoding libraries or network access:

```sh
python Tools/audio_validation_holdout.py --verify
```

After recording a source freeze, reproduce the disclosed corpus as regression:

```sh
python Tools/audio_validation_run.py --holdout --full --evaluation-role seen-regression --output build/audio-holdout-regression
```

It applies the same fixed matching/scoring rules as the development evaluator
and records all detector/header/helper hashes, executable hash, compiler,
fixture-manifest hash and whether source files changed during the run. The
holdout is not used to relax or replace the fixed quality gate. Once its
outcomes have been revealed, any later detector tuning against them must be
identified as a follow-up using seen data, not another first unseen audit. The
first frozen-candidate report must remain available.

This holdout broadens timbre and synthetic-model coverage. It is still a small
controlled corpus of edited isolated recordings and synthetic mixtures. It does
not establish generalization to natural live six-string performances, pickup
noise, every instrument or arbitrary mixtures. It also includes neither a DAW
nor physical input/output hardware.

The first frozen iteration-14 audit is retained at
`validation/audio-iteration14-holdout-full/README.md`, with the original raw
reports and an independent Python scoring reconstruction. It found substantial
mono retrigger and polyphonic recall/false-note errors despite the separate
basic gate passing. Those first-blind observations must remain available after
any later regression fixes; subsequent reuse of this corpus is seen-data work.

## Second sealed confirmation

A second disjoint confirmation set was prepared after preserving the first
blind iteration-14 results. It has 25 waveforms and 46 case/mode combinations
(414 full-matrix runs): nine new licensed recorded notes across three guitar
timbres, independently randomized held tones/articulations/voicings, and
unpitched controls. It has not replaced or changed either previous corpus or
the frozen 30-case gate. `Tests/AudioFixtures/confirmation/README.md` declares
its provenance, limits, source-label sanitation and modifications;
`SEALED_SHA256.txt` records its fixed manifest identity.

The second set was generated and verified without executing the detector. Its
first evaluation followed the iteration-16 source freeze and is explicitly marked
`--evaluation-role first-blind`. Later reuse after examining its failures is
`seen-regression`. The wrapper records this distinction. Preparation of another
confirmation set cannot retroactively turn the first holdout into a passing
unseen audit or erase its recorded errors.

Its first frozen iteration-16 evaluation is retained at
`validation/audio-iteration16-confirmation-first/README.md`. It found three wrong
initial mono notes and additional/missing polyphonic pitches. All nine recorded
inputs were independently reconstructed sample-for-sample from the retained
sources, and every score was independently recomputed from emitted MIDI.
Later corrections use this disclosed set as seen-data regression; the first
blind result and its input audit remain unchanged.

## Final source-19 known-data observations

At 48 kHz / 64 samples, the final frozen implementation matches all **107 mono
reference notes** across the public development corpus and both disclosed
confirmation corpora, with no extra notes, wrong initial notes, octave errors,
early releases, hanging voices or output overflow in those mono cases.
These are known-data regression observations after the earlier blind findings
were used for corrections, not a new unseen-data generalization result.

| Corpus | Mono matched/reference | Poly matched/reference | Poly misses | Poly extra Note Ons | Poly early releases |
| --- | ---: | ---: | ---: | ---: | ---: |
| Public development, 81 cases | 50/50 | 64/67 | 3 | 1 | 1 |
| First disclosed holdout, 69 cases | 30/30 | 58/69 | 11 | 10 | 2 |
| Second disclosed confirmation, 46 cases | 27/27 | 41/44 | 3 | 9 | 0 |

Across these known cases, poly matches 163/180 reference notes with 17 misses,
20 additional Note Ons, 10 octave extras, 10 wrong-before-correct counts and
three early releases. No hanging voice or output overflow was observed. A wrong
note can precede several expected voices; wrong-before-correct is therefore a
per-reference count rather than a count of unique false output events.
The remaining failures concern overlapping octave/harmonic voices, weak voices,
and false additional pitches on some recorded single-note inputs. Poly mode
must not be described as error-free or as having universal six-string accuracy.

Final observed mono first-correct-useful response is about 43–49 ms median across
the three corpora, with a largest observed value of 96.604 ms in these 48 kHz
runs. The poly development corpus still contains late acquisitions up to about
549.6 ms. The first-holdout poly median/P95/maximum is
59.313/88.646/109.979 ms; second-confirmation poly is
53.979/86.979/101.979 ms. These exclude the driver, Ableton, receiver and output
hardware. Neither the E2-under-10-ms nor E3-under-5-ms objective is established;
the measured detector responses themselves exceed those goals.

The fresh native public evidence is retained under
`validation/audio-final-public`; the independent final seen regressions are
`validation/audio-final-holdout` and `validation/audio-final-confirmation`.
The latter two report source stability, independent score reconstruction and
byte-identical emitted events/non-CPU outcomes compared with iteration18.
The native public event file was independently checked against iteration18 as
well. CPU timing remains separately reported with all scheduler outliers and
callback deadline misses; these checks do not provide a general realtime deadline
guarantee. Actual Windows matrix/installer results must be read from the specific
installation package's `Validation` evidence, not inferred from a Linux run.

## Reproducible evaluator

On a machine with GCC or Clang, run:

```sh
python Tools/audio_validation_run.py --output docs/validation/audio-current
python Tools/audio_validation_run.py --output docs/validation/audio-full --full
```

The wrapper records exact detector/header/evaluator and fixture-manifest hashes,
compiler version, executable hash, run configuration and exit status. The first
command uses 48kHz / 64-sample callbacks. The full matrix has 44.1, 48 and 96kHz
rates and 32, 64 and 128-sample callbacks: 729 case/rate/block runs. Source audio
is resampled outside realtime processing using linear interpolation; this is a
controlled test conversion, not the plugin's realtime input path.

A separately built evaluator can also be supplied with `--executable PATH`.
The C++ runner itself is `Tools/audio_validation.cpp`; it links only the standalone
pitch detector. Its Windows entry point uses wide-character paths.

Output contains:

- `audio-cases.csv`: per-case recall/errors, response-time distributions, release
  behavior, CPU callback distributions and deadline misses.
- `audio-events.csv`: every emitted note event with actual sample position.
- `audio-summary.txt`: aggregate observed statistics, clearly marked as measurements.
- `measurement-manifest.json`: exact input/source provenance for that run.

## Scoring

A matched correct note must remain active with at least 20ms of overlap with the active reference note
(or its whole duration if shorter). The onset timestamp is the original emitted
Note On, but a brief correct guess cannot count as a useful low-latency result.
Earlier wrong notes are reported independently. Other counters include missed
reference notes, extra Note Ons, octave errors, premature releases (>10ms before
the defined note release), output still held after the terminal silence, and
fixed-buffer overflow. Monophonic and polyphonic results are kept separate.

The reference onset for real samples is the first absolute PCM amplitude >=.001
(-60dBFS) after the common documented gain. Original attacks/leading codec silence
remain present. This rule is not fitted per sample and is not mechanical pick-
contact timing. The onset of synthetic notes is known directly by construction.

The evaluator includes no audio driver, DAW, receiver synth or acoustic output.
Its sample-based detector response is not physical end-to-end latency. CPU
processing time is measured separately around each callback. Shared VM scheduler
stalls remain in maximum/deadline counts; no outliers are removed.

Successful tool execution means the measurement completed. It is not automatic
product acceptance, and must not be cited as satisfying the user's 5/10ms goal
when the actual quality/response measurements do not satisfy it.

## Three evidence levels and final product acceptance

The following results answer different questions and must remain distinct:

| Evidence | Scope | Meaning of success | What it does not establish |
|---|---|---|---|
| Frozen basic quality gate | 30 selected development cases at 48kHz / 64 samples | The explicit minimum accuracy/lifecycle criteria below passed | Accuracy on the other cases, all host configurations, or the user's 5/10ms target |
| Broad development matrix | All 81 development cases at three rates and three block sizes: 729 runs | Measurement completed with known source/input hashes; all errors remain reported | An automatic musical pass, unseen-data generalization, or final product acceptance |
| Sealed holdout audit | 69 separate cases over the same matrix: 621 runs | Independent observed behavior of the frozen candidate on previously unexamined fixtures | Universal guitar accuracy, a fresh unseen test after subsequent tuning, or physical latency compliance |

Passing the 30-case gate is a prerequisite for automatic packaging. It is not a
substitute for reviewing the broad matrix or holdout. A development snapshot
can pass all mechanical/host tests and the selected quality gate while still
emitting incorrect polyphonic notes elsewhere. Missed, false, octave and
release/retrigger errors from the broader evaluations must not be erased or
relabelled as passed just because a CTest runner returned zero.

Final product claims also need evidence for the actual Windows build and
installer, the compiled VST3's routing/state behavior and the relevant host
configuration. A JUCE test host is not an Ableton Live execution. Detector
response must be assessed together with note accuracy, rather than selecting
an early wrong guess as the latency result. Physical guitar-to-receiver-audio
latency includes input driver/converter, host routing, receiver attack and
output hardware; these standalone tests do not measure that chain.

The user's 5/10ms objectives remain separate acceptance requirements. The basic
quality gate's 60/100/120ms ceilings were chosen only to reject unusable or
unstable note extraction during development. They do not redefine acceptable
live latency. Final delivery documentation must identify which objectives have
measured support and which material limitations remain. No test description
should imply full product readiness from a selected-case pass alone.

## Development findings

The first causal implementation produced many polyphonic false Note Ons and
missed repeated attacks. Independent evaluation caught these before release.
Short-frame harmonics were incorrectly interpreted as additional fundamentals.
Using a complete causal observation window and a supported fundamental reduced
false notes, but increased response time and still missed weak/octave voices.
These are measured engineering tradeoffs, not approved changes to the user's
live-latency requirement. Final measurements must refer to the final source hash;
intermediate reports under `docs/validation/audio-*` are diagnostic history only.

## Frozen basic release quality gate

`Tools/audio_quality_gate.py` is a separate failing/passing release gate. Its
fixed policy is `Tests/AudioFixtures/quality_gate.json`, frozen on 3 November 2026
before the next algorithm iteration. It selects 30 cases without excluding the
known failing guitar/noise/retrigger/chord examples:

| Group | Cases | Maximum first useful detector response |
|---|---:|---:|
| Silence, click, low noise, white noise and colored noise, each mono/poly | 10 | No pitched Note On permitted |
| Synthetic mono strings, missing fundamental, vibrato and both repeat styles | 9 | 60ms |
| All eight recorded mono guitar notes | 8 | 100ms |
| Major triad, six-note open E minor and five-note strummed A minor | 3 | 120ms |

All reference notes must be matched with no extra Note On, earlier wrong note,
octave error, premature release, hanging voice or buffer overflow. Release P95
must be at most 100ms. These are **basic usefulness/accuracy ceilings**, not the
user's acceptable live response. In particular, passing 60/100/120ms checks must
never be described as meeting 5/10ms detector or end-to-end latency goals. Those
goals still require their own measured evidence.

Example CTest-style invocation:

```sh
python Tools/audio_quality_gate.py --evaluator path/to/FlysnapperAudioValidation --fixtures Tests/AudioFixtures --output build/audio-quality
```

The gate validates every source/PCM hash, full 81-row CSV schema and annotation
inventory, finite numbers, count/percentile consistency and fresh evaluator
outputs. Only the fixed 30 cases are subjected to the basic quality ceilings;
the remaining cases remain openly reported measurements. Python optimization
cannot disable these checks. An evaluator failure, malformed/stale output or
failed musical criterion returns nonzero. `Tests/test_audio_quality_gate.py`
contains 24 blackbox regression tests, including optimized-Python, NaN, tampered
policy ceilings, truncated CSV, failed evaluator and reused-success attacks.

Output under `build/audio-quality` includes `metrics.csv`, `events.csv`,
`measurements.txt`, `summary.json` and `report-manifest.json`. The summary always
keeps physical/live5/10ms certification false; passing this gate alone is not
full musical product acceptance. A failed gate must block automatic release
artifact publication.
