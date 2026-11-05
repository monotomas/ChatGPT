# Rejected external-conditioner topology research

This is a bounded negative investigation, **not an approved plugin, installer,
or musical-readiness result**. Product source was not changed by these trials.
The candidate architecture instead under investigation is the original internal
conditioner followed by dynamics at the analysis rate. Nothing in this archive
approves that separate candidate either.

All signals here are explicitly constructed controls or the already disclosed
controlled readiness edition. There are no newly recorded guitar performances.
Every examined case is seen development/regression data. Source snapshots,
actual causal event times, extra/missing notes, failed scores and unsuccessful
variants are retained. No events are backdated and no curve delay is aligned
away.

## Why another conditioner was tried

Putting the new host-rate gate before the detector hid idle hum from its
existing stationary-line learner. Opening the gate then introduced previously
unobserved interference during a held source. A compressor-only ablation in
the separate initial-topology investigation had isolated this interaction.

The proposed alternative ran a raw host-rate `NoiseConditioner` before
`InputDynamics`, while the detector supplied its actual active/pending source
protection. Gate openness was not treated as continuous musical protection.
The raw observer therefore learned and subtracted in one unchanged gain
domain. It still failed concrete musical controls.

## Cold, untrained paired results

Each capture contains all72 original configurations at48kHz/64 samples. The
dated November5 scorer includes65 configurations:34 mono and31 static-poly.
Seven historical poly-bend configurations remain in the raw captures and are
explicitly outside the user's revised requirement; they are never counted as
passes. Absolute latency limits are removed by that dated policy, while
actual timestamps and conditional latency remain reported. Original data and
historical policy bytes were not changed.

| Frontend | Passed/65 | Mono/34 | Poly/31 | Matched/151 | Extra Ons | Early offs |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| Existing new host gate, without raw observer | 35 | 25 | 10 | 130 | 84 | 9 |
| Raw observer plus original internal subtraction | 34 | 24 | 10 | 130 | 90 | 13 |
| Raw observer; internal learner runs but its output is discarded | 37 | 26 | 11 | 130 | 92 | 9 |
| Raw observer; internal processing fully skipped | 37 | 26 | 11 | 132 | 112 | 12 |
| Full skip plus finite excitation pulse at a fresh gate-latch opening | 38 | 27 | 11 | 132 | 106 | 10 |

`cold-comparison.json` retains every changed pass outcome and all aggregate
counts. The underlying per-case reports and raw events retain errors that a
pass-count increase would conceal. Fixing gain1 quiet-note rearticulations
introduced gain8 G1 octave/release errors, handling-case errors or slide
release errors. **None of these five operating points is approved.**

CPU timings and deadline observations remain in the raw captures. These were
nonrandomized runs on a shared cloud machine, including concurrent build work;
they are not isolated runtime bounds or physical-interface latency evidence.

The discard-only method also has an evidence-domain problem: the learner's
hypothetical cleaned floor no longer describes the waveform delivered to the
detector. Full skip avoids that particular mismatch but does not solve the
observed musical failures.

The finite gate-opening pulse pauses learning for the component's existing
80ms excitation interval. It is a pulse, not permanent gate-open protection;
it still does not provide acceptable paired accuracy. Its small pass gain is
not a reason to adopt it.

## Actual cold-learning failure trace

The focused trace uses three unmodified original readiness recordings. In
`quiet_hum_31_gain8_mono`, the external observer was protected by a pending
pitch at0.1853s, before the wanted note starts at0.5000s. Its learned hum RMS
was only about0.000149, against approximately0.0008 raw hum. A partially
cancelled, gated near-mains waveform had become credible pending musical
evidence and interrupted learning.

For the gain1 counterpart, first protection occurred at0.5453s, after the
wanted note began, with learned hum RMS about0.000086 against approximately
0.0001 raw hum. The trace supports a concrete maturity/protection feedback
problem, not a universal claim that every gain8 error has exactly one cause.
See `runs/quiet-trace/` and the exact diagnostic driver in `variants/skip-inner/`.

## One explicitly trained workflow

The bounded trained trial used a different, fully disclosed protocol:

1. Three seconds of quiet input, with raw-observer musical protection off.
2. Seventeen seconds of representative constructed playing, with protection
   forced on and generated tracking muted. `InputDynamics` receives the raw
   signal for its actual calibration statistics.
3. At20s, apply a successful proposal, clear tracking and preserve the raw
   observer. The fresh test note begins at20.5s, after500ms of real quiet
   readaptation. This interval is not omitted from the source or timestamps.

Raw hum phase continues throughout. Calibration and test attacks use different
source seeds; all cases share the disclosed practice sequence. The12 PCM files
are each measured in mono and poly, yielding24 configurations, not24 independent
performances. This trial is the full-skip internal variant.

All24 calibrations succeed. All20 intended tonal targets are found, but that
total does not establish usable polyphony:

* All ten tonal mono configurations have exactly one On/Off, with no extras.
  This includes six quiet G1/E2/E3 gain-pair cases and four near-mains F4/E5
  cases. Actual first-correct decisions are11.98–49.31ms; releases are
  36.65–41.98ms after the actual source release.
* Both mono handling configurations emit no notes.
* Poly emits12 additional tonal Ons, including unwanted G1-to-octave43 and
  E2-to52 interpretations. Both handling configurations together emit14
  unwanted Ons. The G1 first release is approximately617ms too early.

The trained result demonstrates a bounded monophonic mechanism. It does
**not** approve this topology for both modes or justify concealing the cold
failures by starting the gate bypassed. Raw calibration statistics describe a
different, usually higher floor than the precleaned operating input; that
conservative threshold-domain distinction must remain explicit.

`trained-result-summary.json` contains every raw On/Off sequence and per-target
decision/release delta. Original capture manifests are retained under
`historical-metadata/trained/` without rewriting their absolute paths or hashes.

## Other bounded mechanisms

`poly-calibration/` preserves two36-case probes of calibration alone. The
second uses standard six-string Em/G/Am/D-F#/F/C-E voicings, eight positive
stiffness partials per source, independent phase/decay, detuning up to2.75c,
12ms strum spread, three tempos and three common gains. All27 cases at nominal
SNR12/24/36dB calibrate; all nine SNR6 cases reject with `NoPlaying` and zero
qualifying periodic windows. Nominal SNR uses a fixed reference RMS, not the
instantaneous decaying SNR. This does not measure MIDI transcription.

The first probe's two nonstandard voicings are retained as a prior hypothesis;
their results are not silently presented as ordinary guitar fingerings.

`host-withdrawal/` tests actual digital silence after learned50/60Hz hum at
44.1/48/96kHz. The last output above1e-8 amplitude occurs12.92–13.33ms after
cutoff; output is exactly zero after50ms. The full positive captures also end
with zero active voices and no overflow. These bounded checks do not prove
unlimited-duration safety or host/hardware latency.

## Layout, integrity and omissions

`variants/{base,discard-inner,skip-inner,gate-pulse,trained}` contain complete
measured standalone Pitch snapshots and their actual drivers. `base` drives
both the no-observer baseline and double-conditioner modes. It already
contains early November5 admission changes, so a repository commit alone is
not its identity. `source-inventory.json` binds the actual bytes. The MPE
adapter is retained but excluded from standalone builds because it is outside
this raw detector signal path.

Large CSV/JSON/text files are losslessly gzip-compressed. `compression.json`
binds both original and compressed bytes. Historical source manifests retain
their original `Source/...` paths and are mapped into the corresponding
`variants/<name>/` tree; original `/tmp` locations describe past runs, not
required future directory names. Compiler commands retain executable hashes
or locations where available; compiled executables are deliberately omitted.

The original controlled PCM remains in the repository. Positive-control and
trained-workflow PCM is omitted to avoid duplication. `omitted-artifacts.json`
records its hashes and sizes, and deterministic generators are supplied.
The trained generator reproduced all14 PCM/inventory/truth files byte for
byte, and the positive generator reproduced all seven files byte for byte.
`fixtures/trained/helper/generate.py` pins the original helper functions used
by the trained generator. No external datasets, weights or runtimes are added.

Run `python verify_archive.py` for complete archive inventory, compressed-byte,
source-binding and negative-result checks. The top manifest excludes only
itself. Do not rewrite captured whitespace or line endings.

To reproduce into a new scratch directory, with a C++17g++ and Python/NumPy:

```sh
python reproduce.py --variant skip-inner --suite controlled \
  --fixtures /workspace/ChatGPT/Tests/AudioFixtures/readiness \
  --output /tmp/new-rejected-cold-capture
python reproduce.py --variant trained --suite trained \
  --output /tmp/new-trained-capture
```

Controlled scoring is expected to exit1 because acoustic criteria fail.
Positive/trained completion exits do not constitute acoustic acceptance.
`scoring/` retains the exact dated scope adapter, unchanged original scorer and
policy used in these measurements. No Windows, Ableton or physical interface
execution was performed by this archive.

All five retained cold event/metric pairs were rescored with that archived
scorer; both original reports reproduced byte for byte and retained exit1.
`reproduction-checks.json` records these checks. Rebuilding can naturally
change timing measurements even when note events are identical.
The archived trained-workflow source was also rebuilt with `reproduce.py`;
its complete raw event file reproduced byte for byte. Timing measurements
were not required to be identical.
