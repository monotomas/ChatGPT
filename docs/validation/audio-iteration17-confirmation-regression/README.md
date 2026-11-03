# Disclosed second-confirmation regression — frozen iteration 17

This quick 46-case run is **seen-data regression**. The second confirmation's
first blind result is preserved unchanged under
`audio-iteration16-confirmation-first`; its disclosed failures informed the
subsequent corrections. This run must not be represented as another unseen test.

Configuration: 48 kHz / 64-sample callbacks. The measurement manifest records
`evaluation_role: seen-regression`, completion exit 0 and
`source_changed_during_measurement: false`. Evaluator SHA256:
`24f7028d7209aa3100c5c7c6b7c7ab52846c2e5b4457fe0cd1ade8a81d316f25`.

| Mode | Matched/reference | Missed | Extra Note Ons | Octave extras | Wrong before correct | Early releases | Hung/overflow |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| Mono | 27/27 | 0 | 0 | 0 | 0 | 0 | 0/0 |
| Poly | 41/44 | 3 | 9 | 6 | 0 | 0 | 0/0 |

The three earlier wrong initial mono notes are absent. Their first correct useful
responses also improve:

| Input/case | Blind16 | Seen17 |
| --- | ---: | ---: |
| Acoustic C4, `confirm_recorded_03_mono` | 85.396 ms | 72.063 ms |
| Nylon G#4, `confirm_recorded_08_mono` | 163.271 ms | 96.604 ms |
| Nylon A4, `confirm_recorded_09_mono` | 128.646 ms | 93.979 ms |

No other musical score or response outcome changes in this set. Poly retains
the nine additional pitches and three omitted voices described in the first
blind16 report; these errors remain visible rather than being reclassified.

Aggregate first correct useful response median/P95/maximum is
43.313/93.979/96.604 ms for mono and 53.979/86.979/101.979 ms for poly.
These are detector observations, not a physical live chain or certification of
the user's E2-under-10-ms/E3-under-5-ms objective.

Independent Python reconstruction agrees with all score/response/release fields
in all 46 rows. `scoring-audit.json` retains the seen-regression role and marks
block/cross-rate invariance unassessed here. The source/fixture hashes, raw events,
all remaining error cases and untrimmed CPU measurements are retained alongside
this report. Matching every mono reference in these reused cases is a concrete
regression result, not universal musical product acceptance.
