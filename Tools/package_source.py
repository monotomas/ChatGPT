#!/usr/bin/env python3
"""Create a deterministic, complete Flysnapper 0.3.0 source delivery ZIP.

SPDX-License-Identifier: AGPL-3.0-or-later
JUCE is deliberately acquired by the checked builder using dependencies.lock.json;
this is a source package, not evidence of a Windows or Ableton build.
"""
from __future__ import annotations
import argparse
import hashlib
from pathlib import Path, PurePosixPath
import os
import stat
import tempfile
from zipfile import ZipFile, ZipInfo, ZIP_DEFLATED

VERSION = "0.3.0"
PACKAGE_ROOT = f"Flysnapper-{VERSION}"
FIXED_TIMESTAMP = (2026, 11, 3, 0, 0, 0)
ROOT_FILES = frozenset({
    ".gitignore", ".gitattributes", "README.md", "CMakeLists.txt", "BUILD_FLYSNAPPER.bat", "INSTALL_FLYSNAPPER.bat",
    "build_plugin.py", "dependencies.lock.json", "LICENSE.txt", "THIRD_PARTY.md",
})
ROOT_DIRECTORIES = frozenset({"Source", "Tests", "Tools", "docs", ".github"})
EXCLUDED_NAMES = frozenset({".git", ".venv", "__pycache__", "deps", "build", "dist", "logs"})
REQUIRED_FILES = frozenset({
    ".gitattributes", "README.md", "CMakeLists.txt", "BUILD_FLYSNAPPER.bat", "INSTALL_FLYSNAPPER.bat", "build_plugin.py",
    "dependencies.lock.json", "LICENSE.txt", "THIRD_PARTY.md",
    "Source/PluginProcessor.h", "Source/PluginProcessor.cpp",
    "Source/PluginEditor.h", "Source/PluginEditor.cpp",
    "Source/Pitch/PitchDetector.h", "Source/Pitch/PitchDetector.cpp",
    "Source/Pitch/TimeDomainPursuit.h", "Source/Pitch/TimeDomainPursuit.cpp",
    "Source/Pitch/YinEstimator.h", "Source/Pitch/YinEstimator.cpp",
    "Source/Pitch/NoiseConditioner.h", "Source/Pitch/NoiseConditioner.cpp",
    "Source/Pitch/InputDynamics.h", "Source/Pitch/InputDynamics.cpp",
    "Source/Pitch/ArticulationTracker.h", "Source/Pitch/ArticulationTracker.cpp",
    "Source/Pitch/PolyphonicTracker.h", "Source/Pitch/PolyphonicTracker.cpp",
    "Source/Pitch/MpeOutputRouter.h", "Source/Pitch/MpeOutputRouter.cpp",
    "Tests/PitchTests.cpp", "Tests/YinEstimatorTests.cpp", "Tests/ProcessorAudioTests.cpp",
    "Tests/GuitarQualityTests.cpp", "Tests/BendProcessorTests.cpp",
    "Tests/NoiseConditionerTests.cpp", "Tests/InputDynamicsTests.cpp", "Tests/DynamicsProcessorTests.cpp", "Tests/MpeOutputRouterTests.cpp", "Tests/MpeProcessorTests.cpp", "Tests/MpeVst3HostTests.cpp",
    "Source/Core/MidiEngine.h", "Source/Core/MidiEngine.cpp",
    "Source/Core/MusicData.h", "Source/Core/MusicData.cpp",
    "Source/Core/README.md", "Source/Core/Reference/ChordDictionary.h",
    "Source/Core/Reference/ChordNames.h", "Source/Core/Reference/ChordInput.h",
    "Tests/CoreTests.cpp", "Tests/PluginTests.cpp", "Tests/Vst3HostTests.cpp",
    "Tests/test_builder.py", "Tests/test_packaging.py", "Tools/package_source.py",
    "Tools/audio_validation.cpp", "Tools/audio_quality_gate.py", "Tools/audio_validation_run.py",
    "Tools/audio_validation_verify.py", "Tools/audio_validation_holdout.py", "Tools/audio_validation_confirmation.py",
    "Tools/windows_audio_diagnostics.py", "Tools/package_windows.py",
    "Tools/input_dynamics_evidence.py", "Tests/test_input_dynamics_evidence.py",
    "Tools/readiness_capture.cpp", "Tools/run_readiness.py", "Tests/AudioFixtures/readiness/policy.json",
    "docs/MUSICAL_READINESS.md", "docs/MPE_OUTPUT.md",
    "docs/PROTOTYPE_POLICIES.md", "docs/HOST_TEST_PLAN.md", "docs/BRUKERVEILEDNING.md", "docs/INPUT_DYNAMICS.md",
    ".github/workflows/windows.yml",
})


def _safe_name(relative: str) -> None:
    name = PurePosixPath(relative)
    parts = relative.split("/")
    reserved = {"CON", "PRN", "AUX", "NUL"} | {f"{prefix}{i}" for prefix in ("COM", "LPT") for i in range(1, 10)}
    if (name.is_absolute() or not name.parts or any(p in (".", "..", "") for p in parts)
            or any(p.endswith((" ", ".")) or p.split(".", 1)[0].upper() in reserved for p in parts)
            or any(c in relative for c in '\\\n\r\x00:<>"|?*')):
        raise ValueError(f"Unsafe archive path: {relative!r}")


def collect_files(root: Path, output: Path) -> dict[str, bytes]:
    """Read an explicit source inventory; reject links instead of following them."""
    root = root.resolve(strict=True)
    excluded_output = output.resolve(strict=False)
    members: dict[str, bytes] = {}
    folded_names: set[str] = set()

    def include(path: Path) -> None:
        if path.is_symlink():
            raise ValueError(f"Symbolic links are not packaged: {path}")
        if not path.is_file():
            raise ValueError(f"Not a regular source file: {path}")
        # Also catches a symlink in an ancestor, even if supplied by an unusual walker.
        actual = path.resolve(strict=True)
        if actual == excluded_output:
            return
        if not actual.is_relative_to(root):
            raise ValueError(f"Source path escapes repository: {path}")
        relative = path.relative_to(root).as_posix()
        _safe_name(relative)
        folded = relative.casefold()
        if folded in folded_names:
            raise ValueError(f"Case-colliding archive name: {relative}")
        folded_names.add(folded)
        members[relative] = path.read_bytes()

    for name in sorted(ROOT_FILES):
        path = root / name
        if path.exists() or path.is_symlink():
            include(path)
    for name in sorted(ROOT_DIRECTORIES):
        directory = root / name
        if directory.is_symlink():
            raise ValueError(f"Symbolic links are not packaged: {directory}")
        if not directory.exists():
            continue
        for current, directories, filenames in os.walk(directory, followlinks=False):
            for subdir in directories:
                if (Path(current) / subdir).is_symlink():
                    raise ValueError(f"Symbolic links are not packaged: {Path(current) / subdir}")
            directories[:] = sorted(d for d in directories if d not in EXCLUDED_NAMES)
            for filename in sorted(filenames):
                if filename.endswith((".pyc", ".pyo")) or filename in EXCLUDED_NAMES:
                    continue
                include(Path(current) / filename)
    missing = REQUIRED_FILES - members.keys()
    if missing:
        raise ValueError("Incomplete source package; missing: " + ", ".join(sorted(missing)))
    return dict(sorted(members.items()))


def build_package(root: Path, output: Path) -> Path:
    root = root.resolve(strict=True)
    output = output.absolute()
    if output.is_symlink():
        raise ValueError("Refusing to replace a symbolic-link output")
    members = collect_files(root, output)
    manifest = "".join(f"{hashlib.sha256(data).hexdigest()}  {name}\n"
                       for name, data in members.items()).encode("utf-8")
    members["SHA256SUMS.txt"] = manifest
    output.parent.mkdir(parents=True, exist_ok=True)
    temporary: Path | None = None
    try:
        with tempfile.NamedTemporaryFile(dir=output.parent, prefix=".flysnapper-package-", suffix=".tmp", delete=False) as file:
            temporary = Path(file.name)
        with ZipFile(temporary, "w", compression=ZIP_DEFLATED, compresslevel=9) as archive:
            for name, data in sorted(members.items()):
                info = ZipInfo(f"{PACKAGE_ROOT}/{name}", FIXED_TIMESTAMP)
                info.create_system = 3
                mode = 0o755 if name.endswith((".sh", ".py")) else 0o644
                info.external_attr = (stat.S_IFREG | mode) << 16
                info.compress_type = ZIP_DEFLATED
                archive.writestr(info, data, compress_type=ZIP_DEFLATED, compresslevel=9)
        # Verify exactly what was written, including every content hash, before publishing.
        verify_package(temporary)
        temporary.replace(output)
        temporary = None
    finally:
        if temporary is not None:
            temporary.unlink(missing_ok=True)
    return output


def verify_package(path: Path) -> None:
    with ZipFile(path) as archive:
        names = archive.namelist()
        if len(names) != len(set(names)):
            raise ValueError("Duplicate archive members")
        if len(names) != len({n.casefold() for n in names}):
            raise ValueError("Case-colliding archive members")
        prefix = PACKAGE_ROOT + "/"
        for name in names:
            _safe_name(name)
            if not name.startswith(prefix):
                raise ValueError(f"Unexpected archive root: {name}")
            info = archive.getinfo(name)
            if stat.S_ISLNK(info.external_attr >> 16):
                raise ValueError(f"Symbolic link in archive: {name}")
        manifest_path = prefix + "SHA256SUMS.txt"
        declared: dict[str, str] = {}
        for line in archive.read(manifest_path).decode("utf-8").splitlines():
            digest, relative = line.split("  ", 1)
            _safe_name(relative)
            if relative in declared:
                raise ValueError(f"Duplicate manifest member: {relative}")
            declared[relative] = digest
        actual = {name[len(prefix):] for name in names if name != manifest_path}
        if actual != declared.keys() or not REQUIRED_FILES <= actual:
            raise ValueError("Archive inventory differs from manifest or required sources")
        for name, digest in declared.items():
            if hashlib.sha256(archive.read(prefix + name)).hexdigest() != digest:
                raise ValueError(f"Checksum mismatch: {name}")


def main(argv=None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, help="Delivery ZIP path; default dist/Flysnapper-0.3.0-Source.zip")
    args = parser.parse_args(argv)
    root = Path(__file__).resolve().parent.parent
    output = args.output or root / "dist" / f"Flysnapper-{VERSION}-Source.zip"
    try:
        result = build_package(root, output)
    except (ValueError, OSError) as error:
        parser.exit(1, f"Packaging failed: {error}\n")
    print(f"Source package verified: {result}")
    print(f"SHA256: {hashlib.sha256(result.read_bytes()).hexdigest()}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
