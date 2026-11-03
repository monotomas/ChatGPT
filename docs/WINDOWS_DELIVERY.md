# Windows shipping and unattended delivery

The end-user artifact is `Flysnapper-Windows-x64.zip`. It contains a prebuilt
Windows x64 VST3 bundle, `INSTALL_FLYSNAPPER.bat`, a Windows PowerShell 5.1
installer, corresponding source ZIP, license notices, a full inventory/hash
manifest, and the actual Windows test report. End users need no Python,
compiler, CMake or Git.

The installer verifies the complete extracted inventory and Windows PE machine
before writing to the selected VST3 parent directory. It replaces only
`Flysnapper.vst3`, using a verified staging copy, a verified recovery copy outside
the scan directory, and transactional rename/rollback. Reparse points are
rejected before traversal. The extracted package and selected scan directory must be separate in both directions to prevent duplicate scanned bundles. Other plugins are not changed. UAC is requested only
if the selected destination requires elevated access. Closing Ableton before
installation avoids DLL locks.

`Tools/package_windows.py` refuses Linux binaries, x86 PE files, the old routing
prototype, absent pitch implementation, mismatching bundle hashes, missing
Windows toolchain/Unicode probes, or missing/failed/skipped mandatory CTests.
Version 0.2.1 requires nine groups: Core, Yin, Pitch, PluginSmoke, ProcessorAudio,
ActualVst3, AudioQuality, GuitarQuality and BendProcessor. The last two cover
the guitar lifecycle/quality regressions and processor-level bend behavior.
The seven original groups and unchanged 30-case basic audio gate remain
mandatory. A previous seven-group proof cannot qualify the update.
The independent audio summary must pass, its report hashes must match, and its
recorded DSP/source/fixture/policy hashes must match the packaged source.
Stale passing measurements cannot ship after detector changes.

After those tests, `Tools/windows_audio_diagnostics.py` uses the same native
evaluator whose SHA256 is bound to the passed basic test. It runs the complete
public729, first-holdout621 and confirmation414 matrices. The latter two are
explicitly seen-data regressions. Execution, fixture integrity, unchanged source
and evaluator, complete case/rate/block inventory, finite/count-consistent
metrics and matching event counts are required. Musical errors remain in the
reports; completion does not add an accuracy or latency acceptance threshold.

Packaging requires all three completed diagnostic reports and includes them
under `Validation/windows-audio-diagnostics/{public,holdout,confirmation}/`, with
CSV metrics/events, textual measurements, runner logs and provenance manifests.
The aggregate manifest says `measurement_only: true` and `status: completed`.
The unchanged 30-case basic quality gate remains a separate mandatory pass.

Source-provenance keys use forward slashes on every platform. A native Windows
run exposed that serializing `Path` with `str()` produced backslash keys that
the packager could not match. Producers now use `as_posix()`. The packager also
accepts safe older backslash representations, rejecting traversal and duplicate
aliases; it never changes the recorded evidence or source digests.
Passing this gate does not establish physical audio latency or Ableton behavior.

## Build and delivery protocol

Push the complete source to `release/full-flysnapper`. The Windows Actions job
runs builder/packager/installer regressions, builds and tests in a Unicode path,
then packages the tested Windows bundle. The workflow is also manually runnable.
It needs repository `contents: write` for delivery commits, not a separate token.

The job appends commits to `downloads/flysnapper-complete` through ordinary Git.
It never force-pushes. `status.json` states:

- `source_commit`: the precise source revision this job was asked to build;
- `status`: `running`, `completed` or `failed`;
- `run_id`/`run_attempt` and UTC update time;
- `artifact`: only on completion, with path, SHA256 and size;
- `log`: bounded/redacted CI diagnostic text when available.

A failed job preserves the previous successful artifact in Git history/current
tree, but the newest status is failed and carries no artifact claim. Consumers
must require both matching source commit and `completed`. A packaging/provenance
verification error itself publishes a failed state and makes the job fail.

On completion the branch also contains native Windows build/test evidence.
`Tools/publish_windows_delivery.py` requires packaged and external proof to agree
and the Windows build's source SHA to match the job checkout. Download through an
immutable delivery commit, then verify the ZIP hash. GitHub API access is not
needed by a consumer that can fetch this branch.

## Verification scope

`Tests/test_windows_package.py` tests shipping gates, safe paths, deterministic
archives, publication history and redaction. With PowerShell available it
executes real installer functions against deliberately fake PE fixture bytes:
Unicode paths, first installation, replacement, recovery preservation, unrelated
plugin preservation, injected rename-failure rollback and tamper rejection.
These fixtures validate installer logic, not DLL loading. The installer hashes files with builtin .NET SHA256 rather than depending on PowerShell module discovery. Windows CI uses native
Windows PowerShell 5.1; Linux may additionally use verified portable PowerShell 7.
The compiled VST3 tests are a separate mandatory stage.

## MSVC object-path safety

An actual Windows CI run reached the new ProcessorAudioTests target and failed
with C1083 on its longest JUCE compilation unit. Its full `/Fo` object filename
was 264 characters, crossing the legacy Windows 260-character boundary. Shorter
plugin/test targets had already compiled successfully.

Generated project caches now use `build/w-<identity>`, and the independently
verified Unicode-patched JUCE copy uses `build/j/u-<content identity>`. The exact
reported object path becomes 218 characters. CMake additionally receives
`CMAKE_OBJECT_PATH_MAX=220` to hash longer object directories. The original
checkout path remains Unicode; the real wide-argument/resource preflight remains
mandatory. No source or test format is omitted to avoid the failure.

`Tests/test_builder_paths.py` preserves the exact failing Windows path as a
regression and exercises real CMake/Ninja generation for a long Unicode fixture,
confirming hashed object directories under the configured budget. That generator
probe uses a forced compiler descriptor and is not an MSVC execution. The full
Windows build remains the final native proof. Developer checkout paths too deep
even for a hashed object tail fail before expensive compilation with a concrete
message; end users install the prebuilt Windows package.

## Actual packaged-module installation gate

After packaging, Windows CI runs `Tools/verify_windows_install.py`. It verifies
and extracts the **actual ZIP**, invokes that package's PowerShell installer in
library mode inside a private Unicode temporary directory, then performs a second
installation to exercise replacement and the recovery copy. Both installed and
recovery bundle inventories must exactly match the package/build hashes. A
separate control plugin remains untouched. No system plugin directory, UAC prompt
or interactive window is involved.

The helper then launches the built native `FlysnapperVst3HostTests.exe` with the
**installed bundle path**, repeating the full compiled-module host suite against
the installed files. It requires successful process exit, the native Windows
completion marker and a positive assertion count. The ZIP and installed bundle
must remain unchanged. The helper refuses ambiguous/stale host build paths.

For 0.2.1, the helper also replaces the package through its actual
`INSTALL_FLYSNAPPER.bat`, using the builtin Windows command processor, quoted
Unicode paths with spaces/parentheses/ampersands, `-NoUI`, and closed standard
input so the launcher's `pause` cannot block CI. The existing PowerShell entry
point exposes its existing recovery-directory option for private test isolation;
normal interactive defaults and the BAT itself are unchanged. The BAT must exit
successfully, print the actual installed-version marker, create a new verified
private recovery copy, and leave the installed/unrelated payload correct. The
native host loads that final BAT-installed DLL. Its command, BAT hash, result
marker and hashed launcher log are included in installation proof. Legacy 0.2.0
proof remains valid without claiming this additional launcher execution.

`artifacts/installation-validation.json` records the source revision, ZIP hash,
installed/recovery hashes, installer/host exit codes, host executable hash and
assertion count. Its PowerShell and installed-host logs are hashed too. This proof
is produced after packaging and kept outside the ZIP to avoid circular repacking.

Completed publication additionally requires the installation step to succeed and
its proof to match the exact source, ZIP and Windows bundle. Git delivery includes
`validation/windows-installation.json` and the matching logs alongside the ZIP.
If installation validation fails, the failed proof/logs are published for diagnosis
and no completed artifact claim is emitted. Fixture regression tests of this
protocol are separate from the actual native Windows installation gate.
