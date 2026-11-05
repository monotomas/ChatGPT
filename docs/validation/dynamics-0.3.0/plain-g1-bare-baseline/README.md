# Quiet G1 in plain mono also fails with dynamics disabled

This is an isolated actual-processor baseline using the exact native3 archived
header/ABI and linked native3 objects. Only the harness dynamicsEnabled value
is set0, and only the unchanged sealed quiet-hum group is invoked. The waveform,
expected anchor, one-note lifecycle and release assertions are unchanged. The
private report kind is deliberately `FlysnapperDynamicsProcessorBareAblation`,
not the installer's accepted report kind. No pass is claimed.

The first plain-mono G1/gain1 case fails even without the new dynamics. Note31
starts26239, shifts to43 at40191, repeats43 at45951,51839,57599,63487,69375,
and ends76543. The dynamic-enabled native3 case shifts at the SAME40191.
Thus the held octave alias predates applying dynamics in this native3 backend;
this is not a claim about the user's older shipped c5b release, which has a
different detector and input gate.

`manifest.json` preserves test_exit_code1, private source/executable identities
and unchanged linked object/library hashes. Full MIDI is retained. The exact
processor source/header snapshot is `../native-attempt-3/source/`; native binary
payloads are excluded. Later held-octave changes need genuine octave-transition
falsification and a new complete native/processor run.
