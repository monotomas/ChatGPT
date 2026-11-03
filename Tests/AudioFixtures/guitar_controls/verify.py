#!/usr/bin/env python3
"""Verify retained guitar-control PCM/provenance using the standard library.
SPDX-License-Identifier: AGPL-3.0-or-later
"""
import csv
import hashlib
import json
from pathlib import Path
import wave

HERE = Path(__file__).resolve().parent


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def verify(root=HERE):
    manifest = json.loads((root / 'fixtures.json').read_text())
    assert manifest['schema'] == 1 and manifest['scope'] == 'guitar-user-defect-controls'
    assert manifest['sample_rate'] == 48000
    assert manifest['detector_used_during_generation'] is False
    assert manifest['recordings_are_disclosed'] is True
    assert len(manifest['records']) == 42 and len(manifest['cases']) == 55
    by_file = {}
    for record in manifest['records']:
        relative = Path(record['file'])
        assert not relative.is_absolute() and '..' not in relative.parts
        assert record['file'] not in by_file
        by_file[record['file']] = record
        file = root / relative
        assert sha(file) == record['sha256'], record['file']
        with wave.open(str(file), 'rb') as w:
            assert (w.getnchannels(), w.getsampwidth(), w.getframerate(), w.getnframes()) == (1, 2, 48000, record['frames'])
        for item in record['truth']:
            assert 0 <= item['note'] <= 127
            assert 0 <= item['on_sample'] < item['off_sample'] <= record['frames']
        if 'recorded_source' in record:
            source = root.parent / record['recorded_source']
            assert source.resolve().is_relative_to(root.parent.resolve())
            assert sha(source) == record['recorded_source_sha256']
    rows = list(csv.reader((root / 'fixtures.tsv').open(newline=''), delimiter='\t'))[1:]
    assert len(rows) == 55
    for row, case in zip(rows, manifest['cases']):
        truth = ','.join(f"{t['note']}:{t['on_sample']}:{t['off_sample']}" for t in case['truth'])
        assert row == [case['name'], case['family'], case['mode'], case['file'], truth]
        assert case['truth'] == by_file[case['file']]['truth']
    seal = hashlib.sha256((root / 'fixtures.json').read_bytes() + (root / 'fixtures.tsv').read_bytes()
                          + (root / 'pitch-curves.tsv').read_bytes()).hexdigest()
    assert seal == (root / 'SEALED_SHA256.txt').read_text().strip()
    baseline = root / 'baseline-source19'
    original = json.loads((baseline / 'manifest.json').read_text())
    assert original['source_commit'] == 'bcd0b202e9b74c58b7c76e8f77abf2c14541df97'
    assert original['fixture_seal'] == seal and original['rows'] == 55
    for name, digest in original['files_sha256'].items():
        assert sha(baseline / name) == digest, 'Original baseline changed:' + name
    print('Verified42 retained guitar-control PCM waveforms,55 mode cases and original failing source19 evidence.')


if __name__ == '__main__':
    verify()
