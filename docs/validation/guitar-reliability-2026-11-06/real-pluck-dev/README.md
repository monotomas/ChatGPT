# Real-pluck DEV evidence and portable capture

This archive preserves the original **56-run** baseline of exact0.3.0 Pitch
source (`e235c8a`) and a separate portable-tool reproduction. It is evidence
about the detector before MIDI correction/rendering, not a new installer,
physical latency measurement or general musical acceptance.

The [immutable source edition](../../../../Tests/AudioFixtures/guitar_reliability_2026_11_06/README.md)
contains fourteen original CC0 electric-guitar WAVs and the predeclared7/7
development/reserved split. **Only the seven development recordings have been
run here.** Decoder/verifier access to metadata or numeric samples is not a
detector run. No recording, float conversion or executable is duplicated in
this archive. These isolated ordinary/staccato assets do not establish
continuous human slides, hammer/pull sequences or polyphonic performances.

## Captures

* `original-baseline/`: original ignored-workspace harness, exact18 Pitch
  CPP/header inputs, raw N/F/P events, nominal-anchor/core results and manifests,
  all retained byte for byte. Runtime executable is omitted; its original
  hash is retained.
* `original-effective-pitch/`: separate rawP audit. It reconstructs absolute
  pitch including P immediately before N and leaves the nominal results intact.
* `portable-baseline/` and `portable-effective-pitch/`: fresh execution of the
  new `Tools/guitar_reliability.py`, `guitar_reliability_capture.cpp` and
  `guitar_effective_pitch_audit.py`, using the frozen original source.
  These original v1 scripts/results remain unchanged.
* `portable-effective-pitch-v2/`: a subsequent **offline audit of the same
  portable capture**, after strengthening DEV-input validation. Before reporting
  reserved/exposure counts, the current helper verifies the pinned metadata
  edition, exact seven DEV records/four profiles/two contexts, TSV input paths,
  WAV/float identity declarations, before-compile/source/driver hashes, original
  event/metric/case hashes and independently fixed nominal/core bindings.
  `development-bindings.json` records this verification. It reads metadata and
  retained capture text, without loading audio or reexecuting a native binary.
  Both `cases.json` and `summary.json` remain byte-identical to the original
  v1 effective-pitch result; see `effective-pitch-v2-comparison.json`.
* `audit-v2-negative-checks/`: five mutations of copies of the **actual portable
  capture** are rejected before an audit/claim output is created: changed raw
  bytes, a changed core with rebound case hash, reserved record identities with
  rebound event/metric/case hashes, an incomplete55-case capture with rebound
  hashes, and a false metadata digest inserted into both manifests. Mutation
  source, descriptions and actual stderr logs are retained. These are offline
  input-validation checks, not new DSP measurements or release tests.
* `portable-comparison.json`: all56 portable raw event streams are
  **byte-identical** to the original (`events.csv` SHA256
  `7fc00b6d9999abb20a17ad85fc58e95a9776dda9d4b97c40d613560f3b559896`).
  All18 source inputs, nominal case metrics and effective-pitch cases/summary
  match. CPU timings are observed rather than deterministic: deadline misses
  were114 originally and30 in the portable repeat; both are retained.
* `portable-safety-checks.json`: rejects measurement/decoded output inside the
  immutable edition, audit output inside the original capture/edition,
  a reserved-data selector, modified DEV float bytes and a replaced pre-capture
  seal. None of these negative checks invokes the compiler or detector.

`retention.json` explains the archived subset without rewriting any original
manifest. `archive-manifest.json` inventories the archived files independently.
Historical absolute paths in the retained manifests are provenance, not
portable defaults.

## Measured limitations

The original nominal-anchor/core check predicate passes38/56. Acrossall
profiles there are6 nominal-anchor mismatches,17 additional nominal-note
Onsets and no octave Onsets in these seven samples. These are narrow output
metrics, **not a sound-level or musical readiness rate**. All profiles retain
some problems on the selected staccato samples; see the raw event lists and
`cases.json` rather than only the aggregate.

One bend/default/idle staccato-E2 run retains MIDI anchor41 while its absolute
pitch becomes40.x. Initial N41 is preceded by P40.51312637, then P40.3472023
after2.65ms. It must not be called an entirely missing audible E2: all56 runs
have some effective-pitch coverage of the positive core. The separate audit
finds77.914ms total effective core uncovered time (63.265ms silent and14.649ms
outside the author's exact nominal rounding bin), versus137.914ms uncovered
nominal-anchor time.

The fixed positive cores and activity cues were independently declared before
capture. They are not instantaneous physical pitch/contact/damping oracles.
First-On times relative to a file/activity cue remain descriptive. EOF/fades
are not used as physical NoteOff references. MIDI52 staccato generates2–4
Onsets per selected asset in all eight profiles; these are actual same-note
output retriggers, without inventing later physical repick/damping times.

## Reproduce or evaluate a development candidate

From the repository root, using only Python's standard library plus a C++17
GNU-style compiler:

```sh
python -B Tests/AudioFixtures/guitar_reliability_2026_11_06/verify.py
python -B Tests/AudioFixtures/guitar_reliability_2026_11_06/decode_exact.py --output build/guitar-decoded
python -B Tools/guitar_reliability.py --decoded build/guitar-decoded --output build/guitar-dev-current
python -B Tools/guitar_effective_pitch_audit.py --capture build/guitar-dev-current --output build/guitar-dev-current-pitch
```

The decoder writes generated floats outside the immutable edition. Capture
accepts only the seven DEV recordings, with no reserved-selection flag. It
uses original44100Hz numeric samples,64-sample causal blocks, mono plain/bend,
dynamics off/default and cold/350ms digital-idle contexts. It appends300ms of
digital silence for finite-output observation, with no physical damping claim.
It does not normalize, resample, fit thresholds or run a calibration capture.
The nominal note metadata is not supplied as a pitch hint to the detector.

For a private source copy, add `--source-root /path/containing/Source`.
For the exact historical baseline without changing the active checkout, use
`--source-root docs/validation/guitar-reliability-2026-11-06/real-pluck-dev/original-baseline/source`.
All source/driver/compiler/audio/metadata editions are bound in each fresh
measurement manifest. Compilation and capture are serial and do not touch the
shared native CMake/Ninja build cache. The portable reproduction was executed
on Linux; Windows execution is not asserted by this archive.
