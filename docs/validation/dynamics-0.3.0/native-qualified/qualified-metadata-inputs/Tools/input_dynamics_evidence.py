#!/usr/bin/env python3
"""Bind full-processor dynamics measurements to their compiled source and tests.

SPDX-License-Identifier: AGPL-3.0-or-later
These checks establish fresh native processor execution and provenance. They
do not establish physical interface latency or performance in Ableton.
"""
from __future__ import annotations
import csv
import hashlib
import io
import json
import math
from pathlib import Path, PurePosixPath
import re
import shutil
import struct
import wave
import xml.etree.ElementTree as ET

REPORT = "Flysnapper-dynamics-evaluation.json"
EVENTS = "Flysnapper-dynamics-events.csv"
COMPILED_INPUTS = "flysnapper-compiled-inputs.json"
DIRECTORY = "input-dynamics"
MANIFEST = "report-manifest.json"
TEST = "FlysnapperDynamicsProcessor"
EVALUATION_STAGE = "calibrated-input-guitar-evaluation"
EVALUATION_SCOPE = {
    "revision_date": "2026-11-05",
    "purpose": "real-guitar-evaluation",
    "monophonic_bending": "optional",
    "polyphonic_bending": False,
    "polyphonic_output": "discrete-semitone-notes",
    "latency": "optimized-and-reported-per-pitch",
    "real_guitar_test_pending": True,
    "physical_interface_latency_verified": False,
}
READINESS = "Tests/AudioFixtures/readiness"
READINESS_SEAL = "d5af9fb3fefb4d41918e62810d59290c1dd741d95337d58ec34efcdcf14f9fba"
READINESS_METADATA = ("fixtures.json", "fixtures.tsv", "pitch-curves.tsv", "policy.json",
                      "SEALED_SHA256.txt", "verify.py", "generate.py")
READINESS_PCM = {
    "pcm/quiet_hum_31_gain1.wav": "79dd00d38242e96290405ef27d762b86f785ba7b6aa27db21c1e489d59428e72",
    "pcm/quiet_hum_31_gain8.wav": "fe89c188d7b9e027cb844e2fa550529852790e3217a9f528947f01b10353bc55",
    "pcm/quiet_hum_40_gain1.wav": "8c62198f2bae6a37ac37e99473b73ee79fbb9d89b889e15a42c138342a3e0800",
    "pcm/quiet_hum_40_gain8.wav": "5cfbd9ca0f073c54ffce72512d9963fd2139b1b48623d8346471919ca97227eb",
    "pcm/quiet_hum_52_gain1.wav": "06abeaf4e157f5e6db0320f4cc163219b67fa1d8d2a8eb5c20f8f260d9c116e2",
    "pcm/quiet_hum_52_gain8.wav": "81ad7463ad58838f24ef9db4d48a4f6ed75df9ee0de0290cf9d2eb1333b04320",
}
EXTRA_INPUTS = (
    "CMakeLists.txt", "dependencies.lock.json", "build_plugin.py",
    "Tests/InputDynamicsTests.cpp", "Tests/DynamicsProcessorTests.cpp",
    "Tools/input_dynamics_evidence.py", "Tools/package_windows.py",
)


def require(condition: bool, message: str) -> None:
    if not condition:
        raise ValueError("Input dynamics evidence: " + message)


def sha(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def regular(path: Path) -> bytes:
    require(not path.is_symlink() and path.is_file(), "missing or linked file: " + str(path))
    return path.read_bytes()


def source_hashes(root: Path) -> dict[str, str]:
    readiness_inputs(root)
    paths = {p for p in (root / "Source").rglob("*") if p.suffix in (".cpp", ".h")}
    paths.update(root / name for name in EXTRA_INPUTS)
    paths.update(root / READINESS / name for name in (*READINESS_METADATA, *READINESS_PCM))
    return {p.relative_to(root).as_posix(): sha(regular(p)) for p in sorted(paths)}


def readiness_inputs(root: Path) -> dict:
    """Verify actual used PCM, preserving the original controlled edition."""
    base = root / READINESS
    require(not base.is_symlink(), "linked readiness fixture root")
    require(regular(base / "SEALED_SHA256.txt").decode("ascii").strip() == READINESS_SEAL,
            "original readiness seal changed")
    metadata = b"".join(regular(base / name) for name in ("fixtures.json", "fixtures.tsv", "pitch-curves.tsv", "policy.json"))
    require(sha(metadata) == READINESS_SEAL, "original readiness metadata differs from its sealed edition")
    manifest = json.loads(regular(base / "fixtures.json"))
    records = {record["file"]: record for record in manifest["records"]}
    for name, expected in READINESS_PCM.items():
        data = regular(base / name)
        require(name in records and records[name]["sha256"] == expected and sha(data) == expected,
                "changed original readiness PCM: " + name)
        with wave.open(io.BytesIO(data), "rb") as pcm:
            require((pcm.getnchannels(), pcm.getsampwidth(), pcm.getframerate(), pcm.getnframes())
                    == (1, 2, 48000, records[name]["frames"]), "readiness PCM shape: " + name)
    return {"fixture_root": READINESS, "seal_sha256": READINESS_SEAL,
            "files_sha256": dict(READINESS_PCM)}


def native_binary(data: bytes, platform: str) -> None:
    if platform == "Windows-x64":
        require(len(data) >= 64 and data[:2] == b"MZ", "test is not a native Windows executable")
        offset = struct.unpack_from("<I", data, 60)[0]
        require(offset + 6 <= len(data) and data[offset:offset+6] == b"PE\0\0\x64\x86",
                "test executable is not Windows x64")
    elif platform == "Linux-x86_64":
        require(len(data) >= 64 and data[:6] == b"\x7fELF\x02\x01"
                and struct.unpack_from("<H", data, 18)[0] == 62,
                "test executable is not Linux x86_64")
    else:
        require(False, "unknown native execution platform")


def check_report(report_data: bytes, events_data: bytes) -> dict:
    document = json.loads(report_data.decode("utf-8"))
    require(isinstance(document, dict), "report must be an object")
    require(document.get("schema") == 1 and document.get("kind") == TEST,
            "unexpected report schema or test kind")
    require(document.get("status") == "passed", "processor checks did not pass")
    require(type(document.get("checks")) is int and document["checks"] > 0,
            "no completed processor checks")
    require(document.get("physical_latency_measured") is False,
            "report must distinguish native decisions from physical latency")
    fixtures = document.get("fixture_inputs")
    require(isinstance(fixtures, dict) and fixtures.get("fixture_root") == READINESS
            and fixtures.get("seal_sha256") == READINESS_SEAL
            and fixtures.get("files_sha256") == READINESS_PCM,
            "report does not identify the original quiet-note/hum inputs")
    cases = document.get("cases")
    require(isinstance(cases, list) and len(cases) > 0, "no processor cases")
    names = []
    for case in cases:
        require(isinstance(case, dict) and isinstance(case.get("name"), str)
                and case["name"] and case.get("status") == "passed", "unfinished processor case")
        names.append(case["name"])
    require(len(set(names)) == len(names), "duplicate processor cases")
    used_files = set()
    quiet_cases = {}
    for case in cases:
        if "fixture_file" in case:
            name = case["fixture_file"]
            require(name in READINESS_PCM and case.get("expected_fixture_sha256") == READINESS_PCM[name],
                    "processor case fixture identity differs")
            used_files.add(name)
    require(used_files == set(READINESS_PCM), "missing quiet-note/hum fixture cases")
    for filename in READINESS_PCM:
        selected = [case for case in cases if case.get("fixture_file") == filename]
        require(len(selected) == 2, "quiet fixture must run once in each mono mode")
        note = int(PurePosixPath(filename).stem.split("_")[2])
        for bend, suffix in ((False, "mono_notes"), (True, "mono_bend")):
            name = "sealed_" + PurePosixPath(filename).stem + "_" + suffix
            found = [case for case in selected if case["name"] == name and case.get("mono_bend") is bend]
            require(len(found) == 1, "missing discrete/bend quiet fixture mode")
            case = found[0]
            require(case.get("pitch") == note and case.get("reference_on_sample") == 24000
                    and case.get("reference_off_sample") == 74400, "quiet fixture annotation differs")
            decision, off = case.get("decision_sample"), case.get("actual_off_sample")
            require(type(decision) is int and 24000 <= decision < 74400
                    and type(off) is int and 73920 <= off <= 78240, "quiet fixture note lifetime failed")
            latency = case.get("latency_ms")
            require(type(latency) in (int, float) and math.isfinite(latency)
                    and abs(latency - 1000 * (decision - 24000) / 48000) < 0.000001,
                    "quiet fixture latency differs from actual decision time")
            quiet_cases[name] = (note, decision, off)
    require(document.get("events_file") == EVENTS, "unexpected raw event file")
    require(type(document.get("events_rows")) is int and document["events_rows"] > 0,
            "no raw processor events")
    reader = csv.DictReader(io.StringIO(events_data.decode("utf-8"), newline=""))
    required = {"case", "rate", "block", "sample", "type"}
    require(reader.fieldnames is not None and len(reader.fieldnames) == len(set(reader.fieldnames))
            and required <= set(reader.fieldnames), "raw event CSV schema")
    previous = {}
    quiet_events = {name: [] for name in quiet_cases}
    rows = 0
    for row in reader:
        require(set(row) == set(reader.fieldnames) and all(isinstance(value, str) for value in row.values()),
                "truncated raw event CSV")
        require(row["case"] in names, "raw events reference an unknown case")
        rate, block, sample = (int(row[field]) for field in ("rate", "block", "sample"))
        require(8000 <= rate <= 384000 and 1 <= block <= 8192 and sample >= 0,
                "invalid event timing/configuration")
        require(row["type"] in {"note_on", "note_off", "pitch", "control", "N", "F", "P", "C"},
                "unknown MIDI event type")
        key = row["case"], rate, block
        require(sample >= previous.get(key, -1), "raw event decision times move backwards")
        previous[key] = sample
        if row["case"] in quiet_cases and row["type"] in {"N", "F", "note_on", "note_off"}:
            require(rate == 48000 and block == 64 and "note" in row, "quiet fixture raw event configuration differs")
            event_type = "N" if row["type"] in {"N", "note_on"} else "F"
            quiet_events[row["case"]].append((event_type, int(row["note"]), sample))
        rows += 1
    require(rows == document["events_rows"], "raw event row count differs from report")
    for name, (note, decision, off) in quiet_cases.items():
        require(quiet_events[name] == [("N", note, decision), ("F", note, off)],
                "quiet fixture raw notes disagree with its one-note result")
    return document


def _relative(name: str) -> str:
    require(isinstance(name, str), "unsafe proof path")
    path = PurePosixPath(name)
    require(name and not path.is_absolute()
            and all(part not in ("", ".", "..") for part in name.split("/"))
            and not any(c in name for c in "\\\r\n\0:"), "unsafe proof path")
    return name


def verify(root: Path, dist: Path, build_manifest: dict) -> dict[str, bytes]:
    base = dist / DIRECTORY
    require(base.is_dir() and not base.is_symlink(), "directory missing or linked")
    files = {}
    for path in sorted(base.rglob("*")):
        require(not path.is_symlink(), "linked proof member")
        if path.is_file():
            files[_relative(path.relative_to(base).as_posix())] = path.read_bytes()
    require(set(files) == {REPORT, EVENTS, MANIFEST}, "proof inventory differs")
    report = check_report(files[REPORT], files[EVENTS])
    proof = json.loads(files[MANIFEST].decode("utf-8"))
    require(isinstance(proof, dict), "execution proof must be an object")
    require(proof.get("schema") == 1 and proof.get("kind") == "FullProcessorInputDynamics"
            and proof.get("status") == "passed", "missing native execution proof")
    require(proof.get("platform") == build_manifest.get("platform")
            and proof.get("source_commit") == build_manifest.get("source_commit"),
            "build/source context differs")
    require(proof.get("source_sha256") == source_hashes(root), "source differs from processor execution")
    require(proof.get("fixture_inputs") == readiness_inputs(root), "actual readiness input binding differs")
    require(proof.get("report_sha256") == sha(files[REPORT])
            and proof.get("events_sha256") == sha(files[EVENTS]), "report/raw event checksum differs")
    ctest_data = regular(dist / "test-results.xml")
    require(proof.get("ctest_sha256") == sha(ctest_data), "CTest proof differs")
    cases = [case for case in ET.fromstring(ctest_data).findall(".//testcase") if case.get("name") == TEST]
    require(len(cases) == 1 and all(cases[0].find(tag) is None for tag in ("failure", "error", "skipped"))
            and cases[0].get("status", "run") not in ("notrun", "disabled"),
            "processor CTest did not execute exactly once and pass")
    require(proof.get("vst3_sha256") == build_manifest.get("vst3_sha256"), "compiled VST3 binding differs")
    require(proof.get("checks") == report["checks"] and proof.get("events_rows") == report["events_rows"],
            "execution totals differ")
    binary_name = _relative(proof.get("test_binary", ""))
    expected_binary = "FlysnapperDynamicsProcessorTests.exe" if build_manifest.get("platform") == "Windows-x64" else "FlysnapperDynamicsProcessorTests"
    require(PurePosixPath(binary_name).parts[0] == "build" and PurePosixPath(binary_name).name == expected_binary,
            "unexpected native test executable")
    digest = proof.get("test_binary_sha256")
    require(isinstance(digest, str) and re.fullmatch(r"[0-9a-f]{64}", digest) is not None,
            "native test binary hash missing")
    binary_data = regular(root / binary_name)
    require(sha(binary_data) == digest, "native test executable changed or is unavailable")
    native_binary(binary_data, build_manifest.get("platform"))
    require(build_manifest.get("input_dynamics_evidence") == {
        "path": DIRECTORY + "/" + MANIFEST, "sha256": sha(files[MANIFEST])},
        "build manifest does not bind the current dynamics evidence")
    return files


def collect(root: Path, build: Path, dist: Path, build_manifest: dict) -> dict:
    """Called only after the complete, mandatory CTest suite has passed."""
    expected = json.loads(regular(build / COMPILED_INPUTS).decode("utf-8"))
    require(expected == source_hashes(root), "inputs changed after compilation started")
    report_data, events_data = regular(build / REPORT), regular(build / EVENTS)
    report = check_report(report_data, events_data)
    windows = build_manifest.get("platform") == "Windows-x64"
    executable = build / "FlysnapperDynamicsProcessorTests_artefacts" / "Release" / (
        "FlysnapperDynamicsProcessorTests.exe" if windows else "FlysnapperDynamicsProcessorTests")
    executable_data = regular(executable)
    native_binary(executable_data, build_manifest["platform"])
    proof = {
        "schema": 1, "kind": "FullProcessorInputDynamics", "status": "passed",
        "scope": "Fresh native processor/calibration execution; no Ableton or physical interface latency claim",
        "platform": build_manifest["platform"], "source_commit": build_manifest.get("source_commit"),
        "source_sha256": expected,
        "fixture_inputs": readiness_inputs(root),
        "report_sha256": sha(report_data), "events_sha256": sha(events_data),
        "ctest_sha256": sha(regular(dist / "test-results.xml")),
        "vst3_sha256": build_manifest["vst3_sha256"],
        "test_binary": executable.relative_to(root).as_posix(),
        "test_binary_sha256": sha(executable_data),
        "checks": report["checks"], "events_rows": report["events_rows"],
    }
    destination = dist / DIRECTORY
    require(not destination.is_symlink(), "refusing linked evidence destination")
    if destination.exists():
        shutil.rmtree(destination)
    destination.mkdir(parents=True)
    (destination / REPORT).write_bytes(report_data)
    (destination / EVENTS).write_bytes(events_data)
    proof_data = (json.dumps(proof, indent=2, ensure_ascii=False) + "\n").encode("utf-8")
    (destination / MANIFEST).write_bytes(proof_data)
    return {"path": DIRECTORY + "/" + MANIFEST, "sha256": sha(proof_data)}
