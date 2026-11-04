# Joint dynamic polyphonic model experiments

These are **rejected or unfinished research**, not a release recommendation.
Main `Source/Pitch` and public interfaces were not changed. All inputs are the
already disclosed eight synthetic, oracle-seeded six-source controls. Original
source identity, current observation time, missing voices and the original
strict policy are retained; no error curve is shifted or reassigned.

## Results

| Variant | Method | Strict cases passed | Six-source CPU P95 (ms) |
| --- | --- | ---: | ---: |
| 23 | Joint F0/chirp | 3/8 | 2.420 |
| 24 | Velocity information attenuation | 3/8 | 2.355 |
| 25 | Innovation reset / weak coefficient prior | 2/8 | 2.683 |
| 26 | Adaptive per-partial precision | 3/8 | 3.474 |
| 27 | Robust Huber residual | 3/8 | 2.908 |
| 28 | Gaussian reset evidence | 3/8 | 6.026 |
| 29 | Twelve harmonics | 4/8 | 6.647 |
| 30 | Joint stiffness | 3/8 | 3.332 |
| 31 | Radial/tangential + band SNR | 3/8 | 3.018 |
| 32 | Radial/tangential only | 3/8 | 3.433 |
| 33 | Band-SNR only | 3/8 | 3.433 |

These active-thread CPU observations come from a shared managed machine,
including captures before and after its restart; they are not isolated or
certified real-time bounds. Even the better models exceed the entire 1.333 ms
budget of a 64-sample/48 kHz callback before other plug-in work. The restarted
baseline 23 reproduces its original `voices.csv` byte for byte; wall/CPU timing
and therefore the complete capture-manifest hash naturally differ.

## What the physical model changed

Variant 23 models every source with a frequency at the actual past frame center
and a frequency slope. Its sinusoidal basis has quadratic phase, so the model
can publish its own instantaneous frequency at the newest received sample.
A joint regularized linear solve estimates complex harmonic coefficients; a
small Schur-complement Gauss–Newton system updates all source frequencies and
slopes together, with objective backtracking. This differs from shifting a
delayed estimate or extrapolating a postfilter against the known answer.

It substantially improves some previously failed motion examples: C-major7
opposed bends have aggregate P95 about 5.3 cents and maximum 13.7, and B7 has
P95 about 9.8/maximum 22.7. However weak-tail maximum error reaches150 cents and
the neighbouring excitation/phase-reset case reaches216 cents. Varied-window
vibrato/reversal worst-source P95 remains about 26/25.4 cents, above the fixed
25-cent policy. None of these oracle results test note acquisition or MIDI.

The extra methods did not establish a ready alternative:

* Velocity information from the regularized local normal matrix did not stop
  weak-tail drift. Information derived from an already constrained source
  decomposition must not be confused with independent acoustic observability.
* Reducing phase priors on an inferred excitation can release ownership of
  shared harmonics. Variants 25 and 28 suffer errors above 2000 cents after the
  phase-reset example; their lower residual/objective is not correct identity.
* Robust sample residual weighting reduces some extreme phase-reset error,
  but leaves the weak-tail and motion failures.
* Twelve harmonics improve one reversal case, but stiffness/weak-tail failures
  remain and CPU cost rises substantially.
* A jointly fitted physical stiffness parameter improves some spectral bias,
  but adds ambiguity between frequency, slope and stiffness during motion.
* Allowing radial amplitude withdrawal while retaining tangential phase
  prediction does not fix the weak tail. The radial-only ablation worsens
  vibrato/reversal worst-source P95 from about 26 to 40 cents. Thus natural
  amplitude decay alone does not explain or solve this failure. Band-SNR-only
  weighting stays near the original three passing cases.

## Numerical checks and limitations

Finite-difference checks compare the complete regularized objective with the
analytic variable-projection gradient. Retained sampled checks give maximum
absolute differences about 4.2e-8 for weighted residuals, 2.6e-6 for the additional
stiffness parameter, and 5.2e-8 for the anisotropic coefficient prior. These
checks support the sampled derivative calculations, not general tracking
correctness, convergence, musical readiness or CPU deadlines.

The strict labels, policy and original negative captures are unchanged. All
source snapshots, capture/scoring support, original manifests and summaries
are under `variants/`. Raw frame/voice/error CSVs are gzip-compressed without
changing their bytes; `comparison.json` records both raw and compressed hashes.

The retained `variants/chirpN/manifest.json` files are the original capture
manifests. Their `source/Pitch/<filename>` inventory entries map to
`variants/chirpN/Pitch/<filename>` in this archive; the source bytes and hashes
are unchanged. Absolute `/tmp` paths in those manifests describe the historical
capture locations. Use the archived variant directory as `--source`, as shown
below, rather than recreating those temporary paths. The original manifests
and results have not been rewritten for the archive layout.

To reproduce a retained snapshot using the unchanged project fixture edition:

```sh
python Tests/AudioFixtures/readiness/poly_tracking_v1/run.py \
  --source docs/validation/poly-research/dynamic-models-v2/variants/chirp23 \
  --output /tmp/new-dynamic-poly-capture
```

The source snapshots are standalone experiments with their own header, not
drop-in patches for the current product integration. Do not replace the main
tracker or its two-argument preparation interface with these files. A future
model needs source-specific excitation/change-point handling and credible
joint uncertainty under coincident partials; unsupported old pitch should not
be presented as a confidently tracked moving string.
