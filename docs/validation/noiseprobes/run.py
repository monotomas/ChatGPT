#!/usr/bin/env python3
# SPDX-License-Identifier: AGPL-3.0-or-later
"""Build and run archived component diagnostics, never the product plugin."""

import argparse
import csv
import hashlib
import json
import math
import os
from pathlib import Path
import struct
import subprocess
import sys
import wave


HERE = Path(__file__).resolve().parent
REPOSITORY = HERE.parents[2]
SCENARIOS = {
    "warmup": (2000, False, False, False, False),
    "cold": (0, False, False, False, False),
    "prime_before_pluck": (0, True, False, False, False),
    "prime_simultaneous": (0, True, True, False, False),
    "cold_simultaneous": (0, False, True, False, False),
    "clean_tones_strict": (0, False, False, True, False),
    "clean_tones_prime_strict": (0, True, False, True, False),
    "clean_tones_rescue": (0, False, False, True, True),
    "clean_tones_prime_rescue": (0, True, False, True, True),
}


def digest(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def run(command, **kwargs):
    return subprocess.run([str(item) for item in command], check=True,
                          cwd=REPOSITORY, text=True, **kwargs)


def make_clean_controls(output):
    """Known desired tones, not recordings or real electrical interference."""
    directory = output / "clean_controls"
    directory.mkdir(exist_ok=True)
    inventory = directory / "fixtures.txt"
    lines = []
    for frequency in (49.0, 50.0, 60.0):
        for harmonics in (False, True):
            name = f"wanted_{frequency:g}_{'harmonics' if harmonics else 'sine'}.wav"
            path = directory / name
            samples = []
            for index in range(57600):
                time = index / 48000
                envelope = ((1 - math.exp(-time / .0009)) * math.exp(-time / 1.2)
                            if index < 48000 else 0)
                value = math.sin(2 * math.pi * frequency * time)
                if harmonics:
                    value += .35 * math.sin(4 * math.pi * frequency * time + .2)
                    value += .12 * math.sin(6 * math.pi * frequency * time + .4)
                samples.append(round(value * envelope * .4 * 32767))
            with wave.open(str(path), "wb") as audio:
                audio.setnchannels(1)
                audio.setsampwidth(2)
                audio.setframerate(48000)
                audio.writeframes(struct.pack("<" + "h" * len(samples), *samples))
            pitch = 69 + 12 * math.log2(frequency / 440)
            lines.append(f"{json.dumps(str(path))} {pitch:.12f} 0 48000")
    inventory.write_text("\n".join(lines) + "\n")
    return inventory


def summarize(path):
    with path.open(newline="") as file:
        rows = list(csv.DictReader(file))
    if not rows:
        raise RuntimeError(f"No cases in {path}")
    for row in rows:
        for field, value in row.items():
            if field != "file" and value and not math.isfinite(float(value)):
                raise RuntimeError(f"Nonfinite result in {path}: {field}")
    latency = [float(row["first_correct_admitted_ms"]) for row in rows
               if float(row["first_correct_admitted_ms"]) >= 0]
    return {
        "cases": len(rows),
        "raw_correct_frames": sum(int(row["raw_correct_frames"]) for row in rows),
        "raw_valid_frames": sum(int(row["raw_valid_frames"]) for row in rows),
        "conditioned_correct_frames": sum(int(row["conditioned_correct_frames"]) for row in rows),
        "conditioned_valid_frames": sum(int(row["conditioned_valid_frames"]) for row in rows),
        "mains_only_frames": sum(int(row["mains_only_frames"]) for row in rows),
        "mains_explained_frames": sum(int(row["mains_explained_frames"]) for row in rows),
        "cases_without_correct_admission": len(rows) - len(latency),
        "first_correct_admitted_ms_range": [min(latency), max(latency)] if latency else None,
        "primed_cases": sum(int(row["prime_sample"]) >= 0 for row in rows),
        "early_admissions": sum(int(row["early_admissions"]) for row in rows),
        "sha256": digest(path),
    }


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, default=Path("/tmp/flysnapper-noise-probes"))
    parser.add_argument("--mode", action="append", choices=SCENARIOS)
    parser.add_argument("--score", choices=("25cents", "rounded", "both"), default="both")
    parser.add_argument("--cxx", default=os.environ.get("CXX", "g++"))
    args = parser.parse_args()
    output = args.output.resolve()
    output.mkdir(parents=True, exist_ok=True)
    binary = output / "component_probe"
    sources = [HERE / "experimental/Pitch/NoiseConditioner.cpp",
               HERE / "baseline/Pitch/YinEstimator.cpp", HERE / "component_probe.cpp"]
    build_command = [args.cxx, "-std=c++17", "-O2", "-Wall", "-Wextra", "-Wpedantic",
                     "-I", HERE / "experimental", "-I", HERE / "baseline", *sources, "-o", binary]
    build = run(build_command, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    (output / "build.log").write_text(build.stdout + build.stderr)
    inventory = HERE / "fixtures.txt"
    fixture_paths = []
    for line in inventory.read_text().splitlines():
        path, pitch, onset, offset = line.split()
        fixture = REPOSITORY / path
        if not fixture.is_file():
            raise FileNotFoundError(f"Missing fixture: {fixture}")
        if not math.isfinite(float(pitch)) or int(onset) < 0 or int(offset) <= int(onset):
            raise ValueError(f"Invalid inventory row: {line}")
        fixture_paths.append(fixture)
    clean_inventory = make_clean_controls(output)
    scenarios = args.mode or list(SCENARIOS)
    scores = ("25cents", "rounded") if args.score == "both" else (args.score,)
    summary = {}
    for scenario in scenarios:
        warmup, prime, trim, clean, rescue = SCENARIOS[scenario]
        for score in scores:
            label = f"{scenario}_{score}"
            destination = output / f"{label}.csv"
            command = [binary, clean_inventory if clean else inventory, warmup,
                       int(prime), int(trim), 25 if score == "25cents" else 0]
            if clean:
                # A pure sine has one partial. Use the separately stated,
                # highly-periodic rescue rather than requiring two harmonics.
                command.extend((0, 0, int(rescue)))
            with destination.open("w") as file:
                result = run(command, stdout=file, stderr=subprocess.PIPE)
            (output / f"{label}.log").write_text(result.stderr)
            summary[label] = summarize(destination)
            stats = summary[label]
            print(f"{label}: {stats['conditioned_correct_frames']}/"
                  f"{stats['conditioned_valid_frames']} correct/valid frames; "
                  f"{stats['cases_without_correct_admission']} cases without correct admission",
                  flush=True)
    bad_inventory = output / "missing_fixture.txt"
    bad_inventory.write_text(f"{json.dumps(str(output / 'intentionally absent.wav'))} 40 0 48000\n")
    missing = subprocess.run([str(binary), str(bad_inventory), "0", "0", "0", "25"],
                             cwd=REPOSITORY, text=True, capture_output=True)
    if missing.returncode == 0 or "Cannot read fixture:" not in missing.stderr:
        raise RuntimeError("Missing-fixture negative check failed")
    truncated = output / "truncated.wav"
    truncated.write_bytes(b"RIFF" + struct.pack("<I", 100) + b"WAVE" + b"x" * 40)
    truncated_inventory = output / "truncated_fixture.txt"
    truncated_inventory.write_text(f"{json.dumps(str(truncated))} 40 0 48000\n")
    invalid = subprocess.run([str(binary), str(truncated_inventory), "0", "0", "0", "25"],
                             cwd=REPOSITORY, text=True, capture_output=True)
    if invalid.returncode == 0 or "Truncated RIFF/WAV" not in invalid.stderr:
        raise RuntimeError("Truncated-WAV negative check failed")
    (output / "error_checks.json").write_text(json.dumps({
        "missing_fixture": {"exit_code": missing.returncode, "stderr": missing.stderr.strip(),
                            "data_rows_emitted": max(0, len(missing.stdout.splitlines()) - 1)},
        "truncated_wave": {"exit_code": invalid.returncode, "stderr": invalid.stderr.strip(),
                           "data_rows_emitted": max(0, len(invalid.stdout.splitlines()) - 1)}
    }, indent=2) + "\n")
    snapshot_paths = sorted([*HERE.glob("experimental/Pitch/*"), *HERE.glob("baseline/Pitch/*"),
                             HERE / "component_probe.cpp", HERE / "run.py", inventory])
    manifest = {
        "scope": "Previously seen recorded-note component diagnostics with synthetic added noise; not product acceptance",
        "baseline_commit": "c5b05337641b13ae259836f5346e7c1850b7382c",
        "product_sources_used": False,
        "priming_variant": "Archived, unvalidated experiment removed from product",
        "added_noise": {"mains_fundamental_hz": 50.03, "mains_amplitude": .03,
                        "relative_harmonics": [1, .4, .2], "phases_radians": [0, .3, .7],
                        "white_uniform_peak": .000025, "random_seed": 1234567,
                        "clean_tone_controls_have_added_noise": False},
        "guitar_gains": [.003, .01, .03],
        "scoring": "25cents is primary; rounded semitones are an insufficient diagnostic",
        "admission_policy": "Archived strict harmonic probe; explicitly named clean_*_rescue modes additionally permit confidence>.985, harmonic fraction>.4 and phase>.95 single-partial rescue",
        "compiler_command": [str(item) for item in build_command],
        "compiler_version": run([args.cxx, "--version"], stdout=subprocess.PIPE).stdout.splitlines()[0],
        "snapshot_sha256": {str(path.relative_to(HERE)): digest(path) for path in snapshot_paths},
        "fixture_sha256": {str(path.relative_to(REPOSITORY)): digest(path) for path in fixture_paths},
        "clean_control_sha256": {str(path.relative_to(output)): digest(path)
                                 for path in sorted((output / "clean_controls").glob("*"))},
        "results": summary,
    }
    (output / "manifest.json").write_text(json.dumps(manifest, indent=2) + "\n")
    print(f"Manifest: {output / 'manifest.json'}")


if __name__ == "__main__":
    try:
        main()
    except (OSError, ValueError, RuntimeError, subprocess.CalledProcessError) as error:
        print(f"noise probe failed: {error}", file=sys.stderr)
        sys.exit(1)
