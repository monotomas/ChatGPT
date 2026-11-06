# Musical-readiness research controls

This collection was built in response to the user's report that 0.2.1 is not
musically usable. It is a **research/acceptance protocol, not a release claim**.
The numerical policy was written before measuring these inputs. The original
physical E2-under10ms/E3-under5ms goals are retained separately from detector
research checkpoints; passing the latter does not fulfil the former.

`policy.json` requires independently labelled real guitar techniques before a
musical-ready claim. The current data does **not** satisfy that requirement.
The GitHub-hosted CC0 isolated hammer recordings below are useful real evidence,
but real continuous fretted slides, hammer transitions, pull-offs, bends,
vibrato and independently bent polyphonic performances remain missing.

## Controlled edition and first baseline

The38 retained PCM16 waveforms /72 mode cases were generated without reading
any detector output, sealed, then measured against an immutable Git snapshot of
source `c5b05337641b13ae259836f5346e7c1850b7382c`. Only after this first baseline
were the cases disclosed to implementation owners. They are now **seen
regression data**, not an unseen generalization result.

Seal: `d5af9fb3fefb4d41918e62810d59290c1dd741d95337d58ec34efcdcf14f9fba`.

Families include50/60Hz mains combs, inharmonic damped contacts, friction
squeaks, quiet desired strings beside learned hum, common-gain/SNR pairs,
6/12/24dB desired-music/interference mixtures, stepwise fretted slides with
35/65/105ms plateaus, hammer/pull-offs without an amplitude attack, continuous
bends/vibrato, separately bending polyphonic voices and quiet rearticulation.
These are explicitly synthesized, physically inspired controls, **not recorded
string handling or real human continuous performances**.

`fixtures.json` supplies per-note identities, intended technique, source
intervals and the independent pitch trajectory in `pitch-curves.tsv`. The
implementation's voice IDs need not equal the physical string IDs. Matching is
one-to-one by the acoustic note/interval and follows that output voice's causal
pitch messages; output is never shifted back to an acoustic onset or aligned
by a fitted delay.

The original baseline has two distinct views:

- `baseline-c5b/audio-*` uses the old fixed-note evaluator with bend disabled;
  it is diagnostic and cannot establish enabled-bend correctness.
- `baseline-c5b/expression-flushed` captures bend enabled in both requested
  modes, retaining raw On/Off/Pitch events. The old API's missing poly bend and
  voice-ID capability are recorded, not silently simulated.

The earlier `expression` trace is preserved with its original driver. It
supplied no silence after files that contained hum to EOF. An active hum voice
at that EOF is **not** evidence of a note hanging after silence. The authoritative
`expression-flushed` capture supplies250ms of actual digital silence after each
source and reports source length separately from processed length.

The enabled baseline reproduces the reported articulation problem: all six
mono fretted-slide cases emit only one NoteOn instead of eight, and both
hammer/pull cases emit one instead of five. The fast80ms smooth bend with
inharmonic/timbre evolution creates seven NoteOns instead of one. A three-voice
poly case emits false note33 while the actual45/52/57 voices are active: a
controlled **downward** octave artefact, not merely a legitimate octave in the
chord. This does not assert that it is the user's exact recording/failure.

`scoring-v2` independently scores these traces against the predeclared policy.
The0.2.1 controlled baseline has62/72 failed runs; the raw fixed-note and
continuous-expression results must not be conflated. Scoring source snapshots
are retained. Numerical policy/labels were fixed before the first run; scorer
implementation corrections do not rewrite the original raw captures.

## Real recorded hammer evidence

`recorded_hammers` retains four genuine isolated hammer-on samples at two gains
(8 derived waveforms /16 mode cases), original24bit44.1kHz WAVs and CC0/legal,
author SFZ and source SHA evidence. Dataset edition seal:
`059f89547e78bf96de6835f79664116e8c79d6b244082d61b83e051cb6dcb1ae`.

Source: `sfzinstruments/karoryfer.black-and-green-guitars`, commit
`b3b3249d37dc977a1a297bd2dc053e6d9b6b805c`; performer Brian Wood, green Gretsch
Anniversary; Karoryfer Lecolds. Numeric author SFZ mappings establish notes40,
43,45,52. Filename octave labels alone would have been wrong. A separate
long-window FFT check, before detector evaluation, agrees with these mappings;
it did not change the labels.

The excerpts preserve the recorded excitation/timbre and apply only resampling,
scalar gain and a controlled4ms cutoff. Their onset marker is **authored sample
playback start**, not unknown physical hammer-contact timing before the sample
edit. Fractional instantaneous pitch truth is unavailable; cents deviations
from nominal SFZ tuning are diagnostic only. In particular, two samples are
naturally about16cents sharp, so they must not be treated as zero-cent truth.
The original samples' EOFs are not labelled physical note-offs.

Two real author-labelled release-trigger samples are also retained as source
material. They are not called universally identifiable non-tonal negatives:
a release sample can contain real residual pitched string energy. Their SFZ
`trigger=release` master mapping is preserved for context.

The0.2.1 baseline has14/16 failed real-hammer research runs, including initial
notes wrong against the intended SFZ anchor and long response. A later original
source audit confirms real attack-pitch drift: these strict nominal-anchor
failures are not all proven acoustic-frequency mistakes. See
`recorded_hammers/annotation_audit/README.md` for evidence and timing limits.
All sealed labels and failures remain unchanged. It is not a passing
real-technique acceptance.
No real continuous performance was synthesized by concatenating isolated notes
and then mislabeled as a human legato recording.

## Real muted/percussive and quiet controls

`recorded_noise` adds 64 sealed waveforms / 128 mono/poly mode cases from pinned
CC0 Shinyguitar recordings. It includes author-labelled muted/percussive sounds,
quiet real plucks with immediate-start controls, and real note/noise mixtures
at 6/12 dB SNR with matched common gains. Original microphone and magnetic
pickup samples are paired capture views, not independent performances. See
`recorded_noise/README.md` for source, labels, limits and its first unseen
candidate capture: 37/128 complete runs pass, with substantial mixed-noise
rearticulation and poly precision failures. It is now seen regression data.
The c5b baseline and candidate's exact DSP snapshots are retained. No
musical-readiness claim follows from these partial recorded controls.

`poly_tracking_v1` separately audits optimistic synthetic six-source tracking
with all true pitches/identities seeded once. Frozen models 17 and 19 each pass
2/8 strict whole-curve cases, with improvements and regressions between them.
It retains original-ID causal errors and clearly distinguishes tracking from
actual note acquisition. Its post hoc full512 diagnostic does not replace the
original failures or count as fresh unseen evidence.

## Repeatable commands

Normal runs need only C++17 and Python's standard library. Generation requires
NumPy; deriving the retained recorded excerpts also requires FFmpeg. Generation
never imports or runs the detector. Known byte-exact inputs are retained.

```bash
python Tests/AudioFixtures/readiness/verify.py
python Tests/AudioFixtures/readiness/test_scoring.py
python Tools/run_readiness.py --output build/readiness-candidate \
  --fixtures Tests/AudioFixtures/readiness
python Tools/run_readiness.py --output build/readiness-recorded-candidate \
  --fixtures Tests/AudioFixtures/readiness/recorded_hammers
python Tools/run_readiness.py --output build/readiness-recorded-noise-candidate \
  --fixtures Tests/AudioFixtures/readiness/recorded_noise
```

Add `--full` for44.1/48/96kHz ×32/64/128 sample blocks. `--source-snapshot PATH`
can capture a separately frozen source tree. The runner binds source/executable
hashes, checks source stability and retains actual exit codes, raw events,
callback observations and independent scores. Nonzero baseline scores are
expected. `score.py --research-only` can isolate the detector-checkpoint exit
status, but it never changes `musical_readiness_eligible` or grants release
approval. Complete synthetic success still cannot substitute for the missing
real-performance and original physical-latency evidence.

## External recorded-performance data dependency

The official Guitar-TECHS dataset is CC BY4.0 and offers recorded DI/hexaphonic/
microphone guitar plus human technique labels, including bends. Its source site
is pinned at `guitar-techs/guitar-techs.github.io` commit
`19a2954f789bfd192b2b4732788ceb000c1dd687`; download:
`https://zenodo.org/records/14963133`. `zenodo.org` is currently denied by the
enforced cloud HTTP policy. No proxy/Actions/cache bypass was attempted. The
required additive domain has been reported through the supported configuration
workflow. `recorded_hammers/data-research.json` records the other source/license
findings. Mirdata's tiny GuitarSet WAV fixtures were inspected and are dummy
quantized noise, not usable real-guitar excerpts; NATSolo demo outputs were
rendered transcriptions, not original guitar evidence.

`DATA_RESEARCH.md` records the exact primary source URL, verified technique
classes, automated pickup-MIDI annotation limits, unresolved download metadata,
and additional GitHub sources inspected without assuming a usable audio license.
