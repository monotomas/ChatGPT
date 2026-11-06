from pathlib import Path
import hashlib,json,shutil,datetime
origin=Path('/tmp/flysnapper-v4-poly-f0')
dest=Path('/workspace/ChatGPT/docs/validation/guitar-reliability-2026-11-06/fine-frequency-research')
dest.mkdir()
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
entries=[]
for p in sorted(origin.rglob('*')):
 if not p.is_file():continue
 rel=p.relative_to(origin);data=p.read_bytes();binary=data.startswith(b'\x7fELF') or p.suffix in ('.o','.wav','.f32','.pcm','.bin')
 item={'original_path':str(p),'relative_path':rel.as_posix(),'bytes':len(data),'sha256':sha(p),'included':not binary}
 if binary:item['exclusion']='Executable/object/generated binary excluded; exact original hash retained.'
 else:
  out=dest/'preserved'/rel;out.parent.mkdir(parents=True,exist_ok=True);shutil.copyfile(p,out);assert sha(out)==item['sha256']
 entries.append(item)
source_maps={}
for d in sorted(origin.iterdir()):
 if d.is_dir() and (d/'Source').is_dir():source_maps[d.name]={p.relative_to(d).as_posix():sha(p) for p in sorted((d/'Source').rglob('*')) if p.is_file()}
for name in ['base','observed-f0','uncertainty-f0']:
 expected=json.loads((origin/name/'source-inventory.json').read_text())['files_sha256'];assert expected==source_maps[name]
for name in [n for n in source_maps if n.startswith('profile-')]:
 expected=json.loads((origin/name/'manifest.json').read_text())['files_sha256'];assert expected==source_maps[name]
meta={'recorded_at_utc':datetime.datetime.now(datetime.timezone.utc).isoformat(),'origin':str(origin),'source_base_git_commit':'e235c8a098dadce6e0a4b8bd6ca9de789a5d174d','private_sources_are_not_current_production_HEAD':True,'no_reserved_audio_or_real_audio_used':True,'all_original_files_sha256':entries,'actual_complete_source_snapshots_sha256':source_maps,'completeness':'Every original regular file is either copied byte-for-byte under preserved/ or listed with hash and explicit binary exclusion. All full Source trees, raw CSV, source maps, reports, diagnostics, commands, and both WARM/COLD captures are retained. Source manifests bind selected project source files, not system compiler/header closure.'}
(dest/'preservation-inventory.json').write_text(json.dumps(meta,indent=2)+'\n')
print(json.dumps({'retained':sum(e['included'] for e in entries),'excluded_binary':sum(not e['included'] for e in entries),'source_snapshots':{k:len(v) for k,v in source_maps.items()},'bytes_retained':sum(e['bytes'] for e in entries if e['included'])},indent=2))
