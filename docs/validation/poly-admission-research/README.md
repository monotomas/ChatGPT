# Additional-source admission research: not accepted for the product

This directory is a retained research archive. Its synthetic fixtures and
detector-history captures ran on a CPU in the cloud environment. They are
not live audio-hardware measurements or recorded human guitar performances.
The captured histories do exercise the actual frozen detector frontend on
the synthetic two-string signal; that distinction is preserved below.

This experiment investigates why an A2 (MIDI45) plus E4 (MIDI64) can be heard
as only one source by the existing harmonic pursuit. It does **not** modify
the product, restore integrated MIDI acquisition, or establish musical
readiness. All cases examined here are now seen development/falsification
controls. No existing fixture labels or thresholds were changed.

## Main result

A constrained physical-model comparison can recover the clean two-string
hypothesis from a short causal history. It is not robust enough to adopt:
moderate noise defeats early identification, and plausible changes of one
string's harmonic envelope can still produce a false additional octave.
The Python optimizer is also orders of magnitude too expensive for a callback.

The most developed model (`probe_v6_partial_decay.py`) uses one/two physical
string groups, independent complex partial amplitudes, positive stiffness,
common F0/chirp/acceleration, and decay `gamma0 + gammaH*h²`. It correctly
handles the original seven clean mechanism cases and twelve differential
partial-decay cases. It still falsely admits MIDI57 when **one** MIDI45
string's odd partials fade while its even partials rise. The fundamental
remains present. This is the sort of false overtone/source interpretation the
user requires us to prevent; passing easier stationary tests is insufficient.

## Candidate construction and information supplied

The first source is assumed to have been identified by the existing detector.
Companion proposals come from three observed FFT peak bands. No desired voice
count or desired companion note is supplied to fitting/selection. A group is
not admitted merely because it reduces waveform error: model-complexity cost,
physical root energy and covariance-normalized partial support are checked.

For the actual-frontend probe, the prior anchor is parsed from the detector's
real emitted note. Ground-truth companion notes are attached to the output
only after inference. The driver retains histories from the frozen mainline
detector, including its DC blocker, lowpass, resampling and optional automatic
conditioning. The model sees only those trailing samples.

## Why the candidate is physically plausible

Equal-tempered E4 is approximately329.6276Hz; A2's exact third is330Hz, a
difference of approximately1.955cents. A sufficiently precise, coherent
frequency/phase observation can distinguish this from a single ideal stiff
string, whose higher modes stretch upward. Nominal FFT bin width is not an
absolute frequency-estimation limit. However, frequency uncertainty grows
rapidly with noise, short observations, model mismatch and overlapping modes.
This is not a universal identifiability claim for arbitrary guitar recordings.

## Retained progression and falsifications

| Variant | Observation | Decision |
| --- | --- | --- |
| Static frequency/stiffness with spectral-noise-only model comparison | Tiny physical-envelope errors looked extremely significant; false subharmonic/upper groups | Reject |
| Added common decay and full coefficient covariance | Clean pair recovered; stationary boosted-third, stiff-third and missing-root singles retained; moving single strings still split | Insufficient |
| Added causal F0 chirp/acceleration and wider existing-source frequency support | Single chirp retained, but finite-order vibrato approximation still became new sources when the recording's spectral noise floor was almost zero | Reject spectral-noise-only inference |
| Separate conservative null-model uncertainty floor | Seven clean cases correct;54 single-source controls across histories/noise produced no companions, but most noisy true-pair cases missed | Useful mechanism only |
| Differential partial-decay falsification | Seven of16 plausible one-source cases split under the preceding model | Reject preceding physical family |
| Added `gamma0 + gammaH*h²` and gain-rise support | Differential-decay cases fixed; odd/even timbre morph still generates a false octave | Not accepted |

The conservative uncertainty floor is explicitly **model uncertainty**, not
an assertion that wanted string residual is acoustic background. It prevents
a tiny physical-model approximation error from becoming arbitrarily certain
when additive noise is absent. It can also hide a real weak companion; those
misses are retained.

An ordinary pseudoinverse covariance silently drops null directions and can
make exactly shared harmonics appear falsely certain. The probe instead
retains all singular directions when calculating conditional coefficient
variance. This covariance still conditions on optimized nonlinear parameters;
it is not a complete joint frequency/stiffness/profile posterior.

## History/noise matrix for the frozen v5 method

`dynamic-history-noise/` contains63 cases: nine two-source configurations and
54 single-source controls. The true pair is accepted for:

* 256,512 and1024 samples with no added noise;
* 1024 samples with added white-noise standard deviation `.003`.

It is missed at256/512 samples with noise `.003`, and at all three lengths
with noise `.02`. These are21.3/42.7/85.3ms histories at12kHz, not complete-path
latencies. Matrix histories begin58.33ms after synthesis starts, and presume
an already identified first source. Gains/noise here change SNR; they are not equal-SNR gain-invariance
tests. There are no false companions among the54 single-source controls in
that matrix, but the later physical-mismatch controls invalidate a broader
precision claim.38 of181 candidate optimizations failed to converge; all
reported accepted pair hypotheses did converge. Unconverged solves cannot
be used as reliable acquisition evidence.

## Actual detector frontend

`capture_frontend.cpp` renders exactly the three-partial45+64 portion used in
the strict MPE processor control, through an immutable mainline source copy.
The detector emits45 at host sample2687 (55.98ms) and does not emit64.
At each tested subsequent history endpoint64,85.33,106.67,128,170.67 and256ms,
the frozen v5 offline comparison proposes and accepts64, with automatic input
on and off. This shows useful information is present in those histories. It
does **not** mean a new MIDI note was emitted at64ms: no product integration
was performed and Python compute time must not be omitted from latency.

The candidate solve median in the63-case matrix is approximately0.73seconds
on the shared worker. Exact timings and convergence status remain in JSON.
These are implementation diagnostics, not a certified runtime bound. A native
bounded implementation would need analytic derivatives, strict iteration and
candidate limits, numerical/uncertainty validation, and complete callback
measurements before any real-time use could be considered.

## What is still required

Cold-frame residual improvement alone cannot distinguish a new string from
all legitimate changes of an existing string's timbre. Temporal per-partial
profile/phase predictions, explicit excitation/change hypotheses and joint
null-space uncertainty are required. A harmonic-coincident companion needs
new evidence beyond the fact that a second dictionary group fits an evolving
envelope. An exactly identical waveform cannot reveal the performer's intent.

The method has not been run over the full196-case corpus or the recorded-noise
poly corpus: the targeted physical falsification already rejects adoption.
There is no justification for replacing the validated legacy admission path
with this expensive, incompletely validated prototype.

## Reproduction

Python3, NumPy2.3.5 and SciPy1.17.0 were used with single-threaded BLAS. Product
or shared Ninja builds are not needed. Each output directory must be new.

```sh
OPENBLAS_NUM_THREADS=1 OMP_NUM_THREADS=1 python probe_v5_fixed_uncertainty.py \
  --dynamic --output /tmp/admission-v5-reproduce
OPENBLAS_NUM_THREADS=1 OMP_NUM_THREADS=1 python falsify.py
OPENBLAS_NUM_THREADS=1 OMP_NUM_THREADS=1 python falsify_v6.py
```

The two falsification scripts preserve fixed output names and refuse to
overwrite them. To repeat them, copy only the method and driver scripts into
a fresh working directory; do not copy the existing result directories there.
Run from that working directory, leaving this retained archive unchanged.
The generic model runner accepts a new `--output` path as shown above.

`frontend_test.py` additionally needs the retained `frontend-auto.csv`,
`frontend-manual.csv` and corresponding `*-notes.txt` inputs. Copy those into
the fresh working directory with the method and driver if repeating that
offline analysis. It writes new results in that working directory. Rebuilding
`capture_frontend.cpp` needs only the archived C++ files in
`main-snapshot/Source/Pitch`; no shared project build is involved.

Earlier scripts and failed results are retained alongside the final model.
Source hashes, data plans and results manifests identify each run. The
original archive manifest and README are preserved under
`historical-archive-metadata/`; this README adds archive/reproduction
clarifications without changing scripts, input samples or measured results.
The current `archive-manifest.json` hashes every retained file except itself.
Compiled executables, environments and external dependencies are excluded.
