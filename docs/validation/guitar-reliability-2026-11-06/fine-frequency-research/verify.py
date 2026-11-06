from pathlib import Path
import hashlib,json
D=Path(__file__).resolve().parent
sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
m=json.loads((D/'preservation-inventory.json').read_text());included=[]
for item in m['all_original_files_sha256']:
 if item['included']:
  p=D/'preserved'/item['relative_path'];assert p.is_file(),p
  assert p.stat().st_size==item['bytes'] and sha(p)==item['sha256'],p
  assert not p.read_bytes().startswith(b'\x7fELF'),p
  included.append(item['relative_path'])
assert set(included)=={p.relative_to(D/'preserved').as_posix() for p in (D/'preserved').rglob('*') if p.is_file()}
for name,files in m['actual_complete_source_snapshots_sha256'].items():
 actual={p.relative_to(D/'preserved'/name).as_posix():sha(p) for p in (D/'preserved'/name/'Source').rglob('*') if p.is_file()}
 assert actual==files,name
print('PASS: '+str(len(included))+' copied original files; '+str(len(m['actual_complete_source_snapshots_sha256']))+' full Source snapshots; all original binary exclusions hash-bound')

L=D/'lifecycle'
if L.exists():
 m=json.loads((L/'preservation-inventory.json').read_text())
 for item in m['all_original_files']:
  if item['included']:
   p=L/item['relative_path'];assert p.stat().st_size==item['bytes'] and sha(p)==item['sha256'],p
 actual={p.relative_to(L).as_posix():sha(p) for p in (L/'Source').rglob('*') if p.is_file()}
 assert actual==json.loads((L/'source-inventory.json').read_text())['files_sha256']
 for suite in [330,132]:assert (L/('cold'+str(suite))/'events.csv').read_bytes()==(L/('warm'+str(suite))/'events.csv').read_bytes()
 print('PASS: lifecycle source/raw bindings; complete330/132 warm/cold event parity')
seal=D/'archive-seal.json'
if seal.exists():
 m=json.loads(seal.read_text());actual={p.relative_to(D).as_posix():sha(p) for p in D.rglob('*') if p.is_file() and p!=seal}
 assert actual==m['all_archived_files_sha256'],'archive seal mismatch'
 print('PASS: complete archive seal '+str(len(actual))+' files')
