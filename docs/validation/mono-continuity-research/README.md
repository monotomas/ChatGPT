# Rejected monophonic continuity investigation

No variant here is approved for the plugin or an installer. Production `Source/` was not changed. This is a bounded record of eight isolated experiments, including their genuine-note regressions and raw unsuccessful captures. All corpora are now seen regression data.

The useful finding is narrower than a new detector: an established note can remain present in two strong harmonic bands while a handling sound changes several weak bands and the global residual. Treating the changed weak projections as a new pluck causes false same-note retriggers; shortening the pitch-estimation window on that unproved hypothesis also causes release/reacquisition gaps. Positive source continuity reduced those gaps in V6. Requiring the original common phase-reset model then lost genuine independently phased repicks. V8 restores the genuine repicks with a frozen physical old-phase hypothesis, but also restores many false retriggers. Neither tradeoff is acceptable.

## Exact paired results

Only the mono subset is counted below. New recorded-noise data contains 64 mono cases/48 wanted notes; controlled data contains 34 mono cases/77 wanted notes. Captures use 48 kHz, 64-sample blocks, causal decision sample timestamps, the original unchanged score and policies. No onset backdating or fitted pitch-curve time alignment was used. `extra_note_ons` already includes noise-only Ons; the separate noise-only column is a subset, not an additional count.

| Frozen source | Real mono cases passed | Wanted notes matched | Total extra Ons (noise-only subset) | Premature / late releases | Controlled mono cases passed | Controlled notes matched | Controlled extras | Old public/holdout/confirmation mono refs matched |
|---|---:|---:|---:|---:|---:|---:|---:|---:|
| Current main checkpoint | 34/64 | 48/48 | 50 (2) | 28 / 0 | 29/34 | 77/77 | 14 | Not rerun here |
| V1: latched target proof, full tracking frame | 34/64 | 48/48 | 42 (2) | 28 / 0 | — | — | — | 102/107 |
| V2: positive partial continuity fallback | 42/64 | 48/48 | 24 (2) | 17 / 3 | — | — | — | — |
| V3: phase F0 replaces valid YIN | 46/64 | 48/48 | 24 (2) | 14 / 2 | 21/34 | 67/77 | 2 | 103/107 |
| V4: newest-cycle withdrawal gate | 46/64 | 48/48 | 24 (2) | 16 / 0 | — | — | — | — |
| V5: preserve valid YIN trajectory | 44/64 | 48/48 | 26 (2) | 18 / 0 | 27/34 | 77/77 | 1 | — |
| V6: abstain from uncertain phase-F0 update | 46/64 | 48/48 | 24 (2) | 16 / 0 | 29/34 | 77/77 | 4 | 104/107 |
| V7: frozen arm + signed old-phase fit | 38/64 | 48/48 | 42 (2) | 24 / 0 | — | — | — | — |
| V8 final: carrier history + nonnegative old-phase fit | 34/64 | 48/48 | 40 (2) | 28 / 0 | 29/34 | 77/77 | 8 | 107/107 |

A dash denotes an unperformed matrix entry. V6's three missing old notes are two `confirm_articulation_02_mono` repicks and one `confirm_articulation_03_mono` repick. V8 recovers those notes. Both keep the `hidden_recorded_09_mono` startup wrong note before its correct note; fresh-only physical-root guarding is a separate unmerged experiment. V8's PitchTests (111,165 checks) and GuitarQualityTests (93,967 checks) pass, while the stricter fixture errors above remain. Broad functional passes do not remove those failures.

The 34-case controlled comparison preserves all 48 fretted-slide and all ten hammer/pull reference notes in V6/V8. V3's unconditional phase-frequency replacement misses ten slide notes and creates ten late releases. V6/V8 still fail four music-plus-handling cases and the 80 ms bend pitch-error control. A reduction in extra notes has not solved the user's required behavior.

## Whole-source extra-event audit

`extra-audits/` reuses the original interval matching and classifies every unmatched mono interval into disjoint categories. Every remaining wrong pitch in V6/V8 occurs **after** the initial correct note. The score's zero `wrong_before_correct` therefore does not mean wrong-note suppression succeeded.

| Source | Same-time Off/On, same anchor | Same-anchor reacquisition after a gap | Wrong pitch during true note | Noise-only On | Total |
|---|---:|---:|---:|---:|---:|
| Current main | 10 | 34 | 4 | 2 | 50 |
| V6 | 4 | 16 | 2 | 2 | 24 |
| V8 | 14 | 22 | 2 | 2 | 40 |

The main same-anchor gaps range from 24.0 to 98.67 ms, V6 from 18.67 to 69.33 ms, and V8 from 18.67 to 96.0 ms. The two remaining V6/V8 wrong-pitch events are MIDI 38 during a held MIDI 57 mixed with recorded electric percussion at 6 dB nominal SNR (two gains). The 12 dB pair is suppressed, but the 6 dB pair is not.

For the independently audited acoustic MIDI-57/muted-string 12 dB case, main emits a false same-time Off/On at 1.0453125 s. V6 suppresses it. V8 emits a later false Off/On at 1.0693125 s. The lower-noise observer's known-F0 oracle mechanism results therefore do not transfer directly to actual detector quality. See the independent [waveform audit](../../../Tests/AudioFixtures/readiness/recorded_noise/annotation_audit/README.md) and [observer investigation](../harmonic-continuity-research/README.md). None of the labels or pass criteria was changed.

## Additional falsification

The final unchanged V6/V8 executables were also run through ten additional causal synthetic harmonic controls, with the generator retained under `scripts/`. These supplement the fixed corpora; they are not recorded-guitar evidence. At MIDI 40, a phase-continuous two-semitone bend over 25 ms and its return emit `40 → 42 → 40` in both variants. The 80/220 ms counterparts keep one anchor. MIDI 57 keeps one anchor for all three durations. Both variants retain the two- and twelve-semitone hammer/pulls and every 35 ms fretted-slide step. V8 retains three independent-phase same-anchor repicks, whereas V6 emits just the first. Raw sample times and all failures are under each variant's `falsification/` directory.

## Why this stops here

V8 uses the stable observer snapshot from `v7-carrier`: up to 64 causally received F0/time knots, matched carrier trajectory, a pre-arm frozen partial reference, and an old-phase least-squares hypothesis with nonnegative per-band amplitudes and one global polarity choice. Its bounded NNLS objective has a separate mathematical validation in the observer archive. This solves a mathematical subproblem with an accurate known source frequency; it does not establish that the detector supplied a trustworthy source frequency, phase reference, or source ownership.

The remaining architectural problem is a joint uncertainty model for source frequency/trajectory, historical phase and contamination. The actual YIN estimates have source-dependent observation windows and interference errors. The fitted phase reference can follow a contaminating harmonic before an arm; global residual/cooldown timing can arm long after the first physical disturbance. A later model-improvement score can then describe erroneous carrier/reference history as a new excitation. Genuine independently phased plucks also cannot be restricted to one common harmonic phase shift. V8's `phaseFitGainToResidualDb > 0` is a diagnostic comparison, not a calibrated probability or a measured background SNR. Raising/lowering that threshold or overriding all valid YIN estimates would repeat the observed conflicting failures.

A next architecture must preserve a trusted source with positive band evidence while exposing uncertainty about F0/carrier and band ownership, test actual detector-fed phase observations, distinguish abrupt excitation from a continuous trajectory, and withdraw promptly when the source is actually damped. It must pass the genuine repick/slide/hammer controls together with complete noise-mixture scoring before any main-source adoption. These experiments do not prove an implementable universally correct noise/intent discriminator. Recorded electric-percussion tails include some genuinely pitched ringing; their strict suppression labels remain unchanged and the semantic limitation remains documented.

## Reproduction and provenance

`variants/<name>/Source/Pitch/` contains the exact isolated source, including rejected implementations. The corresponding `realnoise/`, `controls/` and `regression/` folders retain raw events, whole-case reports, original console output, generated partial driver and source hashes. Some old regression commands also emitted poly rows; all table totals above explicitly filter mono and make no poly improvement claim. Binaries and duplicated PCM are excluded. Original fixture inventory, policy and seals are under `inputs/`; audio remains in the repository fixture directories. `matrix.json`, `provenance.json` and `SHA256SUMS` support independent review.

The retained scripts preserve their original `/workspace/ChatGPT` and `/tmp` paths. Run against the archived snapshot path and a fresh output directory, for example:

```sh
python docs/validation/mono-continuity-research/scripts/flysnapper_capture_mono.py \
  docs/validation/mono-continuity-research/variants/v8-final \
  Tests/AudioFixtures/readiness/recorded_noise /tmp/recheck-mono-noise
python docs/validation/mono-continuity-research/scripts/flysnapper_partial_score_resume.py \
  /tmp/recheck-mono-noise/PartialMonoCapture \
  Tests/AudioFixtures/readiness /tmp/recheck-mono-controls
python docs/validation/mono-continuity-research/scripts/flysnapper_mono_regression_resume.py \
  docs/validation/mono-continuity-research/variants/v8-final \
  /tmp/recheck-mono-old107 --units
python docs/validation/mono-continuity-research/scripts/flysnapper_mono_falsify_resume.py \
  /tmp/recheck-mono-noise/PartialMonoCapture /tmp/recheck-mono-falsification
```

The native captures are before MIDI/MPE processor routing, on a shared Linux cloud CPU. CPU/deadline observations are retained raw but are not hardware/host performance certification. No Windows, Ableton or physical end-to-end latency acceptance was performed by this investigation. The product remains blocked by the wider readiness failures.
