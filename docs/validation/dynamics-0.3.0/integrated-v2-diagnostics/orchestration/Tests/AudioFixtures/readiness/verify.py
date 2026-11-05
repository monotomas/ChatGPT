#!/usr/bin/env python3
"""Verify sealed readiness data and unchanged original failing baseline captures.
SPDX-License-Identifier: AGPL-3.0-or-later
"""
from pathlib import Path
import csv,hashlib,json,wave
HERE=Path(__file__).resolve().parent

def sha(path):return hashlib.sha256(path.read_bytes()).hexdigest()
def need(value,message):
    if not value:raise ValueError(message)
def relative(root,name):
    path=Path(name);need(not path.is_absolute() and '..' not in path.parts,'Unsafe fixture path')
    result=root/path;need(not result.is_symlink() and result.resolve().is_relative_to(root.resolve()),'Linked/outside fixture path');return result

def verify(root=HERE):
    manifest=json.loads((root/'fixtures.json').read_text());need(manifest['schema']==1 and manifest['sample_rate']==48000,'Unsupported fixture schema')
    need(manifest['detector_used_during_generation'] is False,'Fixture generation used detector output')
    if 'generator_sha256' in manifest:need(manifest['generator_sha256']==sha(root/'generate.py'),'Generator differs from sealed edition')
    if 'derive_script_sha256' in manifest:need(manifest['derive_script_sha256']==sha(root/'derive.py'),'Derivation script differs from sealed edition')
    need(manifest['policy_sha256']==sha(root/'policy.json'),'Predefined policy changed')
    seal=hashlib.sha256(b''.join((root/name).read_bytes() for name in ('fixtures.json','fixtures.tsv','pitch-curves.tsv','policy.json'))).hexdigest()
    need(seal==(root/'SEALED_SHA256.txt').read_text().strip(),'Fixture seal differs')
    by_file={}
    for record in manifest['records']:
        name=record['file'];need(name not in by_file,'Duplicate record');by_file[name]=record
        path=relative(root,name);need(sha(path)==record['sha256'],'Changed PCM:'+name)
        with wave.open(str(path),'rb') as w:need((w.getnchannels(),w.getsampwidth(),w.getframerate(),w.getnframes())==(1,2,48000,record['frames']),'Unexpected PCM shape:'+name)
        if 'source' in record:need(sha(relative(root,record['source']))==record['source_sha256'],'Changed original recording')
        for source in record.get('all_sources',[]):need(sha(relative(root,source['path']))==source['sha256'],'Changed original mixture recording')
    rows=list(csv.reader((root/'fixtures.tsv').open(newline=''),delimiter='\t'))[1:]
    need(len(rows)==len(manifest['cases']),'Inventory count differs')
    curve_keys=set()
    for row,case in zip(rows,manifest['cases']):
        truth=','.join(f"{n['anchor']}:{n['on_sample']}:{n['off_sample']}" for n in case['reference_notes'])
        need(row==[case['name'],case['family'],case['mode'],case['file'],truth,'1'],'Annotation/inventory differs')
        record=by_file[case['file']]
        for note in case['reference_notes']:
            need(0<=note['anchor']<=127 and 0<=note['on_sample']<note['off_sample']<=record['frames'],'Invalid note range')
            curve_keys.add((case['record'],note['id']))
    actual_keys=set()
    for row in csv.DictReader((root/'pitch-curves.tsv').open(newline=''),delimiter='\t'):actual_keys.add((row['record'],row['note_id']))
    need(curve_keys==actual_keys,'Missing/extra truth curve')
    if (root/'source/provenance.json').exists():
        provenance=json.loads((root/'source/provenance.json').read_text())
        for name,digest in provenance['support_sha256'].items():need(sha(root/'source'/name)==digest,'Changed license/author map:'+name)
        for record in provenance['recordings']:need(sha(root/record['original_relative_path'])==record['sha256'],'Changed CC0 source')
    baseline=root/'baseline-c5b'
    if baseline.exists():
        for p in baseline.rglob('manifest.json'):
            item=json.loads(p.read_text())
            need(item.get('source_commit')=='c5b05337641b13ae259836f5346e7c1850b7382c','Baseline source differs')
            need(item['fixture_seal']==seal,'Baseline fixture seal differs')
            for name,digest in item.get('files_sha256',{}).items():need(sha(p.parent/name)==digest,'Original baseline changed:'+str(p.parent/name))
    print(f'Verified {len(manifest["records"])} PCM records, {len(manifest["cases"])} mode cases, seal {seal}.')
    return manifest

if __name__=='__main__':
    verify()
    for name in ('recorded_hammers','recorded_noise'):
        if (HERE/name/'fixtures.json').exists():verify(HERE/name)
