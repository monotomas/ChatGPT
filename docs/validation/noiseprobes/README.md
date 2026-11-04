# Noise conditioning research probes

This archive preserves a failed cold-start priming experiment. It is excluded
from the plugin build. `primeMains` was removed from the product component;
the copy in `experimental/` exists only to reproduce the experiment.

These are component diagnostics using previously seen isolated recordings and
synthetic added noise. They are **not musical acceptance tests**, recordings of
real hum or fret noise, or evidence that Flysnapper is ready for release.

## Reproduce

Python 3 and a C++17 compiler are sufficient. No Python packages, downloads or
plugin installation are required. Run from any directory:

```sh
python3 /workspace/ChatGPT/docs/validation/noiseprobes/run.py \
    --output /tmp/flysnapper-noise-probes
```

The script compiles only the archived sources, writes generated controls,
CSV results and a SHA256 manifest under the output directory, and checks that
a missing input file fails with a nonzero exit code and no fabricated rows.
It does not build the plugin or modify audio fixtures. A smaller run is:

```sh
python3 docs/validation/noiseprobes/run.py \
    --mode prime_simultaneous --score 25cents \
    --output /tmp/flysnapper-noise-probe-simultaneous
```

`baseline/Pitch/YinEstimator.*` exactly matches commit
`c5b05337641b13ae259836f5346e7c1850b7382c`. The experimental conditioner is a
frozen research snapshot, independent of later product edits. The manifest
records the exact archived source and fixture hashes used by each run.

## Inputs and scenarios

The eight files in `fixtures.txt` are derived from edited isolated acoustic
guitar recordings already used by the project. Their CC-BY 3.0 attribution,
source commit and modifications are documented in the
[fixture README](../../../Tests/AudioFixtures/README.md).

The probe adds a synthetic 50.03 Hz mains comb with relative amplitudes
`1, .4, .2`, phases `0, .3, .7`, fundamental amplitude `.03`, and uniform
white noise with peak amplitude `.000025` and fixed random seed `1234567`.
Guitar amplitudes are separately multiplied by `.003`, `.01` and `.03`.
These three gains change guitar-to-noise ratio; they are not equal-SNR
common-gain controls.

| Scenario | Automatic idle time | Priming | Recorded leading silence |
| --- | ---: | --- | --- |
| `warmup` | 2000 ms | Disabled | Retained |
| `cold` | 0 ms | Disabled | Retained |
| `prime_before_pluck` | 0 ms | Enabled | Retained |
| `prime_simultaneous` | 0 ms | Enabled | Trimmed independently for every gain |
| `cold_simultaneous` | 0 ms | Disabled | Trimmed independently for every gain |

Trimming uses the existing reference waveform onset after the frozen DC/lowpass
frontend. That annotation is an amplitude-threshold onset, not a measurement of
mechanical pick contact. Trimming is a startup stress control, not a new live
performance recording.

Clean controls generate intended 49, 50 and 60 Hz sine waves and three-partial
tones, with a .9 ms attack and 1.2 s decay. They contain no added hum or noise.
Their truth pitches retain fractional MIDI values. Strict and explicitly
rescued single-partial admission policies are reported separately; a strict
policy requiring two harmonics cannot establish a pure sine by construction.
The four modes are `clean_tones_strict`, `clean_tones_prime_strict`,
`clean_tones_rescue` and `clean_tones_prime_rescue`. Rescue explicitly permits
confidence above .985, harmonic fraction above .4 and phase coherence above .95.

## Interpret the measurements

The primary pitch criterion is **25 cents**. Rounded-semitone scores are retained
only as a diagnostic: 50 Hz interference itself rounds to G1/MIDI 31 and can
incorrectly look like a successful G1 detection. They do not establish source
separation or pitch-bend accuracy.

`conditioned_correct_frames` counts pitch estimates, not emitted MIDI notes.
The counting interval omits the first 100 ms and last 50 ms of the annotated
note. Its denominator includes only valid estimator frames, so also inspect
cases without a correct admission and the separate latency fields.

`first_correct_admitted_ms` requires both admission and the selected pitch
criterion. `first_any_admitted_ms` may describe a wrong pitch. `-1` means no
matching admission. Timing is relative to the waveform annotation and excludes
the audio driver, host routing, receiver instrument and output hardware.

The archived probe uses a 512-sample, 12 kHz causal history and a 32-sample hop.
Its admission and 50 ms protection policy are preserved for this diagnostic;
they are not the current product policy. Ground truth affects scoring only.

`reconstruction_error_vs_added_noise_db` measures remaining noise plus signal
distortion against the known added noise energy; it is undefined/blank for
clean controls. `reconstruction_error_vs_wanted_db` also reveals alteration of
an intended noiseless tone. The reference is the desired waveform after the
same frozen frontend, so this measures the conditioner's additional effect.
Neither metric proves MIDI event quality. `mains_explained_frames` remains
available even after a source is held; `mains_only_frames` is the admission
flag, which held-source protection can override.

## Corrected diagnostic result

The reproducible snapshot gives these 25-cent results over the eight recordings
and three guitar gains. Counts are estimates, not MIDI events.

| Scenario | Correct/valid frames | Cases without a correct admission |
| --- | ---: | ---: |
| Idle learning before the note | 4746/4755 | 0/24 |
| Cold, leading silence retained | 0/4374 | 24/24 |
| Experimental prime before the pluck | 4740/4750 | 0/24 |
| Experimental prime, simultaneous onset | 231/2674 | 15/24 |
| No prime, simultaneous onset | 0/4658 | 24/24 |

The successful pre-pluck cases first admit a correct pitch after 53.5–80 ms.
The simultaneous-prime range of 56–128 ms covers only cases that ever obtained
a correct admission; it excludes the 15 failures.

No-hum controls further falsify general preservation of new intended tones.
With the explicit single-partial rescue, 49 Hz sine waves are preserved and
first admitted at 42.7 ms. An intended 50 Hz sine is first correctly admitted
at 714.7 ms without priming and 176 ms with experimental priming; at 60 Hz the
times are 640 ms and 96 ms. Reconstruction error relative to the desired tone
is about −2 dB without priming and +2.5–2.7 dB with priming, a large alteration.
The corresponding three-partial 50/60 Hz tones happen to enter earlier and
remain preserved in this snapshot. These controls do not support a universal
claim about every waveform at those frequencies.

The checked-in `corrected/` manifest records exact sources, fixtures, compiler
and result hashes. Re-running the command writes a new manifest under `/tmp`;
it does not replace the retained evidence. Generated-control inventories and
CSV rows include their temporary paths, so their byte hashes depend on the
chosen output directory. The generated waveform hashes and numeric scores
remain available for comparison.

## Failed hypotheses and historical evidence

Two seconds of idle learning, or priming before a later pluck, can reveal quiet
notes under the chosen synthetic hum. Those conditional successes do not
generalize to a quiet desired note starting with unknown interference.

Cold priming failed the simultaneous-onset controls. An unanchored intended
50/60 Hz tone can also resemble mains at startup; priming may alter that wanted
signal. For these reasons the experiment was removed from the product.

`historical/` retains the original CSV files and notes. It is not the output of
the corrected runner. In particular, `results-primed-trim.csv` mutated the truth
onset inside its gain loop: only the first gain was actually trimmed. Its
aggregate score and later-gain timings are invalid as simultaneous-onset
evidence. The corrected runner copies immutable onset/offset values for every
gain. Historical rounded G1 scores have the additional mains-confusion problem
described above. The old `first_accepted_ms=-.083` sentinel is also replaced by
an explicit `-1` and separate correct/any-admission fields.

No claim about recorded handling noise, real fret squeaks, polyphonic tracking,
note-off quality, retrigger accuracy or musical readiness follows from this
archive. Those require the independent acceptance corpora and actual product
event measurements.
