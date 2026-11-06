# Private harmonic-profile kernel performance experiment

This archive preserves a profile-loop optimization of the rejected, private
`time-atoms-held-prediction` model. It is **not adopted in production** and is
not a musical fix. The source model still has the previously reported 330-case
quality failure (298 missed expected onsets, 329 extras). Its fixed-frequency
selection, source model, musical thresholds, and prediction logic are retained.
No installer, binary, audio, precomputed Gram matrix, reserved real input, or
frequency-retune experiment is included here.

The archived full event payloads are byte identical across all 330 generated
configurations. The measured profile-loop thread CPU falls from 13.5295 to
3.0149 seconds, while callback CPU deadline misses fall from 7,241 to 164.
The remaining misses and a 9.221 ms maximum callback mean this is still not a
real-time result. Numerical parity is measured on the frozen corpus; the
floating-point shortlist guard is not a universal equivalence proof.

## Frozen patch and source

`frozen-source/baseline/Source` is byte identical, for every file, to the
preserved origin `/tmp/flysnapper-v4-poly-audit/time-atoms-held-prediction/Source`.
`frozen-origin.json` records that comparison and its complete file map.
`frozen-source/fast/Source` changes only `Pitch/TimeDomainPursuit.cpp`.

| Object | SHA-256 |
| --- | --- |
| Baseline `TimeDomainPursuit.cpp` | `ea6599dcd2e21046de57e7ddc92bb66779f31037508dcf3d57c53e29f0d15ebd` |
| Fast `TimeDomainPursuit.cpp` | `4bf86a11e51ee0d6d6b126fc387483aa5fa8ee867f56eb19a069a1d9d734548f` |
| `profile-fast.patch` | `65d31f735dac9df06fde3f8d2398efdc92aba7c806754273ca4add9c3ca30b9f` |

The patch caches profile normalization for all 256 possible valid-harmonic
masks, with the original product, accumulation, square-root and reciprocal
arithmetic. Each candidate's fixed harmonic phases form a map `D` from eight
amplitudes to the original sixteen sine/cosine coordinates. The numerator is
`a^T D^T correlation` and the denominator is
`a^T D^T (selfGram + ridge*samples*I) D a`. The self-Gram is stored exactly
symmetrically. Computing its projected 8-by-8 lower triangle once per candidate
therefore gives the same real-arithmetic score for all 27 soft profiles.

Floating-point reassociation changes intermediate scores slightly. Compressed
scores select a shortlist; the winning atom and score are rebuilt with the
original 16-dimensional arithmetic. Every profile within
`compressedBest * 1e-9` is rescored in original profile order, using the original
strict `>` comparison. That protects measured close ties and preserves final
score/atom bits when the original winner is shortlisted. The fixed margin has
not been proved sufficient for every input, supported sample rate/window,
compiler, or platform. In particular, an original tiny positive score could
theoretically be omitted by compressed rounding or its zero-score early exit.
Do not describe the patch as universally bit exact. See the independent
[numeric review](independent-numeric-review.md).

## Output and numeric evidence

`payload-cold.cpp` runs a fresh detector for each configuration: 11 fixed chord
sets, five detunes (-40, -20, 0, 20, 40 cents), three harmonic envelopes, and
dynamics off/on. All audio is generated in the driver at 48 kHz, with 64-sample
blocks and one second per configuration. The six open strings begin at
80 + 7*j ms with a 2 ms attack. Driver source fixes every input sample formula.
These are synthetic plucks and do not establish performance on human playing.

The 330-case full-payload capture has 1,686 event rows: 843 Note Ons and 843
Note Offs. Both versions' CSV SHA-256 is
`ab1073a8c4dc0ceca83f880254a37d498bd07f785c586ce88c215557731c599f`.
It compares every event's absolute sample offset, type, note, velocity, voice
ID, pitch-change flag, and float pitch serialized with ten significant digits.
There are no pitch-change events in these polyphonic runs. It does not compare
internal detector state, `Output.overflow`, or the separate `Status` view. The earlier note-only
330-case capture and the separate 30-case timing capture also match byte for
byte; their raw CSVs remain in `original-runs`.

The ordinary audit independently evaluates all original and compressed scores
for 1,638,756 candidate visits / 44,246,412 profile scores over the 30 open-string
configurations. Its maximum relative score difference is 5.116e-15, with zero
compressed/final argmax differences and no extra close-tie rechecks.

`micro.cpp` extracts the actual old/new profile-loop bodies and constructs
deterministic phase/correlation inputs through all 73 note models at 12 kHz /
512 samples. It checks all 6,912 cached normalization values bit for bit. Its
three families contain 3,000 ordinary, 3,000 duplicate, and 3,000 adjacent-float
profile cases. The 243,000 audited scores produce **1,144 compressed argmax
differences** and 141,583 extra original rechecks; the final 9,000 scores and
16-dimensional atoms have **zero bit differences**. This demonstrates that the
fallback executes and fixes actual rounding ties on those finite tests.

## Original-worker timing

The separately instrumented source snapshots use `CLOCK_THREAD_CPUTIME_ID`
around `analyse()` and the profile block. The driver records both thread CPU
and steady-clock wall time around `process()`. They cover 30 fresh open-string
configurations, 22,500 callbacks, 7,241 analysis calls, and 1,638,756 candidate
visits in each version. The 64/48,000 second callback budget is 1.3333 ms.

| Measurement | Baseline | Fast |
| --- | ---: | ---: |
| Profile block thread CPU | 13.529498 s | 3.014909 s |
| Whole `analyse` thread CPU | 17.237292 s | 5.928986 s |
| Whole `process` thread CPU | 17.871375 s | 6.436285 s |
| Callbacks over thread CPU budget | 7,241 | 164 |
| Callbacks over wall budget | 7,241 | 165 |
| Maximum callback thread CPU | 7.563 ms | 9.221 ms |

These totals imply 4.49x, 2.91x, and 2.78x reductions in measured CPU cost for
the three scopes respectively. Profile timings include per-candidate clock
instrumentation overhead. These are single captured runs in a shared cloud
environment, without CPU pinning or a repeated-run confidence interval. The
worst callback did not improve. The original build metadata identifies GCC
14.2.0 with `-std=c++17 -O2 -DNDEBUG`; no `-ffast-math` or target-specific flags
were used. `resume-environment.json` describes the environment observed after
restart, rather than asserting the identity of the original timing host.

The legacy `probe-cold.cpp` and `payload-cold.cpp` metrics named `cpu_*` actually
use steady-clock wall time; use the explicit `thread_*` instrumentation above
for CPU claims. Those legacy wall metrics are retained without renaming.

## Verification and replay

Verify the complete file seal and all seven original run bindings:

```sh
python3 docs/validation/guitar-reliability-2026-11-06/profile-kernel-performance/reproduce.py --suite verify
```

`verify_negative_checks.py --output /tmp/profile-kernel-negative-checks`
tests external copies. It rejects an unexpected nested manifest, a changed
baseline source with a recomputed outer seal, and a changed event capture with
a recomputed outer seal. It preserves the original archive and removes the
mutated copies after saving the rejection evidence.

Replay using only archived sources and drivers, with a new external directory:

```sh
python3 docs/validation/guitar-reliability-2026-11-06/profile-kernel-performance/reproduce.py --suite all --output /tmp/profile-kernel-replay
```

Suites `micro`, `payload`, `timing`, and `audit` can also run separately. The
replay checks patch application against the exact fast-source snapshot, builds
serially with `g++`, binds source/driver/binary hashes and exact commands, checks
full payloads against the original captures, and checks deterministic numeric
audit counts/errors. Binary outputs stay in the external directory. Timing is
re-recorded and is expected to vary. No shared CMake/Ninja tree is used.
Replay requires `g++`, `patch`, and Linux/POSIX thread CPU clocks. Relocated
archive paths are supported; numerical output equality across different
compilers, math libraries, or architectures is not guaranteed. The audit also
checks exact captured maximum errors, which is stricter than event parity.

The `make_fast.py`, `make_micro.py`, `instrument.py`, and `build_and_measure.py`
files are retained original-worker provenance. Some refer to original `/tmp`
paths; `reproduce.py` is the portable entry point. Original per-run `build.json`
and `run.json` contain exact compile commands, compiler version, full source maps,
driver/binary hashes, exit codes, output hashes, and captured elapsed time.
The original worker did not save runtime argument arrays: the 330-case
captures use no argument; the open-string timing/audit captures select chord
index `10`, as reconstructed from their complete 30-row case key sets. New
replay records include both compile and runtime command arrays.
Original micro compile commands were not recorded by the lost worker; the
replay binds newly compiled micro executables explicitly. Original executables
are excluded. `archive-manifest.json` hashes every retained file except itself.

## Completed replay after environment restart

The complete archived-source replay passed. `replay/verification.json` retains
its report; `replay/summary.json` includes new exact compile/runtime arrays,
source/driver/binary hashes, timing aggregates, and the scope of final verifier
strengthening. Both payload executables and both timing executables reproduced
the original worker's executable SHA-256 exactly. The audit executable's
embedded output-path macro changed for the external replay directory. Its
44,246,412 score comparisons, argmax counters, and maximum errors reproduced
exactly. Both micro modes reproduced all deterministic numeric evidence.

| Fresh replay measurement | Baseline | Fast |
| --- | ---: | ---: |
| Profile block thread CPU | 12.252807 s | 2.987944 s |
| Whole `analyse` thread CPU | 15.230662 s | 5.861738 s |
| Whole `process` thread CPU | 15.720181 s | 6.378139 s |
| Callbacks over thread CPU budget | 7,241 | 104 |
| Callbacks over wall budget | 7,241 | 105 |
| Maximum callback thread CPU | 122.722 ms | 9.975 ms |

This second run gives measured ratios of 4.10x, 2.60x, and 2.46x for the three
CPU scopes. Its large baseline tail reinforces the limits of these shared-host
captures. Both versions visit the same 7,241 analyses and 1,638,756 candidates.
The 330 full event payloads and 30 timing event streams match the originals
byte for byte. `negative-checks/negative-checks.json` and the captured rejection
logs show all three verifier mutation checks passing. The final live
`--suite verify` validates the completed archive, including these replay files.
