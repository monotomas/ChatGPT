# Old mono detuned-repick failure: unchanged assertion, actual MIDI dump

The existing +30-cent overlapping repick control was run through the actual
processor with the initial external-dynamics topology. Its waveform and
assertions were unchanged; failure-only logging was added. Source hashes were
stable through the targeted build/run under the shared build lock.

At44.1kHz/range2, all three notes incorrectly use anchor58 instead of nearest
semitone57: Note Ons at705,14935,29047, final Off47980. The first decision is
15.986ms after the source starts. This is an initial semitone-anchor bias, not
an octave failure or lost repick. Later wheel motion must not excuse selecting
the wrong anchor for subsequent MIDI correction.

Full actual MIDI/RPN/wheel bytes and causal samples are retained in the CSV.
The test fails honestly. `source/` was assembled after capture from current
and earlier retained bytes; every stored input matches its contemporaneous
before/after hash in `manifest.json`. No binary or PCM is duplicated here.
