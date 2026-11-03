# Second sealed confirmation corpus

Prepared after preserving the first blind iteration-14 audit and before
examining any detector outputs on this new set. The first holdout remains
unchanged and is now regression evidence when used to guide later changes.
This second set must be evaluated only after another recorded source freeze.
Preparation does not execute, consult or tune the detector.

There are 25 PCM waveforms / 46 case-mode combinations: nine disjoint edited
recorded notes across acoustic, electric and nylon guitar; six independently
seeded held string tones with evolving partial balance and mild beating; four
repeat/legato articulations; four new polyphonic voicings including a weak late
voice; and two unpitched controls. The full three-rate/three-block matrix has
414 runs. Synthetic timing and reference MIDI identities are defined by
construction. The synthetic articulations are controlled signals, not claims
about a human player's exact gestures.

## Source, licensing and attribution

The original recorded assets come from Nicholaus P. Brosowsky's
[tonejs-instruments](https://github.com/nbrosowsky/tonejs-instruments), pinned to
commit `622c2f1c32c8cfce4158ddc3eb26e518ddef37e5`, with sample license
[CC-BY-3.0](https://creativecommons.org/licenses/by/3.0/). Every active recording
has an upstream source path disjoint from both the main corpus and the first
holdout. The upstream README, license and source attribution are retained under
`tonejs`, with SHA256 identities. These recordings share the collection's editing
provenance; they are not an independently recorded natural live performance set.

Attribution: University of Iowa Electronic Music Studios for acoustic guitar;
Karoryfer Samples for electric guitar; Freesound contributor quartertone,
"classicalguitar-multisampled", for nylon/classical guitar; collection and sample
editing by Nicholaus P. Brosowsky. The upstream collection declares trimming,
ramps, normalization, denoising and some pitch correction.

Our derived recordings use mono 48kHz decode, common gain .35, the first 650ms
of the unchanged source including its original attack/leading silence, a 4ms
terminal taper, 50ms leading padding and 250ms trailing silence. Reference onset
uses a fixed absolute amplitude >=.001 (-60dBFS) rule. Original Ogg bytes are
unchanged. Derived recorded WAVs remain CC-BY-3.0 with the modifications declared
here; original synthetic waveforms are AGPL-3.0-or-later with Flysnapper source.

Frequency labels were checked against sustained harmonic peaks before sealing.
One upstream `guitar-nylon/D5.ogg` filename implies MIDI74/587.33Hz, but the
recording's sustained fundamental is approximately 621.8Hz (near MIDI75). It was
excluded **before any detector evaluation**, retained for provenance, and replaced
with an independently checked disjoint A4 recording. This is recorded in the
confirmation manifest. No reference label was changed to fit detector behavior.

## Integrity, seal and evaluation

Generate/verify only:

```sh
python Tools/audio_validation_confirmation.py --verify
```

The verifier uses only Python's standard library. Generation without `--verify`
requires numpy/FFmpeg. It verifies original and derived hashes, PCM parameters,
all annotations/TSV consistency and disjoint recorded-source paths. The sealed
fixture-manifest SHA256 is recorded in `SEALED_SHA256.txt`.

Only after a new candidate's source freeze:

```sh
python Tools/audio_validation_run.py --confirmation --full --evaluation-role first-blind --output docs/validation/audio-confirmation-first
```

The command is documentation, not evidence that it has been run. No second-set
outcomes were inspected while preparing this corpus. Reusing its revealed
outcomes for subsequent fixes must be marked `--evaluation-role seen-regression`.
The first result must remain available; this set must not replace or erase the
first holdout's failures, the broad matrix, or the frozen 30-case gate.

These annotations are waveform/onset/cutoff references, not mechanical pick or
finger-release timestamps. The corpus cannot establish all-instrument or live
multi-string generalization, and does not include a DAW, receiver or physical
5/10ms end-to-end latency chain.
