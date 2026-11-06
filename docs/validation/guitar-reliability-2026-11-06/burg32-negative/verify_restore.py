#!/usr/bin/env python3
"""Verify the archive inventory and optionally restore the lossless study tree."""
import argparse
import gzip
import hashlib
import json
from pathlib import Path

HERE = Path(__file__).resolve().parent


def digest(data):
    return hashlib.sha256(data).hexdigest()


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--output', type=Path)
    args = parser.parse_args()
    inventory = json.loads((HERE / 'archive-manifest.json').read_text())
    actual = {p.relative_to(HERE).as_posix() for p in HERE.rglob('*')
              if p.is_file() and p.name != 'archive-manifest.json'}
    declared = {e['path'] for e in inventory['files']}
    if actual != declared:
        raise SystemExit(f'Physical inventory differs: {actual ^ declared}')
    for item in inventory['files']:
        data = (HERE / item['path']).read_bytes()
        assert len(data) == item['bytes'] and digest(data) == item['sha256'], item['path']
    mapping = json.loads((HERE / 'storage-map.json').read_text())
    output = args.output.resolve() if args.output else None
    if output:
        if output.exists():
            raise SystemExit('Restore output must not exist')
        if output == HERE or HERE in output.parents:
            raise SystemExit('Cannot restore inside the archive')
        output.mkdir(parents=True)
    logical = {}
    for item in mapping['entries']:
        path = Path(item['logical_path'])
        assert not path.is_absolute() and '..' not in path.parts
        data = (HERE / item['storage']).read_bytes()
        if item['encoding'] == 'gzip':
            data = gzip.decompress(data)
        assert len(data) == item['bytes'] and digest(data) == item['sha256'], str(path)
        logical[str(path)] = data
        if output:
            target = output / path
            target.parent.mkdir(parents=True, exist_ok=True)
            target.write_bytes(data)
    before = json.loads(logical['before-first-capture-source.json'])
    # Pre-capture source dictionary keys are relative paths, never mutable root files.
    bindings = before.get('source_sha256', before.get('sha256', before))
    if isinstance(bindings, dict):
        for name, expected in bindings.items():
            if isinstance(expected, str) and len(expected) == 64:
                assert name in logical, name
                assert digest(logical[name]) == expected, name
    saved = json.loads(logical['diagnostic/rms-capture-binding.json'])
    for name, expected in saved['before_source_sha256'].items():
        assert digest(logical['diagnostic/' + name]) == expected, name
    print(json.dumps({'verified_physical_files': len(actual),
                      'verified_logical_files': len(logical),
                      'excluded_executables': len(mapping['exclusions']),
                      'restored': str(output) if output else None}, indent=2))


if __name__ == '__main__':
    main()
