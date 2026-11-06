#!/usr/bin/env python3
"""Verify retained research bytes; does not execute any detector or load audio."""
import argparse
import hashlib
import json
from pathlib import Path

ROOT = Path(__file__).resolve().parent
MANIFEST = ROOT / "archive-manifest.json"


def inventory():
    result = {}
    for path in sorted(ROOT.rglob("*")):
        if path.is_symlink():
            raise ValueError(f"Linked archive entry: {path}")
        if not path.is_file() or path == MANIFEST:
            continue
        data = path.read_bytes()
        result[path.relative_to(ROOT).as_posix()] = {
            "sha256": hashlib.sha256(data).hexdigest(), "bytes": len(data)
        }
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--refresh", action="store_true",
                        help="Explicitly replace the aggregate byte inventory")
    args = parser.parse_args()
    actual = inventory()
    if args.refresh:
        payload = {
            "schema": 2,
            "scope": "Retained research byte inventory, not musical acceptance or toolchain attestation; excludes this manifest only",
            "file_count": len(actual),
            "total_bytes": sum(item["bytes"] for item in actual.values()),
            "files": actual,
        }
        MANIFEST.write_text(json.dumps(payload, indent=2) + "\n")
    expected = json.loads(MANIFEST.read_text())
    if expected.get("schema") != 2 or expected.get("files") != actual:
        old = expected.get("files", {})
        missing = sorted(old.keys() - actual.keys())
        unexpected = sorted(actual.keys() - old.keys())
        changed = sorted(name for name in old.keys() & actual.keys()
                         if old[name] != actual[name])
        raise ValueError(f"Archive differs: missing={missing}, unexpected={unexpected}, changed={changed}")
    if expected.get("file_count") != len(actual) or expected.get("total_bytes") != sum(item["bytes"] for item in actual.values()):
        raise ValueError("Archive aggregate counts differ")
    print(f"Verified {len(actual)} retained files; no detector/audio execution or musical approval")


if __name__ == "__main__":
    main()
