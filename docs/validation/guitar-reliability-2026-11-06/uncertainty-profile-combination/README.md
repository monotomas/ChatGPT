# Private uncertainty-F0 plus profile-kernel combination

This bounded factor combines the frozen uncertainty-F0 research source with
the previously audited harmonic-profile compression. It changes only the
profile computation, gives byte-identical full event payloads on the 330
generated cold configurations, and reduces measured whole-process thread CPU
by 2.47x on the separate 30-configuration timing capture. It remains private,
musically rejected research. It still misses callback deadlines; it is not a
production patch or installer.

The input is the original uncertainty-F0 source from
[fine-frequency-research](../fine-frequency-research/README.md). It excludes
the separately rejected cache-lifecycle semantic ablation. No measured-root
guard, frequency-selection change, new lifecycle rule, musical-threshold
change, or quality grid is part of this factor. No production Source/Tests
were edited by this experiment, and no reserved real inputs were used.

## Exact inputs and transformation

`input-source-inventory.json` is the original 34-file source inventory with
SHA-256 `a51e2d7f4e1babcd568e08b8e2365c5a35f0396f0997c98326988d19e612f784`.
`input-binding.json` binds its complete Source map, the profile generator and
original patch hashes, both new source maps, and unchanged frequency methods.
Both complete uninstrumented sources and both timing sources are retained in
`frozen-source`.

| Object | SHA-256 |
| --- | --- |
| Baseline uncertainty `TimeDomainPursuit.cpp` | `4065814a63c9db3fffb1e486c18b5367802bf85b3ed41657ed97857707c44b09` |
| Combined `TimeDomainPursuit.cpp` | `88b78c18070ece578532c07dd2be2393136402658d67922ef11a3d75698e92c2` |
| Combination `profile-fast.patch` | `d7472b151db75a916d71063dc554bfb213e5aea70a6200312c3b1ac239a2dc19` |

Applying the patch to the baseline reproduces every combined Source file;
`patch-application.txt` and `execution-context.json` retain the proof command.
The only differing file is `Pitch/TimeDomainPursuit.cpp`. The `rebuildModel`
and `retuneObserved` bodies remain byte identical. Each rebuilt Gram entry and
its transpose receive the same rounded float value. Compression constructs
the eight-dimensional projected self-Gram afresh for every candidate after
retuning, using the current rounded sixteen-dimensional Gram. It does not
cache that matrix between candidates or retunes. Its only persistent cache
stores normalization of immutable profile shapes for all 256 valid-harmonic
masks, using original arithmetic.

The combined fast profile-loop body is byte identical to the body reviewed in
[profile-kernel-performance](../profile-kernel-performance/README.md). An
[independent compatibility review](independent-compatibility-review.md)
confirms the lifecycle, symmetry, and normalization assumptions. The algebra
is equivalent in real arithmetic, but the inherited `1e-9` compressed-score
shortlist margin remains empirical floating-point protection. The original
16-dimensional score/atom is recomputed in original profile order for the
shortlist. These finite captures do not prove universal output equivalence
for all inputs, sample rates, compilers, or platforms.

## Full payload factor

`payload-cold.cpp` is the unchanged driver from the profile experiment: 11 chord
sets, five detunes (-40, -20, 0, 20, 40 cents), three harmonic envelopes, and
dynamics off/on. Every one-second case has a freshly prepared detector at
48 kHz with 64-sample blocks. Audio is generated directly in the driver; no
audio or precomputed model payload is stored in this archive.

The baseline and combined captures are byte identical across all 330 cases:
1,508 rows, comprising 754 Note Ons and 754 Note Offs. Both full-event CSVs have
SHA-256 `0c5b65b8ebc5d807c9370b1b8310b7fb35e8955a7d2635fa5b50c712bb3d5e6c`.
The columns compare absolute sample offsets, type/on state, note, velocity,
voice ID, pitch-change flag and float pitch serialized with ten significant
digits. There are no pitch-change events in these polyphonic captures.
`Output.overflow`, internal state and the separate Status view are not
serialized. Each corresponding metrics file confirms all 330 case keys.

The new baseline's note-event projection also exactly reproduces the intended
frequency archive's cold330 event bytes, SHA-256
`e6b267d336f17ad26fb320d42b9aacca6c8eca29f24c561f0f34b0a4da349a95`.
That reference CSV and the comparison binding are retained. This confirms the
baseline identity; it does not convert the model's existing quality failures
into a musical success. Those failures are documented in the frequency archive.

## One timing factor

The separate timing capture uses 30 cold open-string configurations,
22,500 callbacks, 7,241 analyses and 1,648,069 candidate visits in each version.
The note-event CSVs also match byte for byte. `CLOCK_THREAD_CPUTIME_ID` measures
the profile block, whole analyse and whole process scopes; steady-clock wall
deadlines are retained separately. The 64/48,000 second budget is 1.3333 ms.

| Measurement | Uncertainty baseline | Combined |
| --- | ---: | ---: |
| Profile block thread CPU | 12.284346 s | 2.727259 s |
| Whole analyse thread CPU | 16.015012 s | 6.174300 s |
| Whole process thread CPU | 16.533987 s | 6.702252 s |
| Callbacks over thread budget | 7,241 | 392 |
| Callbacks over wall budget | 7,241 | 392 |
| Maximum callback thread CPU | 17.108 ms | 7.616 ms |

Measured ratios are 4.50x, 2.59x and 2.47x for those CPU scopes. The residual
misses and tail remain incompatible with an all-callback real-time claim.
This is one serial capture per version in a shared cloud environment, without
CPU pinning or repeated-run confidence intervals. Instrumentation overhead is
included. The uninstrumented payload driver's legacy `cpu_*` metric names
actually measure steady-clock wall time; use the explicit timing capture for
thread CPU claims. Compiler flags are `-std=c++17 -O2 -DNDEBUG`, with GCC 14.2.0;
no fast-math or target-specific flags are used.

## Integrity and reproduction

All four original captures retain exact compile/runtime commands, compiler,
source/driver/executable hashes, output hashes, exit codes and elapsed time in
their `runs/*/run.json`. Compiled executables remain outside the archive. The
Source maps were verified unchanged before and after compilation. The complete
archive seal includes every retained file except `archive-manifest.json`.

Verify the seal, input identity, source differences and all four run bindings:

```sh
python3 docs/validation/guitar-reliability-2026-11-06/uncertainty-profile-combination/reproduce.py
```

Replay the same bounded factor in a new external directory:

```sh
python3 docs/validation/guitar-reliability-2026-11-06/uncertainty-profile-combination/reproduce.py --output /tmp/uncertainty-profile-replay
```

The replay verifies first, copies frozen inputs, proves patch application,
then runs the archived `run_factor.py` to compile and repeat the four captures.
It stops if payload330 parity fails and compares all replay event streams with
the archive. Binaries and new timings stay outside the archive. The original
factor above was executed once; no second full factor was launched solely to
exercise the archive wrapper. The wrapper's read-only verification and patch
binding were checked locally. Reproduction needs Python, `g++`, `patch`, and
Linux/POSIX thread CPU clocks; exact numeric outputs may vary across compilers,
math libraries and architectures. No shared CMake/Ninja build is used.

Historical preparation/generator files are preserved for provenance; some
contain original absolute `/tmp` paths. Prefer `reproduce.py` for relocation.
The measured-root guard factor belongs to separate follow-up work and is not
included here.
