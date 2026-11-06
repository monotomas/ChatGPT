# Rejected per-note fine-frequency experiment

These private detector experiments are **rejected**. Production Source, tests,
scorer policy and fixtures were not edited; no installer was built. All audio
here is deterministic synthetic input. No real pluck or reserved recording was
run. The source base is private held-prediction work derived from exact Git
`e235c8a098dadce6e0a4b8bd6ca9de789a5d174d`, not current production HEAD.

The immutable preserved tree contains all original sources, drivers, scripts,
commands embedded in those scripts, compile logs, score reports, raw events and
timings, matrix checks, frequency diagnostics, profiling code and manifests.
`preservation-inventory.json` lists every original file and its SHA-256, including
43 excluded executables/objects. `results-summary.json` records full and
standard/outside-prior totals for every original warm and cold raw capture. Nine complete 34-file Source snapshots are
retained and independently matched to their original source manifests where
available. These are selected project inputs, not compiler/system-header closure.
No PCM, float audio, compiled model tables or binary is archived.

## Warm-cache invalidation and authoritative cold results

Original `runs330`/`runs132` captures reused one PitchDetector and called prepare
between configurations. PitchDetector skipped pursuit.prepare at the same
analysis rate; mutable fine-frequency tables survived. Those streams and their
original/corrected manifests are retained as **WARM**, including all earlier
reports. They cannot establish independent per-configuration performance.
The fixed-grid control has exact warm/cold event parity in both matrices.

The authoritative `cold330`/`cold132` drivers allocate a fresh detector for every
configuration. All 330 cases cover eleven chord shapes, five common offsets
(-40/-20/0/20/40 cents), three timbres and dynamics off/default. The 132 cases
cover eleven physical counterexamples, three offsets, two timbres and both
settings. The first108 rows/480 references are within the standard prior;24
rows/120 references are explicit outside-prior counterexamples. Standard
six-string prior intentionally excludes parts of the final
two counterexample shapes. No test selector or wanted chord enters the detector.
Actual causal event samples are scored without backdating.

| Cold330 count | Fixed grid | Weighted peak/h median | Unshared-partial median + uncertainty |
| --- | ---: | ---: | ---: |
| Missing wanted Ons | 298 | 158 | 138 |
| Extra Ons | 329 | 54 | 78 |
| Repeated wanted Ons | 2 | 6 | 4 |
| False lower-octave Ons | 37 | 38 | 44 |
| Early first Offs | 2 | 10 | 16 |
| Late first Offs (>80ms) | 204 | 266 | 290 |

All 810 references are retained. Useful-interval misses equal nominal misses in
this matrix. Cold132 median has184 nominal misses/190 useful misses,92 extras,
16 repeats,64 false lower-octave Ons and174 late first Offs. Uncertainty has
154/160 misses,88 extras,16 repeats,66 false lower-octave Ons and174 late first
Offs. Within the108 standard-prior rows, baseline has222 misses/157 extras/14
repeats/53 falseLOW/116 late Offs; cold median130 nominal/136 useful misses,
78 extras/16 repeats/52 falseLOW/140 late; uncertainty112/118 misses,72 extras/
16 repeats/54 falseLOW/144 late. Full baseline132 totals are278 misses,
169 extras,14 repeats,63 falseLOW and142 late Offs.

Full details, all events, unmatched intervals and both prior warm reports
are retained. Reduced chord omissions do not repair false notes, release timing,
repetition or realtime cost.

Fine F0 comes from the raw cleaned waveform before dynamics; processed waveform
fit/admission stays separate. The first median lets a strong merged fundamental
outvote isolated partials; the nominal open-string E4 is lost. The diagnostic
trace records requested and actual cached F0, ruling out top12 starvation.
At159.98ms E4's requested F0 is -18.4 cents; independent h4/h5/h7 are approximately
-.311/-.010/-.007 cents. The second candidate treats each unshared partial once,
uses their median and retains physical dispersion as uncertainty. It restores
the nominal central E4 but still fails the full matrices. Its44 cold330 falseLOW
Ons include30 phantom MIDI40 from upper triad52/55/59, across every timbre,
and14 phantom43/52 in the even-dominant open-string case.

The matrix audit checks analytic cross terms against actual generated basis,
inverse residual and recurrence basis against sine/cosine. Maximum errors are
1.35578e-5 /8.88178e-16 /5.95272e-8. At most12 models change per frame; measured
rebuild wall time is98.995us median/185.943us max. Passing algebra does not
establish musical validity.

## Actual thread-CPU profile

The cold profile uses30 open-string configurations and7241 analyse calls per
variant. Profile event bytes exactly equal the corresponding cold330 subset.
The preserved instrumentation includes its own overhead. Summed analyse thread
CPU is15.761801s fixed,22.400366s median and16.391980s uncertainty. Median spends
6.729352s retuning (59334 updates); uncertainty spends.814115s (5278 updates).
Each variant records7241 callback thread-deadline misses against the
1333.333us64-sample/48kHz budget, equal to its analyse-call count. Aggregate
counts do not identify individual analysed callbacks. The fixed profile
averages2.177ms per analysis hop. Independent exact
profile-loop optimization is separate work. Shared-cloud wall callback timings
are retained separately and must not be described as physical input latency.

## Reproduction and integrity

Run `python verify.py` from this directory to verify every copied original and
all full source-map bindings. Run `python reproduce.py --output /tmp/new-f0-run
--variant uncertainty-f0 --suite 330 --case 10` to compile the exact archived
source and repeat just30 open-string configurations. Omit --case for the full
matrix; `--suite 132` selects the physical counters. This script only compiles
standalone one-job C++ and never touches the shared native build. It compares
raw event bytes with the original cold rows and retains fresh timings. All
original historical scripts remain unchanged; prefer reproduce.py for a new
output directory because historical scripts contain original absolute paths and
expect their original scratch layout. Repeating prior disclosed synthetic
configurations is reproduction, not new independent validation.

## Separate rejected cache-lifecycle ablation

`lifecycle/` preserves one further private semantic experiment and all four
330/132 cold/warm captures. It is **rejected**, not the archived uncertainty
implementation intended for the independent exact-kernel combination.

Immutable nominal model/Gram backups are prepared outside realtime. Separate
fine-to-nominal and fine/fine cross terms resolve the actually selected model
frequencies. Per-frame use flags clear on every observation; full detector reset
clears both small use/cache-valid flags plus prediction, leaving MB tables
untouched. This experiment also explicitly removes previous-F0 hysteresis and
held smoothing, requests exact current-frame partial-median F0, uses nominal
frequencies for shared-owner masking, and falls back nominal for any currently
unselected note. These are semantic changes, not a silent optimization. The
simple correctness implementation adds large nominal/mixed Gram backups and is
not a compact-memory proposal.

The matrix audit passes2,182,464 scalar checks across96 nominal/fine masks:
cross3.65767e-6, inverse9.99201e-16, basis5.96012e-8, exact symmetry. Reset leaves
all large tables byte-identical. Null/empty evidence falls back nominal. Two
histories containing25 different prior fine caches produce identical current
strengths, confidence, model F0 and prediction without an intervening reset.
All330 and132 **complete warm event streams equal their cold streams byte for
byte** after the lifecycle repair.

| Count | Cold330/810 references | Cold132/600 references |
| --- | ---: | ---: |
| Nominal / useful misses |144 /146 |174 /178 |
| Extra Ons |68 |73 |
| Repeated wanted Ons |12 |14 |
| False lower-octave Ons |40 |53 |
| Early / late first Offs |28 /250 |38 /168 |

Compared with archived uncertainty cold330, nominal misses rise138→144,
repeats4→12, early Offs16→28; fewer extras/late releases do not repair those
regressions. The nominal central default open-string case finds all six wanted
notes but repeats once; first E2 is90.646ms late and other first Ons are
63.646–84.646ms late. Every capture has balanced events/no active EOF voices;
that basic lifecycle condition does not make the output musically usable.
Shared-cloud wall timings are retained; no new thread-CPU profile or physical
interface latency measurement is claimed. Source/test/scorer/fixture production
files were unchanged, and measured-root guard changes were not compounded here.

`source.diff` lists exactly three private changed files, and context/source
manifests bind their actual compiled bytes. The independent read-only review
and algebra/history audits are retained. `reproduce.py --variant lifecycle`
restores this exact private variant; its full original warm/cold run is in
`lifecycle/run.py`. Only the original330/132 synthetic drivers were used for
musical scores. No further algorithm grid was run.
