#!/usr/bin/env python3
"""Preserve the bounded, rejected Burg32 experiment without binary/audio payloads."""
import gzip
import hashlib
import json
from pathlib import Path

ROOT = Path('/workspace/ChatGPT')
PRIVATE = Path('/tmp/flysnapper-v4-burg32')
ARCHIVE = ROOT / 'docs/validation/guitar-reliability-2026-11-06/burg32-negative'


def sha(data):
    return hashlib.sha256(data).hexdigest()


def main():
    if ARCHIVE.exists():
        raise SystemExit('Refusing to overwrite an existing evidence archive')
    ARCHIVE.mkdir(parents=True)
    entries, exclusions = [], []
    for path in sorted(PRIVATE.rglob('*')):
        if not path.is_file():
            continue
        data = path.read_bytes()
        relative = path.relative_to(PRIVATE).as_posix()
        digest = sha(data)
        if data.startswith(b'\x7fELF') or data.startswith(b'MZ'):
            exclusions.append({'logical_path': relative, 'bytes': len(data), 'sha256': digest,
                               'reason': 'Executable excluded; source/compile provenance retained'})
            continue
        if path.suffix.lower() in {'.wav', '.flac', '.f32', '.pcm', '.o', '.a', '.so', '.dll'}:
            raise SystemExit(f'Unexpected audio/build payload: {path}')
        compressed = path.suffix == '.csv'
        stored = f'payload/{digest[:2]}/{digest}' + ('.gz' if compressed else '')
        destination = ARCHIVE / stored
        encoded = gzip.compress(data, compresslevel=9, mtime=0) if compressed else data
        destination.parent.mkdir(parents=True, exist_ok=True)
        if destination.exists() and destination.read_bytes() != encoded:
            raise SystemExit('Payload collision')
        destination.write_bytes(encoded)
        entries.append({'logical_path': relative, 'bytes': len(data), 'sha256': digest,
                        'storage': stored, 'encoding': 'gzip' if compressed else 'raw'})
    (ARCHIVE / 'storage-map.json').write_text(json.dumps({
        'schema': 1, 'private_original_root': str(PRIVATE), 'entries': entries,
        'exclusions': exclusions, 'logical_files': len(entries),
        'unique_payloads': len({e['storage'] for e in entries})}, indent=2) + '\n')
    (ARCHIVE / 'preserve_negative.py').write_bytes(Path(__file__).read_bytes())


if __name__ == '__main__':
    main()
