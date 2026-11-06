# Independent numeric review of the private profile kernel

Reviewed on 6 November 2026. This is a review of the private frozen research
model, not an assessment of musical correctness or readiness for release.

The baseline `TimeDomainPursuit.cpp` is byte identical to
`/tmp/flysnapper-v4-poly-audit/time-atoms-held-prediction/Source/Pitch/TimeDomainPursuit.cpp`.
Its SHA-256 is `ea6599dcd2e21046de57e7ddc92bb66779f31037508dcf3d57c53e29f0d15ebd`;
the reviewed fast source is
`4bf86a11e51ee0d6d6b126fc387483aa5fa8ee867f56eb19a069a1d9d734548f`.

The compression is algebraically equivalent in real arithmetic. For harmonic
`h`, the fixed phase direction maps amplitude `a[h]` to the two coordinates
`a[h] * phaseSin[h]` and `a[h] * phaseCos[h]`. Projecting the correlation onto
these directions gives the same numerator. Projecting the symmetric self-Gram
matrix, adding the diagonal ridge, and summing its diagonal plus twice its lower
triangle gives the same denominator. The baseline stores both halves of this
Gram matrix from the same float value, so its required symmetry holds exactly
in the reviewed source. Invalid harmonics retain zero phase/amplitude and do
not contribute.

The cached normalization evaluates precisely the baseline float-to-double
products, accumulation order, square root, and reciprocal for each of 256 valid
harmonic masks and 27 profiles. All 6,912 cache values compare bit for bit to
fresh baseline evaluation. Profiles are immutable after preparation through
the production interface. A future implementation that mutates profile values
would also have to refresh this cache; frequency-only validity changes can use
the existing mask cache.

Compressed arithmetic changes rounding and is used only to shortlist profiles.
The retained profile is rebuilt and scored with the original 16-dimensional
arithmetic. Profiles within `compressedBest * 1e-9` are re-evaluated in ascending
original profile order, using strict `>` comparisons. If the baseline-winning
profile is in this shortlist, the final score and atom are bit identical and
the baseline tie order is preserved, including the subsequent comparison
between notes.

That fixed relative shortlist margin is an empirical guard, not a formal error
bound for every input, sample rate, window size, compiler, floating-point mode,
or platform. The fast kernel can theoretically omit a baseline winner if
rounding moves it outside the margin, or take its early `compressedChoice < 0`
exit when the baseline has a tiny positive score. The provided evidence shows
neither event on the measured inputs. An unconditional claim of bit-equivalence
would require a proved error bound or a different certified selection rule.

The ordinary score audit covers 44,246,412 profile scores / 1,638,756 candidate
visits at the measured open-string configurations. Maximum relative score
error is `5.1157979267081556e-15`; compressed and final profile-choice differences
are both zero. No reference-zero/approximate-nonzero scores were reported.

The forced micro audit separately covers 3,000 ordinary-profile inputs, 3,000
duplicate-profile inputs, and 3,000 profile inputs separated by successive
float increments of the eighth partial. It cycles through all 73 prepared note
models at 12 kHz / 512 samples, uses deterministic seeded coefficient draws,
and includes every-fourth-trial sparse phase directions. Its 243,000 score
comparisons produce 1,144 compressed profile-choice differences; original-order
rescoring corrects all of them, with 141,583 additional original rechecks.
All 9,000 final scores and atoms are bit identical to baseline. This is useful
positive evidence that the fallback executes and fixes actual close-tie
rounding changes. It is a finite test, not a universal close-tie proof.

I independently reran the preserved `micro-audit` and `micro` executables.
Both exited zero and reproduced all deterministic comparison counts, errors,
and checksums above. Timing values varied as expected. No source, detector
threshold, frequency selection, real input, or musical test was changed.
