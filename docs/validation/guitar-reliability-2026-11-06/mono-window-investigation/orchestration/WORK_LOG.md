# V4 mono latency investigation (private, disclosed regression data)

Base source: e235c8a098dadce6e0a4b8bd6ca9de789a5d174d. All Source copies are
private and carry source-manifest.json. No shared production Source was edited.
Existing sealed PCM/scorers/policies remain unchanged. Driver/source/binary,
fixture seals and actual raw causal event times are bound in each run manifest.

## Fresh variable-overlap YIN: rejected

The ordinary YIN integer-lag difference compares a constant half-frame, so a
fresh root needs two observed periods. Mono publication then requires three
analysis frames and at least 2.25 periods. This accounts for the clean-tone
E2~32/E3~19/E4~13 ms baseline; it does not explain every real attack delay.

`overlap15` scales a lag-dependent trailing overlap to constant comparison
units, extends the maximum lag to two thirds of the available frame and uses
a 1.5-period admission floor. It is past-only and bounded. Global use cuts
quiet E2 to ~25.3 ms but invents octave aliases and changes held releases.

`fresh15b` limits that estimator mode to held<0 and preserves the ordinary
held-YIN arithmetic bit for bit. Controlled mono improves 29/34→31/34 cases,
77/77 references retained, extras8→2, early3→2, no octaves or late releases.
However, paired real/synthetic tests reject adoption: real isolated hammers
wrong-before-correct6→8, recorded-noise plain extras78→87 and octaves1→11;
bend recorded-noise octaves0→8. All raw failures remain under mono-paired/.

## Adaptive held plain observations

`adaptive_held` changes exactly one expression: the held 3-period primary
YIN observation no longer requires monoBend. Before this, default plain mono
always used512 samples (~42.7ms), which swallowed35ms fret notes.

Paired mono-only captures cover all106 mono cases from the three unchanged
sealed corpora, separately in plain and bend modes with integrated default
dynamics. Plain continuous-bend cases keep their historical expression truth
and are labelled diagnostic output-requirement mismatches, never passes.

Plain noncontinuous controls: refs62/74→74/74, extras24→6, octaves12→5,
early6→4; case passes23/31→27/31. Both35ms slides become8/8 references instead
of2/8 each; interior causal onsets are~16–22ms. Recorded-noise plain keeps48/48
references and improves extras78→70, octaves1→0, early32→30, passes30→32/64.
Hammers and all bend-mode raw events/results remain unchanged. No formerly
passing case becomes failing across106 cases per mode.

The32 genuine octave/step/damping cases (64 references) and24 quiet controls
pass; all quiet raw streams are byte-identical. Nonetheless, genuine plain
downward octaves get slower: root35 43.31→48.65ms,40 35.31→48.65,
52 21.98→45.98. Initial acquisition times are unchanged. This tradeoff must
not be hidden behind the56/56 bounded counter count.

## Bounded wider-root fallback research

`adaptive_fallback`: probe512 strict YIN whenever the local primary is invalid.
Rejected: intermediate32 appears during31→33→31; some true octaves worsen.

`adaptive_lower_fallback`: only probe when the independent narrow tracker says
the old oscillator is absent, only consider a new pitch >6 semitones lower,
and require root/primitive support from the full raw-reference observation.
All56 counters pass but the downward-octave timing regressions remain; long
mixed old/new physical-root observations reject the early genuine change.

`adaptive_lower_open`: keeps the narrow-tracker withdrawal and distant-lower
condition, then applies the existing downstream admissibility/raw octave
guards without an extra stale-frame physical-root prerequisite. Testing.

The six-semitone condition is a bounded search trigger, not a declared output
range or octave ban: a3-old-period primary cannot resolve a new root more than
~7 semitones lower with ordinary two-period YIN. Ordinary small fret changes
must continue through the short observation path.

Actual callback deadline figures are shared-worker observations, not certified
real-time bounds. No event is backdated and no old failure is relabelled pass.
