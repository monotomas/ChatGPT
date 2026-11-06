# Real electric-guitar plucks: 6 November 2026 edition

This immutable edition contains **14 original isolated recorded plucks**, each
retained exactly once. It adds ordinary/staccato attacks, strengths and round
robins from a green Gretsch Anniversary recorded by Brian Wood and released
by Karoryfer Lecolds. It is not a continuous human performance, a new musician
or an independent instrument. The source does not specify a raw DI signal path
or every recording edit.

## Original source and rights

Source repository: `sfzinstruments/karoryfer.black-and-green-guitars`, commit
`b3b3249d37dc977a1a297bd2dc053e6d9b6b805c`. Its audio-library `LICENSE` is
CC0-1.0; the pinned author readme (`eguitar.txt`) also explicitly permits
royalty-free commercial and noncommercial use. This permission concerns the
sample library, rather than an inferred software license.

`source-manifest.json` binds every source URL, original SHA256, PCM shape and
author SFZ mapping. All WAVs remain mono 24-bit PCM at 44,100 Hz, without gain,
resampling, trimming, normalization or denoising. Their total size is 7,988,540
bytes. Filename octave conventions are not truth: for example `e3` explicitly
maps to MIDI40, and `e5` to MIDI64 in the retained author SFZ.

## Fixed partition and exposure

`selection-before-capture.json` and `split-before-any-detector.json` were
recorded before any detector capture of these selected recordings. The split
contains seven development and seven reserved recordings, with MIDI40,45,52,
57,64 represented in each partition plus staccato controls. Source metadata,
nominal pitches and annotation metadata are visible to the root and audit
agents. This is not a claim that all metadata is hidden or the instrument is
unseen.

At admission, only the seven development recordings have been exposed to the
detector. Their captures belong to separate research directories, outside this
fixture edition. The seven reserved recordings remain prospectively unrun
until an exact candidate is frozen. Preserve that distinction when reporting
later results; do not relabel reused development data as unseen validation.
No detector output, executable, model weight or generated float audio is
included here.

## Independent annotations and uncertainty

The original `PRE_CAPTURE_SEAL.json` and all five files it binds are retained
byte for byte. The sealed `annotate_independent.py` reads only source WAVs and
author metadata; it imports no detector and reads no MIDI results. The sealed
`independent-acoustic-annotations.json` records:

* author nominal MIDI anchors and source hashes;
* activity cues from fixed waveform amplitude/RMS rules. These describe time
  within an authored sample, **not physical pick/contact time** or certified
  bounds on physical latency;
* predeclared positive cores of 0.12–1.2 seconds for ordinary samples and
  0.08–0.14 seconds for staccato samples. These support useful-note coverage
  and interruption diagnostics in an early tonal body;
* descriptive offline partial-peak observations and 5 ms RMS envelopes. They
  are not instantaneous F0 curves or proof of every sample in a positive core;
* **unknown physical damping times**. EOF, fades and threshold crossings are
  not invented as physical NoteOff truth. Natural release measurements remain
  descriptive; controlled damping tests supply separate offset references.

Offline annotation windows may use samples after their center; the detector
must never use future samples or backdate its MIDI decisions. An initial
disagreement with the nominal SFZ anchor is not automatically proof of a
fabricated acoustic frequency during an uncertain attack transient.

Do not execute the sealed annotation script in this immutable directory: its
historical outputs and timestamps are already frozen. Any reproduction must
use a separate copied work directory and preserve the original edition.

## Integrity and optional numeric decoding

Run `python Tests/AudioFixtures/guitar_reliability_2026_11_06/verify.py`.
`INVENTORY_SHA256.json` independently inventories every edition file except
itself, including legal text, scripts, the original pre-capture seal and all
14 WAVs. Verification checks the complete inventory, original seals, raw PCM
hashes/shapes, fixed partition, author SFZ mappings and pinned legal text.

The optional `decode_exact.py --output /tmp/a-new-work-directory` converts
original 24-bit integers to exactly representable little-endian float32 values
outside this fixture edition. It applies no audio edits. The retained
`float32-exact-manifest.json` records those previously prepared numeric values;
generated float files are deliberately not duplicated in this edition.

Continuous human fretted slides, hammer/pull sequences and natural polyphonic
chord transitions remain absent. Success on these isolated plucks cannot
establish general guitar readiness, Ableton operation or physical latency.
