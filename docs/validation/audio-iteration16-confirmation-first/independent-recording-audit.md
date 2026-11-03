# Independent recorded-input audit for blind iteration 16

This read-only audit concerns the second confirmation corpus after its first
frozen detector evaluation. No detector was rerun or tuned for this analysis.
No source recording, derived WAV, annotation, or original measurement file was
changed.

## Derivation and attribution sanity

All nine active recorded WAVs reconstruct **sample-for-sample** from the retained
Oggs: mono float32 decode at 48 kHz, converted to float64; common gain 0.35;
first 31,200 samples including original attacks/silence; a 192-sample linear
terminal taper; 2,400 leading and 12,000 trailing zero samples; nearest-even
rounding after multiplication by 32,767; PCM16 output. Each reconstructed file
has 45,600 samples, with zero differing PCM values. Recomputing the declared
absolute-amplitude onset rule on the scaled source reproduces all nine
annotations exactly. The common artificial cutoff is fixture sample 33,600.

All retained upstream source SHA256 values match the manifest. The nine active
source paths are disjoint from the public development set and first holdout;
the corpus integrity verifier checks that disjointness. The original Ogg bytes,
upstream README/license/source information, and declared editing operations are
retained under `Tests/AudioFixtures/confirmation/tonejs`.

The notices identify Nicholaus P. Brosowsky's tonejs-instruments collection at
commit `622c2f1c32c8cfce4158ddc3eb26e518ddef37e5`, with CC-BY-3.0 sample
licensing. Source attribution names the University of Iowa Electronic Music
Studios for acoustic guitar, Karoryfer Samples for electric guitar, and
quartertone's classical-guitar multisample collection for nylon guitar. The
collection itself describes trimming, ramps, normalization, denoising and some
pitch correction. These are edited isolated-note assets, not newly recorded
natural multi-string performances.

The unused upstream nylon `D5.ogg` naming discrepancy was identified and
documented before detector evaluation; the active A4 replacement was separately
checked and sealed. It is not a post-result label correction.

## Three wrong initial mono notes

At 48 kHz / 64 samples, these are the complete onset/transition errors:

| Input | Reference | Initial wrong note | Correct note begins | First useful response |
| --- | ---: | --- | ---: | ---: |
| Acoustic C4 | 60, onset 66.583 ms | 32 at 130.646 ms | 151.979 ms | 85.396 ms |
| Nylon G#4 | 68, onset 50.042 ms | 56 at 141.313 ms | 213.313 ms | 163.271 ms |
| Nylon A4 | 69, onset 50.000 ms | 41 at 157.313 ms | 178.646 ms | 128.646 ms |

Each wrong note ends at the identical sample where the correct note starts.
There are no additional same-pitch restarts during the subsequent sustain.
The last two delays account for the high mono P95/maximum, despite every
reference eventually being matched.

I examined raw-input Hann-window spectra immediately before the wrong Note Ons
and again during later sustain. The short windows below are fixture times,
including the 50 ms padding:

| Input/window | Strong resolved pitch family, approximate Hz | Other substantial early components |
| --- | --- | --- |
| C4, 87–130 ms | 261.5, 526, 1,314, 1,578 | 109 Hz at about 12% of maximum peak magnitude; weaker 159/316 Hz |
| G#4, 99–141 ms | 417, 835, 1,253, 1,672, 2,088 | 101 Hz at about 42%; 244 Hz at about 11% |
| A4, 115–157 ms | 441, 881, 1,319, 1,763, 2,206 | 195 Hz at about 28%; 101 Hz at about 16%; 253/307 Hz |

Later 300–550 ms spectra retain the expected pitch families: approximately
261/523 Hz for C4, 414/831 Hz for G#4, and 441/880 Hz for A4. This supports the
reference identities and also shows changing relative harmonic/body energy.
The initial incorrect MIDI roots are not the dominant resolved fundamental.
Nonharmonic/body components can make a numerical period estimate ambiguous,
but this audit does not establish the exact internal branch responsible: no
detector decision trace was recorded here.

Methods: NumPy 2.3.5, FFmpeg 7.1.5; a 131,072-point zero-padded real FFT of each
mean-subtracted Hann-windowed interval, followed by local-maximum selection and
parabolic log-magnitude interpolation. The short physical observation is only
42–43 ms, with roughly 23–24 Hz Rayleigh resolution. Zero padding interpolates
the displayed maxima; it does not create a 0.366 Hz resolving observation or
provide a general cents-accuracy certification. The longer sustain interval has
4 Hz Rayleigh resolution.

## Limits

Source-library labels, acoustic spectra and exact derivations support these
isolated-note references. The corpus has no human-labeled physical pick-contact
or finger-release timestamps. Its onset is a fixed amplitude rule and its end
is an imposed edited cutoff. Therefore neither this audit nor the correct MIDI
event times establish physical end-to-end latency or general live performance
accuracy.

The blind16 musical errors remain errors. Nothing in this input audit changes
their counts, replaces an annotation, or makes the user's 5/10 ms objectives
verified. `scoring-audit.json` binds the original four result files by SHA256;
`recording-derivation-audit.json` records this independent reconstruction and
its input identities.
