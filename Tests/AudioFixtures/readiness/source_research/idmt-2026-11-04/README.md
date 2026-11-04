# IDMT-SMT-Guitar source research

This is metadata and original-author paper research. It is not adoption of a
new audio corpus, a license determination for mirrored recordings, or a passing
Flysnapper evaluation. No dataset audio was downloaded or evaluated by this
audit. `findings.json` retains pinned repositories, source URLs and hashes.

## What the original authors establish

The original [Kehling et al. DAFx 2014 paper](https://github.com/jakobabesser/publications/blob/1a5c54631d4eca40c9c0aea77d94a9e564d1bbe4/pdf/Kehling_2014_DAFX.pdf)
describes direct electric-guitar recordings at 44.1 kHz, with manually authored
XML note parameters. Section 5.1 describes about 4,700 monophonic/polyphonic
note events with realistic guitar licks and introduced playing techniques,
plus 400 isolated normal-expression note events from three guitars.

The declared expression classes are normal, bending, slide, vibrato,
harmonics and dead notes. Plucking styles are finger, picked and muted.
Hammer-on and pull-off occur in its discussion of other literature; they are
not declared classes in its Table 2. Do not claim verified hammer/pull coverage
from this paper. A slide label also does not establish that an individual
recording demonstrates the user's discrete finger-over-frets technique rather
than another slide behavior.

The paper evaluates onsets with 50 ms tolerance and offsets with 200 ms
tolerance, with an additional 5 ms frame allowance. Those published results
do not establish original physical 5/10 ms onset truth or sample-accurate
continuous bend trajectories. Actual annotation precision requires inspection.

The original paper was read from the author's pinned public repository;
its PDF hash is recorded. The PDF and complete extracted copyrighted text are
not redistributed here.

## Available GitHub mirrors and references

`mogeadis/GS-Detector`, pinned `cf3697a57abbe4b21e46245fe900b6abb61ad419`,
contains paths for 234 WAVs in three pickup folders, 78 per folder. The README
maps them to isolated IDMT dataset1 notes from an Ibanez guitar and assumes
known note pitches/onsets/offsets for its string-detection task. All 234 file
names have the same `1111` code. A sampled XML entry is one picked,
normal-expression MIDI40 note with onset 0.227 s and offset 2.5 s. The path
inventory is not verification of actual PCM bytes or of technique-transition
coverage.

The mirror README licenses its source code under MIT and its paper under
CC BY-SA 4.0. It acknowledges the Fraunhofer recordings separately. Those
licenses do not, by themselves, establish the third-party audio's permitted
uses or redistribution terms. Original dataset terms remain unverified.

`Earl-Ju/guitar-technique-classification`, pinned
`63d74e500c6f620b1980eb783f0de7c225ff6685`, contains no audio. Its quick-start
explicitly generates dummy sine-wave bending/vibrato examples. Its MIT
software license cannot make those examples real guitar evidence or license
external datasets.

`pgaft/elec5305-project-520432082`, pinned
`26a268130aac08254f82a1fdb3cbcdbbf28fcfeb`, also contains no audio. Its README
links Zenodo record 7544110 and names `IDMT-SMT-GUITAR_V2.zip`. This is a useful
secondary download pointer, not verification of that record's current files,
checksums, annotation coverage or license.

## Remaining access and verification

The original Fraunhofer hosts (`www.idmt.fraunhofer.de`) and `zenodo.org` are
outside the current enforced network allowlist. GitHub metadata was read
through the configured proxy; no forwarding service or alternate network
route was used. A saved configuration draft does not change runtime access.

Permitted access to the original metadata/terms, followed by exact archive
and annotation inspection, is still required before using these recordings
as an accepted or redistributable validation corpus. In particular, no
real continuous fretted-slide, hammer/pull or independently bent polyphonic
coverage is newly approved by this metadata audit.
