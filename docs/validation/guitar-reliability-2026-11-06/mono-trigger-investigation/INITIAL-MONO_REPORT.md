# 0.3.0 mono failure audit — no production patch adopted

Frozen baseline is e235c8a098dadce6e0a4b8bd6ca9de789a5d174d. All production Pitch sources remain unchanged by this investigation. Snapshot headers expose private fields for debug tracing only; each translation unit uses the same snapshot, and all 12,474 bend-mode raw events across the complete 34 controlled +64 recorded-noise +8 hammer cases exactly match the retained 0.3.0 capture (including pitch events). No zero-event case is omitted from that equality check. Full inputs, modified snapshot code, drivers and raw reports are under `/tmp/flysnapper-v4-mono-audit`.

## Causal failures reproduced

* Acoustic A3 + muted-string recording at SNR12/gain40: reference note57 is held250–1350ms. Output On266.646ms, false same-sample Off/On1045.313ms, final Off1389.313ms. The predictor arms phaseAttack around1034ms. The short fresh observation initially drops currentPitch/history; the held raw-reference oscillator still has about0.94–0.95 local correlation, then a new3-frame confirmation causes the explicit retrigger. Plain mode adds another false472ms retrigger.
* Electric E2 + electric-percussion recording at SNR6/gain40: output On279.979ms, prematureOff490.646ms, reacquisition570.646ms (80ms gap), false same-time Off/Ons674.646 and783.979ms, finalOff986.646ms although the reference lasts1350ms. The raw-reference narrow correlation falls to0.19–0.30 through the first release, so an ordinary positive-period tracker cannot justify blindly preserving this voice. The complete NoteOn/Off timeline is identical with bend on/off.
* Fast35ms fretted slide in plain mode: 512 samples at12kHz is42.67ms, already longer than each physical note. `analyseMono` uses this fixed primary window in plain mode; only held bend mode uses three measured periods. Up and down each match2/8 reference notes. Up emits57→59→61→63→64, skipping all six interior references under the original useful-interval policy. This is a structural observation-window error rather than a gate threshold issue. The independent latency agent owns the isolated adaptive-held-window experiment.
* Broader plain controls reveal false HIGH octave57→69 on music/handling mixtures. These do not reproduce the user's specific lower-octave report, which must remain a distinct open defect; poly audit belongs to the other worker.

Relevant frozen source paths: `PitchDetector.cpp` arming in process(), analyseMono freshCount/primary/independentExcitation, phaseAttack evidence screen, and updateVoices explicit retrigger/release. `ArticulationTracker.cpp` PartialPhaseTracker treats projected phase and normalized power changes as excitation, with40ms held evidence. Plain mode originally never computes that independent phase evidence and skips its excitation veto.

## Baseline coverage gap

| Mode | Suite | Cases passed | Matched/expected | Extra Ons | Early Offs | Octave errors |
|---|---|---:|---:|---:|---:|---:|
| bend | controlled |29/34|77/77|8|3|0|
| plain | controlled non-bend families |23/31|62/74|24|6|12|
| bend | recorded noise |35/64|48/48|50|27|0|
| plain | recorded noise |30/64|48/48|78|32|1|
| bend/plain | recorded hammers |2/8|8/8|6|0|0|

Plain scoring excludes the three continuous-bend cases, marks bend disabled and skips continuous pitch-curve scoring; it preserves all note reference timestamps and the original numerical matching policy. It does not relabel a bent trajectory as a passed plain case. All outputs are supplied signal decisions at48kHz/64samples, not Live/interface latency. Corpora are already seen regressions. Shared-worker CPU/deadline observations are retained but do not establish realtime bounds.

## Bounded rejected experiments

1. `plain-phase`: compute the existing independent partial-phase evidence in plain mode and use the existing .75 excitation condition there. Bend outputs remain unchanged. Plain control extras24→18, octaves12→8; recorded-noise extras78→67, but early Offs32 stay32 and controlled note recall62/74 stays62/74. Paired old107 mono regression loses a genuine middle repick in BOTH confirm_articulation_02_mono and confirm_articulation_03_mono: baseline107/107→105/107. Reject.
2. `variance2`: augment the4-harmonic least-squares fit with residual-derived approximate phase/power uncertainty and require2σ resolved innovations. This is a bounded diagnostic approximation, not a validated statistical guitar/noise probability. Bend noise extras50 and early27 unchanged, controlled extras8→7 with77/77 notes. Plain extras slightly improve beyond plain-phase, but do not repair its ownership problem. No sufficient musical improvement; not adopted.
3. `variance3`:3σ version reduces bend noise extras50→46 but loses the last52 in hammer_pull_no_amplitude_attack_mono (77→76 total wanted notes) and adds a late release. Reject.
4. `plain-owned-octave`: plain-phase plus transfer of the existing coherent-octave guard to plain mode. Only one recorded-noise extra is removed beyond plain-phase; all its true-repick failures remain. Reject without spending further genuine-octave matrix work on an already failed precondition.

No blanket debounce, fixed lower-note ban, gate-threshold change, onset backdating, seal change or scoring tolerance change was used. These results do not supply a releasable mono replacement. A useful next candidate must improve held-source inference while preserving genuinely independent repicks, measured source withdrawal and fast fretting, and must be evaluated in BOTH plain and bend modes. Merely extending release or suppressing all re-ons would conceal the failures above.
