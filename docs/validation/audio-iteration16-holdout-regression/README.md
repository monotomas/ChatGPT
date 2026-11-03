# Disclosed first-holdout regression — frozen iteration 16

This 69-case run reuses the first holdout **after its iteration-14 results were
disclosed and used for corrections**. It is explicitly
`evaluation_role: seen-regression`; it is not another unseen-data audit. The
original iteration-14 reports and audit remain unchanged.

Configuration: 48 kHz / 64-sample callbacks. Source stayed frozen throughout;
measurement completion exit 0. Evaluator SHA256:
`317bd153ec4ab00448f051378f2406dc172dc25e3a9ab37c198cdae7eddc8d7f`.

| Mode | Matched/reference | Missed | Extra Note Ons | Octave extras | Wrong before correct | Early releases | Hung/overflow |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| Mono | 30/30 | 0 | 0 | 0 | 0 | 0 | 0/0 |
| Poly | 58/69 | 11 | 10 | 3 | 4 | 3 | 0/0 |

Iteration 15 had mono 29/30 with four octave extras. Those remaining mono
errors are removed in this run; all 22 electric same-pitch false repeats fixed
in iteration 15 also remain absent. Poly improves from 57/69 and 14 extras to
58/69 and 10 extras, but still has significant errors on multi-voice and some
single recorded-note inputs.

First correct useful response median/P95/maximum: mono
48.646/69.979/77.833 ms; poly 59.313/88.646/109.979 ms. These are detector
observations, not physical driver/DAW/synth or pick-contact measurements.

Every remaining failing case is listed in `errors-by-case.csv`, with raw events
in `audio-events.csv`. Independent Python reconstruction agrees with all 69
C++ score/response/release rows. `scoring-audit.json` retains the seen-regression
role and explicitly marks block/cross-rate invariance unassessed in this quick
single-configuration run. CPU deadline misses and scheduler maxima remain in
the raw report; no outliers were removed.
