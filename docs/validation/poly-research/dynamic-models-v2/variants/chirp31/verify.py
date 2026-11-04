#!/usr/bin/env python3
"""Verify the independent sealed oracle tracking edition, without any DSP.
SPDX-License-Identifier: AGPL-3.0-or-later
"""
from pathlib import Path
import array,csv,hashlib,json,math,sys
HERE=Path(__file__).resolve().parent
def sha(path):return hashlib.sha256(path.read_bytes()).hexdigest()
def rows(path):return list(csv.DictReader(path.open(newline=''),delimiter='\t'))
def verify(root=HERE):
    manifest=json.loads((root/'fixtures.json').read_text());policy=json.loads((root/'policy.json').read_text())
    assert manifest['generator_sha256']==sha(root/'generate.py') and manifest['policy_sha256']==sha(root/'policy.json')
    assert manifest['detector_used_during_generation'] is False and manifest['note_acquisition_tested'] is False
    seal=hashlib.sha256(b''.join((root/f).read_bytes() for f in ('fixtures.json','cases.tsv','seeds.tsv','truth.tsv','policy.json'))).hexdigest()
    assert seal==(root/'SEALED_SHA256.txt').read_text().strip()
    inventory=rows(root/'cases.tsv');seeds=rows(root/'seeds.tsv');truth=rows(root/'truth.tsv');lookup={c['name']:c for c in manifest['cases']}
    assert {c['case'] for c in inventory}==set(lookup)
    for case in inventory:
        item=lookup[case['case']];path=Path(item['file']);assert not path.is_absolute() and '..' not in path.parts
        pcm=root/path;assert not pcm.is_symlink() and sha(pcm)==item['sha256'] and pcm.stat().st_size==int(case['frames'])*4
        audio=array.array('f');audio.frombytes(pcm.read_bytes())
        if sys.byteorder!='little':audio.byteswap()
        assert all(math.isfinite(v) and abs(v)<.95 for v in audio)
        case_seeds=[s for s in seeds if s['case']==item['name']];assert {int(s['voice_id']) for s in case_seeds}==set(range(6))
        assert all(s['voice_id']==s['slot'] for s in case_seeds)
        case_truth=[r for r in truth if r['case']==item['name']];keys={(int(r['sample']),int(r['voice_id'])) for r in case_truth}
        expected={(n,v) for n in range(127,item['frames'],32) for v in range(6)};assert keys==expected and len(keys)==len(case_truth)
        for r in case_truth:
            sample=int(r['sample']);assert int(r['scored'])==int(policy['scored_sample_interval'][0]<=sample<=policy['scored_sample_interval'][1])
        for s in case_seeds:
            original=next(r for r in case_truth if int(r['sample'])==manifest['oracle_seed_sample'] and r['voice_id']==s['voice_id'])
            assert abs(float(s['pitch'])-float(original['pitch']))<1e-8
    print('Verified',len(lookup),'independent oracle controls, seal',seal)
    return manifest
if __name__=='__main__':verify()
