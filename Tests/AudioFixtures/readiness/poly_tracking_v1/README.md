# Independent continuous-poly tracking audit

This is an optimistic, synthetic **tracking-only** audit. Every true source
identity and exact initial pitch is supplied once, before motion. It does not
test note acquisition, real guitar or musical readiness. It must not be cited
as a transcription pass, even when every tracked pitch is accurate.

The eight fresh six-string inputs and numerical policy were written and sealed
before executing frozen model 19. Only after its first capture were details
shared with the implementation owner. Model 17 was subsequently measured on
the same edition without tuning either snapshot. Both frozen sources predate
fixture selection. After disclosure these cases are **seen regression data**.

Seal: `226e669e942444bbf789e8c18d3c86fe34ca48bb3b46dacce600746931287eb5`.
The original fixture/seed/truth/policy files and PCM remain unchanged. The
detector is never imported or run by `generate.py`.

## Scope and scoring

The sources are physically inspired harmonic strings with independent phases,
small detuning, pluck/pickup spectral coloration, stiffness, damping and
high-partial decay. They include fretted six-string G-major, C-major7/G,
D-add9/F-sharp, B7/F-sharp, E7-altered, A-minor6/E, D7/F-sharp and G13 voicings.
Moving cases contain two opposed bends, vibrato/direction reversals, a weak
held tail, dynamic neighbouring-string interference and a coherent phase reset
of one source while others continue. These are controlled waveforms, not
recorded gestures or a human hammer-on performance.

All six true pitches and identities are seeded at source sample 1023
(85.25 ms). Phase, harmonic profiles and model coefficients are **not** injected.
The tracker warms normally until sample 2879 (239.92 ms); scoring includes every
32-sample observation through sample 16511, including motion onset, return and
reversal. Each case has 427 assessed observations × 6 sources = 2562 expected
source frames. Motion is not backdated or aligned to a fitted delay.

The prototype's IDs equal its six internal slots. Physical strings are randomly
permuted onto those slots before seeding; every score follows that original
string-to-ID mapping. A pitch nearer another string is still wrong for its own
ID. Missing voices fail explicitly and cannot disappear into a successful
matched-only error average. Static oracle-carried pitch also does not establish
that a weak source remains independently observable; `phaseObservable` is
reported separately.

The original policy requires zero missing/extra/duplicate source frames,
aggregate median ≤15 cents, P95 ≤25 cents and maximum ≤75 cents, plus per-voice
P95 ≤25 cents and maximum ≤75 cents. The per-voice check prevents four static
strings from hiding two inaccurate moving strings. All timing is causal source
time at observation end. These numerical tracking checks were fixed before
measurement and do not replace real-performance or physical-latency goals.

The raw capture uses a 12-kHz analysis stream, hop 32, external-seed mode,
legacy/early acquisition disabled, and a deliberately permissive −90-dB source
threshold. Three cases vary observation length among 256/320/384/448/512 after
0.50 seconds; others use 512. This is substantially more favourable admission
than a real audio-to-MIDI input with unknown notes and noise.

## Frozen-model results

| Case | 17 P95/max (cents) | 19 P95/max (cents) | Strict pass 17 / 19 |
|---|---:|---:|---|
| G major static | 4.9 / 10.7 | 5.0 / 15.2 | yes / yes |
| C major7 opposed | 15.7 / 34.4 | 19.4 / 49.7 | no / no |
| D add9 vibrato, varied observation | 63.0 / 127.4 | 59.0 / 108.3 | no / no |
| B7 pickup nodes | 16.8 / 36.5 | 16.2 / 45.3 | no / no |
| E7 reversals, varied observation | 58.8 / 81.2 | 71.3 / 98.1 | no / no |
| A minor6 weak held tail | 73.6 / 105.8 | 15.2 / 31.0 | no / yes |
| D7 neighbouring attack/phase reset | 12.8 / 20.6 | 24.7 / 51.7 | yes / no |
| G13 stiff strum, varied observation | 58.6 / 78.9 | 28.2 / 52.9 | no / no |

Both models pass 2/8 cases; model 19's improvements include a large weak-tail
gain and lower stiff-G13 errors, alongside regressions on the neighbouring
phase reset and some moving voices. All original identities remain present:
zero missing, extra or duplicate IDs. The failures therefore remain visible
as current-time pitch errors. C-major7 illustrates why aggregate P95 alone is
insufficient: model 19 aggregate P95 is 19.4 cents, but its moving voices have
P95 37.0/27.2 cents. The original whole-curve results are retained.

`comparison.png`/`comparison.svg` plot the largest original-ID pitch error at
each observation, without alignment. The grey dashed line is the 75-cent
maximum limit; the dotted vertical line marks when variable observation counts
start. These plots are supplementary diagnostics, not a changed scoring rule.

Model 19 snapshot tracker SHA256:
`4bbc34d883cee96253383b87181350e746d0d73110ad2cf0ffe65d6e5d9231f7`.
All six source files, capture/scoring code, raw frame/voice outputs, errors,
timing and manifests are retained under `candidate19`. `baseline17` retains
the exact earlier snapshot separately. Before/after frozen source checks pass.

## Post hoc full512 diagnostic

After the original failed capture, the **same already-seen** three varied-window
inputs were rerun with fixed 512-sample observations. Original audio, source 19
and numerical limits were unchanged. This is a configuration diagnostic and
cannot replace the original six failures or count as fresh unseen evidence.

All three still fail: D-add9 P95/max 27.9/112.4 cents, E7 59.8/99.5, G13
18.7/49.8 (G13's worst individual voice P95 is 47.5). Observation-length changes
exacerbate some errors but do not explain every motion failure.

The diagnostic metadata seal is
`fcffe1407bc186179e46e3abaaad44cddca02f8383555fbbaf75aa4666fe33f5`.
`make_full512_diagnostic.py` reproduces it without editing the original edition;
its metadata is retained in `diagnostics/full512-metadata`. Raw measurements
and the separate snapshot are under `candidate19-full512-diagnostic`.

## Reproduction

```bash
python Tests/AudioFixtures/readiness/poly_tracking_v1/verify.py
python Tests/AudioFixtures/readiness/poly_tracking_v1/test_scoring.py
python Tests/AudioFixtures/readiness/poly_tracking_v1/run.py \
  --source /tmp/readiness-poly-oracle19-source --output /tmp/new-poly19-capture
python Tests/AudioFixtures/readiness/poly_tracking_v1/compare.py
```

Use a new output directory. `run.py` copies the supplied `Pitch/` tree before
compilation, checks hashes, binds the executable/support/raw outputs and retains
the actual nonzero score. `capture.cpp` accesses private state only for the one
declared oracle seed. The four scorer transcript tests explicitly guard against
nearest-ID rescue, omitted missing voices, delayed estimates and duplicate IDs.
No reserved recorded-noise candidate data was evaluated during this audit.
