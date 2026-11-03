# Free-phase harmonic group pursuit

This helper addresses a concrete failure in the original Gaussian-comb matcher:
C3/E3/G3 fundamentals can merge into a single broad FFT maximum, while their
upper partials still provide independent evidence. Requiring each fundamental
to be a separate FFT peak suppresses the entire real chord.

Each MIDI candidate from 28 through 100 has eight partials, with independent
sine/cosine coefficients and h^-0.5 regularisation weights. All 512-sample bases,
analytic cross-Gram terms, and 16-column ridge inverses are prepared outside
processing. During analysis, group pursuit selects a model by its residual
projection improvement. A bounded incremental Cholesky factorisation jointly refits
selected models after each addition. This avoids repeatedly subtracting a fixed
1/h spectral envelope whose stronger-than-average partials can become false
new notes.

Preparation uses approximately 8 MB per helper instance and measured roughly
70 ms in this shared Linux VM. The earlier coordinate-sweep version had a preliminary GCC -O2 microbenchmark of
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

Free harmonic coefficients alone cannot reliably establish separate octave-related
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

## Octave-overlap experiment: joint solve and envelope prior

The current joint refit uses an incremental Cholesky solve, not twelve
coordinate sweeps. Shared octave/fifth columns converge slowly under coordinate
updates: a small waveform residual does not establish correct allocation of
energy between notes. Cholesky factorises at most a 96×96 system (six notes,
sixteen columns each) on the processing thread, using bounded stack arrays.
It adds only the rows for each newly selected model and uses SSE2 dot products
on x86-64. The per-candidate inverses and complete cross-Gram remain prepared
outside processing. This implementation is allocation-free after prepare;
that does **not** mean factorisation-free or safe at every callback deadline.

After an ordinary residual projection stops improving sufficiently, a generic
octave prior can propose a note twelve semitones above an already fitted one.
It fits log-amplitude decay to physical odd-partial coefficients 1/3/5/7 and
requires at least three of even partials 2/4/6/8 to exceed twice the predicted
amplitude. This is a heuristic about harmonic envelopes, not an assumed chord
voicing or a lookup of fixture truth. Within-frame evidence survives joint
coefficient redistribution so that one added octave does not erase the prior
for a different real octave. No evidence is retained across audio frames.

An independent experimental full-corpus run with this helper recovered the
synthetic open E-minor six-note fixture and the three-octave fixture without
extra Note Ons. The first version's sixth open-E note appeared very late (about 305 ms after
its reference onset), because the final fundamental-coefficient gate rejected
an already selected, prior-supported voice. The subsequent independent-partial
check described below recovered all six around 49 ms after onset. Neither
observation is a physical live-latency result. Strummed A-minor recovered all five notes but retained
other lifecycle errors. All eleven single modal controls, the bright-string
control and negative controls remained clean in that run.

The same experiment increased false Note Ons substantially on recorded
mixtures: recovering all six reference pitches in one mixture still produced
69 extra onsets. Its precision is unacceptable without further admission and
model-selection work. Full-corpus results, not only the simple controls, must
determine acceptance. A representative six-note helper-frame benchmark had
median 464 µs, P95 711 µs, P99 894 µs and maximum 1,649 µs on this shared Linux
VM. These CPU timings exclude the surrounding detector/host; they are not a
32-sample deadline guarantee or audio-latency measurement.


For an octave-prior hypothesis with a weak final fundamental coefficient,
acceptance additionally requires model energy above 1.5% of input energy and
at least two independently occupied upper partials (5–8). Each must exceed 10%
of that note's largest harmonic amplitude, and their combined squared amplitude
must exceed 2% of the note's squared harmonic amplitude. Partials within one
radian of accumulated phase over the observation window of another selected
model partial do not count as independent. These conditions address numerical
allocation of nearly shared frequencies; they do not unconditionally admit a
missing fundamental.

In the 81-case experimental comparison, this change reduced the open-E fixture's
maximum first-useful-note delay from 304.65 to 48.65 ms. No other musical outcome
count (matches, false onsets or early releases) changed. The broader recorded
mixture failures remain and must not be hidden by that local improvement.
