# Existing streaming polyphonic methods: source audit and private benchmark

Research date: 2026-11-04. No project source, dependency, installer or model
delivery was changed. External code/weights remain under `/tmp` only.

## Finding

**PitchPlease is a genuine compact streaming implementation worth considering
as a weak admission prior, but its published decoder is not an acceptable
replacement for Flysnapper.** It improves recall on our disclosed polyphonic
fixtures while adding many false notes and octaves. It has no per-voice
continuous-pitch output. The other two inspected projects do not offer the
required fast, complete, independently expressive tracker.

| Primary source | Verified implementation | Material constraint |
| --- | --- | --- |
| `markusstrasser/pitchplease` @ `790096ad20913aaa0e82151d5240098bb96be7d5` | Small Basic-Pitch-derived CQT/CNN port, streaming 22.05 kHz, hop256, AudioWorklet or pure JS/WASM kernels; shipped 507–558 KB models | About26.3 ms feature age before input resampling/delivery and probability decisions; 88 integer-key probabilities, no source identities/bends; many false voices in our tests |
| `johnhyde/ppd` @ `2add692181b4f45d631ba7232dafa0aeaee1e2de` | Current `analyze.cpp` computes/prints FFT spectra; 4096 FFT points with4× raw downsampling | Observation spans16384 input samples (341.3 ms at48 kHz/371.5 ms at44.1 kHz), source hop4096; no implemented MIDI/bend tracker, no top-level license found |
| `jaylmiller/polyphonic_track` @ `45fbea8ce362ad6b3ebfce82dab4bde798b12b87` | Pure Data4096-sample Hann FFT, overlap4; Python sparse dictionary coding after onset | Requires manual instrument-note learning; approximately92.9 ms FFT history at44.1 kHz and ten10-ms OSC messages before one classification; integer NoteOns/global reset, no continuing bend/source analysis; no license found |

These are code inspections, not performance tests of the latter two projects.
The `ppd` README itself says reliability, not speed, is its aim. The Python/Pd
README's statement that one FFT block is examined per onset omits the current
`len(current_note_fft)==10` gate in `tracking_server.py`; after that one
classification it does not infer again until the next onset resets the counter.
Its adjacent-note rejection also excludes candidates less than two semitones
from an already selected note. None of that meets the requested technique and
independent-bend behavior.

## PitchPlease causality and output audit

The repository and published npm3.1.0 copies of `incport.js`, `models.js` and
`tracker.js` are byte-identical. The pure JS kernel has no imports, eval,
filesystem, browser, network or subprocess access. It receives only weights and
chunks passed by the harness. Package installation/scripts and WASM were not
executed.

`incport.js` uses `SR=22050`, `HOP=256`, `WIN=43844`, nine CQT octaves and a
two-frame output offset (`L=2`). Feature normalization retains172 frames of
history, approximately two seconds. Unavailable right-hand CQT samples are
reflected from received history; convolution rows beyond the received feature
edge are zero-padded. This is a specially fine-tuned streaming approximation,
not unchanged offline Basic Pitch. It does not wait for two seconds of new
audio: the initial context is zero padded.

All real samples consumed by a call are already present in the supplied
prefix. The model still labels an older feature frame. Specifically,
`F=e/256+171`, `frame=F-2`, and
`t=(frame*256-43844)/22050 = e/22050-580/22050`. Thus its labelled feature time
is approximately26.304 ms behind the latest internal sample. The48→22.05 kHz
resampler additionally needs48 source samples (1 ms), plus delivery rounding.
The README's “23 ms lookahead” describes two hops, not that complete offset.
The benchmark uses actual availability at the end of each64-sample input chunk;
it never timestamps NoteOns at the older `t` or shifts a curve to fit truth.

The output decoder applies the unmodified published per-key thresholds:
fast0.7/0.42 and piano0.6/0.36. Every accepted key is retained; there is no
top-six limit. Keys are MIDI21–108, with one binary state per integer key.
This cannot represent independent unison voices, continuous bends or
hammer/slide articulation by itself. Internal contour features are not an
exposed or validated per-voice pitch estimator.

## License and asset provenance

Both repository `package.json` and npm3.1.0 metadata explicitly declare MIT.
The repository's package `files` list includes `engine/` and `models/`; the
published23-file package contains those models and has no asset-license
exception. The small tarball was read without installation and its SHA512
matches the registry integrity field. Each model's revision/header and SHA256
are retained.

No full LICENSE/NOTICE/copyright file exists in either the complete pinned Git
tree or published package; the README License heading is empty. Vendor metadata
identifies `pitchtest` training/export revisions, but the cited public upstream
README/license/source lookups return404. Package-wide MIT intent is explicit
enough for the authorized private research benchmark; redistribution notices,
upstream attribution and training/weight provenance still require resolution
before product vendoring. No external weights/kernel were added to the repo.

## Private seen-regression benchmark

Two models were run locally on all196 disclosed public/holdout/confirmation
cases and128 recorded-noise cases:324 cases from174 unique mono48-kHz WAVs per
model. Identical audio shared by mono/poly labels is evaluated once and scored
under both labels; the neural model itself has no mono selector. Each file is
followed by250 ms supplied silence. All88 probabilities and actual emission
times are retained in90-float rows (`featureTime`, `emitTime`, keys21–108), as
are all integer NoteOns/Offs. No source data was transmitted elsewhere.

One-to-one same-note interval matching requires20 ms useful overlap (or the
whole duration for shorter references), retains wrong-before-correct notes,
extra octaves, same-note retriggers and missing/early/late releases. Five manual
transcript tests pass, including an explicit check that emission time, rather
than feature time, determines onset delay. All fixture SHA256s still match.
This is an integer-note admission evaluation, not the stricter MPE/readiness
score, and its clean-case count must not be compared directly with37/128.

| Known196 scope | Model | Correct mono refs /107 | Extra mono Ons | Correct poly refs /180 | Extra poly Ons | Poly octave extras |
| --- | --- | ---: | ---: | ---: | ---: | ---: |
| Primary all88 keys | fast | 98 | 96 | 165 | 157 | 61 |
| Primary all88 keys | piano | 101 | 109 | 172 | 170 | 70 |
| Post-hoc shared MIDI28–100 | fast | 98 | 84 | 165 | 145 | 61 |
| Post-hoc shared MIDI28–100 | piano | 101 | 97 | 172 | 156 | 70 |

The shared-range scores apply Flysnapper's pre-existing28–100 range only after
capture. Every excluded event is retained separately. No within-range error,
threshold, desired voice count or nearest-voice rematching was changed.
The historical legacy comparison has163/180 correct poly references and20
extra Ons; the NN's small recall gain therefore comes with a large precision
loss.

The recorded-noise totals below are **per64 mode-labelled cases**; the mono and
poly labels share identical neural output. Both models find all48 true notes.

| Model/scope | Extras | Noise-only Ons | Octave extras | Same-note retriggers | Early releases | Releases >80 ms late | Matched onset median/P95 |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | --- |
| fast, all88 | 175 | 48 | 58 | 15 | 10 | 17 | 30 /42 ms |
| piano, all88 | 238 | 46 | 53 | 25 | 18 | 0 | 35.3 /48 ms |
| fast, shared28–100 | 152 | 25 | 58 | 15 | 10 | 17 | 30 /42 ms |
| piano, shared28–100 | 229 | 37 | 53 | 25 | 18 | 0 | 35.3 /48 ms |

For fast, the32 music-plus-contact-noise cases alone have80 extra Ons and35
octave extras; the16 quiet real plucks have47 extras. The original source's
percussive labels have the same semantic limitations as the retained corpus;
these totals do not prove every percussion tail is acoustically non-tonal.

The exact45+64 three-partial control demonstrates both opportunity and risk:
piano first accepts64 at36 ms and45 at60 ms, whereas fast accepts64 at117.3 ms.
Both also accept64 in a single45 strong-third counterexample. Fast delays the
octave hammer's57 until582.7 ms (182.7 ms after the change); piano until489.3 ms.
These generated smoke controls are diagnostic, not independent real guitar.

Reference pure-JS cost on this shared runner is approximately0.68/1.21 CPU
seconds per audio second for fast/piano, including cold bootstrap on174 short
streams. Some callbacks take about200 ms; roughly20k callbacks per model exceed
the1.333 ms64-sample budget. This is not native C++/SIMD performance and does
not contradict the author's separate Apple-M3 browser throughput claim. The
current JS reference cannot be placed directly in the plugin audio callback.

## Reproducibility and useful adaptation

All evidence is under `/tmp/flysnapper-causal-implementation`:

* `source-provenance.json`, `published-inventory.json`, model-header JSONs and
  `source-package-parity.json`: exact primary source and asset provenance.
* `bench/run.mjs`: audited-kernel offline harness; no installation or network.
* `bench/full-{fast,piano}.json`: immutable fixture/input inventories and hashes.
* `bench/full-{fast,piano}/`: raw events, raw frame probabilities, primary and
  range-limited scores, and every excluded range event.
* `benchmark-summary.json`, `audit-files-sha256.json`: condensed results/hashes.

The defensible next experiment would use integer NN probabilities as a weak
prior alongside independently supported acoustic source hypotheses, with
causal timing and all false-source controls retained. It must demonstrate a
precision/recall improvement as a complete hybrid; “NN agrees” is not evidence
that a strong overtone is another played string. A native port would require
bounded preallocated buffers, incremental/cached work and measured callback
deadlines. Continuous pitch, source identity, expression routing and technique
classification remain separate unsolved responsibilities. These results do
not approve a new installer.
