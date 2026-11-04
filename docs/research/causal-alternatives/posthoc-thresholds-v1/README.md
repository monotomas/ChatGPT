# Bounded post-hoc threshold study

This is a **seen-data operating-point study**, not a new blind test or a
readiness approval. The grid and criteria were sealed before decoding:
`b2a108ba36ee7a38ab5f02c7efebd541f629bb9d90ef06f0e8ca9a0aa7850290`.

Only previously saved88-key probabilities were read. No neural kernel, model,
audio processing, input modification or new model inference ran. Original
published-default traces remain intact. Decoding retained every accepted
voice, actual original delivery times and all misses/errors. Thresholds were
not selected separately by case, pitch, gain or SNR.

The predeclared On grid was0.7,0.8,0.9,0.95,0.98,0.995, with Off=0.6×On. The
published fast0.7/0.42 and piano0.6/0.36 defaults were also decoded. Both
published-default event transcripts reproduced exactly for all174 unique
inputs, including On/Off sample positions. All saved probability hashes were
unchanged before and after the study.

## Answer to the requested comparison

**No tested point reaches at least163/180 known poly references with at most20
extra Ons**, either over all88 keys or within Flysnapper's existing MIDI28–100
range. Consequently no point also preserves all48 recorded positive targets
with zero noise-only admissions while meeting that legacy poly baseline.

The closest shared-range tradeoff is piano On0.9/Off0.54:166/180 poly references,
26 extras,101/107 mono references with15 extras, all48 recorded positive
targets and no noise-only Ons. It still has53 extras,23 octave extras and22
premature releases on the32 music-plus-contact-noise cases. Its16 quiet plucks
also produce10 extras. Known-poly matched onset median/P95 is44.7/203.3 ms.

Piano0.95/0.57 reduces known-poly extras to15 and retains all48 recorded targets
with zero noise-only admissions, but poly recall falls to156/180. It retains47
extras across the recorded-positive cases and22 premature mixture releases;
known-poly P95 is207.3 ms. Raising thresholds is therefore not a complete
repair. A future acoustically supported hybrid would require its own evidence.

## Shared-range results

The following is the explicitly secondary MIDI28–100 comparison. All88-key
primary results, and every excluded voice, remain retained in the result
files. Ratios below are matched/expected references; extras count additional
NoteOns, including recovered wrong notes and spurious rearticulation.

| Model / On | Poly /180 | Poly extras | Mono /107 | Mono extras | Recorded /48 | Noise-only Ons | Poly onset median/P95 ms |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | --- |
| fast0.7, published | 165 | 145 | 98 | 84 | 48 | 25 | 34.0 /114.3 |
| fast0.8 | 163 | 72 | 98 | 44 | 48 | 11 | 40.7 /156.7 |
| fast0.9 | 148 | 31 | 98 | 22 | 48 | 3 | 42.6 /196.3 |
| fast0.95 | 135 | 20 | 99 | 14 | 38 | 1 | 44.6 /242.0 |
| fast0.98 | 113 | 10 | 95 | 8 | 36 | 0 | 44.6 /311.3 |
| fast0.995 | 71 | 1 | 78 | 1 | 24 | 0 | 44.7 /63.3 |
| piano0.6, published | 172 | 156 | 101 | 97 | 48 | 37 | 32.7 /73.6 |
| piano0.7 | 171 | 117 | 101 | 68 | 48 | 21 | 36.7 /94.0 |
| piano0.8 | 171 | 74 | 101 | 39 | 48 | 1 | 44.6 /102.0 |
| piano0.9 | 166 | 26 | 101 | 15 | 48 | 0 | 44.7 /203.3 |
| piano0.95 | 156 | 15 | 101 | 11 | 48 | 0 | 46.0 /207.3 |
| piano0.98 | 141 | 5 | 100 | 3 | 47 | 0 | 57.3 /323.3 |
| piano0.995 | 95 | 1 | 86 | 1 | 26 | 0 | 104.7 /370.0 |

Latency quantiles describe **matched references only**, alongside their recall.
The shorter fast0.995 P95 results from losing difficult targets, not proving
that those targets became faster. Recorded positives comprise16 quiet plucks
and32 music/noise mixtures per mode; quiet-subset recall is reported separately
in `summary.json`.

The known-poly recall/extras Pareto points are piano0.6,0.8,0.9,0.95,0.98 and
0.995. The fast model's points are dominated on those quality dimensions by
some piano point. This is not a CPU ranking. Joint mono/poly recall, extras and
noise-admission Pareto flags are also retained. Latency is reported explicitly;
it is not used to excuse missed references.

For illustration, all88-key piano0.9 has166/180 references,27 extras and2
noise-only Ons; the shared range removes only one known-poly extra and those
two noise events. Piano0.95 has156/180 references and15 extras in both scopes.
The scope difference does not change the negative answer.

## Retained evidence and reproduction

* `policy.json`, `POLICY_SHA256.txt`: the predeclared grid, scopes and criteria.
* `sweep.py`: our probability-only decoder and unchanged event scorer wrapper.
* `results/summary.json`: every point, counts, latency, family results and Pareto
  flags, including failures of each requested criterion.
* `results/cases.csv.gz`: all9,072 per-case/point/scope score rows, losslessly
  compressed. `storage-map.json` binds its uncompressed CSV hash to the original
  run manifest.
* `results/decoded-events.json.gz`: every threshold-decoded raw On/Off, pitch,
  probability and actual delivery sample, including out-of-range voices.
* `results/manifest.json`: input probability hashes, script/scorer hashes and
  original output hashes. No original frame matrix or external weights are
  bundled.

The omitted probability matrices must still be available separately to rerun
the study. Hashes alone cannot reconstruct them. For the original temporary
location, use a new empty output directory:

```sh
python3 sweep.py \
  --probability-root /tmp/flysnapper-causal-implementation/bench \
  --output /tmp/flysnapper-causal-thresholds-v1-repeat
```

This reruns decoding and scoring only. The original default primary benchmark,
its events and its conclusions have not been replaced by a preferred point.
