# Known-source harmonic continuity research

This is original AGPL-3.0-or-later research code, excluded from the product
build. It is **not musical readiness, MIDI acquisition, real technique, or
physical latency proof**. The product sources were not changed by this work.
The mono owner separately tests integration with actual detector arms and
complete note lifecycles.

The observer receives trailing audio and the frequency of an **already known
source**. The recorded-input probe supplies the nominal reference note, starts
60 ms after its labelled onset and never attempts acquisition. Nominal SFZ
pitch is not instantaneous acoustic frequency truth. Observability and gain
are diagnostic quantities, not a musician's intent.

## Findings and retained failures

| Variant | Observation | Status |
| --- | --- | --- |
| v1 | Partial support uses projected noise, not the loudest harmonic. An independent newest-cycle fit checks amplitude rises, preventing a transient edge in one band from masquerading as several harmonic gains. On 16 quiet and 32 real-note/noise controls, the declared diagnostic phase/energy predicates produce zero proposed attacks. | Useful mechanism; first genuine E2 repick has delayed energy proof, and whole-detector integration still fails. |
| v2 | Persisting every previously trusted phase through uncertain frames recovers genuine repick phase evidence, but noise can gradually poison a reference before it becomes uncertain. | Rejected unqualified persistence: 12 false phase-predicate frames in recorded mixtures. |
| v3 | A top-three harmonic common-gain ownership heuristic removes protective bands during real timbre changes. | Rejected: 280 false phase-predicate frames and a missed genuine repick. |
| v4 | Free-phase versus old-phase fit improvement alone can be large when a contaminated reference rejects the returning original tone. A large ratio can also come from a tiny residual. | Rejected as a standalone attack rule. |
| v5/v6 | Freeze the pre-attack reference only while an actual suspected excitation is pending. Advance carrier phase causally; retain independent partial references, detuning and energy baselines. v6 refits nonnegative amplitudes and permits one global polarity sign. | Opt-in research API; v5/v6 still produce false phase departure on a 25 ms smooth bend. |
| v7 | A bounded history of received frequency/time points supplies a matching past-only carrier trajectory, instead of applying one constant frequency to a changing-pitch frame. | With ideal supplied F0, frozen 25/80/220 ms bends have zero phase departures; actual noisy/lagged F0 integration remains unapproved. |

The recorded v1 experiment has at least two continuing partials on
12,166/12,480 mixture frames and 6,224/6,240 quiet frames. Those counts are
conditional observer evidence, **not** correctly emitted MIDI-note counts.
The diagnostic phase predicate requires at least three departures, a fitted
common reset over 0.2 radians and reset coherence over 0.85. Real plucks can
change partial phases independently, so that common-reset predicate is not
a general acceptance rule. The energy predicate likewise is a research
quantity and does not itself identify a new musical note.

In the representative A3 plus muted-string case, v1 retains strong first and
second partial continuity around the actual false MIDI retrigger at 1.04531 s.
Weaker phase features can be dominated by interference, including accidentally
aligned phase. Phase agreement alone does not establish source ownership.

## Frozen reference and timestamp contract

`freezeReference(true)` must occur at the real arm point **before** the new
waveform is accepted into the source reference. The caller resolves the
pending articulation and resets the observer on an accepted new note or
source stop. Freezing is not a note hold timeout; this class emits and holds
no MIDI notes. Continuing phase evidence alone does not prove audible energy
after physical damping.

`observeAt` accepts the actual measurement time associated with the supplied
frequency. Both that time and `lastSample` use analysis-rate sample indices.
The four-argument `observe` assumes frequency belongs to the newest sample.
An estimator using a historical window must provide its appropriate frequency
timestamp; neither timestamp transport nor an ideal-frequency test proves
that the real estimator has no lag or uncertainty.

v7 retains at most 64 frequency/time points, linearly interpolates received
measurements and extrapolates beyond the latest received point from the last
slope. It fits the current past-audio frame and integrates between actual
phase observation centres. It does not move output events backwards or consume
unreceived future frequency samples. Its ideal-F0 tests are deliberately more
informative than a real noisy detector, and cannot establish end-to-end success.

## Numerical and runtime checks

The old-source objective compares a free complex-harmonic fit with an
amplitude-refitted phase-constrained fit. Known per-band amplitudes are
nonnegative; one shared global polarity sign is allowed. Each NNLS candidate
uses at most 64 coordinate sweeps and checks KKT residuals. An unresolved
candidate supplies no positive excitation gain.

An independent exhaustive active-set calculation checks 48 combinations of
partial counts, known/unknown phases and frame sizes. v6 maximum objective
difference is `2.04e-17`; v7 is `1.95e-17`. These checks establish the linear
objective implementation, not nonlinear pitch accuracy or correct attacks.

The v7 frozen-reference synthetic checks retain common and independent phase
resets, reject a single-band contaminant, and avoid phase departures during
the three known-F0 bend durations. ASan/UBSan found no issue in those checks;
LeakSanitizer was disabled because it is unavailable under the managed tracing
environment. The bounded 64-point history and all fitting storage are fixed.

One-source warmed C++ `new` checks report zero allocations. Shared Linux CPU
P95 observer time ranges from about 21 to 400 microseconds over the tested
frequencies; one observed CPU call reaches 1.39 ms. This excludes the input
conditioner, actual pitch estimator, MIDI routing, host and other voices.
It is not a callback deadline guarantee.

## Reproduce

The archived observer sources and drivers need only a C++17 compiler and
Python's standard library. No model download, installation or product change
is performed. Existing recorded fixtures retain their original CC0 source,
author SFZ mapping and seals in `Tests/AudioFixtures/readiness/recorded_noise`.

```sh
python docs/validation/harmonic-continuity-research/run_recorded.py \
  --variant v1 --output /tmp/harmonic-continuity-v1
```

Use a new empty output directory for every run. The runner records exact
source, driver, fixture and executable hashes, raw per-frame CSVs and the
declared diagnostic counts. It refuses source changes during measurement.
The retained summaries and selected raw traces are compact; complete original
experimental CSVs also remain in `/tmp/flysnapper-harmonic-continuity/recorded-*`
in the development workspace and are reproducible with the archived sources.

`manifest.json` binds the archived text/source files. No compiled binary,
external audio asset or installer is included here. All variants are now
disclosed research/regression material, not unseen acceptance tests.
