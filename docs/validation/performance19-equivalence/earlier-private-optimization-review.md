# Private performance19 rebase against behavioral17

No shared source or fixtures were edited. Baseline is the exact frozen17 snapshot in `baseline17/`; candidate is `candidate18/` (the earlier directory label18 is retained, but the parent has reserved19 for this performance stage). `source17-snapshot.json` binds the captured source and test files. The ready patch is `performance19.patch`.

## Changes and correctness

- Cache independent-of-anchor physical support once per frame; its inputs remain constant during candidate selection.
- Move the same anchor rejection before residual correlations. Keep the active-upper exemption in that rejection. The existing selected-model-dependent active-upper cache is still reset for EVERY selection iteration; it is never promoted to a frame cache.
- Use bounded binary search only when the producer asserts frequency-ordered finite FFT peaks. The flag defaults false for other callers, retaining bounded linear search. Local maxima cannot occupy adjacent bins, and their interpolated offsets are capped at±0.5, so the actual producer's list remains strictly ordered.
- Skip projections for invalid prepared harmonics: their entire bases/cross-Gram rows are exactly zero, and projections start zero-initialized.

All 196 previously disclosed cases are now SEEN REGRESSIONS, not a new blind audit: main 81, first holdout 69 and former confirmation 46. For EACH set, the event CSV is byte-identical and every non-CPU per-case field is identical between baseline and candidate. All four combined ABBA runs also have byte-identical events and non-CPU metrics. No thresholds, musical policies, observation windows or accepted-candidate arithmetic changed.

48,000 randomized finite peak controls cover both novelty modes, negative novelty when filtering is disabled, threshold-adjacent frequencies, zero/one/two/full peak counts, and clamped out-of-range counts. Candidate/baseline hashes match, and sorted/fallback results match. Invalid NaN magnitude or required-novelty values are handled more conservatively; sanitized detector evidence never contains them.

The candidate passes all 108,908 MATCHED CURRENT Pitch controls and 4,327 YIN controls with AddressSanitizer, UndefinedBehaviorSanitizer and LeakSanitizer enabled (`detect_leaks=1`). Pitch controls include acquired recorded-octave continuity, independent upper damping under bright lower harmonics, range/endpoints and prepared-processing C++ allocation checks. No sanitizer was disabled. An independent read-only agent found no blocking equivalence/bounds issue.

## CPU observations

Configuration: 48kHz/64samples, all 196 cases in the same order. ABBA is baseline,candidate,candidate,baseline. The timer is steady-clock WALL processing time, including shared-VM scheduling stalls. No samples/outliers were removed. Compiler: GCC 14.2.0, C++17, -O2 -DNDEBUG.

| Poly run | Median µs | P95 µs | P99 µs | Maximum µs | Callback deadline misses |
|---|---:|---:|---:|---:|---:|
| Baseline0 |1.402|475.488|955.382|6778.099|331|
| Candidate1 |1.372|362.713|719.616|47593.412|144|
| Candidate2 |1.362|334.311|611.077|11010.010|99|
| Baseline3 |1.372|374.499|650.655|8204.431|111|

P95 is lower in both candidate runs than either baseline. The faster baseline at the end shows that system conditions changed; the candidate's first P99 exceeds that faster baseline, and one uncensored candidate maximum is 47.6ms. These measurements support the expected benefit of removing redundant work, but do NOT establish a hard callback ceiling, driver/DAW behavior, physical latency or the user's 5/10ms goals. Mono processing code is unchanged; its timing differences reflect measurement/system noise.

`paired-cpu.json` includes median/P95/P99/max, every deadline count and mean per-case P95/P99 for BOTH modes. `performance19-proof.json` combines that record, hashes, all equivalence checks, sanitizer evidence and original/joined fixture provenance.

## Integration

Do not apply during another source freeze. Root should first commit/release 17 and decide whether the behavioral trend 18 is adopted. If the target changes, rebase these three files and recheck events before publication. Never drop an active-upper exemption from the early veto or cache selected-model-dependent support across iterations. The known semantic limits of the underlying detector remain; performance equivalence does not turn its musical errors into passes.
