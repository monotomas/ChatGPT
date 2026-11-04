# MPE routing architecture under development

This describes the `research/musical-readiness` implementation. It is not a
new release or evidence that the audio tracker has reached musical readiness.

`MPE Output` is an explicit, saved choice, initially off. Ordinary projects
keep the existing note/channel behavior. Enabling it makes mono and poly audio
tracking request continuous expression, independent of the legacy `Mono Bend`
switch. The audio tracker must still distinguish a new fretted note from a
continuous bend; the MIDI router does not guess articulation from loudness.

## Output and receiving instrument

Independent pitch needs an MPE receiver and a host route that preserves MPE.
The output uses a lower zone with master channel 1 and fourteen members:

| Channels | Ownership |
| --- | --- |
| 1 | Zone configuration and centred global pitch; no performance notes |
| 2–7 | Six stable audio voice identities |
| 8–15 | Eight re-encoded external/diagnostic voices |
| 16 | Reserved for chord input; no output notes |

Every member uses a 48-semitone pitch range. The router configures that range
and sends initial pitch before each Note On. Its master range is zero, and it
centres the master before a phrase so a previous ordinary keyboard wheel
cannot bias newly generated strings. A fresh phrase re-announces the zone to
support a receiver inserted or rerouted between phrases. No protocol can
automatically reconstruct already sounding notes in a newly inserted receiver.

External voices use their own pool so a keyboard wheel cannot bend the audio
strings. The ninth external voice steals the oldest external member. Bounded
FIFO bindings retain stolen held keys until their corresponding Note Off,
preventing that old release from stopping the replacement voice. There are
512 such bindings; exhausting a bounded event/binding capacity stops the zone
and reports a capacity fault. Panic recovers the allocator.

The fixed member range can represent a combined deviation up to 48 semitones
in either direction. Larger combined input master/member expression saturates
at that wire limit. An ordinary single-channel receiving instrument cannot
provide independent bends using this stream.

Ableton's source device must have MPE enabled, with the lower zone restricted
to fourteen members so input channel 16 remains available for chord control.
The receiving route/instrument must preserve and interpret MPE. The pinned
[Live 12 manual, section 10.5](https://github.com/J0rgeSerran0/Ableton_Live_Manual_to_HTML_and_PDF/blob/4bb26b7814a581cf6d460ff4e5f04036daef23fe/docs/Ableton_Live_12-en.md)
explicitly supports MPE output from MIDI-output plug-ins with MPE enabled.
This documents feasibility. Actual Flysnapper execution in Live, including
the chord track's channel 16 after enabling MPE, remains unverified.

## Input contract while MPE Output is selected

Playing MIDI on channel 1 retains ordinary keyboard semantics, initially a
two-semitone wheel range. Channels 2–15 accept lower-zone member input,
initially a 48-semitone per-note range, allowing a host to distribute playing
notes among members. Channel 16 always remains chord control.

Input RPN 6 on master 1 changes or disables the lower input zone; it never
includes channel 16. Input RPN 0 on a member configures the entire input zone's
member sensitivity, matching the standard first-member configuration message.
Master RPN 0 configures master sensitivity separately. Master and member pitch
combine on the associated re-encoded external voice. They do not reach the
output master or the generated members.

Master sustain, sostenuto, Hold 2, pressure and other performance controls
apply only to external voices. Future notes inherit held wheels/pedals and
the latest applicable master/member controls. Software pedal handling keeps
keyboard sustain separate from generated strings. Sostenuto snapshots keys
already held when it is pressed. An input zone change releases external
bindings while preserving generated voices and the chord analysis.

The legacy output mode preserves ordinary multichannel MIDI behavior. The
selected MPE contract assigns channels 2–15 member semantics; ordinary
multichannel instruments expecting another unannounced wheel range need an
explicit sensitivity or legacy mode.

## Correction, reset and evidence

Scale/chord correction chooses the output anchor at Note On. Fractional pitch
then remains relative to the acoustic/input anchor, preserving expression
around the corrected note. Note Off retains the original corrected binding
through chord changes. A discrete articulation releases the old anchor before
initial pitch and Note On for the new anchor, keeping its stable audio voice.

Changing output format releases voices in their previous format before
accepting new notes. It preserves the held chord input. Generated-source
shutdown leaves external voices alone; transport/state panic releases all
performance voices. Output mode changes may also require changing the host or
receiver's explicitly selected MPE mode.

`FlysnapperMpeOutput` tests the bounded router against an independent JUCE
`MPEInstrument`, including actual reconstructed frequencies, source isolation,
pedals, overlapping corrected notes, stealing, reset, zone/range changes,
receiver replacement and randomized lifecycles. `FlysnapperMpeProcessor` adds
real sidechain samples through the processor. `FlysnapperMpeVst3Host` loads the
compiled plug-in and tests the VST3 event adapter before that receiver. Signal
groups remain strict even while the audio backend is unfinished. These checks
do not establish Live routing, audio-driver latency or general guitar quality.
