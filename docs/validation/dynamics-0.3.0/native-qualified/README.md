# Qualified native stage: all 15 groups and 86 processor cases pass

This is the fresh Linux native run after the builder/packager metadata was
qualified as `calibrated-input-guitar-evaluation`. Its collected build manifest,
CTest XML/full test output, fresh processor JSON/raw MIDI and source-bound
input-dynamics proof are retained. All 15 native groups pass and all 86 bounded
actual-processor cases pass. The module SHA256 is
`ab8247caab6c3b8da82155ebea0e458b806afa4d3336a5983090d4b5483b9e7f`.

The before-compile map covers all 49 selected project inputs: all 29
`Source/` C++/header files, the two selected changed test C++ files
(`DynamicsProcessorTests.cpp` and `InputDynamicsTests.cpp`), and the selected
build, helper and fixture files. The `Source/` files, selected test files and
CMake input are byte-identical to the
[native4 snapshot](../native-attempt-4/README.md); the VST3 module hash also
matches. This inventory is not all test sources, JUCE/system headers or the
full compiler dependency closure; pinned external dependency and toolchain
evidence is separate. Only three recorded build/packaging metadata inputs changed:
`build_plugin.py`, `Tools/package_windows.py` and
`Tools/input_dynamics_evidence.py`. Their exact qualified bytes are retained
under `qualified-metadata-inputs/`, with the complete new before-compile map.
All other inputs link to the existing native4 source snapshot rather than
creating another 49-input copy. In particular, no additional WAV is copied here.

The processor report/raw/CTest/module hashes are cross-bound by the retained
proof and independently rechecked when this archive is assembled. This is
technical evidence for the requested real-guitar evaluation. Broader acoustic
failures remain reported separately. Windows building, the real packaged
installer, installed-VST3 execution, Ableton and physical audio-interface
latency are not asserted by this Linux checkpoint.
