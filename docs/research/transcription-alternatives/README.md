# Alternative transcription methods inspected

These primary-source inspections inform the November 4 research. They are not
benchmarks of the models, and no external model/runtime code was adopted.
`source-provenance.json` records pinned revisions and file hashes; the pinned
files match the preliminary source reads byte for byte.

## Spotify Basic Pitch

Inspected revision
[`fa5997af0a8210982619003269994a1be25eddf3`](https://github.com/spotify/basic-pitch/tree/fa5997af0a8210982619003269994a1be25eddf3).
`basic_pitch/constants.py` specifies 22,050 Hz audio, a 256-sample hop (about
11.61 ms), a 27.5 Hz base frequency and two-second training examples.
`basic_pitch/models.py` combines CQT features with temporal convolutions using
`padding="same"`, including time kernels larger than one frame.

Those symmetric temporal convolutions consume future feature frames for a
given output. Fast inference throughput or the hop interval alone therefore
does not establish causal live decision latency. Two-second examples are not,
by themselves, a proof of two-second irreducible latency either. Supplying
zeros for future audio, shifting convolution kernels or retraining a causal
variant changes the operating conditions and needs new accuracy/latency
evidence, especially on bends, weak fretting attacks and real noise. It is not
a verified drop-in repair for this plug-in's low-latency requirements.

## NeuralNote v2 / MuScriptor

Inspected revision
[`755332b53e1c62ed0105b10ebff341618b23dd69`](https://github.com/DamRsn/NeuralNote/tree/755332b53e1c62ed0105b10ebff341618b23dd69).
Its README describes replacing Basic Pitch with MuScriptor transformer models
of 103 million to 1.4 billion parameters. Listed model downloads are about
210 MB, 620 MB and 2.7 GB. The documented workflow gathers audio, then runs
Transcribe and exports MIDI from a piano roll.

That workflow is useful for transcription, but supplies no evidence of
sample-causal, continuously routed guitar MIDI at the required live onset
latency. Real-time factor is inference throughput, not live first-note delay.
No models were downloaded or tested here, so this report makes no comparative
quality claim. A future causal model remains a research option; its latency,
technique discrimination, resource use and licensing would need verification.

## Current investigation

The implemented research instead measures bounded, past-only tonal/noise
evidence, articulation and continuous multi-source harmonic/phase models.
Static joint harmonic fits alone do not identify source ownership when string
partials coincide. Temporal complex-coefficient priors improve some controlled
bends but regress other motion/phase-reset cases in independent tests. This
is an unresolved modeling problem, not a reason to label an incomplete
installer finished. See the retained [model audit](../../validation/musical-readiness-research/README.md)
and [readiness protocol](../../../Tests/AudioFixtures/readiness/README.md).

A later [streaming-implementation benchmark](../causal-alternatives/README.md)
inspects three additional candidates and measures the compact PitchPlease
models privately against 324 disclosed project cases. They recover additional
notes but produce many more false voices; neither is accepted as a standalone
replacement. External kernels and model weights are excluded from this source
delivery, with provenance and redistribution limitations recorded separately.
