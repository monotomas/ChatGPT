#!/usr/bin/env python3
"""Verify/restore lossless rejected mono-trigger studies without running audio."""
from pathlib import Path,PurePosixPath
import argparse,gzip,hashlib,json
ROOT=Path(__file__).resolve().parent

def sha(data):return hashlib.sha256(data).hexdigest()
def rel(name):
    if not isinstance(name,str) or '\\' in name or '\x00' in name:
        raise ValueError('Unsafe path')
    parts=name.split('/')
    if PurePosixPath(name).is_absolute() or any(x in ('','.','..') for x in parts):
        raise ValueError('Unsafe path: '+name)
    return Path(*parts)
def prohibited(data,name):
    return (data.startswith((b'\x7fELF',b'MZ',b'\0asm',b'!<arch>'))
            or data[:4]==b'RIFF' and data[8:12]==b'WAVE'
            or Path(name).suffix.lower() in {'.o','.obj','.so','.dll','.exe','.a','.lib','.pyc','.wav','.f32','.flac','.mp3','.aif','.aiff','.onnx','.pt','.pth','.tflite','.h5','.safetensors','.npy','.npz'})
def verify_archive():
    for p in ROOT.rglob('*'):
        if p.is_symlink():raise ValueError('Archive contains a link: '+str(p))
    manifest=json.loads((ROOT/'archive-manifest.json').read_text())
    actual={p.relative_to(ROOT).as_posix():sha(p.read_bytes()) for p in ROOT.rglob('*') if p.is_file() and p!=ROOT/'archive-manifest.json'}
    if actual!=manifest['files_sha256']:raise ValueError('Archive inventory/hash mismatch')
    storage=json.loads((ROOT/'storage-map.json').read_text());logical={};decoded={};used=set()
    for item in storage['entries']:
        name=item['logical_path'];rel(name);stored_name=item['stored_path'];path=ROOT/rel(stored_name);used.add(stored_name)
        if name in logical:raise ValueError('Duplicate logical name '+name)
        if stored_name not in decoded:
            payload=path.read_bytes()
            if len(payload)!=item['stored_bytes'] or sha(payload)!=item['stored_sha256']:raise ValueError('Stored payload differs '+name)
            if item['encoding']=='gzip':payload=gzip.decompress(payload)
            elif item['encoding']!='raw':raise ValueError('Unknown payload encoding')
            decoded[stored_name]=payload
        payload=decoded[stored_name]
        if len(payload)!=item['bytes'] or sha(payload)!=item['sha256']:raise ValueError('Logical payload differs '+name)
        if prohibited(payload,name):raise ValueError('Forbidden runtime/audio/model payload '+name)
        logical[name]=payload
    if len(logical)!=storage['logical_files'] or len(used)!=storage['unique_payloads']:raise ValueError('Storage count differs')
    if sum(len(v) for v in logical.values())!=storage['logical_bytes']:raise ValueError('Logical byte count differs')
    snapshots=json.loads((ROOT/'source-snapshots.json').read_text())['snapshots'];prefixes=set()
    for snapshot in snapshots:
        prefix=snapshot['logical_prefix'];rel(prefix)
        if prefix in prefixes:raise ValueError('Duplicate source prefix')
        prefixes.add(prefix)
        if len(snapshot['files_sha256'])!=snapshot['files']:raise ValueError('Source count differs')
        for name,digest in snapshot['files_sha256'].items():
            key=prefix+'/'+name;rel(name)
            if key not in logical or sha(logical[key])!=digest:raise ValueError('Source differs '+key)
    excluded={r['logical_path']:r for r in json.loads((ROOT/'exclusions.json').read_text())['files']}
    if any(name in logical for name in excluded):raise ValueError('Excluded runtime was copied')
    bindings=json.loads((ROOT/'runtime-bindings.json').read_text())['executables']
    for row in bindings:
        name=row['executable_logical_path'];external=excluded.get(name)
        if external is None or row['excluded_executable_sha256']!=external['sha256'] or row['excluded_executable_bytes']!=external['bytes']:raise ValueError('Runtime exclusion binding differs')
        if row['source_logical_prefix'] not in prefixes:raise ValueError('Unknown source snapshot')
        driver=row['driver_logical_path']
        if driver not in logical or sha(logical[driver])!=row['driver_sha256']:raise ValueError('Driver binding differs')
        if 'recorded_measurement_manifest' in row:
            proof=json.loads(logical[row['recorded_measurement_manifest']])
            if proof.get('executable_sha256')!=external['sha256'] or row['recorded_executable_hash_matches_excluded'] is not True:raise ValueError('Original executable attestation differs')
    return logical,storage,snapshots

def restore(logical,output):
    out=output.resolve()
    if out.exists() or out.is_relative_to(ROOT) or ROOT.is_relative_to(out):
        raise ValueError('Restore destination must be new and outside archive')
    out.mkdir(parents=True)
    for name,data in logical.items():
        target=out/rel(name);target.parent.mkdir(parents=True,exist_ok=True);target.write_bytes(data)
    return out

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output',type=Path,help='Restore all logical bytes to a fresh external directory')
    parser.add_argument('--project-root',type=Path,help='Additionally verify shared metadata/license files only; never reads PCM or f32')
    args=parser.parse_args();logical,storage,snapshots=verify_archive()
    if args.project_root:
        for item in json.loads((ROOT/'external-inputs.json').read_text())['files']:
            name=item['project_path'];path=args.project_root/rel(name)
            if path.suffix.lower() in ('.wav','.f32'):raise ValueError('Unexpected audio in metadata-only input list')
            if path.is_symlink() or not path.is_file() or path.stat().st_size!=item['bytes'] or sha(path.read_bytes())!=item['sha256']:raise ValueError('Canonical metadata differs '+name)
    if args.output:restore(logical,args.output)
    print('Verified',len(logical),'logical files,',storage['unique_payloads'],'unique lossless payloads,',len(snapshots),'complete source snapshots; no audio/runtime/model execution.')
if __name__=='__main__':main()
