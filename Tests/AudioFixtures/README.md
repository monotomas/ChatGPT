# Independent audio-to-MIDI validation fixtures

This corpus is independent of the detector's implementation. Its signals and
reference annotations are not changed to make detector output pass. It contains
46 mono PCM16/48kHz waveforms and 81 mode/case combinations. The evaluator also
resamples to 44.1/96kHz and uses 32/64/128-sample callback sizes for the full matrix.

## Recorded acoustic guitar attribution

Eight guitar notes in `tonejs/*.ogg` are taken verbatim from
[Nicholaus P. Brosowsky's tonejs-instruments](https://github.com/nbrosowsky/tonejs-instruments),
commit `622c2f1c32c8cfce4158ddc3eb26e518ddef37e5`, under the upstream stated
[Creative Commons Attribution 3.0](https://creativecommons.org/licenses/by/3.0/) sample license.
The upstream `sample-source-info.txt` identifies the acoustic guitar source as
the University of Iowa Electronic Music Studios musical instrument sample
library, <http://theremin.music.uiowa.edu/>. Attribution: University of Iowa
Electronic Music Studios acoustic guitar recordings; sample editing and
collection by Nicholaus P. Brosowsky. The upstream LICENSE.md, README.md and
source attribution are retained, with SHA256 and source path for every file in
`tonejs/manifest.json`. No upstream code is used by the detector.

The upstream samples have already been trimmed, ramped, normalized, denoised and
in some cases pitch-corrected. Therefore these are actual **edited isolated
acoustic recordings**, not a new live guitar performance or a GuitarSet benchmark.
The original Ogg files remain unchanged. We decoded to mono 48kHz with FFmpeg,
retained the first 700ms including the original attack and leading codec silence,
multiplied amplitude by .45, added 50ms silence before the file, and used a 4ms
terminal taper at the controlled note release. Derived WAV files are also
CC-BY3.0; this paragraph identifies those modifications.

The real-note reference onset is the first sample with absolute amplitude at
least .001(-60dBFS) after the same common gain. This fixed waveform-onset rule is
not optimized per file. It may include codec pre-echo or pre-tonal attack, and
is not a direct measurement of mechanical pick contact. Reference MIDI pitches
are those of the upstream named sample(E2=40,E3=52 etc.), checked against the
sustained recording spectrum. The controlled note-off is the end of our crop,
not a claim about the original player's release.

`recorded_E_octaves` and `recorded_strings` are **artificial mixtures of isolated
real recordings**, with independently known pitches and onset offsets. They are
not natural simultaneous/strummed guitar recordings. They exercise harmonic
collisions, different pitches and overlapping decays but do not establish full
live polyphonic guitar accuracy.

## Original synthetic fixtures

The generated modal plucks have a pluck-position spectral comb, random partial
phases, frequency-dependent decay, attack transient and defined damping. Other
cases use an independently implemented noise-excited Karplus–Strong delay loop.
Cases cover E2 through high guitar registers, semitone runs in both directions,
repeated equal notes, missing fundamentals, detuning, vibrato, soft/noisy attacks,
major triads, six-note open chords, staggered attacks, adjacent semitones,
octaves, and a weak note below a strong note. Silence, an unpitched transient
and low-level noise are explicit negative controls.

Synthetic waveforms and annotations were created for Flysnapper in 2026 and are
available under AGPL-3.0-or-later with the project's source. Regeneration uses
`Tools/audio_validation_fixtures.py`(numpy and FFmpeg required only to regenerate).
Checked-in PCM fixtures require no runtime codec or numpy dependency.

## Integrity and measurement

`fixtures.json` records waveform hashes, sampling parameters, families and all
MIDI note-on/off annotations. `fixtures.tsv` is the equivalent dependency-free
C++ runner input. Run `python Tools/audio_validation_verify.py` to check every
original/derived hash and annotation using only Python's standard library.

`Tools/audio_validation.cpp` measures emitted MIDI against those annotations.
A correct note must remain emitted for at least 20ms(or the reference note's
shorter remaining duration) to count as a useful result. Short correct guesses
cannot inflate the low-latency score. Wrong earlier notes, missing notes,
extra note-ons, octave errors, premature releases and hanging voices are
counted separately. Reported detector response excludes the audio driver,
Live routing, receiver synth and output hardware. CPU callback time is measured
separately and cannot demonstrate physical guitar-to-audio latency.

A successful measurement-tool process means that measurement completed. It does
not mean that all observations meet the user's musical quality or 5/10ms targets.
