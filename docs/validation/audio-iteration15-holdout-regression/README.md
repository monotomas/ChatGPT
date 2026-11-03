# Iteration 15: previously disclosed holdout regression

This is a **seen-data regression**, not a second blind evaluation. The first
genuinely blind iteration-14 audit disclosed concrete failures in these same
recordings. Iteration 15 then changed the mono note lifecycle and guarded a poly
single-voice repick path. All original iteration-14 reports and annotations remain
unchanged. The separate second confirmation corpus has not been evaluated.

Command:

```sh
python Tools/audio_validation_run.py --holdout --evaluation-role seen-regression --output docs/validation/audio-iteration15-holdout-regression
```

The 69 cases use 48 kHz and 64-sample blocks. Fixture manifest SHA-256 is
`816faff8b414f579e2b107057113c0c34df8685abfc768d26c978197e6e7d692`.
The compiled evaluator SHA-256 is
`667ef9c69c1e7d9020359bccadf8beac3efa7b7e97bc50be144e56826237c694`;
all source hashes are in `measurement-manifest.json`. The source remained
unchanged during compilation and measurement. GCC 14.2 compiled C++17 with
`-O2 -DNDEBUG`.

## Musical results

The comparison uses only the iteration-14 rows with the same 48 kHz / 64-sample
configuration; the nine-configuration blind report is not compared against this
single-configuration regression as an aggregate.

| Mode / iteration | Expected | Matched | Missed | False note-ons | Octave errors | Wrong before correct | Premature offs |
|---|---:|---:|---:|---:|---:|---:|---:|
| Mono 14 | 30 | 29 | 1 | 26 | 4 | 1 | 6 |
| Mono 15 | 30 | 29 | 1 | 4 | 4 | 1 | 1 |
| Poly 14 | 69 | 57 | 12 | 14 | 6 | 6 | 5 |
| Poly 15 | 69 | 57 | 12 | 14 | 6 | 6 | 5 |

All 22 false same-pitch mono repeats in recorded cases 01–05 disappeared; each
of those five cases also lost its premature off. Their first correct detections
were retained. These are the five sustained electric samples independently
examined in the [original recording audit](../audio-iteration14-holdout-full/independent-recording-audit.md).
No other musical scoring count changed. There were no hanging notes or output
buffer overflows in either mode.

Remaining errors are listed below. Each count is for this one configuration.
Octave errors are a subset of false note-ons, not additional events.

| Case | Mode | Matched / expected | False note-ons | Octave | Wrong before correct | Premature offs |
|---|---|---:|---:|---:|---:|---:|
| hidden_modal_07 | mono | 1 / 1 | 3 | 3 | 0 | 1 |
| hidden_modal_14 | mono | 0 / 1 | 1 | 1 | 1 | 0 |
| hidden_modal_07 | poly | 1 / 1 | 2 | 1 | 0 | 1 |
| hidden_modal_14 | poly | 0 / 1 | 1 | 1 | 1 | 0 |
| hidden_modal_16 | poly | 1 / 1 | 1 | 1 | 0 | 0 |
| hidden_poly_01 | poly | 5 / 6 | 0 | 0 | 0 | 0 |
| hidden_poly_02 | poly | 3 / 4 | 0 | 0 | 0 | 0 |
| hidden_poly_03 | poly | 3 / 6 | 3 | 0 | 3 | 1 |
| hidden_poly_04 | poly | 4 / 6 | 0 | 0 | 0 | 1 |
| hidden_poly_05 | poly | 4 / 5 | 0 | 0 | 0 | 1 |
| hidden_poly_06 | poly | 4 / 5 | 0 | 0 | 0 | 0 |
| hidden_poly_08 | poly | 2 / 3 | 0 | 0 | 0 | 0 |
| hidden_poly_09 | poly | 1 / 2 | 1 | 1 | 1 | 1 |
| hidden_recorded_06 | poly | 1 / 1 | 2 | 0 | 0 | 0 |
| hidden_recorded_11 | poly | 1 / 1 | 3 | 2 | 1 | 0 |
| hidden_recorded_12 | poly | 1 / 1 | 1 | 0 | 0 | 0 |

The remaining mono failures are two weak-fundamental synthetic plucks: one
eventually acquires an upper octave during a sustained reference note, and one
emits the upper octave without a useful correct note. Poly failures retain the
earlier weak/shared-partial voice omissions and overtone/body-resonance ghosts.
These outcomes still fail broad musical accuracy; removal of the electric mono
retrigger defect does not establish product acceptance.

## Response and processing time

| Mode | First useful correct note median | P95 | P99 / max |
|---|---:|---:|---:|
| Mono | 48.646 ms | 69.979 ms | 77.833 ms |
| Poly | 59.313 ms | 88.646 ms | 109.979 ms |

Response is measured from the fixed annotated acoustic threshold to the actual
emitted MIDI sample offset. It excludes audio interface, host and receiving
instrument latency. These values do not satisfy the requested 5 / 10 ms detector
response goals. Missing notes are counted separately and do not enter the latency
quantiles. This quick regression makes no rate/block invariance claim.

| Mode | Callback median | P95 | P99 | Maximum | Missed callback deadlines |
|---|---:|---:|---:|---:|---:|
| Mono | 1.262 µs | 28.522 µs | 69.372 µs | 7090.408 µs | 5 |
| Poly | 1.372 µs | 387.519 µs | 654.431 µs | 8081.102 µs | 29 |

Callback wall times were measured on a shared Linux VM and include scheduling
stalls; they are neither isolated CPU benchmarks nor Windows host verification.
Raw sample-offset events, all case metrics and the manifest are retained beside
this file. The [first blind report](../audio-iteration14-holdout-full/README.md)
remains the evidence for unseen iteration-14 generalization.
