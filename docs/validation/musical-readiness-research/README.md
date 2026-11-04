# Research evidence; release blocked

These are development and falsification records from November 4. They preserve
failed hypotheses and source snapshots. They do not approve the working tree,
an installer, musical readiness, Ableton integration or physical latency.
`research-provenance.json` binds the retained files to their exact hashes.
Compiled experiment executables and the complete third-party Live manual are
excluded.

`poly-audit/AUDIT.md` records independent, oracle-seeded tracking probes. Seeds
deliberately supply correct initial voices to test the continuous tracking
model separately from admission. The results expose shared-harmonic bend
deadlocks, adjacent-voice coalescence, stiffness bias and execution deadlines.
Its checkpoint17 source, raw traces and identity errors are retained. Generic
nearest-pitch error summaries that omit unmatched pitches must not replace the
identity-specific errors, which include the complete failed bends. An exact
Gram optimization was validated independently; that numerical success does
not solve the acoustic failures.

`noise-probes` contains component probes using previously disclosed real
isolated notes with artificial mains/noise. Warm-up and cold-start results
have different scope. Hum-before-pluck success cannot be generalized to a
quiet note starting simultaneously with unknown hum. Optional mains-priming
experiments are unaccepted. Actual recorded handling/squeak coverage is not
supplied by these constructed mixtures.

The early simultaneous-onset probe later proved flawed: it mutated truth
onsets inside its gain loop. Its aggregate result and later-gain timings are
withdrawn. The corrected, independently trimmed runner and the explicit
historical-data warning are retained in
[noiseprobes](../noiseprobes/README.md). Use that archive for reproducibility;
the older copy here remains historical evidence of the failed experiment.

`host-audit` records the pinned manual and JUCE source evidence behind MPE
output feasibility and the resulting channel/range/lifecycle review. The
router owner subsequently addressed the concrete protocol defects. The
historical review is preserved; it should not be read as an assertion that
those defects remain in the latest router. Native receiver/VST3 harnesses do
not execute Ableton Live or verify chord-channel preservation after Live
enables MPE.

Some original report paths refer to `/tmp` experiment locations. The matching
source snapshots and textual/CSV records are retained here with their original
relative hierarchy. Current independent controlled and recorded-input scoring
lives under `Tests/AudioFixtures/readiness/`, with separate source snapshots
and explicit seals. None of the reports should be silently updated to describe
a later implementation.
