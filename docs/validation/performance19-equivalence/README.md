# Final output-preserving performance change

The baseline is source18 at commit
`aa6d56cc94f2727e03ebd088ed583ec5c8f5bda1`. The final candidate has byte-identical
MIDI event CSVs and identical every non-CPU case metric over all 196 now-known
development and regression cases. This is an equivalence check, not a new blind
accuracy test. Source hashes and comparison results are in `proof.json`.

The change caches immutable anchor evidence once per frame, applies the same
anchor veto before expensive residual projections, searches frequency-ordered
FFT peaks by bounded binary search, and skips prepared invalid harmonic bases
that are exactly zero. Other finite peak evidence retains the bounded linear
fallback. Selected-model-dependent active-octave support keeps its separate
per-selection-iteration cache and exemption; it is not cached across changes to
the selected models. No musical threshold or admission policy changed.

All 111,165 current pitch controls pass, including the acquired-octave,
independent damping, held-stiff and genuine-repick regressions. The final source
still requires its fresh native/VST3, sanitizer and Windows validation; earlier
private sanitizer results are not substituted for those runs.

The earlier private review/proof is retained with its original source17 labels.
It includes 48,000 peak-lookup comparisons and uncensored paired CPU observations.
P95 improved, while scheduling stalls remained, including a 47.6ms maximum.
Neither those earlier observations nor faster lookup establishes a hard callback
deadline, arbitrary-audio correctness or the physical 5/10ms objectives.
