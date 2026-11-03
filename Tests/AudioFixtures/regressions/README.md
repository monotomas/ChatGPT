# Supplemental lifecycle regression

This fixture is separate from the immutable development, holdout and confirmation
corpora. It does not change the frozen 30-case quality policy.

`bright_lower_upper_damped.wav` contains generated E2 and E3 plucks, both starting
at 50ms. E3 is damped at 300ms; the brighter E2 continues until 650ms. A rejected
octave-retention approach kept E3 alive for more than 200ms after its damping,
because the lower string's own high harmonics resembled an upper voice.

The routine `FlysnapperPitch` test requires both voices to be acquired once and
the independently damped E3 to release within 100ms. It also checks the existing
recorded octave-mixture fixture: an already acquired E3 must not disappear while
its source continues. These are lifecycle checks, not full polyphonic accuracy
or physical latency acceptance.

The PCM and annotations are identified in `manifest.json`. Generation uses the
project's modal string generator in `Tools/audio_validation_fixtures.py`, with
seeds 1/2, gains .18, brightness .55/1.25 and durations .6/.25 seconds; the notes
are mixed after 50ms of leading silence. No external samples are used in this
supplemental waveform. License: AGPL-3.0-or-later.

Three additional articulation files protect the polyphonic phase check:

- `held_stiff_E2.wav`: a continuous E2 with slightly inharmonic string modes;
  period-estimation bias must not restart its held note.
- `twelfth_withdrawal_E2.wav`: a weaker B3 is removed while E2 continues. The
  current detector does not acquire that B3 in this control; the narrowly scoped
  regression requires the continuing E2 not to restart. The full independent
  evaluator continues to report the missing B3 rather than treating this as
  complete polyphonic acceptance.
- `continuous_repick_E2.wav`: three independently phased plucks replace the
  string without silence; rejecting false attacks must preserve these repicks.

Their annotations, exact SHA256 values and generation parameters are retained
in `manifest.json`. These waveforms are AGPL-3.0-or-later project test data.
`python generate.py --output <new-directory>` regenerates and verifies all four
files; the normal test workflow needs neither NumPy nor regeneration.
