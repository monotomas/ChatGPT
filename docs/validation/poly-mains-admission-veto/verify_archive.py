#!/usr/bin/env python3
# SPDX-License-Identifier: AGPL-3.0-or-later
"""Verify this rejected research archive without rebuilding or changing it."""
from pathlib import Path
import csv
import hashlib
import json

ROOT = Path(__file__).resolve().parent


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def read_json(path):
    return json.loads(path.read_text())


def main():
    manifest = read_json(ROOT / 'archive-manifest.json')
    actual = {p.relative_to(ROOT).as_posix() for p in ROOT.rglob('*')
              if p.is_file() and p.name != 'archive-manifest.json'}
    assert actual == set(manifest['files_sha256']), 'Archive file set changed'
    for name, digest in manifest['files_sha256'].items():
        assert sha(ROOT / name) == digest, name

    for variant in ('baseline', 'candidate'):
        source = ROOT / variant
        frozen = read_json(source / 'source-manifest.json')
        for name, digest in frozen['files_sha256'].items():
            assert sha(source / name) == digest, (variant, name)
        build = read_json(ROOT / 'build' / variant / 'build-manifest.json')
        assert build['source_changed'] is False
        for name, digest in build['source_sha256'].items():
            assert sha(source / name) == digest, (variant, name)
        for capture in (ROOT / 'reports' / variant).glob('*/measurement-manifest.json'):
            record = read_json(capture)
            assert record['capture_exit_code'] == 0
            assert record['source_changed_during_measurement'] is False
            assert record['source_sha256'] == build['source_sha256']

    comparison = read_json(ROOT / 'comparison.json')
    evidence = read_json(ROOT / 'evidence-summary.json')
    for dataset, record in evidence['raw_equality'].items():
        name = 'events.csv' if dataset in ('controlled72', 'recorded-noise128') else 'audio-events.csv'
        paths = [ROOT / 'reports' / v / dataset / name for v in ('baseline', 'candidate')]
        assert sha(paths[0]) == record['baseline_sha256']
        assert sha(paths[1]) == record['candidate_sha256']
        assert (paths[0].read_bytes() == paths[1].read_bytes()) == record['equal']
        assert record['equal'] == comparison[dataset]['raw_events_equal']
        assert record['equal'] == (dataset != 'controlled72')
    for version in ('positive', 'positive2'):
        paths = [ROOT / f'{v}-{version}-events.csv' for v in ('baseline', 'candidate')]
        assert paths[0].read_bytes() == paths[1].read_bytes()
        record = evidence['positive_controls'][version]
        assert sha(paths[0]) == record['baseline_sha256'] == record['candidate_sha256']

    scores = {v: read_json(ROOT / 'reports' / v / 'controlled72/scoring/cases.json')
              for v in ('baseline', 'candidate')}
    for loss in evidence['newly_missed_references']:
        for variant in ('baseline', 'candidate'):
            score = next(r for r in scores[variant] if r['case'] == loss['case'])
            assert score['matched'] == loss[variant]['matched']
            assert score['missed'] == loss[variant]['missed']
        assert loss['candidate']['missed'] - loss['baseline']['missed'] == 1
    assert len(evidence['newly_missed_references']) == 3
    mono = {s['case'] for s in scores['baseline'] if s['mode'] == 'mono'}
    mono_events = []
    for variant in ('baseline', 'candidate'):
        with (ROOT / 'reports' / variant / 'controlled72/events.csv').open() as handle:
            mono_events.append([r for r in csv.DictReader(handle) if r['case'] in mono])
    assert mono_events[0] == mono_events[1]
    print(f"Verified {len(actual)} archived file hashes; rejected guard retains all three wanted-note regressions.")
    print('Unchanged raw events: legacy 196, recorded-noise 128, both positive-control sweeps, controlled mono.')


if __name__ == '__main__':
    main()
