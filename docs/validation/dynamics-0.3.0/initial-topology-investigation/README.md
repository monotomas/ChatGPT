# Initial topology investigation: rejected integration behavior

These are seen-regression captures of the initial external dynamics -> existing
internal NoiseConditioner -> detector topology. They are not measurements of a
later accepted arrangement or proof of general musical readiness. No source,
policy or numerical thresholds changed during each retained capture. Every
scoring failure is preserved. All data is48kHz/64samples only.

| Input path | Controlled scoped pass | Mono controlled pass | Recorded-noise pass |
| --- | ---: | ---: | ---: |
| Frozen previous source, dynamics off, fixed poly |35/65|29/34|45/128|
| Initial candidate, default dynamics |35/65|25/34|47/128|
| Same candidate, gate-only ablation |35/65|25/34|Not run|
| Same candidate, compression-only / effectively open gate |35/65|29/34|Not run|

Seven continuous-poly-bend cases remain raw captured data and are explicitly
outside the user's new scope, never counted as passes. All remaining original
acquisition, slide/hammer, noise and release criteria remain unchanged. Absolute
latency is descriptive rather than a gate, as explicitly requested November5.

Default dynamics reduces some poly noise-only admissions, but introduces four
mono case failures: G1+hum late release, E2/E3+hum repeated false articulations,
and one music/handling steady-pitch threshold crossing. Gate-only reproduces
all four; compression-only restores their baseline passes. In the retained
quiet-hum traces, the gate mutes all375 pre-note chunks, so downstream stationary
conditioning cannot observe the actual idle hum before the gate opens. This
supports the gating/conditioning-history interaction, not a blanket claim that
compression is harmful or that a new topology is already approved.

Quiet-hum paired levels are gain1/gain8 (not gain4). Recorded percussion editions
include high normalized peaks; a default-60dB opening threshold cannot reject
arbitrarily louder interference. Every extra/missed note and premature release
remains reported in `comparison.json` and complete per-case scores. None of
these broader diagnostic failures has been relabelled a pass.

## Exact source and driver editions

`snapshots/` contains both frozen DSP inputs. `runs/` retains original raw events,
metrics, score failures, compiler commands and manifests. PCM stays in its sealed
repository location and is not duplicated. Executables are excluded.

Three capture-driver editions were used during development. Their exact earlier
text was reconstructed after capture by reversing only known local edits; each
retained driver byte-for-byte matches its recorded SHA256 before it is accepted
here. The baseline-off driver predates sample-by-sample gate permissions, but its
frontend is entirely disabled. Both initial-candidate and ablation drivers
retain each actual sample's admission transitions. No onset is backdated.

`orchestration/common/` includes the scorer, original unchanged matcher/verifier,
and a reproduction runner. That retained runner exposes all ablation CLI choices;
the original default-only runner differed only in its accepted choice list.
The baseline policy before a textual acceptance-role clarification is preserved
separately with its exact recorded hash. Numerical scoring rules did not change.
The candidate and ablations use the later explicit role text.

To repeat, use a scratch copy of the project, overlay the selected frozen Pitch
source and exact driver edition, use the matching retained policy edition, then
run the retained runner with a fresh output directory and the original sealed
fixtures. Do not edit these measured outputs or the original labels. Paths in
historical manifests describe their original execution locations.
