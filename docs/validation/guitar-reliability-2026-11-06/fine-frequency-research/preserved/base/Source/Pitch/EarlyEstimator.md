# Rejected early-regression candidate

`EarlyEstimator` is an isolated research experiment. It is **not enabled by the
shipping pitch detector** and its `accepted` flag is a heuristic, not a pitch
certificate or latency guarantee. It was retained to make the discarded
low-observation hypothesis reproducible instead of silently promoting early
wrong notes into a latency claim.

The candidate fits six free-phase partials for each MIDI hypothesis36–88.
Sine/cosine columns have their means removed, harmonic weights are h^-0.7,
and ridge regularisation is0.02N. `prepare()` computes all bases/inverses;
`estimate()` allocates nothing and reads only the available trailing frame.
A proposal needs explained score>=0.93, best-minus-runner-up margin>=0.025,
and fundamental coefficient energy>=2% of model coefficient energy. Polyphonic
requests deliberately abstain because a single harmonic model does not establish
independent voices.

At48kHz,192/256/384/512 observations mean4/5.333/8/10.667ms of data, before driver,
host, output instrument or audio-output latency. Standalone tests accepted
18/23/30/35 of49 clean sinusoidal notes respectively, with no wrong accepted
sinusoidal pitch, and rejected100 deterministic white-noise frames. The
remaining90/196 sinusoidal cases abstained. These are useful algorithm controls,
not guitar evidence.

A separate check used the independent fixtures under `Tests/AudioFixtures`.
The original annotated waveform onset was kept (first |sample|>=0.001 after the
shared gain); no per-file onset optimisation was applied. Each prepared frame
was moved every32 full-rate samples through the first50ms after onset. Results:

| Frame samples | Recordings with any accepted proposal /8 | Recordings with any correct accepted proposal /8 | Observed wrong proposals |
|---|---:|---:|---|
|192|0|0|None; all abstained|
|256|1|1|None in these frames|
|384|3|2|A2→MIDI56, G3→56, B3→58|
|512|3|2|B3→58, F#3→55|

Neither E2 nor E3 had a correct accepted proposal within50ms at any of the four
observation lengths. At384 samples the first wrong accepted proposals appeared
around26–29ms after waveform onset; at512 samples some wrong proposals preceded
the later correct B3 decision. Thus even stringent score/margin tests did not
make a short flexible harmonic model trustworthy on guitar attacks. It must not
be used to emit early notes merely because its clean-sinusoid checks pass.

Reproduce the basic implementation checks:

```sh
g++ -std=c++17 -O2 -I Source Source/Pitch/EarlyEstimator.cpp Tests/EarlyEstimatorTests.cpp -o early-tests
./early-tests
```

The independent production detector and `TimeDomainPursuit` use their own
measurement and acceptance checks. A full observation period is not asserted as
a universal mathematical lower bound; these results concern this tested
candidate and dataset.
