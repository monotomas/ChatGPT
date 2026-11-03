# Disclosed first-holdout regression — frozen iteration 17

This quick 69-case run is **seen-data regression**, not another blind audit.
It reuses the first holdout after the iteration-14 findings were disclosed.
The original blind14 report and audit remain unchanged.

Configuration: 48 kHz / 64-sample callbacks. The measurement manifest records
`evaluation_role: seen-regression`, completion exit 0 and
`source_changed_during_measurement: false`. Evaluator SHA256:
`24f7028d7209aa3100c5c7c6b7c7ab52846c2e5b4457fe0cd1ade8a81d316f25`.

| Mode | Matched/reference | Missed | Extra Note Ons | Octave extras | Wrong before correct | Early releases | Hung/overflow |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| Mono | 30/30 | 0 | 0 | 0 | 0 | 0 | 0/0 |
| Poly | 58/69 | 11 | 10 | 3 | 4 | 2 | 0/0 |

All mono outcomes remain clean. Relative to iteration16, the only changed
musical score in this set is `hidden_poly_03_poly`: one early release is removed.
Poly recall and false-note counts otherwise remain unchanged, with substantial
errors still listed in `errors-by-case.csv` and the raw MIDI events.

First correct useful response median/P95/maximum is
48.646/69.979/77.833 ms for mono and 59.313/88.646/109.979 ms for poly.
These are detector observations relative to the waveform annotations, not
physical pick/driver/DAW/receiver latency or evidence of the 5/10 ms objective.

Independent Python reconstruction agrees with every score/response/release
field in all 69 rows. `scoring-audit.json` explicitly marks block/cross-rate
invariance unassessed in this single-configuration run. CPU maxima and callback
deadline misses remain in the measured report; no outliers were removed.
