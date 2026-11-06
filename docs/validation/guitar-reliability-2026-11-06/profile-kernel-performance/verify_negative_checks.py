#!/usr/bin/env python3
"""Exercise archive rejection on external copies; never modify the evidence."""
import argparse
import hashlib
import json
from pathlib import Path
import shutil
import subprocess
import sys

ROOT = Path(__file__).resolve().parent


def rebind_seal(root):
    path = root / 'archive-manifest.json'
    data = json.loads(path.read_text())
    data['files'] = {p.relative_to(root).as_posix(): {
        'sha256': hashlib.sha256(p.read_bytes()).hexdigest(), 'bytes': p.stat().st_size}
        for p in root.rglob('*') if p.is_file() and p != path}
    path.write_text(json.dumps(data, indent=2) + '\n')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    output = args.output.resolve()
    if output == ROOT or ROOT in output.parents:
        parser.error('Output must be outside archive')
    output.mkdir(parents=True, exist_ok=False)
    results = []
    cases = (
        ('unexpected_nested_manifest', 'Archive inventory/hash mismatch'),
        ('baseline_source_rebound_seal', 'Frozen baseline origin map mismatch'),
        ('event_payload_rebound_seal', 'Original event binding mismatch'),
    )
    for name, expected in cases:
        copy = output / name
        shutil.copytree(ROOT, copy)
        if name == 'unexpected_nested_manifest':
            nested = copy / 'unexpected/archive-manifest.json'
            nested.parent.mkdir()
            nested.write_text('{}\n')
        elif name == 'baseline_source_rebound_seal':
            source = copy / 'frozen-source/baseline/Source/Pitch/TimeDomainPursuit.cpp'
            source.write_bytes(source.read_bytes() + b'\n// Intentional negative-check mutation.\n')
            rebind_seal(copy)
        else:
            events = copy / 'original-runs/payload-cold330-fast/events.csv'
            events.write_bytes(events.read_bytes() + b'\n')
            rebind_seal(copy)
        command = [sys.executable, '-B', str(copy / 'reproduce.py'), '--suite', 'verify']
        result = subprocess.run(command, text=True, capture_output=True)
        assert result.returncode != 0 and expected in result.stderr, (name, result)
        (output / (name + '.stdout.txt')).write_text(result.stdout)
        (output / (name + '.stderr.txt')).write_text(result.stderr)
        results.append({'case': name, 'rejected': True, 'returncode': result.returncode,
                        'expected_reason': expected, 'command': command})
        shutil.rmtree(copy)
    (output / 'negative-checks.json').write_text(json.dumps(results, indent=2) + '\n')
    print(json.dumps(results, indent=2))


if __name__ == '__main__':
    main()
