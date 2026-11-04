# Attack-pitch annotation audit

This is a post hoc interpretation audit of **already-seen** recordings, not a
new acceptance edition or a relabelled pass. All original sealed labels,
policy, PCM and failing c5b/candidate results remain unchanged.

The author SFZ establishes the intended sampler-key anchors 40, 43, 45 and 52.
It does not establish a constant fundamental at every instant of each raw
recording's attack. The original long-window source audit supported the
sustained nominal tuning only. It was insufficient to establish first-20-ms
acoustic pitch truth, and the edition correctly marks fractional pitch truth
as unavailable.

`report.json` analyzes original 24-bit 44.1-kHz WAV bytes directly, without any
Flysnapper filtering, normalization, resampling or DSP imports. It retains
source hashes and reproducible short-window spectral and phase-advance
diagnostics. Upper-partial identity is assumed from the nominal range;
short-window overlap, attack transients and nonstationarity remain limitations.
Zero-padding does not create actual frequency resolution. These estimates
are acoustic evidence, not a newly certified instantaneous F0 curve.

The medians below use qualifying local peaks in partials 2–5. Values are MIDI
pitch; a one-unit difference is one semitone. Times are relative to **authored
sample playback start**, not an unknown physical finger-contact time.

| Source filename | SFZ anchor | 0–20 ms partial median | 0–28 ms partial median | 30–60 ms partial median |
|---|---:|---:|---:|---:|
| `hammer_a3_rr1.wav` | 45 | 44.21 | 44.78 | 45.16 |
| `hammer_e3_rr1.wav` | 40 | 41.38 | 41.25 | 40.06 |
| `hammer_e4_rr2.wav` | 52 | 51.11 | 51.33 | 52.06 |
| `hammer_g3_rr1.wav` | 43 | 42.16 | 42.22 | 43.18 |

Independent phase advance broadly agrees that these recordings contain attack
pitch drift. Thus an initial note differing by one semitone from the SFZ key
cannot automatically be called a fabricated frequency or an octave-tracking
mistake. The input itself differs from its later intended anchor.

However, agreement with an early **history window** is not proof of accurate
pitch **at emission time**. A Hann window over the complete attack history has
its energy center substantially earlier than the emitted NoteOn. This matters
when pitch moves rapidly. For example, the frozen v10 mono capture gives:

| Source | NoteOn after playback start | Emitted pitch | Complete-history partial median | 20-ms window centered on NoteOn | 30-ms window centered on NoteOn |
|---|---:|---:|---:|---:|---:|
| `hammer_a3_rr1.wav` | 52.65 ms | 45.09 | 45.07 | 45.20 | 45.15 |
| `hammer_e3_rr1.wav` | 28.65 ms | 41.31 | 41.25 | **inconclusive: one qualifying partial** | 40.52 |
| `hammer_e4_rr2.wav` | 20.65 ms | 51.12 | 51.12 | 51.90 | 51.75 |
| `hammer_g3_rr1.wav` | 28.65 ms | 42.21 | 42.22 | 43.00 | 43.07 |

Centered windows use future source samples for offline annotation only. They
are never permitted as detector input or causal latency evidence. Their
window dependence is retained, particularly the strongly nonstationary low-E
attack. For the other two early outliers, these diagnostics support a concern
that the emitted pitch follows earlier attack history after the current source
has moved nearer its intended anchor. They do not justify declaring the tracker
instantaneously correct merely because a causal historical FFT agrees.

The frozen capture being compared is `/tmp/flysnapper-mono-v10-recorded-hammers`;
its raw events, callback report and measurement manifest are hash-bound in
`report.json`. Candidate-specific values are supplementary comparisons, not
inputs to the independent source analysis. Reproduce into a new directory:

```bash
python Tests/AudioFixtures/readiness/recorded_hammers/audit_attack.py \
  --output /tmp/new-hammer-annotation-audit \
  --capture /tmp/flysnapper-mono-v10-recorded-hammers
```

The strict edition continues to ask for the intended SFZ anchor and retains
every failure. Its `wrong-before-correct` count must be described as **wrong
against the intended nominal anchor**, rather than universal proof of an
acoustic frequency error. A future independently annotated real-performance
edition should distinguish intended note identity, actual transient F0,
stable pitch and physical onset/damping times before detector evaluation.
Neither revised interpretations nor newly fitted curves may retroactively turn
these known strict failures into unseen success.
