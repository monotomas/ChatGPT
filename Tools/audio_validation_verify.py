#!/usr/bin/env python3
"""Verify checked-in acoustic fixtures without numpy, ffmpeg or network access.
SPDX-License-Identifier: AGPL-3.0-or-later
"""
from pathlib import Path
import argparse,hashlib,json,wave

def require(condition,message='Validation failed'):
    if not condition:raise ValueError(message)

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('root',nargs='?',type=Path,default=Path(__file__).resolve().parents[1]/'Tests/AudioFixtures')
    root=parser.parse_args().root
    provenance=json.loads((root/'tonejs/manifest.json').read_text())
    for item in provenance['files']:
        path=root/'tonejs'/item['local_file']
        require(hashlib.sha256(path.read_bytes()).hexdigest() == item['sha256'], path)
    corpus=json.loads((root/'fixtures.json').read_text())
    by_file={}
    for item in corpus['records']:
        path=root/item['file']
        require(path.resolve().is_relative_to(root.resolve()), path)
        require(hashlib.sha256(path.read_bytes()).hexdigest() == item['sha256'], path)
        with wave.open(str(path),'rb') as audio:
            require(audio.getnchannels() == 1 and audio.getsampwidth() == 2, path)
            require(audio.getframerate() == item['sample_rate'], path)
            require(audio.getnframes() == item['frames'], path)
        for t in item['truth']:
            require(0 <= t['note'] <= 127 and 0 <= t['on_sample'] < t['off_sample'] <= item['frames'], item)
        require(item['file'] not in by_file, item['file'])
        by_file[item['file']]=item
    names=set()
    for case in corpus['cases']:
        require(case['name'] not in names, case['name']);names.add(case['name'])
        require(case['file'] in by_file and case['truth'] == by_file[case['file']]['truth'], case)
        require(case['mode'] in ('mono', 'poly'), case)
    rows=(root/'fixtures.tsv').read_text().splitlines()[1:]
    require(len(rows) == len(corpus['cases']), "Validation failed: len(rows) == len(corpus['cases'])")
    for row,case in zip(rows,corpus['cases']):
        fields=row.split('\t')
        expected_truth=','.join(f"{t['note']}:{t['on_sample']}:{t['off_sample']}" for t in case['truth'])
        require(fields == [case['name'], case['family'], case['mode'], case['file'], expected_truth], case)
    require(len(names) == 81, 'Validation failed: len(names) == 81')
    print(f"Verified {len(provenance['files'])} pinned source files, {len(by_file)} PCM waveforms and {len(names)} case annotations")

if __name__=='__main__':main()
