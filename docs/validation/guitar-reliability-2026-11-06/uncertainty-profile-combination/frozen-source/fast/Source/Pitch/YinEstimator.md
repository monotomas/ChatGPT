# Causal YIN candidate and comparison

`YinEstimator` implements the cumulative mean-normalised difference function
(CMNDF). Integer lags use the same comparison length, half the supplied
frame, so increasing lag cannot obtain a deceptively low error merely by
comparing fewer samples. The first positive local minimum below the configured
threshold is selected and its period is interpolated. At short lags
(less than sixteen samples), a bounded cubic Lagrange fractional-delay search
refines the actual difference function. Merely fitting a parabola to integer
CMNDF values was insufficient: the filtered MIDI100 first-period minimum could
be estimated just above threshold even though its actual fractional-period
error was low. That caused an octave-low double period. Direct fractional
comparison corrects this without a pitch-specific octave exception. The same
issue also affected a filtered C6 period of about 11.47 samples. On very short
frames, cubic interpolation clips its comparison to the actually available
samples and normalises the comparison length; it never reads scratch samples
left over from a preceding call.

No future audio, dynamic allocation, GUI, locks or IO occur in `estimate()`.
The caller controls onset/release, voice lifecycle and confidence stability.
The estimator uses at most 1,024 available trailing samples. A 512-sample frame
at 12 kHz spans **42.667 ms**, regardless of a plugin's reported latency. The
lowest measurable pitch also depends on the available two-period observation;
the supported MIDI bounds alone do not make a short frame sufficient.

A generic harmonic-series check addresses a measured guitar-attack failure.
An isolated body resonance near 110 Hz plus a dominant 165/330/495-Hz series can
make the numerical common period correspond to 55 Hz. Plain YIN can therefore
report MIDI 33 for an intended E3, with high periodicity. Twelve causal complex
projections measure partial power at the candidate period. If more than 85% of
that power lies on every second, third or fourth partial, with at least two
supported partials, the dominant series is a candidate. Before promoting it,
Hann-windowed projections check whether at least three resolved off-series
partials instead support the original root. Each must have at least 4% of the
largest measured partial's amplitude and exceed projections one observation
resolution step to either side. This prevents a coherent weak odd series from
being discarded merely because its even harmonics contain most of the energy.
Confidence after a promotion is capped by its power fraction.

A related check can recover a supported lower octave when the shorter period
already passed CMNDF: its observed doubled period must have less than half the
normalised error, and the lower fundamental plus at least three odd partials
must be physically supported. It reads only the supplied trailing frame.
These are instrument/timbre heuristics, not arbitrary-audio proofs. Very faint
odd components can still leave octave ambiguity; no threshold change makes
every missing or weak fundamental recoverable.

There is no permanent +/-35-cent acceptance gap. A stable detuned note is
rounded to its nearest MIDI note. Initial attack stability must be assessed by
the caller from consecutive continuous pitch estimates, not by refusing pitches
that are far from an equal-temperament centre forever. The lag search includes
the half-semitone rounding bins at both configured endpoints, while the final
integer output remains restricted to MIDI 28–100. In particular, MIDI 28 at
minus 40 cents is still an allowed MIDI-28 result.

## Standalone observations

The independent guitar fixtures were used without changing their annotations.
The causal test frontend is a 12-Hz DC blocker, four low-pass stages, and
48-to-12-kHz decimation. A trailing 512-sample frame is analysed every 32
analysis samples. Three matching candidate notes form a confirmation. Original
waveform onset is measured with the same fixed absolute 0.001 threshold for all
recordings; it is used only for reporting latency, never by the estimator.

All eight recordings had the correct first confirmed note and no wrong
confirmation events in this isolated comparison:

| Recording | First note | Waveform onset to confirmation (ms) |
|---|---:|---:|
| E2 |40|58.792|
| A2 |45|61.229|
| D3 |50|74.292|
| G3 |55|58.604|
| B3 |59|60.521|
| E3 |52|59.500|
| E4 |64|64.667|
| F#3 |54|63.417|

A separate fixture sweep also found correct first candidates for the tested
modal/plucked strings, Karplus strings, missing-fundamental note, detuning,
vibrato, soft and bright controls, while silence, unpitched clicks and white/
coloured noise produced no confirmed pitches. These checks concern candidate
acquisition; they do not validate full note lifecycle, retriggering, polyphony,
VST3 adaptation, a DAW or physical end-to-end latency.

The standalone executable now passes 4,327 checks. These include the original
pure-tone, nonfinite-input and eight-recording checks; 770 filtered high-note
controls over MIDI 79–100, five starting phases and seven prefix/trailing frame
endpoints; and 2,190 filtered pure/harmonic controls over all MIDI 28–100,
five phases and minus/zero/plus 40-cent detuning. Endpoint outputs remain
bounded, and independent even-series body and coherent weak-odd controls retain
their intended note identities. Another 1,040 checks compare identical short
frames after different preceding inputs, guarding against stale scratch data.
AddressSanitizer, UndefinedBehaviorSanitizer and LeakSanitizer also pass all
4,327 checks.
The final streaming detector must still pass its independent complete
lifecycle, polyphonic and integration evaluations after this helper changes.

```sh
g++ -std=c++17 -O2 -I Source Source/Pitch/YinEstimator.cpp Tests/YinEstimatorTests.cpp -o yin-tests
./yin-tests Tests/AudioFixtures
```

These observations improve the tested monophonic acquisition errors, but do
**not** meet or establish the requested physical E2-under-10-ms or E3-under-5-ms
targets.
