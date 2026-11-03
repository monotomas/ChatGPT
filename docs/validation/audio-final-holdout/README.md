# Final source 19 — disclosed first-holdout regression

This quick 69-case run is **seen-data regression**, not another blind audit.
The first holdout's original blind 14 report remains unchanged. Configuration:
48 kHz / 64-sample callbacks. Source stayed frozen, evaluator completion was 0,
and both the run manifest and independent audit retain the seen-regression role.

Evaluator SHA256:
`971436cc4474563c849a3f5e41753253c400edcd61db842bae95badf03521502`.

| Mode | Matched/reference | Missed | Extra Note Ons | Octave extras | Wrong before correct | Early releases | Hung/overflow |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| Mono | 30/30 | 0 | 0 | 0 | 0 | 0 | 0/0 |
| Poly | 58/69 | 11 | 10 | 3 | 4 | 2 | 0/0 |

All 69 score/response/release rows independently reconstruct exactly from the
raw events and frozen annotations. All per-case non-CPU outcomes and the entire
emitted event file are **byte-identical to iteration 18**. The comparison proof
records both event hashes and all compared fields. Source hashes stayed stable.

Mono first correct useful response median/P95/maximum is
48.646/69.979/77.833 ms; poly is 59.313/88.646/109.979 ms. Remaining poly errors
are retained in `errors-by-case.csv`. These observations establish neither
general musical accuracy nor the user's physical 5/10 ms objective.

This single-configuration run does not assess cross-rate/block invariance.
Untrimmed CPU data remain in the report and are separate from output equivalence.
The final Windows package carries its own native Windows matrix/installer proof.
