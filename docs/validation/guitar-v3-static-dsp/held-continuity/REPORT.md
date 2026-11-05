# Static-poly held-source continuity: reject all three prototypes

No product files were changed by this audit. Four isolated executables ran
the five already-seen corpora:81 public,69 holdout,46 confirmation,72 controlled
and128 recorded-noise configurations,396 per variant. All captures completed.
This is a bounded negative experiment, not a musical-readiness recommendation.

The baseline's16 Pitch CPP/H files and fixed-note driver equal git
`ddceefbf9a49f2f265cb692863ab43ad4c2389a9` byte-for-byte. Each variant compiled
with g++14.2, C++17, `-O2 -DNDEBUG`,48kHz/64samples. The driver is the unchanged
`Tools/audio_validation.cpp`: monoBend and polyBend are both false. Thus static
poly goes through `updateVoices` directly. The earlier archived readiness
driver enabled both expression flags and published poly through a continuous
shadow tracker. Those signal paths, interval scorers and EOF policies differ;
the totals here must not be called the earlier73/216 readiness result.

The five corpora are seen regressions. No fixture labels, PCM or thresholds
were changed. The fixed driver does not append post-file silence. Its EOF-hung
counts for hum-to-EOF controls are not proof of hanging after supplied silence.
Raw On/Off decisions retain their actual sample timestamps; none are backdated.

## Bounded progression

V1 (`candidate`) adds held-only continuation when the global YIN hint still
identifies the same established root confidently and a physical root plus
coprime upper series exists. All396 raw-event outputs are byte-identical to
baseline. The easy same-anchor case was already retained by the existing fit.

V2 (`candidate2`) broadens held continuation to an observed root with independent
peak support, at least three partials and primitive coprime low-series evidence,
even without a global YIN period. It never admits a new source and suppresses
repicking on fallback-only frames. It lowers real-noise false On counts but
extends unwanted ringing and introduces new genuine-note errors.

V3 (`candidate3`) fixes a mechanical V2 artifact: a fallback's root-only
amplitude must not overwrite the stored full-model source amplitude, since
normal-model recovery can then look like a new1.35xattack. It also withholds
the added fallback when the observed low root is within the existing narrow
50/60Hz low-carrier neighborhood. This is not a new-note mains veto. V3 fixes
the extra same-note repick in `confirm_voicing_01_poly`, but the broader musical
regressions remain. The narrow carrier dispatch does not remove them.

| Poly result | Baseline | V1 | V2 | V3 |
|---|---:|---:|---:|---:|
| Old196poly references matched |163/180|163/180|163/180|163/180|
| Old196poly extra On |22|22|23|22|
| Old196poly early Off |3|3|4|3|
| Recorded-noise target notes matched |48/48|48/48|48/48|48/48|
| Recorded-noise extra On |224|224|199|199|
| Recorded-noise early Off |18|18|12|12|
| Recorded-noise late Off |4|4|4|4|
| Noise-only On subset |70|70|63|63|
| Noise-only false voice duration |10.584s|10.584s|12.443s|12.443s|
| Noise-only union duration |7.344s|7.344s|7.816s|7.816s|

False voice duration sums concurrent wrong voices; union duration counts only
time with at least one wrong voice. Lower event counts can mean longer unwanted
notes, not safer noise rejection. On controlled noise, V3 adds one handling
On and extends false voice duration11.444→11.884s. It makes the real target in
both `quiet_hum_64_gain1_poly` and `quiet_hum_64_gain8_poly` end prematurely,
despite improving some other mixed-noise intervals. These in-scope failures
are enough to reject adoption. Poly-bend cases are retained in the full raw
controls and are not used as the reason for rejection under the revised scope.

The separate release audit uses exactly the fixed driver's first usable
20ms one-to-one interval match, early release<-10ms and descriptive late-release
counts>80ms (100ms for named6dB controls). It is a supplementary diagnostic,
not a replacement acceptance policy. Existing source tails commonly release
several milliseconds later under fallback; late counts are retained.

## Decision and useful finding

Do not merge these patches. Physical source continuity is useful positive
evidence, but it also preserves previously mis-admitted pitched handling/hum.
The forthcoming dynamics frontend needs to be evaluated as an actual complete
signal path. Isolated reductions in note-on totals are insufficient evidence
of improvement. No model architecture or new threshold sweep follows this
bounded trial.

## Retained evidence

`baseline/`, `candidate/`, `candidate2/`, `candidate3/` contain frozen source,
driver, compilation logs and complete five-suite raw events/case summaries.
`candidate*.patch` are exact code deltas. `summary-v*.json` gives paired totals;
`changed-case-metrics-v*.json` preserves changed case metrics.
`audit_releases.py` writes `release-duration-audit.json` from unchanged raw
events and repository fixture inventories. `manifest.json` binds the files;
compiled executables are deliberately excluded from its retained-file list.
The temporary directory contains no copied PCM or external model payloads.

To reproduce in a fresh output directory, compile each retained variant:

```sh
g++ -std=c++17 -O2 -DNDEBUG -I VARIANT/Source VARIANT/audio_validation.cpp \
  VARIANT/Source/Pitch/{ArticulationTracker,EarlyEstimator,NoiseConditioner,PitchDetector,PolyphonicTracker,TimeDomainPursuit,YinEstimator}.cpp \
  -o /tmp/new-static-capture
/tmp/new-static-capture /workspace/ChatGPT/Tests/AudioFixtures /tmp/new-static-results
```

Use the holdout, confirmation, readiness and readiness/recorded_noise fixture
directories for the other suites. Shared CMake/Ninja state is not involved.
