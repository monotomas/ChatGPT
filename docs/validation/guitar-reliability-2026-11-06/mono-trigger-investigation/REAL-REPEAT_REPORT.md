# Real DEV staccato-repeat diagnosis: all tested repairs remain rejected

Baseline: e235c8a098dadce6e0a4b8bd6ca9de789a5d174d, before root's separate safe plain-fret patch. No production Source/test/fixture/scorer bytes were changed by this agent. All variants are isolated under `/tmp/flysnapper-v4-real-repeat-audit`; full raw records/snapshots/drivers remain there. This report does not grant a release or musical-readiness claim.

## Evidence boundary and authored asset

Only the seven DEVELOPMENT plucks were captured; prospective reserved seven detector inputs remain unrun. This agent accidentally viewed the full independent annotation JSON once (including reserved descriptive metadata); this was disclosed to root. Therefore reserved metadata is not claimed fully hidden, although reserved waveforms were not loaded by this agent and no reserved detector output was examined or produced.

Target is the author-mapped isolated staccato asset `Samples/green/stac/staccato_e4_rr3.wav`, nominal MIDI52, source SHA256 `8c9d3d8a8a9a1880d44c83d4f966bd0de0139b685df244e7b4ac8e4afe8108f8`; exact 24bit→float copy SHA `97db48102811372e848495600d374bc112773ca449c3ba475f19739dc12797a7`. Author SFZ region maps that single sample to key52. It does NOT document a physical pick count or per-recording damping time. The report consequently calls the symptom repeated nominal NoteOns within one authored isolated asset, not independently proven physical repicks/releases.

The original waveform has one dominant attack around22ms. Its20ms RMS falls from about−29.8dBFS at210ms to−45.7dBFS at240ms, then a much quieter body/tail persists. This is descriptive acoustic withdrawal, not newly invented physical damping truth. All observed repeats occur in that later tail. `baseline-real-repeat-diagnosis.png` shows original RMS and causal output/analysis state; the centered offline RMS plot is never fed into the detector.

## Exact baseline reproduction

A private debug snapshot exposes internal state and adds trace-only fields. ALL eight target profiles (plain/bend × dynamics off/default × cold/350ms idle) reproduce the retained558 events EXACTLY, including pitch events. Typical cold bend/dynamics-off output is On52@74.649ms, same-sample Off/Ons317.324,402.653,487.982ms, then finalOff607.982ms. Exact sample timestamps are authoritative in events.csv; there is no onset backdating. The repeats are explicit phaseAttack retriggers, not release/reacquisition gaps.

Before the energy withdrawal, pitch confidence is about.995 and harmonic phase coherence about.999. In the later body/tail, confidence is typically.93–.96 and phase coherence.91–.96 while accumulated relative-phase excitation is often saturated at1. Changes in normalized harmonic power alone are not the principal trigger: powerChange near many repeats is below.14, whereas phase departure is about1rad. The real-file aggregate baseline has56 profiles:38 pass bounded note/core checks,17 repeated nominal Ons,6 nominal-anchor disagreements, one nominal positive-core miss. The separate effective-pitch audit is required before interpreting a bend-anchor disagreement as an audible miss.

## Lifecycle proposals falsified before adoption

Three causal withdrawal-state proposals suppressed all17 DEV repeats and raised bounded DEV passes38→46/56, leaving first decisions and final source-relative Offs unchanged in the target. They were STILL REJECTED. A separately predeclared controlled counter holds a quiet oscillator after20dB withdrawal, then applies a genuine independent-phase same-note excitation without an amplitude increase. Baseline recognizes14/16 cases (the two MIDI40 bend cases already fail). Unconditional post-withdrawal phase veto recognizes0/16; ongoing high-confidence bypass7/16; arming-only high-confidence bypass10/16. Hiding those genuine notes is unacceptable. No old107/step acceptance claim is made for already-failed lifecycle candidates.

## Raw-reference topology ablations

The original mono innovation compares POST-dynamics waveform error with a RAW host-envelope denominator. Three separate ablations use the existing NC-cleaned pre-dynamics `rawReferenceRing`, never pre-NC rawAnalysisSample. The poly waveform/normalization branch is explicitly retained; this investigation makes no new poly execution claim.

| Variant | Old107 OFF | Old107 DEFAULT | DEFAULT extras / early | DEV repeated nominal Ons |
|---|---:|---:|---:|---:|
| baseline |107/107|104/107|1 /0|17|
| raw phase tracker only |107/107|104/107|1 /0|17|
| raw innovation only |107/107|107/107|2 /1|19|
| raw phase + innovation |107/107|107/107|2 /1|20|

Raw phase-only output is byte-identical across every legacy stream and all20,911 DEV events. Raw innovation recovers the three FINAL repicks missed with DEFAULT dynamics: continuous_repick_52_mono and confirm_articulation_02/03. This differs from the earlier unified-plain-phase experiment, which lost middle repicks with dynamics OFF.

Raw innovation nevertheless adds a false same-sample40 Off/On183.979ms in hidden_recorded_01_mono (original79.979ms On and743.979ms Off). Broad bend recorded-noise extras50→52 and early Offs27→28; bend controlled extras8→9; plain noise extras78→79. These are material regressions, so unconditional raw innovation is rejected.

The proposed narrow flag-only repair—clear monoTrackedOnly after adopting a valid independent raw-reference global YIN candidate—has byte-identical old107 outputs and does not recover DEFAULT104. Diagnostic traces show zero adopted-reference/monoTrackedOnly overlaps on the examined missing-repick cases. Thus that logical hypothesis does not explain these actual misses and is not adopted.

## Normalization versus period bias

Optimal raw local period tracking before the false E2 event is extremely coherent (> .99); the global YIN prior differs by only roughly−.23..+1.24cents. Refining the innovation predictor period alone preserves14/16 quiet counters and107/107 legacy references but DOES NOT remove false184ms. This weakens period bias as the cause of that event.

The actual RAW innovation arm occurs at157.3125ms, before the eventual183.979ms On: smoothed numerator .00477627 divided by fast host envelope .00945476 yields .50517, crossing the unchanged.5 threshold. The existing period-smoothed onsetEnvelope is .018945, which would yield .25211 at the SAME state. Envelope attack/release constants are.6/2ms, innovation smoothing1.5ms. The larger residual at waveform minima is therefore a concrete bandwidth/normalization defect.

Replacing the denominator outright with onsetEnvelope removes that E2 false event but creates a false same-note68 Off/On239.979ms in confirm_recorded_08_mono. At higher pitches its mean level can be below the old peak-following envelope, inadvertently amplifying the ratio. The parameter-free monotonic floor `max(envelope,onsetEnvelope,gate)` prevents that amplification.

The RAW-innovation + monotonic-floor candidate preserves all107 references in BOTH dynamics states, retains baseline14/16 quiet counter cases, removes the old OFF E2 extra/early release, and restores DEFAULT's three final repicks without a new legacy extra. Its legacy aggregate becomes one existing startup error and zero early releases. However complete falsification STILL REJECTS IT: DEV repeats17→19; broad bend noise extras50→53/early27→28; controlled bend extras8→9. Plain noise78→77 improves slightly but does not cancel newly failing cases. `raw-onset-floor.patch` is explicitly a REJECTED private candidate, not an approved production patch.

## Conditional-certainty ideas are not yet a repair

Before the strong hidden E2 false event, the existing raw highlyPeriodic predicate (.985 confidence/.95 phase coherence/.4 harmonic fraction) is TRUE in all34 callback snapshots145–190ms: raw correlation.990–.996, phase coherence.991–.997. Thus conditioning raw innovation alone on high periodicity cannot distinguish that false event; real phase resets actually lose periodicity briefly.

The new floor-specific broad regressions occur with current raw confidence below that predicate (.82–.96), so a current-predicate conditional floor would exclude them. But caching that source certificate for a physical period also admits known false disturbances: E2 muted-string arm463.979ms has certificate age12.0ms versus period12.11ms; A3 percussion arm454.646ms has age4.0ms versus period4.45ms. Longer grace windows only retain those contradictions. No cached conditional candidate was coded or threshold grid searched.

All controls retain genuine-note misses, extras, release errors, raw causal times and shared-worker deadline observations. No physical audio-interface/Live latency, continuous human technique coverage or general intent classifier is claimed. The specific real-repeat symptom is now traced and reproduced, but this investigation has not produced an acceptable fix.
