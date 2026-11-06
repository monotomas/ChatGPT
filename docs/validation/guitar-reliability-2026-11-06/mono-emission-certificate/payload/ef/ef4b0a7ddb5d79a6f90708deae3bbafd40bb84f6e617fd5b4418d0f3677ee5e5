#!/usr/bin/env python3
from pathlib import Path,PurePosixPath
import argparse,json,hashlib,gzip
A=Path(__file__).resolve().parent
p=argparse.ArgumentParser();p.add_argument('--output',type=Path);p.add_argument('--project-root',type=Path);a=p.parse_args()
sha=lambda b:hashlib.sha256(b).hexdigest()
def safe(x):
 s=PurePosixPath(x)
 if s.is_absolute() or '\\' in x or any(v in ('','.','..') for v in x.split('/')):raise ValueError('Unsafe path '+x)
 return Path(*s.parts)
m=json.loads((A/'archive-manifest.json').read_text())['files'];actual={f.relative_to(A).as_posix() for f in A.rglob('*') if f.is_file() and f.name!='archive-manifest.json'}
assert actual==set(m),'Archive file set changed'
for name,info in m.items():
 f=A/safe(name);assert not f.is_symlink();b=f.read_bytes();assert sha(b)==info['sha256'] and len(b)==info['bytes'],name
logical={}
for e in json.loads((A/'storage-map.json').read_text())['files']:
 name=e['logical_path'];safe(name);assert name not in logical;blob=(A/safe(e['stored_path'])).read_bytes();assert sha(blob)==e['stored_sha256'] and len(blob)==e['stored_bytes'];assert e['encoding'] in ('gzip','raw');data=gzip.decompress(blob) if e['encoding']=='gzip' else blob;assert sha(data)==e['sha256'] and len(data)==e['bytes'];logical[name]=data
for s in json.loads((A/'source-snapshots.json').read_text())['snapshots']:
 for name,digest in s['files_sha256'].items():assert sha(logical[s['logical_prefix']+'/'+name])==digest
for item in json.loads((A/'runtime-bindings.json').read_text())['files']:
 assert item['recorded_compile_bindings'];assert all(b['manifest'] in logical for b in item['recorded_compile_bindings'])
if a.project_root:
 for e in json.loads((A/'external-inputs.json').read_text())['files']:assert sha((a.project_root/safe(e['project_path'])).read_bytes())==e['sha256'],e['project_path']
if a.output:
 out=a.output.resolve();assert out!=A and A not in out.parents and not out.exists(),'Use a new external restore directory';out.mkdir(parents=True)
 for name,data in logical.items():f=out/safe(name);f.parent.mkdir(parents=True,exist_ok=True);f.write_bytes(data)
print('PASS',len(m),'archive files;',len(logical),'logical files; source/runtime bindings verified; no audio or native code executed')
