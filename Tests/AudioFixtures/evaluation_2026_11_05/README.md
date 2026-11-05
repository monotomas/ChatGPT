# Revised evaluation scope, 5 November 2026

The user now requests a well-tested build for evaluation with their real guitar.
Polyphonic bending is explicitly removed. Polyphony must still output separate
correct semitone notes; monophonic bending and articulation remain in scope.
Latency is optimized and reported by pitch, with no former absolute millisecond
acceptance cutoff. Gate threshold, hysteresis and release have a documented
noise/tail tradeoff, with learned starting values and manual adjustment.

This directory introduces a new dated policy; it does not edit the sealed
November4 recordings, labels, original policy or historical failed results.
`Tools/guitar_evaluation_score.py` uses the original acoustic interval matcher
with a documented view adapter: static poly notes are keyed by MIDI note,
continuous poly pitch curves are not scored, and seven historical poly bend
configurations are listed as outside scope rather than passed. All remaining
controlled acquisition, slide/hammer, noise and release cases remain scored.
Mono curves retain their full original criteria. Unexpected poly pitch events
are a failure. Each missed expected note stays in the per-pitch timing table;
reported delay distributions are explicitly conditional on matched notes.

The broader old corpora are diagnostic regression evidence, not a promise that
an amplitude gate can separate every wanted note from equally loud, overlapping
pitched percussion. Their per-case accuracy/release failures remain failures.
Mandatory automated checks include the unchanged frozen30 packaging gate and
native integration groups, including actual-processor frontend/calibration
controls. Passing these bounded controls prepares the requested real-guitar
evaluation; it does not establish universal transcription quality or replace
the user's subsequent real recording/environment observation.

The final capture runner supports `--frontend off` and `--frontend default`.
It delivers raw PCM directly to the detector. The latter profile configures
the detector's actual integrated dynamics after its original DC filtering,
lowpass/resampling and stationary-noise conditioning, at the analysis rate.
Actual sample-level gate admission remains inside that implementation; the
driver does not approximate it with an external conditioner or a whole-block
permission. It is a standalone signal-path measurement before MIDI
correction/routing. The separate
`FlysnapperDynamicsProcessor` test executes the actual processor, calibration,
host parameters, correction and MIDI serialization. Its synthetic20-second
calibration profile is stated in the report and is not fitted to the recorded
noise evaluation fixtures. No actual audio interface or Ableton is involved.

The retained broad mono fixtures enable monophonic bending. They cannot prove
plain mono behavior on their own. Mandatory processor controls also cover
plain-note operation, each of the six pinned quiet-mains inputs in both mono
settings, and genuine octave changes with dynamics enabled. This distinction
caught an existing G1→G2 alias that bending-only captures concealed.

`--frontend gate-only` and `--frontend compression-only` are declared developer
ablations, not additional product modes. Earlier external-dynamics captures
keep their exact historical driver editions under the retained investigation;
their failures are not rewritten as measurements of the final integration.

Example, using a frozen source snapshot and a new output directory:

```sh
python Tools/guitar_evaluation.py --source-snapshot /tmp/frozen-source \
  --fixtures Tests/AudioFixtures/readiness/recorded_noise \
  --frontend default --output /tmp/new-nov5-capture
```

`--full` measures44.1/48/96kHz ×32/64/128samples. A nonzero scoring exit retains
the raw capture, misses, extras, early releases and the source/policy hashes.
Never replace that exit with a musical-readiness claim. The separate original
30-case gate continues to use its own unchanged evaluator and policy.
