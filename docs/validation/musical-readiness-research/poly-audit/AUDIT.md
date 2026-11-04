# Independent polyphonic tracking audit

This audit uses isolated snapshots and probes under `/tmp/flysnapper-poly-audit` only. No project source or tests were edited. The module was deliberately unwired during the first snapshots; these are tracking-model measurements, not a ready plug-in or transcription acceptance test.

## Reproducibility and scoring limits

`snapshot`, `snapshot-optimized`, and `snapshot-widephase` contain exact source copies with adjacent SHA256 JSON inventories. Compiler/environment are recorded in `environment.json`. `probe.cpp` is an ordinary acquisition probe which was compiled but not used for the conclusions. `oracle*.cpp` deliberately inject known voice identities and integer initial F0 values into the isolated implementation to remove Note On acquisition as a confound. They generate causal, phase-continuous harmonic sources, with different partial phases, at 12kHz analysis rate and hop32. They do not claim to reproduce all guitar acoustics or to measure an Ableton callback.

Every frame trace and failed case is retained. `oracle-summary.csv` reports nearest-pitch matches within60 cents; **its pitch-error quantiles exclude unmatched pitches**, so they must not be mistaken for total pitch error. Upper-string conclusions use identity-specific `bent-identity-errors.json`, which correctly includes the full200-cent misses. The strummed fixture's generic extra-voice score includes a deliberate80ms truth warm-up grace and is not evidence of false MIDI onsets; its raw traces remain available.

## Main verified failure: shared-partial tracking deadlock

With known six voices40/47/52/55/59/64 (open Em), bend one of52,59,64 upward2 semitones over400ms while all others hold. Each source has eight independently phased partials with amplitude proportional to h^-1.35; no noise is needed.

In `snapshot-optimized`, all three bent identities remain at exactly52/59/64 through700-1050ms, while truth is54/61/66. Each has200-cent P95 and maximum error after the bend. There are255/318 assessed frames missing the correct bent pitch and retaining the old pitch.

The cause is observable in code: shared-partial rejection uses current **model** frequencies; upper-string harmonics1-4 overlap lower-string partials. Only harmonic5 remains independent among the permitted first five, while a pitch update requires two agreeing independent strong partials. The old F0 therefore cannot move and cannot escape the shared classification even when the true string bends away.

`snapshot-widephase` uses all available harmonics and a lower weak-partial gate, plus full512 phase observations when more than two voices are held. This fixes the eight-partial B3/E4 cases (post-bend errors at most1.7 and0.75 cents), but E3 still remains around52.13-52.20 instead of54 (187.34-cent P95 error). With only three audible partials per source, **all three upper-string cases still freeze exactly at the old pitch**,200 cents wrong. This is not an acquisition problem; perfect initial identity/Note On seeds do not solve it.

Artifacts: `optimized/oracle-six_bend_*.csv`, `widephase/oracle-six_bend_*.csv`, and each directory's `bent-identity-errors.json`.

## Coalescing adjacent voices and inharmonicity

With perfectly seeded stationary notes52 and53, `snapshot-optimized` drifts to52.2051 and52.5936 by220ms and stays there through1s despite confidence near0.98. A100-cent interval collapses to38.85 cents. Once sufficiently close, shared-partial vetoes prevent correction. P95 nearest-voice error is40.6 cents.

The widephase/lower-gate snapshot improves that ideal harmonic case to15.8-cent P95, but a mild stiff-string variant (B=.0003/.0006) regresses from zero bad frames to137/318 bad frames. Those stiffness values stretch the eighth partial approximately16/32 cents. The implementation has a stiffness field, but inspected production preparation paths use its default zero; it does not currently estimate the source stiffness.

Free sine/cosine coefficients make shared harmonics non-identifiable from one frame. At an exact coincident partial, ridge regression can distribute energy among source models according to basis weights instead of true source ownership. A second string an octave above another is especially ambiguous because its spectrum is a subset of the lower string's harmonic series. Static spectral fit alone cannot establish whether that second string was played; temporal/source priors or independent onset evidence are required. Lowering a partial-amplitude threshold cannot solve this mathematical ambiguity.

## Phase update audit

The changing-window correction has the appropriate sign and reference. A fitted coefficient's transported endpoint phase is approximately the true phase at the observation center plus the model carrier's half-window advance. Consecutive observation centers are separated by:

`effectiveHop = hop - (currentCount - previousCount)/2`.

The implementation's additional half-window carrier shift accounts for changing model frequency. There is no evident endpoint/center sign error. Independent low-note fractional-pitch probes switching observation lengths between256 and512 remain within0.55 cent in the harmonic stationary case. A low-note bend with varying length has similar error to a fixed length (approximately5.5 versus6.3 cents P95).

The estimator still observes past window centers. For a512-sample window at12kHz, that center is21.3ms behind the newest sample; three periods of low E are approximately438 samples, with an18.3ms center age. Additional smoothing adds response delay. Causal computation does not remove this statistical delay.

A separate code-level bug was reported to the owner: an unsupported but retained voice keeps its old phase observation, while the next phase comparison used only one hop. The owner subsequently added accumulated missing-observation time (`hop + state.absent`). Another valid option is to invalidate the phase reference during unsupported observations, then restart it on recovery. Any future skipped analyses must also pass the actual elapsed hop.

The owner is also adding a post-update joint residual-cost veto. The inspected earlier snapshots computed residual before applying `observation.frequency`, so a phase update could make the joint model worse and collapse voice separation without an acceptance check. This fix requires new benchmark evidence; it is not assumed successful here.

## Performance and exact optimization validation

Original joint Gram construction at six voices has96 columns and4656 lower-triangle entries; each entry independently evaluates two sum/difference Dirichlet sums. Roughly37k sin/cos evaluations per joint fit dominate avoidable work. Reusing those sums once for all four sine/cosine block combinations removes approximately4x that transcendental work even when bends invalidate caches.

The owner implemented that reuse. `gram-equivalence.cpp` independently compares optimized solved coefficients against the original scalar-cross implementation for30 combinations (1-6 voices;128/192/256/384/512 samples; detuned and stiff basis models). **Maximum coefficient difference:1.33357e-16.**

An oracle six-voice static phrase measured median analysis time about1.26ms before and0.707ms after this change, with optimized P95 about1.09ms in one run. These are shared-cloud wall times; scheduling/throttling causes large retained outliers, and later runs are not isolated timing certificates.

At48kHz/64samples the entire callback budget is1.333ms. Analysis occurs every32 samples at12kHz, or every128 input samples, but the callback which performs an analysis must still finish in1.333ms. Dividing its cost over two callbacks is not a deadline proof. These probes exclude the legacy Note On/TDP authority, conditioning, MIDI routing, and JUCE overhead. Adding the old authority and a second joint residual validation solve can consume the remaining margin.

Concrete exact-cost improvements available before changing architecture:

- Cache basis/means by frequency, observation count, and stiffness.
- Cache per-source pair Gram blocks; when one string bends, held-held blocks remain unchanged.
- Cache the complete factorization when all model frequencies and count remain unchanged.
- Reuse FFT/periodicity evidence from the existing discrete Note On authority in external-seed mode instead of computing duplicate independent FFT/YIN passes.
- Validate complete integrated callback costs and deadlines, including worst six-voice bend activity; retain scheduler outliers and distinguish wall time from active CPU time.

For accuracy, prefer transported complex-coefficient/phase priors or bounded frequency proposals accepted against joint fit and fresh prediction evidence. Unconditionally trusting shared-partial phase is unsafe; permanently vetoing it freezes valid bends. Full512 observations improve conditioning but increase observation-center age. These tradeoffs require explicit regression evidence on the retained adversarial cases.

## Final bounded retake: frozen checkpoint17

The final retake uses the owner's frozen `/tmp/flysnapper-polyprobe/integrated17/Pitch` source, copied into `snapshot-checkpoint17` with `checkpoint17-sha256.json`. It includes exact Gram reuse, residual reuse, lower partial/SNR thresholds, the missing-phase-elapsed correction, and `phaseObservable` with reduced confidence for unobservable voices. It contains **no blanket post-update residual veto**. The owner tested and rejected that veto after it froze an otherwise good two-source bend near57.45 (about155-cent P95 error): short-window phase and full512 stationary-fit objectives observe different time centers, so an unconditional full-window cost comparison can reject a valid present-time bend. That failed experiment must not be portrayed as a completed fix.

`oracle-checkpoint17.cpp` runs the same oracle identity tests with3,8,12 physical partials per source. All identity errors below concern the bent source after the bend completes,700-1050ms:

| Bent voice | 3 partials | 8 partials | 12 partials |
| --- | --- | --- | --- |
| E3,52→54 | 200 cents, frozen52 | 180.7 cents, frozen52.193 | 186.7 cents, frozen52.133 |
| B3,59→61 | 200 cents, frozen59 | maximum1.7 cents | maximum1.7 cents |
| E4,64→66 | 200 cents, frozen64 | maximum0.75 cent | maximum1.18 cents |

All failed upper-string cases have `phaseObservable=false` throughout the measured completed-bend interval, with reduced confidence. This honestly exposes missing observability; it does not implement the required pitch bend. Raw traces, confidence, phase-observability flags, wall times, active thread CPU times, and identity summaries are in `checkpoint17/`.

For adjacent stationary52/53, checkpoint17 P95/max error is18.2/32.7 cents. For the stiff-string variant it is24.3/39.0 cents, with no unmatched>60-cent frames in this retake. These improve earlier failures but still show material pitch bias with perfect initial seeds.

To separate scheduling noise, this retake also records `CLOCK_THREAD_CPUTIME_ID`. Six-voice backend P95 active CPU time is approximately0.63-0.81ms depending on the case; the measured maximum is2.776ms in the E3 twelve-partial case. These probes still exclude old discrete onset authority, input conditioning, MIDI/JUCE/host cost, and cold acquisition. Some backend-only calls already exceed the entire1.333ms callback budget. The cgroup permits4 CPU equivalents over5 affinity CPUs (`cpu.max=400000 100000`); its throttling statistics are saved in `cgroup-cpu.json`. Neither active CPU time nor wall time from this shared machine is a certified real-time bound.

The bounded audit is complete: it establishes exact optimization equivalence and a valid changing-window phase correction, while preserving concrete unresolved polyphonic pitch/observability and callback-budget failures. No source changes or installer were produced by this audit.
