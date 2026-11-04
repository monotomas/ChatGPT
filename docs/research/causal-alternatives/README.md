# Causal alternatives: retained negative research

This archive records source inspection and a **private, seen-regression**
benchmark of PitchPlease3.1.0. It does not approve a replacement detector or a
Flysnapper installer. No external kernel, model weights, package tarball,
repository checkout or complete third-party README is included.

The [report](REPORT.md) explains the three inspected implementations, actual
audio availability, output limitations and licensing evidence. PitchPlease is
a compact streaming CQT/CNN implementation, but its published integer-key
decoder produces substantially more false notes on these inputs. Its ordinary
88-key output has no independent continuous pitch or source identities.

| Model | Known poly references /180 | Primary extra Ons, all88 keys | Secondary extra Ons, MIDI28–100 | Recorded-noise extra Ons, all88 / shared range |
| --- | ---: | ---: | ---: | ---: |
| fast | 165 | 157 | 145 | 175 /152 |
| piano | 172 | 170 | 156 | 238 /229 |

Recorded-noise totals are per64 mode-labelled cases; identical audio has the
same neural output under mono and poly labels. Both models find all48 target
notes there, but with many extra voices, octave errors and bad releases. These
integer-event scores do not implement the stricter MPE/readiness policy and
must not be equated with its37/128 result.

## Scope and timing

The original benchmark covered324 cases from174 unique mono48-kHz PCM files:
the previously disclosed196 public/holdout/confirmation cases plus128 recorded
noise cases. Inputs, labels, original thresholds and operating points were
unchanged. Every source was followed by250 ms of supplied silence. Thresholds
were not tuned and accepted voices were not reduced to a desired count.

All events use **actual64-sample delivery availability**, not the model's older
feature timestamp. The primary result retains all88 keys, MIDI21–108. The
secondary MIDI28–100 comparison was explicitly post hoc and applies only
Flysnapper's pre-existing declared range. All excluded events remain in
`range-excluded-raw-events.json`; no in-range error was removed.

## What is retained

* `provenance/`: pinned source URLs/hashes, repository/package byte parity,
  published file inventory, selected MIT package metadata and failed upstream
  provenance lookups. Package-wide MIT intent is explicit and includes the
  model assets without a discovered exception, but full notices and upstream
  training/weight provenance remain unresolved for redistribution.
* `configurations/`: exact source-file hashes and case/reference inventories.
  Paths describe the original execution workspace; they are not portable
  discovery instructions.
* `results/{fast,piano}/`: complete actual On/Off traces, callback metrics,
  primary scores, post-hoc range scores and excluded range events.
* `smoke/`: actual traces for the explicit45+64, single strong-third and modal40
  diagnostics. These are generated diagnostics, not real guitar acceptance.
* `harness/`: **our** local capture harness, scorer, range comparison and five
  manual transcript tests. No third-party inference implementation is bundled.
  `original-score.py.txt` preserves the exact original scorer; `score.py`
  changes only provenance reporting so retained-event scoring works without
  the deliberately omitted external kernel. Matching and scoring rules are
  unchanged.

The large90-column probability matrices remain only in the original `/tmp`
research directories. `provenance/omitted-artifacts.json` records their hashes,
byte counts, row counts and format, as well as the omitted kernel/weight asset
hashes. Those hashes are not their contents: the repository archive can
reproduce event scoring, but cannot inspect every discarded probability or
rerun inference without separately obtaining the exact audited assets. The
temporary originals may not survive another environment replacement.

`archive-manifest.json` binds the retained files. This report preserves negative
results; changing the detector later must create a new measurement rather than
rewrite these traces.

## Repeat the event scoring without external code or weights

From this directory:

```sh
python3 harness/test_score.py
python3 harness/score.py configurations/fast.json results/fast
python3 harness/score.py configurations/piano.json results/piano
python3 harness/range_comparison.py configurations/fast.json results/fast
python3 harness/range_comparison.py configurations/piano.json results/piano
```

These commands write derived score files. Copy the archive to a scratch
directory first if preserving byte-exact evidence. The scorer uses only the
retained case labels and raw events. It does not fetch or execute a model.

To rerun inference, create a separate private directory, obtain the pinned
kernel and exact package assets after reviewing their license/provenance, and
provide `incport.mjs` and the selected `<model>.bin` alongside `run.mjs`.
`harness/make_configurations.py` can create equivalent case inventories for a
different checkout. Neither script installs a package or executes package
scripts. No native C++ port, DAW behavior or musical readiness is established.

A later [bounded post-hoc threshold study](posthoc-thresholds-v1/README.md)
decoded only the saved probabilities. No tested point matched the legacy
polyphonic recall/extra-note baseline while also retaining all48 recorded
positive targets with zero noise-only admissions. Its tradeoffs and failures
are retained separately; the original default results above are unchanged.
