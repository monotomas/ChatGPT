# Fresh missing-root support: step fix and sparse-source tradeoff

This audit tests one change to the **fresh** polyphonic missing-root anchor
guard. It does not use the rejected odd-envelope censoring or held-continuity
prototypes. No product files were edited by this audit agent. Main-source
adoption and native regression tests remain the parent agent's responsibility.

## Exact change and acoustic hypothesis

The existing direct physical-root branch is unchanged. When a fresh YIN
anchor lacks an observed root, its existing fifth-and-seventh support must
also contain an observed second **or** third partial. The added observations
use the same existing0.008 relative magnitude and1.5% frequency tolerance.
There is no threshold sweep, global mains veto or held-note revalidation.
`candidate.patch` is the complete code delta.

The diagnosed59→61 plus held64 case presents real277Hz and329Hz components.
A spurious55Hz/MIDI33 common divisor can use277Hz as a fifth partial and a
Hann sidelobe near385Hz as a seventh. The parent's diagnostic trace measured
the latter at about2% of the strongest spectral peak, enough to pass the old
guard. There is no corroborating second/third partial. This experiment tests
the additional support rather than raising every spectral threshold.

## Targeted18-step grid: positive and still limited

Three source transitions57/59/60→+2 are rendered beside held64, with
3/4/5/6/8/12 partials. The first source changes pitch at sample19200(400ms);
both sources continue until43200(900ms), followed by actual silence. The
baseline rerun reproduces the parent's supplied raw CSV byte-for-byte.

| Observed step-grid result | Baseline | Candidate |
|---|---:|---:|
| Both intended initial source notes acquired |18/18|18/18|
| Intended replacement pitch acquired |18/18|18/18|
| Held64 first Off at/after source cutoff |12/18|18/18|
| Replacement source first Off at/after cutoff |12/18|18/18|
| Unexpected note-ons |15|8|

All six59→61 cases previously emit false33 and drop both61and64 at sample22911
(477.31ms), while their real audio continues to900ms. The candidate removes33
and keeps both intended notes through cutoff, releasing at46719(973.31ms),
73.31ms after source withdrawal. The12-partial case also loses false73.
`step18-source-continuity.json` explicitly checks first Off times; onset counts
alone would have hidden the baseline's premature releases.

Other errors remain. In particular the six-partial59→61 control still emits
unrelated71and76. The regression should assert targeted33 rejection and
required61/64 continuity, not claim complete error-free transcription.

The earlier30-case octave-step grid is also retained. Its complete raw
baseline/candidate output is byte-identical:24/30 initial two-source
acquisitions,21/30 new replacement-pitch onsets and20 unexpected onsets.
Those known failures are not fixed by this narrow change.

## Sparse-source falsification: six wanted-root regressions

After the positive grid and unchanged396-case comparison, the root agent
requested an explicit sparse missing-fundamental falsification. Its separate
plan was fixed before execution: roots28/40/52/64, only fifth/seventh amplitudes
0.16/0.11, plus equal second/third amplitudes0.16×ratio for ratios
0/0.002/0.01/0.05. Each case has a2ms attack,750ms ringing source and250ms
actual silence, at48kHz/64samples. Phases are0.17×harmonic radians. These are
synthetic physically periodic sparse spectra, not recorded guitar examples.

| Sparse root | Baseline acquired | Candidate acquired |
|---|---:|---:|
|28|0/4|0/4|
|40|4/4|2/4|
|52|4/4|2/4|
|64|4/4|2/4|
|All|12/16|6/16|

For roots40/52/64, pure fifth/seventh input and0.002 low-partial ratios each
lose the intended root under the guard: six newly exposed wanted-root misses.
The candidate instead emits upper aliases, retained in `sparse.csv` and
`sparse-summary.json`. At ratios0.01/0.05 the correct roots are retained with
exactly the baseline's actual On/Off samples and continue through cutoff.
Root28 was already missed by both variants and is not a candidate regression.
`sparse-predeclared.json` and `sparse_missing_root.cpp` retain the full plan
and generator. There was no additional ratio tuning or guard variant.

This confirms the conservative tradeoff rather than universal missing-root
correctness. The guard is suitable only as explicitly scoped guitar behavior
if these sparse-source limitations are accepted and documented. Passing396
earlier configurations did not expose them. Do not claim that all valid weak-
or missing-fundamental strings are preserved, or that overtones are solved.

## All396 seen fixed-note corpus configurations are unchanged

The current fourteen compiled detector CPP/H files and fixed driver were
byte-identical to the immutable baseline from the prior odd-censor audit.
The396 baseline captures and30-case baseline grid were therefore reused,
explicitly bound by source/raw hashes in `reused-baseline.json`. They are
**not** represented as new baseline executions. The new18-step baseline was
actually rerun. Every candidate corpus capture was newly executed.

| Seen suite | Configurations | Baseline/candidate raw events |
|---|---:|---|
| Public |81|Byte-identical|
| Holdout, previously disclosed |69|Byte-identical|
| Confirmation |46|Byte-identical|
| Controlled readiness |72|Byte-identical|
| Recorded noise/quiet notes |128|Byte-identical|
| Total |396|No behavioral changes|

Thus all matched/missed/wrong/extra notes, early/late releases, output latency,
noise-only durations and hanging observations are exactly preserved on those
cases. This does not mean those cases all pass. For example known poly recall
remains163/180 with22 extras; recorded-noise poly remains48/48 with224 extras,
18 early offs and10.584s false noise-only voice duration. The supplementary
release audit is retained without changing its matching or policy.

This fixed-note driver sets both monoBend and polyBend false. Static poly uses
`updateVoices` directly. Its totals cannot be equated with the earlier
expression-enabled73/216 readiness results. The new dynamics frontend is not
part of this direct-detector measurement. All corpora are seen regressions,
not independent unseen validation. No labels, PCM or policy were changed.

## Scope, reproducibility and decision

This fixes a reproducible virtual-root/premature-release failure and preserves
all396 old raw outputs, but it has the six sparse-source wanted-root losses
above. It is a scoped conservative choice, not an across-the-board accuracy
improvement. Adoption still needs a native regression, an explicit product
limitation and current complete-product validation.
It is not itself a musical-readiness, Windows, Ableton or hardware-latency claim.

The captures use g++14.2/C++17, `-O2 -DNDEBUG`,48kHz/64sample callbacks, causal
decision samples and bounded existing DSP. No event backdating, curve alignment
or expected-note-count forcing is used. CPU/deadline data comes from a shared
cloud worker; reused baseline timing is historical and is not a randomized
performance comparison. No realtime or CPU improvement is asserted.

`baseline/` and `candidate/` contain the frozen fourteen detector source/header
files, fixed/grid drivers, complete raw grid and corpus output, compile command
inventories and logs. `InputDynamics` and `MpeOutputRouter` were deliberately
excluded because neither belongs to this direct detector signal path and the
former was being edited independently. `predeclared.json` pins the one rule.
`summary.json`, `grid-summary.json`, `step18-source-continuity.json` and
`release-duration-audit.json` retain the quantitative results. The later
`sparse-*` files retain the separate falsification and all resulting aliases.

`reused-baseline.json` also records the original archive/executable hashes and
the37 reused file bindings. `manifest.json` hashes retained source/text records
and records compiled-executable hashes. Object files and executables are not
included in its retained-file inventory. There is no copied PCM or external
model payload. The fixed driver does not append silence to ordinary corpora;
EOF-hung observations for hum-to-EOF cases are not hanging-after-silence proof.

To reproduce the new grids in fresh output files:

```sh
g++ -std=c++17 -O2 -DNDEBUG -I VARIANT/Source VARIANT/step18.cpp \
  VARIANT/Source/Pitch/{ArticulationTracker,EarlyEstimator,NoiseConditioner,PitchDetector,PolyphonicTracker,TimeDomainPursuit,YinEstimator}.cpp \
  -o /tmp/new-anchor-step
/tmp/new-anchor-step > /tmp/new-anchor-step.csv
```

Use `grid30.cpp` for the original octave grid. Use `audio_validation.cpp` and
fixture/output directory arguments for the five corpus captures. Reproduction
should create fresh outputs and preserve this evidence directory unchanged.
Shared CMake/Ninja state was not used by this experiment.
