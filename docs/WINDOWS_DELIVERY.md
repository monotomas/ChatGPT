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
Required targets are Core, Pitch, PluginSmoke, ProcessorAudio, ActualVst3 and AudioQuality. The independent audio summary must pass, its report hashes must match, and its recorded DSP/source/fixture/policy hashes must match the packaged source. Stale passing measurements cannot ship after detector changes.
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
