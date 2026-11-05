# Native4: all15 groups passed,86 bounded processor cases

The complete native suite passes after the integrated post-conditioning
input dynamics/reference fix and physical held-octave correction. The actual
processor's86 bounded cases include all63rate/block/pitch measurements,
calibration success/resume/correction/cancellation/failure/reset behavior,
12sealed quiet-mains controls in plain and bend modes, and4 genuine octave
changes. Existing mono-bend, protocol, GUI and compiledVST3 groups pass too.

This is technical acceptance for preparing the requested real-guitar evaluation,
not universal musical-readiness, actual Windows/Ableton execution or measured
physical audio-interface latency. Broader seen acoustic diagnostics still have
failures and remain reported separately. No acoustic policy was relaxed.

All 49 selected project inputs were copied after execution while frozen;
every copied byte matches the actual before-compile fingerprint retained here.
The inventory includes all 29 `Source/` C++/header files, the two selected
changed test C++ files (`DynamicsProcessorTests.cpp` and `InputDynamicsTests.cpp`),
and the selected build, helper and fixture files listed in that map. This is
not all test sources, JUCE/system headers or the full compiler dependency
closure; pinned external dependency and toolchain evidence is separate. The
later build-stage metadata qualification is separate from this exact runtime
checkpoint. JSON/rawCSV/CTestXML/full logs, screenshots, binary hashes and the
collected input-dynamics proof are retained; runtime binaries are excluded.
The 49-input snapshot intentionally includes the six canonical quiet-mains
WAVs named by its before-compile fingerprint. Every copied byte matches the
original sealed fixture edition. These selected copies preserve exact input
binding, rather than supplying new recordings or additional independent data.
`native-summary.json` reports actual latency and release samples from this
fresh passed86-case report, not a previous failed attempt.
