# Polyphonic tracker research — 4 November 2026

This is a record of rejected experiments and an unfinished research backend. It is **not musical-readiness evidence or a release recommendation**. Mainline continuous polyphony is admitted only by the existing discrete detector; an oracle seed experiment tests tracking after a correct initial answer has already been supplied.

## Interface and ownership

`Source/Pitch/PolyphonicTracker.{h,cpp}` owns six bounded source identities (0–5), fractional absolute MIDI pitch, measured harmonic level/confidence, phase innovation, actual observation length, and release. A source identity is independent of its rounded note. `Settings.externalSeedMode=true` plus `queueAdmission(note,retrigger)` takes confirmed legacy Note Ons. Fixed-note legacy Offs cannot terminate a still-bending source. The caller emits MIDI and runs per-voice `ArticulationTracker`; this backend does not implement MPE routing.

All working arrays and optional fixed-frequency dictionaries are allocated in `prepare`. The audio callback uses bounded work and no allocation, locks or IO. `prepare(rate,false)` avoids duplicate dictionaries in an externally seeded backend. The optional self-seeding path was materially inferior to legacy admission and must not be treated as an approved replacement.

## Experiments and negative controls

| Experiment | Observation | Decision |
| --- | --- | --- |
| Free-frequency candidates from spectral peaks with joint free-phase fits | Public poly: 40/67 reference notes, 1,282 extra On events; callback P95 around 5.9 ms | Reject |
| Legacy seeds plus unconstrained short-window Gauss–Newton F0 refinement | Even stationary C/E/G triad produced 76 false On events; source estimates drifted | Reject |
| Independent-partial phase velocity with changing-window/carrier correction | Stationary triad 3/3 and zero extras; simple two-source bend P95 error about 9 cents | Useful local mechanism; insufficient acceptance |
| Same phase tracker, exact legacy external admissions, unconditioned known 196-case inventory | 160/180 poly references and 18 extra On events, versus unchanged legacy 163/180 and 20 extras | Still inferior recall; not ready |
| Hard shared-partial veto, open Em chord with individually bent upper string | E3 retained wrong pitch by roughly 181–187 cents with 8/12 physical partials; all three-partial E3/B3/E4 bend cases froze by 200 cents | Critical tracking failure; retained identity cannot imply reliable F0 |
| Accept new F0 only if full 512-sample stationary fit error improves | Short observation targets current pitch while full-frame fit targets an older mean; two-source bend froze and P95 error reached 155 cents | Reject blanket cost veto |

The initial fixed-frame acquisition also contained pre-onset zero samples. Trimming to actual new excitation prevented early multi-note ghosts. Faster acquisition cannot be counted by backdating events.

A scalar-versus-paired Gram calculation was independently checked over 30 cases, 1–6 voices, 128–512 samples, slight detuning and stiffness. Maximum coefficient difference was 1.34e-16. Computing the sine/cosine block from each oscillator pair once removes repeated transcendental calculations without changing the solved model.

`phaseObservable=false` and reduced per-voice confidence mark missing independent pitch evidence. During a short unsupported interval the source can retain identity; the next phase comparison must use the actual elapsed samples, including unsupported hops.

## Current research direction, not adopted proof

An isolated temporal-prior prototype transports each previously fitted complex partial coefficient into the current frame and carrier basis. The linear model penalizes departure from that causal amplitude/phase prediction. This prevents arbitrary reassignment of a held string's partials to another source. Shared partials then receive reduced weight rather than an unconditional veto. A longer joint phase observation is used for nearby roots that a short isolated observation cannot separate.

In oracle-seeded synthetic probes this removed the known open-Em upper-string bend deadlock for 3, 8 and 12 physical partials per source. Nearby 52/53 roots required the longer observation: short isolation regressed to almost 60-cent errors, while the longer observation reduced the static case below one cent and the stiff case to approximately 9-cent P95 / 18-cent maximum error. These are **previously seen controlled signals with correct initial source identities supplied**, not real-guitar transcription results. The prototype remains isolated under `/tmp/flysnapper-polyprobe/phase-prior19` until full admission/noise/articulation and independent controls pass. A separate admission/retargeting checkpoint21 preserved the public legacy result (64/67 plus one extra) with input conditioning disabled; it still could not acquire an actual simultaneous 45/64 three-partial dyad, so this is not a completed polyphonic solution.

A genuinely independent sealed eight-case optimistic tracking audit of prototype19 passed only **2/8** cases. Opposing bends, phase-reset neighbors, vibrato/reversal with changing observation lengths, and stiff strings remained inaccurate (per-voice P95 up to 71 cents and maxima above a semitone). A post-hoc fixed-512 observation diagnostic did not resolve these failures. The promising previously seen oracle results therefore do not generalize sufficiently; prototype19/21 remains unaccepted. These failures and exact input/policy/source seals are retained in `Tests/AudioFixtures/readiness/poly_tracking_v1`.

A follow-up past-only linear end-pitch prediction (prototype22) was also rejected: it passed 0/8 cases, amplified static and weak-tail phase noise, and did not repair vibrato/reversal errors. Predicting the current sample is not backdating, but an uncertain phase estimate cannot be made reliable by adding an unqualified velocity extrapolation. A future state estimator needs a coherent observation-time and uncertainty model, with excitation changes handled separately from frequency motion.

## Material unresolved problems

- Exact legacy admission is already too slow and inaccurate on difficult guitar mixtures; running two pipelines does not resolve this.
- A static harmonic overlap test can reject a legitimate newly strummed octave. A change hypothesis needs temporal excitation/phase evidence. Conversely, a new octave-up fret may leave only even partials of an old model: narrow retargeting must preserve a genuinely held lower string with a physical root or primitive odd partials.
- Model stiffness is present in the model representation but is not estimated in the live backend. Real string partials extend beyond the eight modeled harmonics; their interference must be included in uncertainty and false-source controls.
- The pitch estimator and articulation classifier need consistent observation timing. Full-frame fit confidence alone cannot identify a hammer-on or distinguish one from a bend.
- Six-voice backend CPU P95 was approximately 0.6–0.8 ms in CPU-clock probes, with outliers beyond the 1.333 ms budget of a 64-sample/48 kHz callback. This excludes the legacy admission pipeline and other plugin work. Shared-runner wall-clock spikes are retained separately, not averaged away.
- Readiness snapshot 16 passed only 23/72 controlled runs. Hum, squeak, weak guitar, bending, articulation, release and latency still failed. Input conditioning before confirmed admission contaminated the learned noise floor on rich guitar; conditioned and unconditioned comparisons must remain separate.
- Real continuous independently bending polyphonic guitar, DAW end-to-end latency, and full musical use remain unverified.

Temporary experiment source, raw CSVs and snapshots are preserved under `/tmp/flysnapper-polyprobe` and `/tmp/flysnapper-poly-audit`. They are not installation artifacts.
