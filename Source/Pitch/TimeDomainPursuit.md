# Free-phase harmonic group pursuit

This helper addresses a concrete failure in the original Gaussian-comb matcher:
C3/E3/G3 fundamentals can merge into a single broad FFT maximum, while their
upper partials still provide independent evidence. Requiring each fundamental
to be a separate FFT peak suppresses the entire real chord.

Each MIDI candidate from 28 through 100 has eight partials, with independent
sine/cosine coefficients and h^-0.5 regularisation weights. All 512-sample bases,
analytic cross-Gram terms, and 16-column ridge inverses are prepared outside
processing. During analysis, group pursuit selects a model by its residual
projection improvement. Twelve bounded block-coordinate sweeps jointly refit
selected models after each addition. This avoids repeatedly subtracting a fixed
1/h spectral envelope whose stronger-than-average partials can become false
new notes.

Preparation uses approximately 8 MB per helper instance and measured roughly
70 ms in this shared Linux VM. A preliminary GCC -O2 microbenchmark measured
about 227 microseconds per synthetic triad analysis, and about 435 microseconds
for a dense six-recording mixture. These are kernel timings, not deadline or
latency guarantees. SSE2 accelerates the projection dot products on x86-64;
other architectures use the same bounded scalar calculation.

At 12 kHz, a full 512-sample observation spans **42.667 ms**. Earlier source
samples are not future data, but this trailing observation still contributes
real detection delay and transient smearing. Zero reported plugin latency does
not remove it. The helper must not be advertised as satisfying the physical
E2-under-10-ms or E3-under-5-ms goals.

To avoid invented subharmonic roots, candidate coefficient energy must include
a meaningful fundamental contribution (5% before selection, 3.5% after joint
refit). Relaxing this criterion to 0.8% created fictitious low notes during the
independent synthetic triad's decay. A root can have a meaningful regression
coefficient even when its FFT peak is unresolved; these are different tests.

The standalone controls recovered exactly MIDI 48/52/55 in a synthetic major
triad and 52/53 in adjacent-semitone input. Independent fixture frames from
`major_triad.wav` and `adjacent_semitone.wav` at 100 ms produced the same sets.
The dense real-recording mixture remained poor: one measured 100-ms frame
produced 43/45/47/52 instead of the six reference notes. That is a failure, not
an accepted transcription. The helper alone does not solve arbitrary guitar
polyphony.

Free harmonic coefficients cannot reliably establish separate octave-related
voices: an upper octave's partials are a subset of the lower note's partials.
Nor does the conservative fundamental criterion support every missing-
fundamental signal. These limitations must stay visible in the independent
end-to-end fixture measurements; passing the basic controls does not establish
product acceptance.

Standalone helper controls, including the rejected early candidate's tests:

```sh
g++ -std=c++17 -O2 -DFLYSNAPPER_TIME_DOMAIN_TESTS -I Source Source/Pitch/EarlyEstimator.cpp Source/Pitch/TimeDomainPursuit.cpp Tests/EarlyEstimatorTests.cpp -o harmonic-tests
./harmonic-tests
```

That run passed 1,400 checks. The same test binary also passed AddressSanitizer
and UndefinedBehaviorSanitizer instrumentation. Sanitizer testing concerned
these helpers; it did not test a DAW, audio driver or full VST3 chain.
