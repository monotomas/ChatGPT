# Disclosed first-holdout regression — frozen iteration 18

This quick 69-case run is **seen-data regression**, not a new blind audit.
It reuses the first holdout disclosed at iteration 14. The original blind 14 and
blind 16 source reports remain unchanged.

Configuration: 48 kHz / 64-sample callbacks. Source stayed frozen throughout;
the manifest records `evaluation_role: seen-regression`, completion exit 0 and
`source_changed_during_measurement: false`. Evaluator SHA256:
`0c8b1c057286fde9ad87525926d5c89d161cdc5fe6c3fc8d619ba7065d9ebe10`.

| Mode | Matched/reference | Missed | Extra Note Ons | Octave extras | Wrong before correct | Early releases | Hung/overflow |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| Mono | 30/30 | 0 | 0 | 0 | 0 | 0 | 0/0 |
| Poly | 58/69 | 11 | 10 | 3 | 4 | 2 | 0/0 |

Every per-case musical score, response percentile and release percentile is
identical to iteration 17. The entire emitted `audio-events.csv` is also
**byte-identical**, including note identities, velocities and sample positions.
`comparison-with-17.json` records the compared fields and both event-file hashes.

First correct useful response median/P95/maximum remains
48.646/69.979/77.833 ms for mono and 59.313/88.646/109.979 ms for poly.
These are detector observations, not physical live-chain latency or evidence
of the user's 5/10 ms objective. Existing polyphonic errors remain errors.

Independent Python reconstruction agrees with all 69 score/response/release
rows. The audit explicitly marks cross-rate/block invariance unassessed in
this single-configuration run. CPU/scheduler measurements remain in the raw
report and are not part of the event-equivalence claim.
