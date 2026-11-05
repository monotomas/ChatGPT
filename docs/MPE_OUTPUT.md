# Optional MPE output in 0.3.0

This describes the optional wire format in the 5 November guitar-evaluation
scope. Ordinary routing remains the default. Protocol success is separate
from acoustic accuracy and actual execution in Ableton.

`MPE Output` is an explicit, saved choice, initially off. Ordinary projects
keep the existing note/channel behavior. Enabling it makes mono audio request
continuous expression, independent of the legacy `Mono Bend` switch. Poly
remains fixed semitone Note On/Off, with neutral member pitch; MPE does not
enable polyphonic bending. Mono must still distinguish a new fretted note from
a continuous bend; the MIDI router does not guess articulation from loudness.

## Output and receiving instrument

Independent pitch needs an MPE receiver and a host route that preserves MPE.
The output uses a lower zone with master channel 1 and fourteen members:

| Channels | Ownership |
| --- | --- |
| 1 | Zone configuration and centred global pitch; no performance notes |
| 2–7 | Six generated note identities, stable while each note sounds |
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
through chord changes. Mono articulation releases the old anchor before
initial pitch and Note On for the new anchor. Static poly notes have independent
lifetimes; a new note need not inherit a previous note's member or physical
string identity, and observation windows can produce a short legato overlap.

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
groups test the dated mono-expression/static-poly contract. The older ambiguous
45+64 three-partial source remains a reported acquisition diagnostic rather
than a false claim of recovered independent strings. These checks do not
establish Live routing, audio-driver latency or general guitar quality.
