# Independent review of pending-repick phase guards

This is private research only. No shared source, production fixture, expected annotation or saved blind report was changed. No second-confirmation input was inspected. Exact source and evaluator hashes are in `manifest.json`. The baseline PitchDetector.cpp hash `e773f491...` matches committed source16. The two candidate implementations were supplied by the DSP lead.

## Controlled inputs and observed outcomes

`make.py` creates36 held/independently damped cases,48 expected notes. There are24 held single-note cases at MIDI40/52/64, two brightnesses, two phases, and stiffness B=0/.001;12 cases contain a lowerE2 continuing while an upper+12/+19 stops independently. Modal frequencies are `f*h*sqrt((1+B*h*h)/(1+B))`, preserving the actual fundamental. PCM/gain/crop/annotations are fixed before detector execution. These are small targeted controls, not a new blind acceptance set.

| Implementation | Matched /48 | Extra Note Ons | Early offs | Hung / overflow |
|---|---:|---:|---:|---:|
| Source16 baseline |38|26|16|0 /0|
| Fundamental-only phase guard |38|21|12|0 /0|
| Fundamental OR third-harmonic phase guard |38|23|13|0 /0|

The third-harmonic extension reintroduces two false restarts that the fundamental-only guard rejects. In `damp_upper59_B0.0_s904`, the unadmitted B3 ends at300ms while E2 continues. LowerE2 is falsely restarted at349.31ms. The fundamental phase distance is only.00055, but third-harmonic distance is.253. Withdrawing a component near three times the lower frequency genuinely changes the measured third-harmonic phase; that does not establish a new lower-note attack. The B=.001/seed101 counterpart adds another restart at253.31ms.

All24 held single-note cases retain6 false restarts across both guards. Instrumentation proves these restarts are phase-path events, not ordinary-amplitude-triggered emissions. In the steady bright inharmonicE2, the measured YIN period is142.745 samples while the physical fundamental period is approximately145.62. The cached nominal demodulation frequency is therefore biased upward by roughly34 cents. The unchanged physical fundamental accumulates phase drift across the48ms gap, producing distances.479 and.301 and passing the.08 threshold. The analogous E3 case produces distance1.071. Branch traces and actual emitted events are in `instrument/`.

The ordinary poly amplitude detector also arms in some of these controls (fast/slow ratios just over1.9), but the logged restarts above explicitly have `phase=1`. It would be incorrect to attribute the held-case extras solely to the ordinary amplitude path.

## Interpretation

The phase-distance threshold.08 corresponds to approximately.284 radians,16.3degrees. For a stiff string the third partial's natural frequency is not exactly3*f0. At small B its frequency offset is approximately12*B*f0, so ordinary pre/post delay can exceed this phase threshold without an attack. A YIN-derived period can itself be biased by stiff upper partials, so this is also an observed problem for fundamental-only demodulation.

Pre-event phase-velocity estimation/extrapolation is consequently needed for every tested harmonic, not only the third, if this design is extended. Two past windows could estimate residual phase drift and its uncertainty. Actual resolved partial frequencies are another possible reference. Short windows, leakage and changing modal balance limit prediction precision; neither approach has been implemented or validated in this review.

Requiring at least two independently observed coprime odd partials among3/5/7 to change could reject the observed single third-harmonic withdrawal, but nominal h*f0 comparisons would still mistake stiffness drift for an attack. It needs calibrated pre-event phase trends and meaningful relative-amplitude/SNR checks. The current absolute gate floor alone does not establish independence or source identity. A real repick with coincident fundamental phase and withdrawal of an unadmitted upper source can remain observationally ambiguous.

## Bounds and causality review

- The pre-arm projection reads the ring over approximately[-3P,-P), after checking that the history is filled. P is clamped to[2,1024/3]. Counts/indexes remain within the fixed ring; adding2N before modulo keeps indexes nonnegative.
- The post projection reads approximately2P already received samples. Its time origin `pendingAge-n+1` agrees with the ring after insertion of the current sample. The minimum age max(576,ceil(3P)) keeps it strictly after the suspected event.
- Saturating age and the120ms cancellation bound the pending state. No future sample, allocation or unbounded iteration is introduced.
- Third-harmonic evidence is used only when3/P<.45, below the declared analysis cutoff. Its pre-arm projection is currently computed even when unusable, a small efficiency issue rather than a bounds defect.

**Recommendation:** the candidates improve some existing harmonic damping controls, but neither is a general repick guarantee. Do not approve the OR-third extension on the basis of those controls alone. Preserve the observed stiff-string and3:1 withdrawal failures, and choose a documented conservative precision/recall policy unless a bounded phase-trend design is independently verified. This review changes no production behavior.

## Subsequent pre-event phase-trend prototype

The DSP lead then supplied `phase-trend`. This uses two prior one-period projections for each available odd harmonic1/3/5/7, estimates residual angular velocity, extrapolates it to a fresh one-period post-event window, and requires changed fundamental or at least two changed upper odd harmonics.

I independently copied, compiled and ran this source against the36 controls. Every emitted event is byte-identical to the lead's saved result; source/binary hashes are in `trend-independent/manifest.json`. Outcomes:38/48 matched,9 extras and9 early offs, no hangs/overflow. No individual case gains any miss, extra or premature off versus baseline. The24 held singles improve6→3 extras and4→3 early offs: one bright stiffE2 and both stiffE3 phase seeds still restart. Thus the trend corrects a substantial part of the frequency-bias problem, but not all nonlinear changing-timbre/projection errors.

The formula is correct: `atan2(Im(conj(older)*newer), Re(conj(older)*newer))` estimates the signed residual rotation across `past-recent` samples. Newer-to-post displacement is `age+recent-n+1`; equal-length projection windows make centre offsets cancel. The implemented extrapolation ratio therefore has the correct sign and duration. Ring bounds, filled-history check, finite positive power checks and harmonic cutoff remain sound. The extra four harmonic loops are fixed and allocate nothing.

The lead's saved public81/seen69 counts retain source16 outcomes, and24 independent articulation cases retain45/48 matched with zero extras. Normal/bright/position/recorded-offset damping sets retain the reported improvements; the existing source16 misses remain visible. These observations make this prototype a materially cleaner follow-up than the fundamental-or-third comparison. They are not a broad correctness guarantee or a reason to delay an unrelated packaged baseline indefinitely.

Residual caveats: one-period windows are short at high notes, residual phase is circular, and extrapolation can amplify phase-estimation noise. The power check is absolute rather than a full relative-SNR/uncertainty model. A true coincident-fundamental repick and a multi-partial withdrawal can remain acoustically ambiguous. The prototype only filters an existing pending-repick path; unchanged or remaining failures must be reported rather than described as solved.
