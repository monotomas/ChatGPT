#!/usr/bin/env python3
"""Verify exact archive inventory, compression and stated negative results.
SPDX-License-Identifier: AGPL-3.0-or-later
"""
from pathlib import Path
import csv,gzip,hashlib,io,json
ROOT=Path(__file__).resolve().parent
def digest(data):return hashlib.sha256(data).hexdigest()
def raw(name):
 p=ROOT/name
 return p.read_bytes() if p.exists() else gzip.decompress(p.with_name(p.name+'.gz').read_bytes())
def main():
 manifest=json.loads((ROOT/'archive-manifest.json').read_text());listed=manifest['files']
 actual={p.relative_to(ROOT).as_posix() for p in ROOT.rglob('*') if p.is_file() and p.name!='archive-manifest.json'}
 assert actual==set(listed),'Incomplete archive inventory'
 for name,entry in listed.items():
  data=(ROOT/name).read_bytes();assert len(data)==entry['bytes'] and digest(data)==entry['sha256'],name
  assert data[:4] not in (b'\x7fELF',b'\0asm') and data[:2]!=b'MZ',name
 compression=json.loads((ROOT/'compression.json').read_text())
 for name,entry in compression.items():
  data=(ROOT/entry['retained_path']).read_bytes();unpacked=gzip.decompress(data)
  assert digest(data)==entry['gzip_sha256'] and digest(unpacked)==entry['raw_sha256'] and len(unpacked)==entry['raw_bytes'],name
 inventory=json.loads((ROOT/'source-inventory.json').read_text())
 for variant,files in inventory.items():
  for name,value in files.items():assert digest((ROOT/'variants'/variant/name).read_bytes())==value,(variant,name)
 probe=json.loads((ROOT/'poly-calibration/flysnapper-poly-calibration-probe-manifest.json').read_text())
 for historical,entry in probe['files'].items():
  location=ROOT/'variants/base/Source/Pitch'/Path(historical).name if '/Source/Pitch/' in historical else ROOT/'poly-calibration'/Path(historical).name
  assert location.stat().st_size==entry['bytes'] and digest(location.read_bytes())==entry['sha256'],historical
 reproduction=json.loads((ROOT/'reproduction-checks.json').read_text())
 assert reproduction['positive']['file_count']==7 and reproduction['positive']['all_byte_identical']
 assert reproduction['trained']['file_count']==14 and reproduction['trained']['all_byte_identical']
 for name in ('default-gate','double-conditioner','discard-inner','skip-inner','gate-pulse'):
  assert reproduction[name]['scorer_exit_code']==1 and reproduction[name]['byte_identical_original_reports']
 assert reproduction['trained_full_source_rebuild']['compile_exit_code']==0
 assert reproduction['trained_full_source_rebuild']['capture_exit_code']==0
 assert reproduction['trained_full_source_rebuild']['raw_events_byte_identical']
 comparison=json.loads((ROOT/'cold-comparison.json').read_text())
 expected={'default-gate':35,'double-conditioner':34,'discard-inner':37,'skip-inner':37,'gate-pulse':38}
 for name,passed in expected.items():
  summary=json.loads(raw(f'runs/cold/{name}/scoring/summary.json'));cases=json.loads(raw(f'runs/cold/{name}/scoring/cases.json'))
  assert summary['runs']==65 and summary['out_of_scope_runs']==7 and summary['passed_runs']==passed
  assert sum(c['passed'] for c in cases)==passed and comparison[name]['counts']==summary['counts']
 warm=json.loads((ROOT/'trained-result-summary.json').read_text())
 assert len(warm)==24 and all(row['calibration'].startswith('1,') for row in warm)
 mono=[r for r in warm if r['case'].endswith('_mono')]
 assert len(mono)==12
 assert all(r.get('extra_onsets',0)==0 and (r.get('matched',True)) for r in mono)
 assert all(not r['note_events'] for r in mono if r['case'].startswith('handling'))
 poly=[r for r in warm if r['case'].endswith('_poly')]
 assert sum(r.get('extra_onsets',0) for r in poly)==12
 assert sum(sum(e[0]=='N' for e in r['note_events']) for r in poly if r['case'].startswith('handling'))==14
 rows=list(csv.DictReader(io.StringIO((ROOT/'poly-calibration/flysnapper-poly-calibration-probe-v2.csv').read_text())))
 assert len(rows)==36
 assert sum(int(r['success']) for r in rows if int(r['snr_db'])>=12)==27
 assert all(r['success']=='0' and r['failure']=='1' and float(r['tonal_seconds'])==0 for r in rows if r['snr_db']=='6')
 withdrawal=(ROOT/'host-withdrawal/flysnapper-host-observer-withdrawal.txt').read_text().splitlines()
 assert len(withdrawal)==6 and all(float(line.split(',')[-1])==0 for line in withdrawal)
 print(f'PASS: {len(listed)} archived files, {len(compression)} lossless bindings, exact source inventories and retained failures.')
if __name__=='__main__':main()
