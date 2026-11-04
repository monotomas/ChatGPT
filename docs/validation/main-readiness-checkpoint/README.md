# Exact current-main readiness checkpoint

This captures DSP source at commit
`ddceefbf9a49f2f265cb692863ab43ad4c2389a9`: main mono-v20 with the original
main poly17 backend, after removal of unused experimental mains priming.
It is a failing research checkpoint, not a new release or musical-readiness
approval. None of the isolated continuity, admission or dynamic-poly prototypes
was substituted into these measurements.

All three suites are **seen regression data**. Their original first-unseen
results remain elsewhere; repeating them here does not create new unseen
evidence. Labels, policies, thresholds, PCM and seals were not changed.

## Scope and source binding

The source was copied to a separate immutable `/tmp` tree before compilation.
An identical retained copy is under `source/Source/Pitch/`. Hashes before and
after the copy and after all three captures agree. The runner, capture driver,
scorer and integrity checker also remained unchanged and are retained under
`orchestration/` with hashes. The source manifest identifies the exact commit.

Each suite compiled and captured successfully, then returned score exit code
**1** because acoustic criteria failed. All three failing scores were retained;
the next suite was run despite the previous nonzero result. Raw events,
metrics, compiler logs, scorer summaries and exact hashes are included.

Configuration is **48 kHz / 64 samples only**, using the independent detector
capture before MIDI correction/MPE routing. This is not the full sample-rate/
buffer matrix, Windows execution, Ableton execution, or physical interface
latency measurement. Data PCM is referenced in its sealed original locations
and is not duplicated here.

## Current results

| Suite | Passed | Failed | Mono pass | Poly pass |
| --- | ---: | ---: | ---: | ---: |
| Controlled signals | 31/72 | 41 | 29/34 | 2/38 |
| Real isolated hammer samples | 5/16 | 11 | 2/8 | 3/8 |
| Real noise/quiet-note controls | 37/128 | 91 | 34/64 | 3/64 |
| All configurations | 73/216 | 143 | 65/106 | 8/110 |

The controlled mono path matches all 48 fretted-slide and 10 hammer/pull
reference notes in their dedicated families without extra notes. However,
four music/handling cases fail with 14 extra onsets and premature first
releases, and the fastest smooth-bend curve still exceeds its P95 error
limit. Poly finds only 6 of 48 fretted-slide references and fails the four
independent-bend cases on retained error/release criteria.

The real isolated hammer edition still has six mono wrong-before-correct
onsets. Its nominal SFZ anchors and early acoustic pitch drift have separately
documented uncertainty; this result is not silently relabelled or called six
proven instantaneous acoustic-frequency errors. These isolated samples do
not establish continuous human hammer/pull transition coverage.

On recorded-noise controls, all 16 quiet real mono plucks pass. Mono mixtures
find all 32 intended notes but produce 48 extra onsets and 28 premature first
offs, mostly unwanted rearticulation. Poly mixtures also find all 32 notes
but produce 96 extra onsets, 20 octave errors, 16 early offs and 12 late offs.
Poly noise-only controls generate 65 onsets. These remain substantial musical
failures. The author-labelled percussion suppression-intent boundary is
documented separately and does not imply every recording is wholly non-tonal.

No hanging voices or output overflow were observed in these 216 captured
configurations, including the supplied post-file silence. That bounded
observation does not establish an unlimited-duration guarantee.

## Matched-note decision times

| Suite | Mono median / P95 | Poly median / P95 |
| --- | ---: | ---: |
| Controlled | 15.98 / 46.65 ms | 47.98 / 65.98 ms |
| Real isolated hammer samples | 60.65 / 63.31 ms | 65.98 / 65.98 ms |
| Real noise/quiet-note controls | 29.98 / 45.98 ms | 55.31 / 69.98 ms |

These summarize only matched truth intervals. Misses, wrong-first notes and
extra notes remain in the accuracy results; a short conditional median does
not erase them. Decisions keep their actual causal output timestamps. No
backdating or fitted pitch-curve delay alignment is used. Recorded timing is
relative to authored sample playback, not unknown physical pick/fret contact.
The original physical E2 <10 ms/E3 <5 ms goals remain unverified.

## Separate native verification

The retained Pitch source hashes match the
[native mono-v20/poly17 checkpoint](../musical-readiness-research/native-checkpoint-v20/README.md),
where 12 of 13 test groups passed and strict simultaneous poly source
acquisition failed. Those native protocol/integration checks and these broader
acoustic scores are different evidence. Passing compilation, most unit groups,
or a simple settled two-string bend cannot approve this failing checkpoint.

`checkpoint-summary.json` binds every output and preserves exit codes,
source stability, seen-regression roles and explicit blocked readiness.
`mode-family-metrics.json` provides descriptive aggregates of the unchanged
per-case scorer reports. No installer is produced or approved here.

## Reproduce

From the repository root, use the retained runner and source snapshot with a
fresh output directory, for example:

```sh
python docs/validation/main-readiness-checkpoint/orchestration/Tools/run_readiness.py \
  --source-snapshot docs/validation/main-readiness-checkpoint/source \
  --fixtures Tests/AudioFixtures/readiness/recorded_noise \
  --output /tmp/flysnapper-main-noise-recapture
```

Use `Tests/AudioFixtures/readiness` or its `recorded_hammers` subdirectory for
the other suites. Failing score exits are expected. Running all suites must
preserve each exit rather than stop after the first acoustic failure.
