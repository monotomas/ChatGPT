# Odd-mode censoring correction: isolated adoption rejected

This is one predeclared change to `TimeDomainPursuit::refreshOctaveEvidence`,
not a threshold sweep. Product files were not edited by this audit agent.
The isolated baseline includes the parent's current mechanical static-voice
IDs and new-note gate API. Fourteen detector CPP/H files and both drivers are
frozen here. `InputDynamics` and `MpeOutputRouter` are excluded because neither
is part of this direct detector capture. `predeclared.json` binds the source
hashes taken before compilation; the source remained stable while copied.

## Hypothesis and exact candidate

The old odd-envelope regression substitutes1e-9 for an absent odd harmonic.
For example, a source containing harmonics1..6 has real odd modes1,3,5 and no7.
The substituted log-amplitude can force the fitted slope/intercept down and
manufacture broad even-mode excess, resulting in an additional octave note.

The candidate fits only positive odd amplitudes exceeding2% of the largest
odd amplitude. At least three odd observations and a denominator>1e-9 are
required. The original slope bounds and all other calculations remain.
No values were retuned after results were seen. No active-upper-support rules
were changed. `candidate.patch` is the complete delta.

This fixes some finite-harmonic null-model errors, but it also removes evidence
needed by existing octave/source allocations. Treating the correction as a
complete detector improvement would conceal genuine-note losses.

## Exact30-case voicing grid

The parent's grid synthesizes moving45/47/48/50/52 plus held64, moves the first
source up an octave at400ms, and uses3/4/5/6/8/12 partials. The baseline raw CSV
is byte-identical to the parent's supplied original grid capture. No desired
voice count is supplied to inference. On/Off decisions retain causal times.

| Descriptive grid metric | Baseline | Candidate |
|---|---:|---:|
| Both initial source notes found before400ms |24/30|21/30|
| Unexpected note-on outside the phase's two intended pitches |20|12|
| New moving-pitch note-on after400ms |21/30|21/30|

The unchanged final count conceals different errors. The candidate loses the
previously found held64 beside52 for3/4/5 partials, and loses the correct45→57
replacement in five previously successful45+64 cases. It fixes the47/H4,H6
false-octave cascades. The52→64 cases also reach physical unison with the held64;
duplicate string counting there is not a universal MIDI requirement. Full raw
events and per-case results are retained rather than collapsing these into a
single purported musical score.

## Paired396 seen corpus configurations

Both variants ran81 public+69 holdout+46 confirmation+72 controlled+128
recorded-noise configurations at48kHz/64samples. All captures returned0.
The unchanged fixed-note driver disables both monoBend and polyBend; this is
static quantized poly, not the previous expression-enabled73/216 evidence.
All corpora are already seen regressions. PCM, labels and policies are unchanged.

| Poly observation | Baseline | Candidate |
|---|---:|---:|
| Old196 reference notes matched |163/180|162/180|
| Old196 extra note-ons |22|16|
| Old196 octave errors |11|7|
| Old196 early note-offs |3|4|
| Old196 late note-offs |10|10|
| Recorded-noise target notes matched |48/48|48/48|
| Recorded-noise extra note-ons |224|207|
| Recorded-noise octave errors |32|18|
| Recorded-noise early note-offs |18|16|
| Recorded-noise late note-offs |4|4|
| Noise-only onset subset |70|69|
| Noise-only false voice duration |10.584s|10.280s|
| Noise-only union duration |7.344s|7.216s|

The key in-scope musical regression is `hidden_poly_01_poly`, the six-source
E-major40/47/52/56/59/64 control. Baseline matches five of six, omitting64.
The candidate additionally omits59/B3 and ends52/E3 at sample7935(165.31ms),
far before truth26400(550ms). Baseline retains52 until sample29055. This is
not a small tolerance-boundary difference. Confirmation wrong-before-correct
events also grow0→2, despite fewer total extras.

Controlled-poly matched notes, total extra note-ons, early note-offs and
noise-only durations are unchanged in aggregate; octave errors rise18→19.
Per-case events remain available. Mono is unchanged by the TDP patch.

The release-duration diagnostic uses the fixed driver's first usable20ms
one-to-one interval matching, early<-10ms and descriptive late>80ms(100ms for
named6dB controls). It is supplementary evidence, not a new acceptance policy.
The fixed driver does not append post-file silence. EOF-hung counts for controls
with hum to EOF are not proof of hanging after silence. No event backdating,
latency alignment, missing-note deletion or output-count forcing is used.

## Decision

Do not adopt this isolated patch. It helps a specific censored-envelope modeling
failure but introduces wanted chord-note losses. A future correction needs a
complementary, validated account of physical-root withdrawal and genuine octave
source evidence. No further support-ratio sweep or model change was attempted.
The new dynamics frontend must be measured as the actual integrated signal path;
these direct-detector results do not measure that frontend.

## Evidence and reproduction

`baseline/` and `candidate/` retain exact detector/driver sources, compile logs,
grid raw CSV and complete five-suite raw On/Off/case results. Executables remain
temporary and are excluded from the file inventory. There is no copied PCM,
external model or dependency payload. `summary.json`, `grid-summary.json`,
`legacy-changed-metrics.json` and `release-duration-audit.json` retain the numbers
above. `audit_releases.py` recomputes supplementary release/duration metrics
from the existing repository fixtures and raw events. `manifest.json` hashes
the complete retained text evidence.

To reproduce a variant in fresh output directories:

```sh
g++ -std=c++17 -O2 -DNDEBUG -I VARIANT/Source VARIANT/grid.cpp \
  VARIANT/Source/Pitch/{ArticulationTracker,EarlyEstimator,NoiseConditioner,PitchDetector,PolyphonicTracker,TimeDomainPursuit,YinEstimator}.cpp \
  -o /tmp/new-odd-grid
/tmp/new-odd-grid > /tmp/new-odd-grid.csv
```

Replace `grid.cpp` with `audio_validation.cpp` and call the resulting executable
with fixture/output directories for the five corpus captures. Shared Ninja or
CMake state is not involved. Cloud CPU/deadline timings are descriptive, not
Windows, Ableton, audio-hardware or realtime certification.
