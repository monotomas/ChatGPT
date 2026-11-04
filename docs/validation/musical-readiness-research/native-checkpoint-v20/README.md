# Native research checkpoint

This is the mainline mono-v20 / poly-checkpoint17 research path, after removal
of unused experimental mains priming. Source hashes stayed unchanged during
the build and execution. It is not a release or Windows/Live proof.

The complete native suite ran: **12 of 13 groups passed**. The strict
`FlysnapperMpeProcessor` group failed because simultaneous audio sources 45 and
64 were not both acquired. Its other independently executed groups passed.
The failure is preserved; the test was not skipped or weakened.

Passing groups include the existing 30-case audio gate, full declared-range
and detuning mechanisms, guitar controls, core correction, processor/GUI,
ordinary compiled-VST3 tests and MPE protocol/compiled-VST3 receiver tests.
The simple compiled-VST3 two-string bend checks settled pitch, not the full
motion trajectory. These successes do not override failures in the broader
independent acoustic controls.

Python regressions: 152 tests completed, one platform-specific skip. The
separate readiness scorer transcript tests and dataset seals were also
checked. They validate measurement machinery, not detector quality.

`manifest.json` records source hashes, the Linux VST3 hash, actual test exit
code and explicit false readiness/Windows/Live flags. `ctest-results.xml` and
`ctest-output.txt` retain all failures. Source is the research implementation
at this checkpoint, not the old c5b release that supplied historical baselines.

The damaged Ninja dependency cache was repaired without changing source or
results. An immediate identical build performed no C++ compilation; cache
validation found no invalid path IDs. This makes later builds reliable and
does not improve acoustic latency.
