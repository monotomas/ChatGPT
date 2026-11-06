# Static-poly failures and rejected source-model experiments

The user rejected Windows0.3.0 for double triggers, missed notes, unplayed lower
octaves and latency. This is **development evidence, not an approved engine,
installer or musical-readiness result**. No runtime Source file was changed by
this investigation. The root agent separately added an actual-processor
regression and a monophonic fix; neither is represented as this private poly
candidate's source identity.

## One clean example reproduces the problem

`drivers/probe.cpp` creates standard open-guitar notes40/45/50/55/59/64, with
onsets80/87/94/101/108/115ms. Each source has eight partials,2ms attack,1.1s
exponential decay and an authored600ms cutoff. There is no added noise or later
excitation. This is constructed audio, not a recorded guitar or physical latency
measurement. The dynamics-enabled and disabled central0.3.0 event streams agree.

The exact e235 baseline emits40 at167.979ms,50 at178.646ms and55 at183.979ms.
It omits45/59/64. At279.979ms it sends Off40, On40 and the unplayed On43;43
persists until738.646ms. Note40's decision latency is87.979ms from its own
80ms onset. Thus a clean waveform already reproduces all four symptoms.
The parent also reproduced this stream through the actual plugin processor;
this archive itself measures only the standalone detector.

MIDI43 is a lower octave of played55, despite being above the lowest played40.
The initial exploratory below-lowest-note check was too narrow; the retained
analysis explicitly distinguishes that check from unplayed octaves of any true
reference. The330 baseline contains32 such unplayed lower-octave Ons in20
configurations. Neither this definition nor its counts declares the octave
association a proven physical source identity.

## Measurements and scope

The330 matrix is11 single/dyad/chord constructions ×5 common detunings
(-40/-20/0/+20/+40cents) ×3 harmonic envelopes ×2 dynamics settings. There
are810 intended one-time note identities across it. The envelopes are normal
1/h^1.2, odd partials reduced to15%, and differential partial decay. They are
now disclosed/seen regression controls. They are not330 independent recordings.

The summaries' historical `perfect` field means **onset identity completeness**:
every intended pitch occurred once, with no other pitches. It does not check
release timing, usable duration, latency or a predeclared complete acceptance
policy. Its values must never be relabelled as full musical case passes.
Misses, different-pitch extras and same-pitch repeats are reported separately.
Raw Offs and actual decision samples are retained. No event is backdated.

| Method | Missing identities | Different-pitch extra Ons | Same-pitch repeats | Onset-complete /330 |
| --- | ---: | ---: | ---: | ---: |
| e235 baseline |320|162|26|136|
| Five observed-tuning banks |208|200|42|160|
| Tuning held while voices sound |210|210|40|154|
| Explicit standard-guitar assignment only |270|146|24|136|
| Stable tuning plus guitar assignment |170|188|36|148|
| Soft envelope atoms, held prediction and fresh repick window |298|329|2|90|
| Restored measured-root physical evidence, no upper-anchor veto |266|221|2|112|

The soft-envelope held method improves the66 nominal0cent configurations from
38miss/44extra/12repeat to24/6/2, and fixes the central six-note example. Across
all detunings it is clearly insufficient. Nominal success must not hide its
off-grid failures.

The separate measured-root factor restores the original .05 residual-fit and
.035 final-joint-fit fundamental evidence, measured before profile amplitudes
are imputed. It preserves original physically verified anchor and missing-root
exceptions without reinstating blanket upper-anchor pruning. On the30 upper
triad {52,55,59} configurations, misses/extras improve46/40 to12/10 and all30
phantom40 lower-octave Ons disappear. On the30 dense open-string configurations,
misses/extras regress80/18 to88/20 and four lower-octave Ons appear, versus zero
in that held-atom family. The central nominal six-string example remains correct.
The full330 factor is therefore also rejected; the family regression is retained
in `summaries/guard-targeted-analysis.json` and the full event scoring in
`summaries/measured-guard-full-analysis.json`.

The132 additional constructions use per-string offsets -7/+3/-2/+8/-5/+1cents
and positive per-source inharmonicity. The108 standard-guitar configurations
include barre F/B, G/Am/D shapes, the24-fret register, three octaves and later
added octave/near-third notes. Baseline108 results are126miss/72extra/24repeat;
held-atom candidate results are222/157/14. The remaining24 bass/drop-D controls
are retained separately: the forced standard-guitar prior is explicitly
inapplicable to them. Exclusion is never counted as a pass and the production
Generic/bass/drop-D engine was not silently changed.

## Causal mechanisms and falsifications

* A512-sample observation at12kHz already costs42.7ms. Six or ten32-sample
  confirmations add16 or26.7ms. A smaller host buffer cannot remove that floor.
* The legacy dictionary fits exact MIDI frequencies; moderate detuning increases
  misses and false models. Observed common-tuning banks improve recall while
  worsening extras/repeats, so neither bank version is approved.
* Disabling anchor pruning leaves the central bad On stream unchanged. Removing
  octave-envelope fallback loses the wanted50/55 without fixing false43.
  The fallback is also one route by which real upper notes escape hidden lower
  groups. These ablations were30 open-string variants, not full330 approvals.
* A standard six-string instrument prior requires jointly feasible one-note-per-
  string assignment over tuning40/45/50/55/59/64 and24 frets. It rejects
  simultaneous40/43 without globally forbidding43. The constraint must also
  apply in octave fallback; the earlier bypass version is retained. This is
  only an explicit instrument-model experiment, not a new production setting.
* Twenty-seven time-domain power-law/parity/pluck-position atoms constrain each
  selected group to one scalar amplitude with causal residual-derived phases.
  A50% soft blend avoids replacing every envelope with one power law. Scalar
  joint fitting prevents the free16-coefficient solve from undoing the shape
  constraint. The unconstrained-range version nevertheless chooses false28/31
  common-divisor groups in the central example and is rejected.
* Reducing the new-source relative cost from.102 to.02 helps contained B3/E4
  residuals only with the new source model. The old guitar model at.006 still
  misses those notes. Neither a lower internal cost nor a more flexible model
  alone is an acceptance result; no frozen acoustic policy was relaxed.
* Initial polyAdmission is extended120ms until the first sounding voice. It
  permits pending **new** voices; it is not independent evidence for repicking
  an already sounding note. Global1ms amplitude crests from beating can cross
  the attack ratio even with no new excitation.

The selected-model trace isolates a separate feedback failure. At frame3744
(internal12kHz samples) the six correct groups have free-joint relative fit MSE
about.00039. At frame3776 the instantaneous selection switches to40/45/54/55/62,
dropping real50/59/64, while the published six voices are still held. Relative
MSE jumps to.07227 and a false predictor innovation fires at3788. The next
frame restores all six correct models, but the120ms admission window persists
and a beat later retriggers45. Prediction must therefore distinguish a held
source hypothesis from one transient classifier selection. The retained
`trace-selected-time-atoms` source and log show the full sequence.

Fitting the **actually held**, bounded six-source model for prediction and
separating short fresh repick evidence from new-source admission fixes the
nominal central example. It does not make a complete articulator: the genuine
repick controls below reject it.

## Genuine chord repicks and gain changes

**Counter-construction audit:** The first (`chord_repick_probe.cpp`) edition
independently hard-cuts the old excitation600ms after its own start while the
new excitation continues. That artificial old-component withdrawal at680–715ms
is not a separate authored pluck and can create extra phase changes. Its original
source, labels and failures remain intact. Late On associations in that edition
cannot all be attributed to the desired second gesture. The corrected explicit
v2 (`chord_repick_probe_v2.cpp`) lets both components ring and damps the same
string's old/new components together at1000ms plus the string's7ms offset.
Neither edition is a new recorded performance or a rewritten old fixture.


`drivers/chord_repick_probe.cpp` supplies four families: one held six-string
strum, two actual six-string strums, two five-string Am strums, and a held strum
with an80ms positive gain ramp. The second physical excitation starts400ms,
again spread7ms per string. Later held MIDI cannot satisfy a later authored
repick reference without a fresh On within that reference window.

There are48 technical configurations. Held and gain-ramp families each repeat
the same constructed input across12 parameter combinations; they are not12
independent performances. Each real-repick family uses three phases ×two
new-excitation gains ×two dynamics settings, six constructed inputs each.

| Repick method | Extras on held control /12configs | Any second-reference open On /72 | Extras during those real-repick configurations | Extras on gain ramp /12configs |
| --- | ---: | ---: | ---: | ---: |
| e235 baseline |24|16|30|84|
| Held prediction + fresh global window |0|8|8|12|
| Per-partial phase v0 |108|62|208|144|
| Phase v1: conditional-set reset + local On permission |24|48|198|84|
| Phase v2: full conditional covariance + gesture-state reset |0|34|20|24|
| Phase v3: temporal ridge mean + positive common-gain nuisance |12|34|30|12|

The preceding long-window associations are not usable-onset acceptance.
Within120ms of the actual second excitation, the baseline/held-window/phase-v2/
phase-v3 counts are only4/0/6/12 of72. Even that120ms descriptive bound is not
a musical-latency approval. `summaries/repick-v1-timed-counts.json` retains the distinction.

The corrected v2 counters reject the prototypes too: baseline finds14/72 second
open-string Ons, phase-v2 finds8/72 and the waveform-NULL guard finds10/72.
The NULL guard removes gain-ramp false Ons (72/72 intended identities and no
extras), but misses62 of72 genuine second open-string gestures and54 of60 Am
gestures. Those are not suppressed or counted as sustained success.

The bounded NULL experiment transports the preceding held total-wave fit and
regresses the current past window on a positive quadratic common gain plus DC.
It records before/after RSS and uses the existing factor-four innovation margin
against preceding stable model precision. Its causal trace explains all151 held
frames and76 gain-ramp frames in focused intervals. During a real400–550ms
repluck interval,29 of57 frames are unexplained while28 are explained as the
wave settles. This is useful mechanism evidence, not a working articulator.
`variants/time-atoms-phase-null-guard/captures/chord-repicks-corrected-v2/null-rss-diagnostics.json` and the full NULL trace retain the actual values.

The gain-ramp false events in phase-v2 were traced to the **source-specific**
phase path: at12kHz clocks6208 and6464, sourcePhase=1 while globalPhase,
freshWindow and polyAdmission are all0. The apparent changes are in h5/h7
coefficients, while h1/h2 remain nearly stable. Thus the new phase observation
is not assumed reliable merely because a global amplitude gate was removed.
A true E2 phase event at5120 is separately blocked by instantaneous fresh-note
classification until5760; that53ms delay exposes a distinct ownership/state
problem. The same-source Off/On needs conditioned held evidence and atomic
commitment, rather than another new-source confirmation. A bounded private
ownership experiment improves corrected open repicks10/72 to12/72, while extras
rise6 to8; Am remains6/60 with26 extras. Held/gain controls remain clean. The
observed E2 Off/On advances53.333ms to26.646ms after its own second excitation.
That isolates avoidable state latency without making the articulator usable.
The original owned capture completed all48 configurations, but its tool session
was lost at the environment reconnect before final process-status recovery; its
complete raw event file and this limitation are retained.

All of these are rejected for production. The nominal held success and higher
true-repick recall do not excuse false retriggers, lost real gestures or the
still-bad five-string Am acquisition. Phase histories must not compare
coefficients across different held-model conditioning sets. The same-source
Off/On also needs local authorization; it must not depend on an unrelated
fresh-source window or instantaneous composite amplitude crest. v2/v3 use a
source-specific phase route for multi-source repicks; diagnostic traces verify
the actual branch rather than inferring its cause from aggregate counts.

A lifecycle check rebuilt phase-v2 with a fresh PitchDetector for each of the48
original counter configurations: its event CSV is byte-identical to the reused-
object capture. Clock/held-set guards worked in that harness. The original330/
132 probes explicitly reuse prepare(); fixed dictionary tables were immutable.
No result here is silently used for the separately mutable fine-F0 experiment.
The NULL prototype additionally exposes an explicit tracking reset without
rebuilding immutable tables, for later host lifecycle work.

No real-recorded DEV expansion or Windows release was justified after these
controlled falsifications. The independently running fine-frequency experiment
uses a separately frozen snapshot and is not silently included here.

## One observed confidence-gap loss

The focused G3 trace in `variants/phase-gap-instrumented/captures/REPORT.md`
uses exactly one corrected-v2 real strum and the unchanged NULL-guard model.
It observes the same six held identities throughout. The renderer's near-opposite
new G3 component cancels most root power briefly; the fitted root still records
about2.284rad of phase change over the transition. Current residual-derived
uncertainty makes three root observations unreliable. The state clears a
.804rad accumulated departure, overwrites phase with those unreliable angles,
and discards about.731rad of real phase movement at reacquisition. H2 retains
its reference, but the two above-threshold departures no longer coincide.

Conditional coherence/conditioning pass for this root, and the waveform NULL
is unexplained throughout the missed gesture, so neither a held-set reset nor
a NULL veto accounts for it. This trace supports one bounded state experiment:
retain trusted pre-event phase through a confidence gap inside the existing
clock-gap bound, and compare the recovered reliable phase with its transported
trusted reference. The trace itself contains no such fix or success claim.

## Final bounded factors and decision

The trusted-reference bridge implements exactly the confidence-gap hypothesis
above, within the unchanged128-sample bound. It never updates phase from an
unreliable angle. Its compile and corrected48 capture both exit0. Independent
matching finds240/276 initial references unchanged, second references16 to18
of132, and all256 commonly matched On samples identical. It recovers the two
G3 gestures in the traced input at45.646ms after the second excitation.

The same factor raises extras32 to60. Each held/gain family adds12 false E2
Ons50.646ms after authored damping, rather than during the positive gain rise;
real open-strum extras rise6 to10, and Am remains inadequate. The independent
report, all408 reference timings, extra events and reproducible audit are under
`variants/time-atoms-phase-null-trusted-bridge/captures/`. The hypothesis was
causally useful for one lost gesture but the factor is rejected; no further
threshold search or phase family was launched.

One separate final330 run applies only restored measured-root evidence to the
independently parity-checked uncertainty-F0 plus profile-fast input. The latter
source and original full-payload capture are retained as `uncertainty-fast-input`;
the capture is a copied prior execution, not a second run. The new factor retains
the original mutable frequency model and uses a fresh detector for every
configuration. These are cold-configuration results and do not approve the
known frequency-cache prepare/reset lifecycle. No phase observer is compounded.

| Cold330 factor | Missing identities | Extra Ons | Repeats | Unplayed lower-octave Ons | Onset-complete |
| --- | ---: | ---: | ---: | ---: | ---: |
| Uncertainty F0 + exact fast kernel input |138|78|4|44|214|
| Same input + measured-root evidence |134|48|10|10|232|

The useful-interval audit, requiring20ms of real overlap, improves672 to674
of810 references; two new ever-On identities are too short to count. Early
first Offs rise16 to20; late first Offs fall290 to266. All Offs/voice IDs/
velocities and actual samples are retained. No events are backdated.

The upper triad improves8 misses/32 extras to0/4, with all30 phantom lower
roots removed. The {45,64} dyad regresses0 misses/6 extras to10/16. Dense six
open strings improve48 misses/20 extras to40/10 across180 references, still
missing40. All ten remaining lower-octave extras are unplayed52 in that dense,
even-dominant family. The nominal central six-note case is complete, but its
actual per-source latencies remain66.979–124.646ms. That success cannot establish
musical readiness. See `summaries/uncertainty-fast-measured-guard-analysis.json`
and `summaries/uncertainty-fast-measured-guard-family-audit.json`.

The exact five inserted guard blocks are audited by removing them to reconstruct
the parity-proved input byte-for-byte; the actual-input diff is retained. No
frequency-retuning or fast-kernel bytes were changed. The modified loop context
is documented rather than presented as a clean original-patch application.
No further132, recorded-DEV expansion, production poly change or installer is
justified by this rejected result. This concludes the bounded investigation.

## Retention and reproduction

`research-snapshots.json` binds the exact reconstructed Source trees to actual
retained baseline files and per-variant overlays. All baseline Source bytes were
independently compared with Git e235. `drivers/`, `recipes/`, `summaries/`, raw
captures and diagnostics retain the unsuccessful variants and parameter choices.
The aggregate reconstruction inventories were assembled **after capture**;
they are not labelled contemporaneous before-compile attestations. Original
per-variant source/compile/run records are also retained, with their stated scope. These are selected project inputs,
not a full compiler/system-header dependency closure.

No PCM, native executable or model weight is copied. Excluded executable hashes
are recorded. Reconstruct a variant into a fresh private directory with
`python reconstruct.py --variant <name> --output /tmp/new-private-variant`, then
compile a retained driver with `g++ -std=c++17 -O2 -DNDEBUG -I <variant>/Source`
and every `Source/Pitch/*.cpp` except `MpeOutputRouter.cpp`. That adapter belongs
to the separate MIDI core and is not in this detector signal path. Frame/block
settings in the probes are48kHz/64samples. The captures are causal cloud CPU
work, not real-time certification or an Ableton/audio-interface measurement.
