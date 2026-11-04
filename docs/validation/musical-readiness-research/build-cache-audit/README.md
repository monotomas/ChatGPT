# Repeated Ninja rebuild diagnosis

Shared build files were inspected read-only. All Ninja experiments ran only in `/tmp/flysnapper-ninja-audit` copies. No Ninja/CMake command was run against the real build.

## Concrete cause

The copied `.ninja_deps` is820680 bytes and contains2300 path records /350 dependency records. Record sizes reach EOF correctly, but36 path-ID checksums are wrong. The first bad path record is at byte760688: its ordinal requires ID2264, but its stored checksum encodes ID2261. ID2261 was already used by `FlysnapperBendProcessorTests.dir/Tests/BendProcessorTests.cpp.o`; the bad record names `FlysnapperGuitarQualityTests.dir/Tests/GuitarQualityTests.cpp.o`. A duplicate GuitarQuality path later appears with another ID. This is a stale/interleaved dependency-ID history, not a valid dependency cache.

Ninja labels this checksum corruption "premature end of file; recovering" even though record lengths reach EOF. Consequently, checking lengths alone misses the actual failure. Recovery loses dependency records after the valid prefix, which causes targets without valid recorded dependencies to rebuild.

This pattern is consistent with earlier overlapping writers or recovery interactions. It does **not** establish that the root's current serial builds overlap. No Ninja/CMake writer was visible in this agent's process scans; cross-tool PID namespace visibility is a limitation. The original historical trigger remains unproven.

The two Ninja executable paths are byte-identical, SHA256 `08639e194fffa7f08b259fc4abfa4803aff66b64de52549cee42ec527d55cea6`, so merely pinning the other path cannot repair this persisted cache corruption.

The copied `.ninja_log` contains209 valid five-field records and no malformed final line. It does not independently reproduce the warning.

## Isolated repair verification

- Deps-only copy plus minimal manifest: `ninja -t recompact` reproduced the exact warning.
- Log-only copy: no warning.
- Full copied build manifest, copied rules, and both logs: first recompact warned and repaired; second recompact did not warn.
- Full-manifest recompact retained2264 paths /104 live dependency records in404400 bytes, with every path-ID checksum valid. This retains the valid prefix and latest live entries; it does not require discarding the good `.ninja_log`.

Exact snapshots and JSON parse results are adjacent. `check_deps.py` is a reusable **read-only** checker that verifies lengths, path checksums, and referenced IDs.

## Safe instructions for root after the active build finishes

Wait until all commands accessing this build are idle. Back up `.ninja_deps` and `.ninja_log` into `/tmp`. Then run the pinned Ninja's `-t recompact` once against `build/native-release`. Use `check_deps.py` on the resulting real `.ninja_deps` to confirm all checksums are valid. Run one normal repair build to recreate the discarded invalid tail dependencies. A second identical build should then avoid recompiling unchanged JUCE files.

Do not delete `.ninja_log` by default: it is valid, contains useful command hashes, and was not the warning's source. Deleting both logs would unnecessarily discard more incremental build state.

Serialize all commands touching this shared build, including CMake configure and Ninja `-t` / `-n` diagnostics, using a common advisory lock outside the build directory. Loading a corrupt dependency log can perform recovery/truncation, so ostensibly diagnostic Ninja invocations are not guaranteed to leave that file unchanged. If the original trigger recurs after clean recompact and strictly serialized access, retain the new corrupt bytes and investigate that distinct reproduction before blaming inherited corruption again.
