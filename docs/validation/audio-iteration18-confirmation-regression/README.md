# Disclosed second-confirmation regression — frozen iteration 18

This quick 46-case run is **seen-data regression**, reusing the second
confirmation after its first blind 16 results were disclosed. The original
blind 16 report and independent recording audit are retained unchanged.

Configuration: 48 kHz / 64-sample callbacks. Source stayed frozen throughout;
the manifest records `evaluation_role: seen-regression`, completion exit 0 and
`source_changed_during_measurement: false`. Evaluator SHA256:
`0c8b1c057286fde9ad87525926d5c89d161cdc5fe6c3fc8d619ba7065d9ebe10`.

| Mode | Matched/reference | Missed | Extra Note Ons | Octave extras | Wrong before correct | Early releases | Hung/overflow |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| Mono | 27/27 | 0 | 0 | 0 | 0 | 0 | 0/0 |
| Poly | 41/44 | 3 | 9 | 6 | 0 | 0 | 0/0 |

Every per-case musical score, response percentile and release percentile is
identical to iteration 17. The entire emitted `audio-events.csv` is also
**byte-identical**, including velocities and sample positions.
`comparison-with-17.json` binds the comparison to both event-file hashes.

First correct useful response median/P95/maximum remains
43.313/93.979/96.604 ms for mono and 53.979/86.979/101.979 ms for poly.
The previously corrected initial mono mistakes remain absent. Poly's nine
additional pitches and three omitted voices remain openly reported.

Independent Python reconstruction agrees with all 46 score/response/release
rows. The source/fixture hashes, raw events and untrimmed CPU measurements are
retained. Cross-rate/block invariance is unassessed in this quick run; none of
these results establish universal musical correctness or physical 5/10 ms latency.
