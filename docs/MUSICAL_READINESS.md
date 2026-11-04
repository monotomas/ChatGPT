# Musical readiness work after 0.2.1

This work is on `research/musical-readiness`. It is unfinished research, not a
new installer or a claim of musical acceptance. The previous 0.2.1 installer
does not meet the user's November 4 requirements. No new release version has
been assigned.

## Acceptance evidence

The [policy](../Tests/AudioFixtures/readiness/policy.json) was defined before
measuring the new implementation. The independently prepared controlled
edition contains 38 PCM recordings and 72 mono/poly cases. Its seal is
`d5af9fb3fefb4d41918e62810d59290c1dd741d95337d58ec34efcdcf14f9fba`.
Keep that edition, the previous 30-case packaging gate, and previous disclosed
corpora intact. Once examined during tuning, a corpus is a regression set;
it is no longer an unseen final test.

Scoring must retain actual decision timestamps, one-to-one note/voice
matching, misses, extra notes, wrong notes before correct notes, release
errors, pitch-curve error and execution deadlines. Do not backdate events or
align a delayed bend curve to truth before scoring it. Implementation voice
IDs need not equal physical string IDs. Synthetic success alone does not
establish musical readiness.

The policy requires independent real-guitar evidence for fretted slides,
hammer-ons, pull-offs, bends, vibrato and handling/squeak noise. Existing
isolated guitar recordings, constructed mixtures and artificial interference
are valuable controls, but do not supply all those recorded techniques.
The original E2 <10 ms/E3 <5 ms complete-chain goals remain unverified.

## Work in progress

* Causal stationary mains cancellation and gain-relative tonal/noise evidence.
  Mechanism tests include cold/ramped hum, quiet protected notes near mains
  frequencies, simultaneous 50/60 Hz families and gain changes. Passing these
  tests is not a general classifier of a musician's intentions.
* Causal mono acquisition and per-voice articulation evidence. Fretted steps
  and weak new excitations must be distinguished from fast smooth bends,
  without declaring every pitch change a new note.
* Continuous polyphonic source tracking. Harmonic overlap, false subharmonics,
  new-source admission and independent pitch curves remain substantial
  unresolved work. A successful two-tone example is not sufficient.
* Optional standard MPE output. Generated voices and ordinary external
  playing notes occupy separate member channels. Core scale correction,
  note bindings, pedals, stealing, reset and mode changes must remain correct.
  JUCE MPE receiver tests check the stream; they are not an Ableton test.

No aggregate readiness result has passed. Do not turn mechanism check counts,
an existing release's validation, or partial latency gains into a new release
claim. Full native/Windows/installer verification follows a frozen candidate
that meets the audio requirements; it cannot substitute for those requirements.
The builder currently marks every collected result as a research prototype;
the Windows packager rejects that stage. This explicit release hold remains
even if all protocol/unit tests pass. Retained failed model experiments and
host reviews are in [the research evidence](validation/musical-readiness-research/README.md).

The [native mono-v20/poly17 checkpoint](validation/musical-readiness-research/native-checkpoint-v20/README.md)
passed 12 of 13 groups with unchanged source hashes; the strict simultaneous
poly hammer/source-acquisition group failed. Broader independent evidence is
more restrictive: the first new recorded-noise candidate passed only 37/128
runs, with many false same-note rearticulations and extra poly voices. A fresh
oracle-seeded six-source tracking set passed 2/8 for both models 17 and 19,
with different improvements and regressions. Correct initial voices were
deliberately supplied in that experiment; it does not test acquisition.
All of these failures remain release blockers.

## Host evidence

The pinned [Live 12 manual copy](https://github.com/J0rgeSerran0/Ableton_Live_Manual_to_HTML_and_PDF/blob/4bb26b7814a581cf6d460ff4e5f04036daef23fe/docs/Ableton_Live_12-en.md)
states in section 10.5 that MIDI-output plug-ins with MPE enabled can output
MPE. It documents feasibility, not execution of this build in Live. Incoming
playing/chord channels after enabling source-device MPE, and preservation of
per-note pitch through the receiving device route, are not yet runtime
verified. Existing ordinary sidechain and MIDI routing was confirmed by the
user before this work and should be preserved.

## External recordings

The pinned [Guitar-TECHS site](https://github.com/guitar-techs/guitar-techs.github.io/tree/19a2954f789bfd192b2b4732788ceb000c1dd687)
describes the [Zenodo dataset](https://zenodo.org/records/14963133), a potential
source of labelled recorded techniques. The current cloud network policy
blocks `zenodo.org`. An additive domain change has been saved in the cloud
configuration draft; that draft does not change the running policy. Continue
with legitimately available sources without bypassing the policy. Verify
the actual source repository, license, file hashes and annotations before
using or redistributing any recording.
