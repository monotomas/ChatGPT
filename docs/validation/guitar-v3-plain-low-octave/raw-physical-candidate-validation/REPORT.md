# Period-matched cleaned-reference validation

This tests the parent's isolated candidate against the exact frozen Native3
baseline. It changes **only PitchDetector.cpp**: held octave-ambiguity physical
checks use the cleaned reference before dynamics, with period-matched history.
The reference is after stationary-noise conditioning, not unfiltered sidechain
audio. The downward check uses at least512 samples or3.25 candidate periods;
the upward check uses the tracker's actual observed length, also at least512.
Both are bounded by actual received count(max1024). No thresholds, blind pitch
latch, future audio or added source model were introduced.

The two frozen16-file detector snapshots are retained. No project files,
existing capture helper or parent candidate source were edited by this audit.
Compilation/captures use standalone g++14.2, C++17, `-O2 -DNDEBUG`.

## Exact24-case quiet comparison

The original Native3 baseline's24 captures are explicitly reused with byte
bindings; they are not claimed as newly rerun. The candidate uses the exact
same driver and six sealed31/40/52 gain1/8 PCM files, monoBend false/true,
dynamics false/true,48kHz/64samples+250ms flush. Other factory defaults,
including bend range2, are unchanged.

| Quiet observation | Baseline | Candidate |
|---|---:|---:|
|Wanted references acquired|24/24|24/24|
|Extra note-ons|21|0|
|Premature first Offs|4|0|
|Late first Offs(>80ms)|0|0|
|Overflow/end-active/unclosed/orphan outputs|0|0|

Only the four plain31 cases change. They keep their identical initial On and
now retain31 until76671(+47.31ms after truth74400), replacing the old40191 Off
and false43 repicks. Every N/F/P event in the other20 configurations is
**exactly identical**, including all previously clean bend-enabled cases.
`quiet-comparison.json` retains all per-case intervals and exact raw comparisons.

## Predeclared true-change/damping falsification

There are32 cases per variant: lower roots31/35/40/52 ×plain/bend ×four families:
true instantaneous octave up, true instantaneous octave down, two-semitone
fretted up/return, and physical damping. Downward tests start at root+12, so
all targets remain in the declared range. Dynamics is enabled and bend range
is12 in this separate counterset, as explicitly requested.

The generator uses12 partials1/h^1.25, fixed0.17h phase offsets,0.15 amplitude,
a2ms initial attack and one continuous mild exponential envelope. Phase is
continuous across instantaneous pitch changes and there is **no amplitude or
attack reset** at transitions. Final physical damping takes4ms, followed by
250ms actual digital silence. References identify damping initiation, so
release times do not silently move truth to a later sample.

Octave schedules are200→650→1100ms; fretted schedules200→550→900→1250ms;
the damping-only source starts200ms and damps at750ms. The complete plan was
written before compiling/running and remains `predeclared-counterset.json`.
The generator never observes detector results.

Both variants acquire **all64/64 wanted references**, with zero extra Ons,
misses, premature/late releases, orphan Offs, duplicate Ons, hangs or overflow.
All four complete counter CSVs(events, references, status, capture metrics) are
**byte-identical** between variants. Thus every causal On/Off, pitch event and
status value in these counters is preserved; no newly hidden low-register miss
or added decision delay occurs in this specific set.

The actual measured delays still matter. This is not an absolute low-latency
approval: first/changed-note decisions range13.31–63.31ms; release delays
range17.98–60.65ms. Here are all transition/damping observations, identical for
baseline and candidate:

| Lower root/mode | Octave-up On delay | Octave-down On delay | +2/return On delays | Damping release |
|---|---:|---:|---:|---:|
|31/plain|51.31ms|53.98ms|63.31/59.98ms|52.65ms|
|31/bend|45.98ms|53.98ms|63.31/38.65ms|60.65ms|
|35/plain|24.65ms|43.31ms|57.98/57.31ms|49.98ms|
|35/bend|43.31ms|48.65ms|47.31/33.31ms|57.98ms|
|40/plain|24.65ms|35.31ms|41.98/49.31ms|44.65ms|
|40/bend|21.98ms|51.31ms|47.31/41.31ms|49.98ms|
|52/plain|51.31ms|21.98ms|47.31/27.98ms|41.98ms|
|52/bend|24.65ms|45.98ms|17.98/19.98ms|44.65ms|

The one-to-one matching requires20ms usable overlap and retains every
unmatched interval. Early release is<-10ms, late release>80ms. These are
descriptive existing release criteria; no absolute onset-latency gate is
invented. `actual-latency-by-case.json` includes initial notes and every actual
sample decision/Off remains in `counter-comparison.json` and raw CSVs.

## Limits and disposition

This is positive bounded evidence for the candidate's physical check change:
it fixes the reproduced quiet plain-mode alias without blocking the32 true
change/damping cases or altering their timing. The parent's separate functional
Pitch/Guitar tests and complete native product checks remain necessary.

These synthetic integer-note counters do **not** erase earlier cold near-mains
detuning failures at31+40cents,43+40cents and47−40cents, or establish performance
for every low-register/timbre/noise condition. Those earlier15 misses remain
in the separate876-row audit. This counterset also does not replace recorded
human legato/bend evidence or the existing noisy mixed-source regressions.
There is no claim of musical perfection, Windows/Live execution or physical
audio-interface latency. Neither calibration lifecycle nor processor/UI routing
is independently exercised by this standalone test.

## Retained proof

`baseline/` and `candidate/` contain frozen Source, compile logs and complete
counter outputs; candidate also retains all24 quiet outputs. All N/F/P and
per-block status/meter values are available. `reused-quiet-baseline.json`
identifies the untouched earlier24-case source/driver/raw evidence by hash.
`measurement-manifest.json` records actual compile commands, source hashes,
successful exits and source stability. `candidate.patch` is the exact code
delta. `manifest.json` binds text/source records; binaries/objects are excluded
from that inventory and their executable hashes recorded separately.

To reproduce in fresh output directories, compile either frozen variant with
`counter_capture.cpp` and its eight Source/Pitch CPP files, then run the
executable with a new output-directory argument. `quiet_capture.cpp` uses the
original fixture-directory/output-directory arguments. `analyze.py` documents
the interval matching and paired analysis; preserve existing outputs when
repeating. Shared CMake/Ninja state was not used.
