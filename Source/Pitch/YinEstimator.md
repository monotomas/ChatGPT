# Causal YIN candidate and comparison

`YinEstimator` implements the cumulative mean-normalised difference function
(CMNDF). Each tested lag uses the same comparison length, half the supplied
frame, so increasing lag cannot obtain a deceptively low error merely by
comparing fewer samples. The first positive local minimum below the configured
threshold is selected and its period is interpolated. At very short lags
(less than eight samples), a bounded cubic Lagrange fractional-delay search
refines the actual difference function. Merely fitting a parabola to integer
CMNDF values was insufficient: the filtered MIDI100 first-period minimum could
be estimated just above threshold even though its actual fractional-period
error was low. That caused an octave-low double period. Direct fractional
comparison corrects this without a pitch-specific octave exception.

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
supported partials, the dominant series is preferred. Confidence is capped by
its power fraction. This is an instrument/timbre heuristic, not an arbitrary-
audio mathematical guarantee.

There is no permanent +/-35-cent acceptance gap. A stable detuned note is
rounded to its nearest MIDI note. Initial attack stability must be assessed by
the caller from consecutive continuous pitch estimates, not by refusing pitches
that are far from an equal-temperament centre forever.

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

The standalone executable passed 607 checks, including pure tones throughout
MIDI 28-100, a stable +40-cent tone, nonfinite input and all eight recordings.
An additional315 controls cover notes92-100 through the actual causal48-to-12kHz
frontend, with five starting phases and seven prefix/trailing observation
endpoints from128 through1024 samples (each estimator frame capped at512).
Every filtered high-note control produced its correct MIDI note.
AddressSanitizer and UndefinedBehaviorSanitizer also passed. The final detector
must still pass its independent full streaming evaluation after integration.

```sh
g++ -std=c++17 -O2 -I Source Source/Pitch/YinEstimator.cpp Tests/YinEstimatorTests.cpp -o yin-tests
./yin-tests Tests/AudioFixtures
```

These observations improve the tested monophonic acquisition errors, but do
**not** meet or establish the requested physical E2-under-10-ms or E3-under-5-ms
targets.
