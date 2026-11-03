# Full audio/MIDI integration validation

This document records scope. Actual run results must be taken from the current
CTest logs and build manifest; existence of this file is not a passing result.

`FlysnapperProcessorAudio` supplies real sidechain sample buffers to the actual
JUCE processor and consumes its real output MIDI. It checks:

- Disabled sidechain, connected digital silence, and independently disabled
  audio detection. Ordinary MIDI remains functional with detection disabled.
- Monophonic additive tones at 44.1/48/96 kHz with 17/64/257-sample blocks,
  correct octave, channel 1, no repeated attack for an uninterrupted tone,
  and eventual release. First useful-note times are printed.
- Audio-derived C# decisions entering Chord Notes correction; a chord change
  at the exact detected event sample is applied first, and a change one sample
  later cannot affect the earlier note. Note Off follows the original binding.
- Concurrent generated/external ownership, including colliding output notes,
  source-only stop, reacquisition and the final owner's release.
- Polyphonic C/E/G through the actual wrapper and safe mono/poly mode changes.
- Audio settings state roundtrip and identical MIDI with editor open/closed.
- Repeated signal/silence, sidechain disconnect, panic and recovery cycles.

The 150 ms acquisition limit in this integration suite is a functional liveness
bound. It is not the user's required 5/10 ms live latency and is not reported as
such. Independent detector accuracy/latency evaluation must report first
correct useful notes, false and missing notes, octave errors and onset/release
errors on synthetic and recorded signals. Driver, Ableton routing, receiver
instrument and acoustic latency are outside an in-process test executable.

Final readiness additionally requires the compiled VST3 host suite, actual
Windows build/test log, installer validation and a complete prebuilt Windows
package. Any remaining unverified behavior must be stated in the final report.

The compiled `FlysnapperActualVst3` host suite also opens the actual built module
with an SDK kAux sidechain enabled. Its full-audio fixtures include a mono C#
corrected to C by simultaneous channel-16 controls while an external A is
corrected to G, a polyphonic C/E/G signal, and detector disable that releases
only the generated A while leaving an external C held. These checks send real
audio samples, not the diagnostic pulse parameter. The state test includes all
five audio controls; legal float values are obtained through the actual VST3
parameter parser rather than assuming between-step normalized values survive
roundtrip unchanged.
