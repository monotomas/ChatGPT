#!/usr/bin/env python3
"""Verify lossless v4 latency evidence; optionally restore to a fresh directory."""
from pathlib import Path,PurePosixPath
import argparse,gzip,hashlib,json
ROOT=Path(__file__).resolve().parent
def sha(data):return hashlib.sha256(data).hexdigest()
def relative(name):
    p=PurePosixPath(name)
    if p.is_absolute() or any(x in ('','..','.') for x in name.split('/')) or '\\' in name:
        raise ValueError('Unsafe path: '+name)
    return Path(*p.parts)
def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--output',type=Path);a=p.parse_args()
    manifest=json.loads((ROOT/'archive-manifest.json').read_text())
    actual={p.relative_to(ROOT).as_posix():sha(p.read_bytes()) for p in ROOT.rglob('*') if p.is_file() and p.name!='archive-manifest.json'}
    if actual!=manifest['files_sha256']:raise ValueError('Archive inventory/hash differs')
    storage=json.loads((ROOT/'storage-map.json').read_text());logical={}
    for entry in storage['entries']:
        stored=ROOT/relative(entry['stored_path'])
        if stored.is_symlink():raise ValueError('Linked payload')
        raw=stored.read_bytes()
        if len(raw)!=entry['stored_bytes'] or sha(raw)!=entry['stored_sha256']:raise ValueError('Stored payload differs')
        raw=gzip.decompress(raw) if entry['encoding']=='gzip' else raw
        if len(raw)!=entry['bytes'] or sha(raw)!=entry['sha256']:raise ValueError('Logical payload differs')
        if entry['logical_path'] in logical:raise ValueError('Duplicate logical path')
        relative(entry['logical_path']);logical[entry['logical_path']]=raw
    variants={name.split('/')[1] for name in logical if name.startswith('source/') and name.endswith('/source-manifest.json')}
    reconstructed={}
    for variant in sorted(variants):
        doc=json.loads(logical[f'source/{variant}/source-manifest.json'])
        for name,digest in doc['source_sha256'].items():
            key=f'source/{variant}/{name}'
            raw=logical.get(key,logical.get(f'source/baseline/{name}'))
            if raw is None or sha(raw)!=digest:raise ValueError('Reconstructed source differs: '+key)
            reconstructed[key]=raw
    if a.output:
        out=a.output.resolve()
        if out.is_relative_to(ROOT) or ROOT.is_relative_to(out) or out.exists():raise ValueError('Output must be a fresh directory outside archive')
        out.mkdir(parents=True)
        for name,raw in {**logical,**reconstructed}.items():
            target=out/relative(name);target.parent.mkdir(parents=True,exist_ok=True);target.write_bytes(raw)
    print(f'Verified {len(logical)} logical files, {len(variants)} complete source variants; no PCM/binary execution.')
if __name__=='__main__':main()
