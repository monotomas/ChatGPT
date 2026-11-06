#!/usr/bin/env python3
"""Verify this immutable edition without invoking a detector or annotation fit."""
from __future__ import annotations
import hashlib
import json
from pathlib import Path, PurePosixPath
import re
import wave

ROOT = Path(__file__).resolve().parent
INVENTORY = "INVENTORY_SHA256.json"
COMMIT = "b3b3249d37dc977a1a297bd2dc053e6d9b6b805c"
REPOSITORY = "sfzinstruments/karoryfer.black-and-green-guitars"
LICENSE_SHA = "a2010f343487d3f7618affe54f789f5487602331c0a8d03f49e9a7c547cf0499"
README_SHA = "c5de1216ecef300f0bf3337c04b40f1b7e11e5d0ee172269a1ac344b7819b595"


def require(condition: bool, message: str) -> None:
    if not condition:
        raise ValueError(message)


def regular(name: str) -> bytes:
    relative = PurePosixPath(name)
    require(not relative.is_absolute() and ".." not in relative.parts
            and "\\" not in name, "Unsafe edition path: " + name)
    path = ROOT / name
    require(path.is_file() and not path.is_symlink(), "Missing/linked file: " + name)
    return path.read_bytes()


def digest(name: str) -> str:
    return hashlib.sha256(regular(name)).hexdigest()


def document(name: str) -> dict:
    return json.loads(regular(name))


def main() -> None:
    inventory = document(INVENTORY)
    require(inventory["schema"] == 1 and inventory["self_excluded"] == INVENTORY,
            "Unexpected integrity-inventory schema")
    actual = set()
    for path in ROOT.rglob("*"):
        require(not path.is_symlink(), "Symlinks are not edition files")
        if path.is_file():
            relative = path.relative_to(ROOT).as_posix()
            if relative != INVENTORY:
                actual.add(relative)
    require(actual == set(inventory["files_sha256"]), "Edition inventory differs")
    for name, expected in inventory["files_sha256"].items():
        require(digest(name) == expected, "Edition hash differs: " + name)
    seal = document("PRE_CAPTURE_SEAL.json")
    require(len(seal) == 5, "Pre-capture seal inventory differs")
    for name, expected in seal.items():
        require(digest(name) == expected, "Original pre-capture bytes differ: " + name)
    source = document("source-manifest.json")
    require(source["repository"] == REPOSITORY and source["source_commit"] == COMMIT
            and source["license"] == "CC0-1.0", "Source identity/rights changed")
    require(digest("original-source/LICENSE") == LICENSE_SHA,
            "Original CC0 legal text differs")
    require(digest("eguitar.txt") == README_SHA, "Original author readme differs")
    readme = document("legal-readme-provenance.json")
    require(readme["sha256"] == README_SHA and readme["path"] == "eguitar.txt"
            and readme["url"] == f"https://raw.githubusercontent.com/{REPOSITORY}/{COMMIT}/readme.txt",
            "Author readme provenance differs")
    for name, expected in source["support_sha256"].items():
        require(digest("original-source/" + name) == expected,
                "Original source support file differs: " + name)
    records = source["files"]
    require(len(records) == 14 and len({record["path"] for record in records}) == 14,
            "Expected fourteen unique source recordings")
    wave_paths = {"original-source/" + record["path"] for record in records}
    require({name for name in actual if name.endswith(".wav")} == wave_paths,
            "Each original WAV must appear exactly once")
    for record in records:
        name = "original-source/" + record["path"]
        require(digest(name) == record["sha256"], "Original PCM differs: " + name)
        require(len(regular(name)) == record["bytes"], "Original WAV length differs")
        require(record["url"] == f"https://raw.githubusercontent.com/{REPOSITORY}/{COMMIT}/{record['path']}",
                "Source URL differs")
        with wave.open(str(ROOT / name), "rb") as wav:
            shape = (wav.getnchannels(), wav.getsampwidth(), wav.getframerate(), wav.getnframes())
        require(shape == (1, 3, 44100, record["wave"]["frames"]),
                "Original PCM shape differs")
        sfz = regular("original-source/" + record["mapping_file"]).decode("utf-8")
        regions = [region.strip() for region in sfz.split("<region>")]
        require(record["mapping_region"] in regions, "Author SFZ region differs")
        require("sample=../" + record["path"] + "\n" in record["mapping_region"]
                and re.search(r"(?:^|\s)pitch_keycenter=" + str(record["midi_nominal"]) + r"(?:\s|$)",
                              record["mapping_region"]) is not None,
                "Author sample/numeric-pitch mapping differs")
    require(sum(record["bytes"] for record in records) == 7988540,
            "Original WAV inventory size differs")
    selected = document("selection-before-capture.json")
    require([(record["path"], record["midi_nominal"]) for record in selected["files"]]
            == [(record["path"], record["midi_nominal"]) for record in records],
            "Original pre-capture selection differs")
    split = document("split-before-any-detector.json")
    require(split["created_before_any_detector_capture"] is True
            and split["selection_sha256"] == digest("selection-before-capture.json"),
            "Original pre-capture split binding differs")
    development = {record["path"] for record in split["development"]}
    reserved = {record["path"] for record in split["reserved_holdout"]}
    require(len(development) == len(reserved) == 7 and not development & reserved
            and development | reserved == {record["path"] for record in records},
            "Fixed development/reserved partition differs")
    require(all({40, 45, 52, 57, 64} <= {record["midi_nominal"] for record in split[name]}
                for name in ("development", "reserved_holdout")),
            "Partition nominal-pitch coverage differs")
    annotation = document("independent-acoustic-annotations.json")
    require(annotation["created_before_any_detector_capture"] is True
            and annotation["source_manifest_sha256"] == digest("source-manifest.json")
            and annotation["split_sha256"] == digest("split-before-any-detector.json"),
            "Original annotation bindings differ")
    notes = {record["path"]: record for record in annotation["notes"]}
    require(len(notes) == 14 and set(notes) == {record["path"] for record in records},
            "Annotation recording inventory differs")
    for record in records:
        note = notes[record["path"]]
        require(note["source_sha256"] == record["sha256"]
                and note["midi_nominal_from_author_sfz"] == record["midi_nominal"]
                and note["physical_damping_reference"] is None
                and note["split"] == ("development" if record["path"] in development else "reserved_holdout"),
                "Acoustic annotation identity/uncertainty differs")
    floats = document("float32-exact-manifest.json")
    require(len(floats["files"]) == 14 and floats["gain"] == 1
            and floats["resampling"] is False and floats["silence_padding"] is False,
            "Recorded numeric conversion metadata differs")
    conversions = {record["source_path"]: record for record in floats["files"]}
    require(set(conversions) == {record["path"] for record in records},
            "Numeric conversion source inventory differs")
    for record in records:
        conversion = conversions[record["path"]]
        require(conversion["source_sha256"] == record["sha256"]
                and conversion["midi_nominal"] == record["midi_nominal"]
                and conversion["frames"] == record["wave"]["frames"]
                and conversion["sample_rate"] == 44100,
                "Numeric conversion source binding differs")
    require(not any(name.endswith((".f32", ".exe", ".so", ".dll", ".onnx", ".pt"))
                    for name in actual), "Generated float/model/binary payload is not admitted")
    print(f"Verified {len(actual)} edition files, 14 original PCM24 WAVs, fixed 7/7 partition, "
          "author SFZ mappings, CC0 terms and all original pre-capture seals; no detector executed")


if __name__ == "__main__":
    main()
