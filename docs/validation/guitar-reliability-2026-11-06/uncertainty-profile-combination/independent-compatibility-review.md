# Independent compatibility review: uncertainty F0 plus profile compression

Read-only source review on 6 November 2026. No CPU experiment was run and no
source, existing archive, lifecycle rule, or frequency-selection rule was
changed by this review.

The complete baseline file map matches both `input-binding.json` and the actual
archived uncertainty-F0 source at
`docs/validation/guitar-reliability-2026-11-06/fine-frequency-research/preserved/uncertainty-f0/Source`.
The fast map also matches the binding. The only differing file is
`Pitch/TimeDomainPursuit.cpp`, from baseline SHA-256
`4065814a63c9db3fffb1e486c18b5367802bf85b3ed41657ed97857707c44b09` to fast
`88b78c18070ece578532c07dd2be2393136402658d67922ef11a3d75698e92c2`.

The `rebuildModel` and `retuneObserved` method bodies are byte identical between
baseline and fast. Every affected Gram entry and its transpose are assigned
from the same float `crossValue` during a rebuild, including self blocks. Thus
the symmetric self-Gram assumption required by the compressed quadratic form
continues to hold exactly after a retune. Rebuilding self entries in both
orders may overwrite earlier values, but each write updates both transposes
together, so it does not break that symmetry.

`retuneObserved` completes before the analysis projections and candidate loop.
The projected eight-dimensional self-Gram is rebuilt for each candidate from
the current sixteen-dimensional Gram. It is not retained across frames or
frequency changes, so the profile optimization introduces no stale-geometry
cache after dynamic retuning.

The only values cached across frames by this patch are normalization factors
for the immutable 27 profile shapes. The profiles are written in preparation;
neither frequency method mutates them. All 256 valid-harmonic masks are cached
using the original product, accumulation, square-root, and reciprocal
arithmetic. A retune that changes harmonic validity selects the corresponding
mask and does not require rebuilding these profile normalization values.

The fast profile-loop body is byte identical to the independently reviewed
fixed-frequency fast profile-loop body. Its real-arithmetic factorization is
therefore compatible with the current retuned self-Gram, and its shortlist
rescoring retains the original final atom/score arithmetic and profile order.
No additional lifecycle, model, threshold, or frequency semantics are added by
the combination beyond this existing profile-kernel transformation.

The existing finite-precision limitation still applies: the relative `1e-9`
shortlist margin is an empirical guard, not a proved universal rounding bound.
An original winning profile outside that shortlist, or a tiny original positive
score lost through the compressed zero-score early exit, could theoretically
change selection. This static compatibility review does not establish payload
parity for the retuned model; that requires the separately recorded integration
captures. It makes no claim of musical improvement or real-time readiness.
