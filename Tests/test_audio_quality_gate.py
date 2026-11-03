# SPDX-License-Identifier: AGPL-3.0-or-later
"""Black box regressions for the frozen release gate, without a detector build.

The evaluator and fixture verifier subprocesses are replaced at their process
boundary. All reference annotations and policy values come from the checked-in
81-case corpus. A passing fake evaluator proves gate behavior, not audio quality.
"""
from __future__ import annotations

import contextlib
import copy
import csv
import hashlib
import io
import json
import runpy
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path
from types import SimpleNamespace
from unittest import mock


ROOT = Path(__file__).resolve().parents[1]
GATE = ROOT / "Tools/audio_quality_gate.py"
FIXTURES = ROOT / "Tests/AudioFixtures"
FIELDS = (
    "case", "family", "mode", "rate", "block", "expected", "matched",
    "missed", "false_note_ons", "octave_errors", "wrong_before_correct",
    "early_offs", "hung", "overflow", "latency_median_ms", "latency_p95_ms",
    "latency_p99_ms", "latency_max_ms", "release_median_ms", "release_p95_ms",
    "cpu_median_us", "cpu_p95_us", "cpu_p99_us", "cpu_max_us", "deadline_misses",
)
LATENCY_FIELDS = FIELDS[14:18]
RELEASE_FIELDS = FIELDS[18:20]
CPU_FIELDS = FIELDS[20:24]
COUNT_FIELDS = FIELDS[5:14] + ("deadline_misses",)


class AudioQualityGateTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.reference = json.loads((FIXTURES / "fixtures.json").read_text())["cases"]
        cls.policy = json.loads((FIXTURES / "quality_gate.json").read_text())
        cls.gated_names = {case["name"] for case in cls.policy["cases"]}

    def setUp(self):
        temporary = tempfile.TemporaryDirectory()
        self.addCleanup(temporary.cleanup)
        self.base = Path(temporary.name)
        # The mocked process boundary is portable: no executable bit, shebang,
        # shell, compiler, or platform-specific fake .exe is needed.
        self.evaluator = self.base / "fake evaluator.exe"
        self.evaluator.write_bytes(b"fake evaluator identity for provenance\n")
        self.runs = 0

    def valid_rows(self):
        rows = []
        for case in self.reference:
            count = len(case["truth"])
            row = dict.fromkeys(FIELDS, "0")
            row.update(case=case["name"], family=case["family"], mode=case["mode"],
                       rate="48000", block="64", expected=str(count), matched=str(count))
            for field in LATENCY_FIELDS:
                row[field] = "5.000000" if count else ""
            for field in RELEASE_FIELDS:
                row[field] = "20.000000" if count else ""
            for field in CPU_FIELDS:
                row[field] = "1.000000"
            rows.append(row)
        return rows

    @staticmethod
    def row(rows, name="modal_40_mono"):
        return next(row for row in rows if row["case"] == name)

    @staticmethod
    def csv_text(rows, fields=FIELDS):
        stream = io.StringIO(newline="")
        writer = csv.DictWriter(stream, fieldnames=fields, extrasaction="ignore")
        writer.writeheader()
        writer.writerows(rows)
        return stream.getvalue()

    def fixture_metadata(self, policy):
        fixtures = self.base / "fixture metadata"
        fixtures.mkdir(exist_ok=True)
        for name in ("fixtures.json", "fixtures.tsv"):
            (fixtures / name).write_bytes((FIXTURES / name).read_bytes())
        (fixtures / "quality_gate.json").write_text(json.dumps(policy), encoding="utf-8")
        return fixtures

    def run_gate(self, rows=None, *, raw_csv=None, exit_code=0, verifier_exit=0, omit=(),
                 output=None, fixtures=FIXTURES, optimized=False):
        self.runs += 1
        output = output or self.base / f"report-{self.runs}"
        invocations = []
        stdout, stderr = io.StringIO(), io.StringIO()
        if rows is None:
            rows = self.valid_rows()
        content = self.csv_text(rows) if raw_csv is None else raw_csv

        def fake_run(command, **kwargs):
            invocations.append((command, kwargs))
            if Path(command[0]).resolve() == Path(sys.executable).resolve():
                self.assertEqual(Path(command[1]), ROOT / "Tools/audio_validation_verify.py")
                self.assertEqual(Path(command[2]), fixtures.resolve())
                self.assertTrue(kwargs.get("check"), "Fixture verification must fail closed")
                if verifier_exit:
                    raise subprocess.CalledProcessError(verifier_exit, command)
                return subprocess.CompletedProcess(command, 0)
            self.assertEqual(Path(command[0]), self.evaluator.resolve())
            self.assertEqual(Path(command[1]), fixtures.resolve())
            current = Path(command[2])
            self.assertNotEqual(current, output.resolve())
            self.assertEqual(current.parent, output.resolve())
            self.assertEqual(list(current.iterdir()), [], "Evaluator must receive an empty fresh folder")
            files = {
                "audio-cases.csv": content,
                "audio-events.csv": "case,rate,block,on,note,velocity,sample,time_ms\n",
                "audio-summary.txt": "Fake evaluator measurements; not detector certification.\n",
            }
            for name, text in files.items():
                if name not in omit:
                    (current / name).write_text(text, encoding="utf-8")
            return subprocess.CompletedProcess(command, exit_code)

        argv = [str(GATE), "--evaluator", str(self.evaluator), "--fixtures", str(fixtures),
                "--output", str(output)]
        with mock.patch.object(sys, "argv", argv), mock.patch.object(subprocess, "run", side_effect=fake_run), \
                contextlib.redirect_stdout(stdout), contextlib.redirect_stderr(stderr):
            try:
                if optimized:
                    # Exercise the real CLI under the semantics of python -O.
                    namespace = {"__file__": str(GATE), "__name__": "__main__"}
                    exec(compile(GATE.read_text(), str(GATE), "exec", optimize=1), namespace)
                else:
                    runpy.run_path(str(GATE), run_name="__main__")
            except SystemExit as error:
                code = error.code
            else:
                self.fail("Gate did not return an explicit process status")
        return SimpleNamespace(code=code, output=output, calls=invocations,
                               stdout=stdout.getvalue(), stderr=stderr.getvalue())

    def assert_rejected(self, result):
        self.assertNotEqual(result.code, 0, result.stdout + result.stderr)
        summary = result.output / "summary.json"
        if summary.exists():
            self.assertNotEqual(json.loads(summary.read_text())["status"], "passed")

    def test_all_valid_metrics_pass_with_exact_boundary_values(self):
        rows = self.valid_rows()
        for case in self.policy["cases"]:
            if case["maximum_detector_response_ms"] is not None:
                row = self.row(rows, case["name"])
                for field in LATENCY_FIELDS:
                    row[field] = str(case["maximum_detector_response_ms"])
                for field in RELEASE_FIELDS:
                    row[field] = "100"
        result = self.run_gate(rows)
        self.assertEqual(result.code, 0, result.stderr)
        self.assertEqual(len(result.calls), 2)
        summary = json.loads((result.output / "summary.json").read_text())
        self.assertEqual((summary["status"], summary["cases_checked"], summary["cases_passed"],
                          summary["cases_failed"]), ("passed", 30, 30, 0))
        self.assertEqual({check["case"] for check in summary["checks"]}, self.gated_names)
        self.assertFalse(summary["physical_live_latency_verified"])
        self.assertFalse(summary["user_5_10ms_goals_verified"])
        manifest = json.loads((result.output / "report-manifest.json").read_text())
        self.assertEqual(manifest["evaluator_exit_code"], 0)
        for name, key in (("summary.json", "summary_sha256"), ("metrics.csv", "metrics_sha256")):
            self.assertEqual(hashlib.sha256((result.output / name).read_bytes()).hexdigest(), manifest[key])
        self.assertTrue((result.output / "events.csv").is_file())
        self.assertTrue((result.output / "measurements.txt").is_file())

    def test_each_zero_required_metric_blocks_each_of_the_frozen_30_cases(self):
        for name in sorted(self.gated_names):
            for field in self.policy["required_zero"]:
                with self.subTest(case=name, metric=field):
                    rows = self.valid_rows()
                    self.row(rows, name)[field] = "1"
                    self.assert_rejected(self.run_gate(rows))

    def test_missing_correct_reference_note_is_rejected(self):
        rows = self.valid_rows()
        self.row(rows, "major_triad_poly").update(matched="2", missed="1")
        self.assert_rejected(self.run_gate(rows))

    def test_detector_response_ceiling_is_enforced_for_each_group(self):
        for name, maximum in (("modal_40_mono", 60), ("recorded_E2_mono", 100),
                              ("major_triad_poly", 120)):
            with self.subTest(case=name):
                rows = self.valid_rows()
                self.row(rows, name)["latency_max_ms"] = str(maximum + 0.000001)
                self.assert_rejected(self.run_gate(rows))

    def test_release_p95_ceiling_is_enforced(self):
        rows = self.valid_rows()
        self.row(rows)["release_p95_ms"] = "100.000001"
        self.assert_rejected(self.run_gate(rows))

    def test_required_correct_response_and_release_cannot_be_blank(self):
        for field in ("latency_max_ms", "release_p95_ms"):
            with self.subTest(metric=field):
                rows = self.valid_rows()
                self.row(rows)[field] = ""
                self.assert_rejected(self.run_gate(rows))

    def test_nonfinite_numbers_are_rejected_in_every_float_column(self):
        for name in ("modal_40_mono", "modal_40_poly", "digital_silence_mono"):
            for field in LATENCY_FIELDS + RELEASE_FIELDS + CPU_FIELDS:
                for value in ("nan", "inf", "-inf"):
                    with self.subTest(case=name, metric=field, value=value):
                        rows = self.valid_rows()
                        self.row(rows, name)[field] = value
                        self.assert_rejected(self.run_gate(rows))

    def test_malformed_numeric_counts_and_floats_are_rejected_in_all_rows(self):
        for name in ("modal_40_mono", "modal_40_poly"):
            for field in COUNT_FIELDS + LATENCY_FIELDS + RELEASE_FIELDS + CPU_FIELDS:
                with self.subTest(case=name, metric=field):
                    rows = self.valid_rows()
                    self.row(rows, name)[field] = "not-a-number"
                    self.assert_rejected(self.run_gate(rows))

    def test_negative_counts_and_cpu_measurements_are_rejected(self):
        for field in COUNT_FIELDS + CPU_FIELDS:
            with self.subTest(metric=field):
                rows = self.valid_rows()
                self.row(rows, "modal_40_poly")[field] = "-1"
                self.assert_rejected(self.run_gate(rows))

    def test_impossible_counts_and_percentile_order_are_rejected_in_ungated_rows(self):
        for field, value in (("matched", "2"), ("missed", "1"), ("early_offs", "2"),
                             ("wrong_before_correct", "2"), ("octave_errors", "1"),
                             ("latency_median_ms", "6"), ("release_median_ms", "21"),
                             ("cpu_median_us", "2")):
            with self.subTest(metric=field):
                rows = self.valid_rows()
                self.row(rows, "modal_40_poly")[field] = value
                self.assert_rejected(self.run_gate(rows))

    def test_optional_measurements_follow_the_reference_match_counts(self):
        for name, field, value in (("digital_silence_mono", "latency_max_ms", "0"),
                                  ("digital_silence_mono", "release_p95_ms", "0"),
                                  ("modal_40_poly", "latency_median_ms", ""),
                                  ("modal_40_poly", "release_median_ms", ""),
                                  ("modal_40_poly", "cpu_max_us", "")):
            with self.subTest(case=name, metric=field):
                rows = self.valid_rows()
                self.row(rows, name)[field] = value
                self.assert_rejected(self.run_gate(rows))

    def test_rate_and_block_are_checked_in_all_81_rows(self):
        for name in ("modal_40_mono", "modal_40_poly"):
            for field, value in (("rate", "44100"), ("block", "128")):
                with self.subTest(case=name, metric=field):
                    rows = self.valid_rows()
                    self.row(rows, name)[field] = value
                    self.assert_rejected(self.run_gate(rows))

    def test_reference_annotation_mismatch_is_rejected_for_any_case(self):
        for name in ("modal_40_mono", "modal_40_poly"):
            for field, value in (("expected", "0"), ("family", "invented"), ("mode", "invented")):
                with self.subTest(case=name, annotation=field):
                    rows = self.valid_rows()
                    self.row(rows, name)[field] = value
                    if field == "expected":
                        self.row(rows, name)["matched"] = value
                    self.assert_rejected(self.run_gate(rows))

    def test_missing_extra_duplicate_and_replaced_case_inventory_is_rejected(self):
        baseline = self.valid_rows()
        variants = {
            "missing": baseline[:-1],
            "extra": baseline + [dict(baseline[0], case="invented_extra")],
            "duplicate": baseline[:-1] + [baseline[0]],
            "replaced": baseline[:-1] + [dict(baseline[-1], case="invented_replacement")],
        }
        for name, rows in variants.items():
            with self.subTest(inventory=name):
                self.assert_rejected(self.run_gate(rows))

    def test_csv_schema_must_be_complete_and_unambiguous(self):
        rows = self.valid_rows()
        canonical = self.csv_text(rows)
        lines = canonical.splitlines()
        variants = {
            "missing unused column": self.csv_text(rows, tuple(f for f in FIELDS if f != "cpu_max_us")),
            "duplicate header": canonical.replace("cpu_max_us", "cpu_p99_us", 1),
            "duplicate extra header with valid repeated values": self.csv_text(rows, FIELDS + ("cpu_max_us",)),
            "extra column": self.csv_text(rows, FIELDS + ("unexpected",)),
            "extra row value": "\n".join([lines[0], lines[1] + ",extra", *lines[2:]]) + "\n",
            "truncated row": "\n".join([lines[0], ",".join(lines[1].split(",")[:-1]), *lines[2:]]) + "\n",
        }
        for name, content in variants.items():
            with self.subTest(schema=name):
                self.assert_rejected(self.run_gate(raw_csv=content))

    def test_cached_success_cannot_replace_missing_fresh_measurements(self):
        output = self.base / "reused report"
        self.assertEqual(self.run_gate(output=output).code, 0)
        (output / "audio-cases.csv").write_text(self.csv_text(self.valid_rows()), encoding="utf-8")
        result = self.run_gate(output=output, omit={"audio-cases.csv"})
        self.assert_rejected(result)
        self.assertFalse((output / "metrics.csv").exists())
        self.assertFalse((output / "summary.json").exists())

    def test_nonzero_evaluator_exit_cannot_use_fresh_or_cached_success(self):
        for writes_metrics in (False, True):
            with self.subTest(writes_metrics=writes_metrics):
                output = self.base / f"previous success {writes_metrics}"
                self.assertEqual(self.run_gate(output=output).code, 0)
                result = self.run_gate(output=output, exit_code=7,
                                       omit=set() if writes_metrics else {"audio-cases.csv"})
                self.assert_rejected(result)
                self.assertFalse((output / "metrics.csv").exists())
                self.assertFalse((output / "summary.json").exists())

    def test_missing_evaluator_auxiliary_reports_are_rejected(self):
        for name in ("audio-events.csv", "audio-summary.txt"):
            with self.subTest(file=name):
                self.assert_rejected(self.run_gate(omit={name}))

    def test_fixture_verification_failure_and_missing_evaluator_fail_closed(self):
        result = self.run_gate(verifier_exit=3)
        self.assert_rejected(result)
        self.assertEqual(len(result.calls), 1, "Failed fixture verification must prevent evaluation")
        self.evaluator.unlink()
        result = self.run_gate()
        self.assert_rejected(result)
        self.assertEqual(len(result.calls), 1, "Missing evaluator must not reuse previous measurements")

    def test_quality_criteria_remain_limited_to_the_frozen_30_cases(self):
        rows = self.valid_rows()
        row = self.row(rows, "modal_40_poly")
        self.assertNotIn(row["case"], self.gated_names)
        row.update(matched="0", missed=row["expected"], false_note_ons="2", octave_errors="1",
                   wrong_before_correct="1", hung="1", overflow="1")
        for field in LATENCY_FIELDS + RELEASE_FIELDS:
            row[field] = ""
        result = self.run_gate(rows)
        self.assertEqual(result.code, 0, result.stderr)

    def test_an_ungated_early_release_may_legitimately_have_no_release_quantiles(self):
        rows = self.valid_rows()
        row = self.row(rows, "modal_40_poly")
        row["early_offs"] = row["matched"]
        for field in RELEASE_FIELDS:
            row[field] = ""
        result = self.run_gate(rows)
        self.assertEqual(result.code, 0, result.stderr)

    def test_policy_cannot_reclassify_a_case_to_relax_its_frozen_ceiling(self):
        policy = copy.deepcopy(self.policy)
        case = next(case for case in policy["cases"] if case["name"] == "modal_40_mono")
        case.update(group="recorded-mono", maximum_detector_response_ms=100)
        rows = self.valid_rows()
        self.row(rows)["latency_max_ms"] = "75"
        self.assert_rejected(self.run_gate(rows, fixtures=self.fixture_metadata(policy)))

    def test_policy_inventory_and_exact_rules_cannot_be_relaxed(self):
        variants = {}
        for key, value in (("version", 2), ("sample_rate", 44100), ("block_size", 128),
                           ("release_p95_max_ms", 101), ("required_zero", ["missed"])):
            policy = copy.deepcopy(self.policy)
            policy[key] = value
            variants[key] = policy
        for name in ("missing case", "extra case", "replacement case"):
            policy = copy.deepcopy(self.policy)
            if name == "missing case":
                policy["cases"].pop()
            elif name == "extra case":
                policy["cases"].append(copy.deepcopy(policy["cases"][0]))
            else:
                policy["cases"][-1]["name"] = "invented_case"
            variants[name] = policy
        for name, policy in variants.items():
            with self.subTest(policy=name):
                self.assert_rejected(self.run_gate(fixtures=self.fixture_metadata(policy)))

    def test_optimized_python_still_rejects_invented_reference_counts(self):
        rows = self.valid_rows()
        self.row(rows).update(expected="0", matched="0")
        self.assert_rejected(self.run_gate(rows, optimized=True))


if __name__ == "__main__":
    unittest.main()
