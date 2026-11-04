#!/usr/bin/env python3
"""Reproduce a seen full512 configuration diagnostic without editing the suite.
SPDX-License-Identifier: AGPL-3.0-or-later
The original first-unseen acceptance failures remain unchanged.
"""
from pathlib import Path
import argparse,csv,hashlib,json,shutil
HERE=Path(__file__).resolve().parent
def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--output',type=Path,required=True);a=p.parse_args();root=a.output.resolve();root.mkdir()
    for name in ('generate.py','policy.json','capture.cpp','score.py','verify.py','run.py'):shutil.copyfile(HERE/name,root/name)
    manifest=json.loads((HERE/'fixtures.json').read_text());selected={c['name'] for c in manifest['cases'] if c['observation']=='varied'}
    manifest['cases']=[c for c in manifest['cases'] if c['name'] in selected]
    for case in manifest['cases']:
        case['observation']='full512';(root/Path(case['file']).parent).mkdir(exist_ok=True);shutil.copyfile(HERE/case['file'],root/case['file'])
    manifest.update(edition=manifest['edition']+'-post-hoc-full512-diagnostic',post_hoc_configuration_override=True,unseen=False,original_edition_seal=(HERE/'SEALED_SHA256.txt').read_text().strip(),interpretation='Identical already-seen audio and source19, count-mode override only. Original acceptance result unchanged.')
    (root/'fixtures.json').write_text(json.dumps(manifest,indent=2)+'\n')
    for name in ('cases.tsv','seeds.tsv','truth.tsv'):
        with (HERE/name).open(newline='') as f:reader=csv.DictReader(f,delimiter='\t');header=reader.fieldnames;rows=[r for r in reader if r['case'] in selected]
        if name=='cases.tsv':
            for row in rows:row['observation']='full512'
        with (root/name).open('w',newline='') as f:writer=csv.DictWriter(f,fieldnames=header,delimiter='\t',lineterminator='\n');writer.writeheader();writer.writerows(rows)
    seal=hashlib.sha256(b''.join((root/f).read_bytes() for f in ('fixtures.json','cases.tsv','seeds.tsv','truth.tsv','policy.json'))).hexdigest();(root/'SEALED_SHA256.txt').write_text(seal+'\n');print('Seen full512 diagnostic seal:',seal)
if __name__=='__main__':main()
