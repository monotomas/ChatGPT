# Final source 19 — disclosed second-confirmation regression

This quick 46-case run is **seen-data regression**. The second confirmation's
first blind 16 findings and input audit remain unchanged. Configuration:
48 kHz / 64-sample callbacks. Source stayed frozen, evaluator completion was 0,
and the run manifest/independent audit explicitly retain the seen-regression role.

Evaluator SHA256:
`971436cc4474563c849a3f5e41753253c400edcd61db842bae95badf03521502`.

| Mode | Matched/reference | Missed | Extra Note Ons | Octave extras | Wrong before correct | Early releases | Hung/overflow |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| Mono | 27/27 | 0 | 0 | 0 | 0 | 0 | 0/0 |
| Poly | 41/44 | 3 | 9 | 6 | 0 | 0 | 0/0 |

All 46 score/response/release rows independently reconstruct exactly. All per-case
non-CPU outcomes and the entire emitted event file are **byte-identical to
iteration 18**; `comparison-with-18.json` records the fields and event hashes.
The three former wrong initial mono notes remain absent. Poly's additional
pitches and omitted voices remain openly reported rather than relabelled.

Mono first correct useful response median/P95/maximum is
43.313/93.979/96.604 ms; poly is 53.979/86.979/101.979 ms. These are observations
on disclosed cases, not new unseen-data generalization or physical 5/10 ms latency.

Cross-rate/block invariance is unassessed in this quick run. CPU measurements
and all scheduler outliers remain separate from event equivalence. The final
Windows package must supply its own native matrix and installation evidence.
