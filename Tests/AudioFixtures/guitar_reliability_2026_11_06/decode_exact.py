#!/usr/bin/env python3
"""Decode original PCM24 exactly, writing only to a separate work directory."""
from __future__ import annotations
import argparse
import array
import hashlib
import json
from pathlib import Path
import sys
import wave

ROOT = Path(__file__).resolve().parent


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    output = args.output.resolve()
    if output == ROOT or ROOT in output.parents:
        parser.error("The immutable fixture edition cannot be an output directory")
    manifest = json.loads((ROOT / "source-manifest.json").read_text())
    expected = json.loads((ROOT / "float32-exact-manifest.json").read_text())
    by_source = {record["source_path"]: record for record in expected["files"]}
    for record in manifest["files"]:
        source = ROOT / "original-source" / record["path"]
        raw_file = source.read_bytes()
        if hashlib.sha256(raw_file).hexdigest() != record["sha256"]:
            raise ValueError("Changed original WAV: " + record["path"])
        with wave.open(str(source), "rb") as wav:
            if (wav.getnchannels(), wav.getsampwidth(), wav.getframerate()) != (1, 3, 44100):
                raise ValueError("Unexpected original PCM shape")
            pcm = wav.readframes(wav.getnframes())
        values = array.array("f")
        for index in range(0, len(pcm), 3):
            integer = int.from_bytes(pcm[index:index + 3], "little", signed=True)
            values.append(integer / 8388608.0)
            if int(values[-1] * 8388608.0) != integer:
                raise ValueError("Float32 conversion did not roundtrip original PCM24")
        if sys.byteorder != "little":
            values.byteswap()
        encoded = values.tobytes()
        target_record = by_source[record["path"]]
        if hashlib.sha256(encoded).hexdigest() != target_record["sha256"]:
            raise ValueError("Decoded numeric values differ from the recorded input")
        target = output / target_record["path"]
        target.parent.mkdir(parents=True, exist_ok=True)
        target.write_bytes(encoded)
    output.mkdir(parents=True, exist_ok=True)
    (output / "float32-exact-manifest.json").write_bytes(
        (ROOT / "float32-exact-manifest.json").read_bytes())
    print("Decoded 14 original WAVs losslessly outside the fixture edition")


if __name__ == "__main__":
    main()
