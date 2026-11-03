# Independent audio-to-MIDI evaluation

The measurement corpus and evaluator were developed separately from the detector.
They do not use chord correction: pitch errors cannot be hidden by the allowed
note mask. Reference annotations, source audio and matching criteria remain
unchanged while the detector is improved.

## Corpus and licensing

`Tests/AudioFixtures/README.md` contains full source attribution and limitations.
There are 46 PCM waveforms and 81 case/mode combinations, including eight actual
edited acoustic guitar recordings from a pinned CC-BY-3.0 collection, independent
modal and Karplus–Strong plucks, and deliberately difficult polyphonic mixtures.
Recorded-note mixtures are artificial mixtures, not natural multi-string guitar
performances. These tests do not establish accuracy for all guitar techniques,
amplifiers, pickups, acoustic environments or live performances.

The original recording files, upstream license/attribution, source commit and
SHA256 values are retained. All derived PCM/annotation hashes are verified with:

```sh
python Tools/audio_validation_verify.py
```

The checked-in PCM fixtures and native C++ evaluator require no audio decoder,
network connection or numpy at execution time. Regeneration alone needs numpy
and FFmpeg, via `Tools/audio_validation_fixtures.py`.

## Reproducible evaluator

On a machine with GCC or Clang, run:

```sh
python Tools/audio_validation_run.py --output docs/validation/audio-current
python Tools/audio_validation_run.py --output docs/validation/audio-full --full
```

The wrapper records exact detector/header/evaluator and fixture-manifest hashes,
compiler version, executable hash, run configuration and exit status. The first
command uses 48kHz / 64-sample callbacks. The full matrix has 44.1, 48 and 96kHz
rates and 32, 64 and 128-sample callbacks: 729 case/rate/block runs. Source audio
is resampled outside realtime processing using linear interpolation; this is a
controlled test conversion, not the plugin's realtime input path.

A separately built evaluator can also be supplied with `--executable PATH`.
The C++ runner itself is `Tools/audio_validation.cpp`; it links only the standalone
pitch detector. Its Windows entry point uses wide-character paths.

Output contains:

- `audio-cases.csv`: per-case recall/errors, response-time distributions, release
  behavior, CPU callback distributions and deadline misses.
- `audio-events.csv`: every emitted note event with actual sample position.
- `audio-summary.txt`: aggregate observed statistics, clearly marked as measurements.
- `measurement-manifest.json`: exact input/source provenance for that run.

## Scoring

A matched correct note must remain active for at least 20ms (or the reference
note's shorter remaining duration). The onset timestamp is the original emitted
Note On, but a brief correct guess cannot count as a useful low-latency result.
Earlier wrong notes are reported independently. Other counters include missed
reference notes, extra Note Ons, octave errors, premature releases (>10ms before
the defined note release), output still held after the terminal silence, and
fixed-buffer overflow. Monophonic and polyphonic results are kept separate.

The reference onset for real samples is the first absolute PCM amplitude >=.001
(-60dBFS) after the common documented gain. Original attacks/leading codec silence
remain present. This rule is not fitted per sample and is not mechanical pick-
contact timing. The onset of synthetic notes is known directly by construction.

The evaluator includes no audio driver, DAW, receiver synth or acoustic output.
Its sample-based detector response is not physical end-to-end latency. CPU
processing time is measured separately around each callback. Shared VM scheduler
stalls remain in maximum/deadline counts; no outliers are removed.

Successful tool execution means the measurement completed. It is not automatic
product acceptance, and must not be cited as satisfying the user's 5/10ms goal
when the actual quality/response measurements do not satisfy it.

## Development findings

The first causal implementation produced many polyphonic false Note Ons and
missed repeated attacks. Independent evaluation caught these before release.
Short-frame harmonics were incorrectly interpreted as additional fundamentals.
Using a complete causal observation window and a supported fundamental reduced
false notes, but increased response time and still missed weak/octave voices.
These are measured engineering tradeoffs, not approved changes to the user's
live-latency requirement. Final measurements must refer to the final source hash;
intermediate reports under `docs/validation/audio-*` are diagnostic history only.
