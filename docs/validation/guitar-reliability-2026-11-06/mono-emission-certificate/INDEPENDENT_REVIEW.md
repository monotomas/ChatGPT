# Read-only causal review of deferred mono repick emission

Reviewed 6 November 2026. **Reject the current composition for production.**
Removing the 17 DEV repeats and retaining quiet 14/16 does not justify the
additional 69 repick in the controlled plain SNR24 gain1/gain4 cases whose
physical reference is 57. The baseline's earlier wrong 69 does not make an
additional Note On correct.

The reviewed final source is `scoped-with-root-frets/Source`.
`Pitch/PitchDetector.cpp` SHA-256 is
`69738995571f05a389ba2915338a1a74b4f8ca2cad87311d0ec54847e5e82273`;
YIN is `d218c5860878fc722f231edafd5a6ad0ea5e65ba52fba6f752ff89ad0b00b7fa`
and NoiseConditioner is
`5f68d01a8cd4202dab066aac99b8d6067d055caa5e7bd9fb08766d5cde7862a1`.

The causal coupling is explicit in the code. An innovation edge immediately
resets pitch history/confirmations and arms `phaseAttack`; that provisional
state clips YIN's `freshCount` and primary observation before emission is
certified. `ownedSameAnchor` can then defer the original poor-quality veto by
agreeing with the same already-held alias. The RAW certificate searches only
within 0.8 semitone of that prior. At held 69 it cannot test the physical 57
root. High normalized lag correlation, harmonic fit and within-window phase
coherence establish current periodic support; they do not independently
establish a new excitation. An unchanged even-dominant 57 series can support
all three near 69. The certificate assesses current audio with `held=true` and
`excitation=false`; it contains no independent pre/post excitation comparison.

The owner's frozen trace confirms this at sample 62847: the original quality
veto is true, ownership is true, pending phase age/primary is 218 analysis
samples, and the false 69 Off/On is emitted. The narrow RAW track actually uses
96 recent samples (8 ms), with correlation 0.9903275967, harmonic fraction
0.9947906137 and coherence 0.9995455146. The contemporaneous onset ratio is
1.063 and innovation ratio 0.0928; the earlier provisional edge supplied the
pending permission. Both full-512 RAW and post-dynamics YIN return pitch
68.97544098 with confidence 0.9120752811. The two trace versions reproduce the
same six event payloads. The narrow track correlation and global YIN
confidence are different statistics; their numerical difference is not solely
a window-length effect.

Thus a full-window root-agreement check does not veto this regression, and
decoupling the pending phase window alone is not a demonstrated fix. The
minimal causal prevention is to keep a provisional edge from changing pitch
acquisition/confirmation state or gaining Note On permission through a
continuity-only check of its own prior. Source ownership is necessary for
deferral but insufficient evidence of re-excitation. No validated replacement
certificate preserving the reported quiet-note benefit is established here;
raising confidence thresholds or adding another window policy would be new
algorithm work. Freeze this composition as negative evidence.

This review read the final source and the owner's
`scoped-counter-trace/counter` and `counter-raw` captures. It performed no new
capture, compilation, audio processing, source edit, or threshold experiment.
