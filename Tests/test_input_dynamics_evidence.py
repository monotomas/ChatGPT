# SPDX-License-Identifier: AGPL-3.0-or-later
"""Evidence protocol fixtures; these do not execute a plugin or prove its DSP."""
from __future__ import annotations
import copy
import importlib.util
import json
import shutil
from pathlib import Path
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location("dynamics_evidence_tests", ROOT / "Tools/input_dynamics_evidence.py")
evidence = importlib.util.module_from_spec(spec)
spec.loader.exec_module(evidence)


class InputDynamicsEvidenceTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        self.build = self.root / "build/native-release"
        self.dist = self.root / "dist"
        self.build.mkdir(parents=True)
        self.dist.mkdir()
        for name in (*evidence.EXTRA_INPUTS, "Source/PluginProcessor.cpp", "Source/Pitch/InputDynamics.h"):
            path = self.root / name
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_text("fixture input " + name, encoding="utf-8")
        for name in (*evidence.READINESS_METADATA, *evidence.READINESS_PCM):
            source = ROOT / evidence.READINESS / name
            target = self.root / evidence.READINESS / name
            target.parent.mkdir(parents=True, exist_ok=True)
            shutil.copyfile(source, target)
        (self.build / evidence.COMPILED_INPUTS).write_text(json.dumps(evidence.source_hashes(self.root)))
        self.report = {
            "schema": 1, "kind": evidence.TEST, "status": "passed", "checks": 12,
            "physical_latency_measured": False,
            "cases": [{"name": "calibration", "status": "passed"}, {"name": "quiet_tail", "status": "passed"}],
            "fixture_inputs": {"fixture_root": evidence.READINESS, "seal_sha256": evidence.READINESS_SEAL,
                               "files_sha256": dict(evidence.READINESS_PCM),
                               "hash_role": "Expected identities; protocol fixtures do not execute native DSP"},
            "events_file": evidence.EVENTS, "events_rows": 2,
        }
        self.events = b"case,rate,block,sample,type,note\nquiet_tail,48000,64,2047,N,52\nquiet_tail,48000,64,30047,F,52\n"
        for filename, digest in evidence.READINESS_PCM.items():
            note = int(Path(filename).stem.split("_")[2])
            for bend, suffix in ((False, "mono_notes"), (True, "mono_bend")):
                name = "sealed_" + Path(filename).stem + "_" + suffix
                self.report["cases"].append({
                    "name": name, "status": "passed", "fixture_file": filename,
                    "expected_fixture_sha256": digest, "mono_bend": bend, "pitch": note,
                    "reference_on_sample": 24000, "reference_off_sample": 74400,
                    "decision_sample": 25000, "actual_off_sample": 76000, "latency_ms": 1000 / 48,
                })
                self.events += (f"{name},48000,64,25000,N,{note}\n{name},48000,64,76000,F,{note}\n").encode()
        self.report["events_rows"] = len(self.events.splitlines()) - 1
        (self.build / evidence.REPORT).write_text(json.dumps(self.report))
        (self.build / evidence.EVENTS).write_bytes(self.events)
        (self.dist / "test-results.xml").write_text('<testsuite><testcase name="FlysnapperDynamicsProcessor" status="run"/></testsuite>')
        self.test_binary = self.build / "FlysnapperDynamicsProcessorTests_artefacts/Release/FlysnapperDynamicsProcessorTests"
        self.test_binary.parent.mkdir(parents=True)
        binary = bytearray(64)
        binary[:6] = b"\x7fELF\x02\x01"
        binary[18:20] = (62).to_bytes(2, "little")
        self.test_binary.write_bytes(binary)  # Header fixture only; never executed.
        self.build_manifest = {"platform": "Linux-x86_64", "source_commit": "a" * 40, "vst3_sha256": "b" * 64}

    def collect(self):
        self.build_manifest["input_dynamics_evidence"] = evidence.collect(self.root, self.build, self.dist, self.build_manifest)
        return evidence.verify(self.root, self.dist, self.build_manifest)

    def test_complete_capture_is_bound_to_sources_binary_events_and_ctest(self):
        files = self.collect()
        self.assertEqual(set(files), {evidence.REPORT, evidence.EVENTS, evidence.MANIFEST})
        self.assertEqual(files[evidence.EVENTS], self.events)
        proof = json.loads(files[evidence.MANIFEST])
        self.assertEqual(proof["source_sha256"], evidence.source_hashes(self.root))
        self.assertIn("no Ableton", proof["scope"])

    def test_missing_fresh_report_or_raw_events_cannot_collect(self):
        for name in (evidence.REPORT, evidence.EVENTS):
            with self.subTest(name=name):
                path = self.build / name
                data = path.read_bytes()
                path.unlink()
                with self.assertRaises(ValueError):
                    self.collect()
                path.write_bytes(data)

    def test_source_changed_since_build_snapshot_is_rejected(self):
        (self.root / "Source/Pitch/InputDynamics.h").write_text("changed after compile started")
        with self.assertRaisesRegex(ValueError, "changed after compilation"):
            self.collect()

    def test_original_readiness_pcm_and_report_pins_are_verified(self):
        name = next(iter(evidence.READINESS_PCM))
        path = self.root / evidence.READINESS / name
        original = path.read_bytes()
        path.write_bytes(original[:-1] + bytes([original[-1] ^ 1]))
        with self.assertRaisesRegex(ValueError, "readiness PCM"):
            self.collect()
        path.write_bytes(original)
        self.report["fixture_inputs"]["files_sha256"][name] = "0" * 64
        with self.assertRaisesRegex(ValueError, "quiet-note/hum inputs"):
            evidence.check_report(json.dumps(self.report).encode(), self.events)

    def test_each_quiet_fixture_requires_both_modes_and_matching_raw_lifetime(self):
        variants = []
        for key, value in (("mono_bend", True), ("decision_sample", 23000),
                           ("actual_off_sample", 73000), ("latency_ms", 0)):
            report = copy.deepcopy(self.report)
            report["cases"][2][key] = value
            variants.append(report)
        report = copy.deepcopy(self.report)
        report["cases"].pop(2)
        variants.append(report)
        for report in variants:
            with self.subTest(report=report), self.assertRaises(ValueError):
                evidence.check_report(json.dumps(report).encode(), self.events)
        changed = self.events.replace(b"25000,N,31", b"25001,N,31", 1)
        with self.assertRaisesRegex(ValueError, "raw notes disagree"):
            evidence.check_report(json.dumps(self.report).encode(), changed)

    def test_native_header_must_match_execution_platform(self):
        self.test_binary.write_bytes(b"not a native binary")
        with self.assertRaisesRegex(ValueError, "Linux x86_64"):
            self.collect()
        for data, platform in ((b"not PE", "Windows-x64"), (b"" * 64, "unknown")):
            with self.subTest(platform=platform), self.assertRaises(ValueError):
                evidence.native_binary(data, platform)

    def test_unfinished_empty_duplicate_or_physically_mislabelled_reports_are_rejected(self):
        variants = []
        for key, value in (("status", "failed"), ("checks", 0), ("checks", True), ("cases", []),
                           ("events_rows", 0), ("events_rows", 3), ("physical_latency_measured", True),
                           ("events_file", "../old.csv")):
            doc = copy.deepcopy(self.report)
            doc[key] = value
            variants.append(doc)
        doc = copy.deepcopy(self.report)
        doc["cases"][0]["status"] = "skipped"
        variants.append(doc)
        doc = copy.deepcopy(self.report)
        doc["cases"].append(doc["cases"][0])
        variants.append(doc)
        for doc in variants:
            with self.subTest(report=doc), self.assertRaises(ValueError):
                evidence.check_report(json.dumps(doc).encode(), self.events)

    def test_raw_event_time_and_case_integrity_is_checked(self):
        for events in (
                self.events.replace(b"quiet_tail,48000", b"unknown,48000", 1),
                self.events.replace(b",2047,N", b",-1,N"),
                self.events.replace(b",30047,F", b",20,F"),
                self.events.replace(b",2047,N", b",2047,unknown"),
                self.events.replace(b",48000,64", b",nan,64", 1),
                self.events.replace(b",30047,F,52", b",30047,F")):
            with self.subTest(events=events), self.assertRaises(ValueError):
                evidence.check_report(json.dumps(self.report).encode(), events)

    def test_post_capture_changes_cannot_reuse_proof(self):
        self.collect()
        paths = [self.test_binary, self.dist / "test-results.xml", self.root / "Source/PluginProcessor.cpp",
                 self.dist / evidence.DIRECTORY / evidence.REPORT, self.dist / evidence.DIRECTORY / evidence.EVENTS]
        for path in paths:
            with self.subTest(path=path):
                data = path.read_bytes()
                path.write_bytes(data + b"changed")
                with self.assertRaises((ValueError, json.JSONDecodeError)):
                    evidence.verify(self.root, self.dist, self.build_manifest)
                path.write_bytes(data)

    def test_skipped_failed_missing_or_duplicate_ctest_is_rejected(self):
        for test in ("", '<testcase name="FlysnapperDynamicsProcessor"><skipped/></testcase>',
                     '<testcase name="FlysnapperDynamicsProcessor"><failure/></testcase>',
                     '<testcase name="FlysnapperDynamicsProcessor" status="notrun"/>',
                     '<testcase name="FlysnapperDynamicsProcessor"/>' * 2):
            (self.dist / "test-results.xml").write_text("<testsuite>" + test + "</testsuite>")
            with self.subTest(test=test), self.assertRaisesRegex(ValueError, "CTest"):
                self.collect()

    def test_unlisted_or_linked_evidence_is_rejected(self):
        self.collect()
        path = self.dist / evidence.DIRECTORY / "old.txt"
        path.write_text("not the current run")
        with self.assertRaisesRegex(ValueError, "inventory"):
            evidence.verify(self.root, self.dist, self.build_manifest)
        path.unlink()
        original = self.dist / evidence.DIRECTORY / evidence.EVENTS
        original.unlink()
        try:
            original.symlink_to(self.build / evidence.EVENTS)
        except (OSError, NotImplementedError):
            self.skipTest("symlinks unavailable")
        with self.assertRaisesRegex(ValueError, "linked"):
            evidence.verify(self.root, self.dist, self.build_manifest)


if __name__ == "__main__":
    unittest.main()
