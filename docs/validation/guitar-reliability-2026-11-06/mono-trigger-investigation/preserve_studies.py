from pathlib import Path,PurePosixPath
import subprocess,json,hashlib,gzip,datetime,shutil,sys
REPO=Path('/workspace/ChatGPT');DEST=REPO/'docs/validation/guitar-reliability-2026-11-06/mono-trigger-investigation'
BASE='e235c8a098dadce6e0a4b8bd6ca9de789a5d174d'
ROOTS={'initial-mono':Path('/tmp/flysnapper-v4-mono-audit'),'real-repeat':Path('/tmp/flysnapper-v4-real-repeat-audit')}
if DEST.exists() and any(DEST.iterdir()):raise RuntimeError('Refuse nonempty owned archive')
DEST.mkdir(parents=True,exist_ok=True)
def sha(data):return hashlib.sha256(data).hexdigest()
def jwrite(path,value):path.parent.mkdir(parents=True,exist_ok=True);path.write_text(json.dumps(value,indent=2,ensure_ascii=False)+'\n')
def safe(name):
 p=PurePosixPath(name)
 if p.is_absolute() or '\\' in name or any(t in ('','.','..') for t in name.split('/')):raise ValueError(name)
 return Path(*p.parts)
entries=[];blobs={};excluded=[];original_to_logical={};logical_data={}
def add(logical,data,origin=None,preferred=None):
 safe(logical)
 if logical in logical_data:raise RuntimeError('Duplicate logical entry: '+logical)
 digest=sha(data)
 if digest in blobs:stored,encoding,encoded=blobs[digest]
 else:
  encoding='gzip' if logical.endswith('.csv') else 'raw'
  encoded=gzip.compress(data,compresslevel=9,mtime=0) if encoding=='gzip' else data
  suffix=''.join(PurePosixPath(logical).suffixes[-1:]) or '.txt'
  stored=preferred or f'payloads/{digest}{suffix}'+('.gz' if encoding=='gzip' else '')
  path=DEST/safe(stored);path.parent.mkdir(parents=True,exist_ok=True);path.write_bytes(encoded)
  blobs[digest]=(stored,encoding,encoded)
 entries.append(dict(logical_path=logical,sha256=digest,bytes=len(data),stored_path=stored,encoding=encoding,stored_bytes=len(encoded),stored_sha256=sha(encoded),origin=origin))
 logical_data[logical]=data
 if origin:original_to_logical[origin]=logical
# Readable reports are also deduplicated payloads for their original logical copies.
for tag,root in [('initial-mono',REPO/'build/guitar-v4-mono-failure-audit'),('real-repeat',REPO/'build/guitar-v4-real-repeat-audit')]:
 for p in sorted(root.rglob('*')):
  if p.is_symlink():raise RuntimeError('Symlink in report '+str(p))
  if p.is_file():
   preferred=(f'{tag.upper()}_REPORT.md' if p.name=='REPORT.md' else None)
   add(f'reports/{tag}/{p.relative_to(root).as_posix()}',p.read_bytes(),str(p),preferred)
# Preserve the original complete tracked Source tree, not current root's adopted fret change.
source_names=subprocess.check_output(['git','ls-tree','-r','--name-only',BASE,'--','Source'],cwd=REPO,text=True).splitlines()
canonical={}
for name in source_names:
 data=subprocess.check_output(['git','show',BASE+':'+name],cwd=REPO);logical='canonical/e235/'+name
 add(logical,data,BASE+':'+name,logical);canonical[name]=sha(data)
jwrite(DEST/'canonical/e235/source-manifest.json',dict(commit=BASE,source_sha256=canonical,scope='Complete tracked project Source tree; not compiler/JUCE/system dependency closure'))
# All original study files are kept byte-for-byte except explicitly inventoried runtime binaries.
for tag,root in ROOTS.items():
 for p in sorted(root.rglob('*')):
  if p.is_symlink():raise RuntimeError('Linked study input '+str(p))
  if not p.is_file():continue
  data=p.read_bytes();reason=None
  if data.startswith(b'\x7fELF'):reason='ELF runtime executable'
  elif data.startswith(b'MZ'):reason='PE runtime executable'
  elif data.startswith(b'\x00asm'):reason='WASM runtime'
  elif data.startswith(b'RIFF') and data[8:12]==b'WAVE':reason='PCM wave'
  elif data.startswith(b'!<arch>'):reason='object/static library'
  elif p.suffix.lower() in {'.o','.obj','.so','.dll','.exe','.a','.lib','.pyc','.f32','.wav','.flac','.mp3','.aif','.aiff','.onnx','.pt','.pth','.tflite','.h5','.safetensors','.npy','.npz'}:reason='forbidden binary/audio/model type'
  logical=f'studies/{tag}/{p.relative_to(root).as_posix()}'
  if reason:excluded.append(dict(logical_path=logical,original_path=str(p),sha256=sha(data),bytes=len(data),reason=reason));continue
  add(logical,data,str(p))
# Capture exact evaluator/driver editions and sealed metadata without copying PCM.
shared=['Tools/audio_validation.cpp','Tools/guitar_evaluation_capture.cpp','Tools/guitar_evaluation_score.py','Tools/guitar_evaluation.py','Tests/AudioFixtures/readiness/score.py','Tests/AudioFixtures/evaluation_2026_11_05/policy.json']
for name in shared:
 data=(REPO/name).read_bytes();pinned=subprocess.check_output(['git','show',BASE+':'+name],cwd=REPO)
 if data!=pinned:raise RuntimeError('Shared evaluator changed since base '+name)
 add('bindings/e235-evaluators/'+name,data,str(REPO/name))
# Existing disclosed fixtures are shared canonical inputs. Include metadata only.
fixture_roots=['Tests/AudioFixtures','Tests/AudioFixtures/holdout','Tests/AudioFixtures/confirmation','Tests/AudioFixtures/readiness','Tests/AudioFixtures/readiness/recorded_noise','Tests/AudioFixtures/readiness/recorded_hammers']
external=[]
for rootname in fixture_roots:
 folder=REPO/rootname
 for name in ['fixtures.json','fixtures.tsv','pitch-curves.tsv','policy.json','SEALED_SHA256.txt','verify.py','generate.py','LICENSE.txt','README.md']:
  p=folder/name
  if p.is_file():
   data=p.read_bytes();add('bindings/fixture-metadata/'+rootname+'/'+name,data,str(p));external.append(dict(project_path=rootname+'/'+name,sha256=sha(data),bytes=len(data),kind='canonical fixture metadata'))
# New DEV input ownership is preserved by reference to the separately owned sealed fixture edition.
newroot=REPO/'Tests/AudioFixtures/guitar_reliability_2026_11_06'
for name in ['PRE_CAPTURE_SEAL.json','selection-before-capture.json','split-before-any-detector.json','source-manifest.json','independent-acoustic-annotations.json','float32-exact-manifest.json','legal-readme-provenance.json','decode_exact.py','verify.py','original-source/LICENSE']:
 p=newroot/name
 if p.is_file():external.append(dict(project_path=p.relative_to(REPO).as_posix(),sha256=sha(p.read_bytes()),bytes=p.stat().st_size,kind='separately retained new fixture metadata/license; no reserved detector output'))
# A real trace v1 was debug-extended in place for v2; retain the exact inverse reconstruction explicitly.
v1={}
for p in (ROOTS['real-repeat']/'Source').rglob('*'):
 if p.is_file():v1['Source/'+p.relative_to(ROOTS['real-repeat']/'Source').as_posix()]=p.read_bytes()
name='Source/Pitch/PitchDetector.h';s=v1[name].decode();assert s.count('float auditIndependentExcitation=0,auditHarmonicFraction=0,auditPhaseCoherence=0,auditSnr=0;')==1;s=s.replace('float auditIndependentExcitation=0,auditHarmonicFraction=0,auditPhaseCoherence=0,auditSnr=0;','float auditIndependentExcitation=0;');v1[name]=s.encode()
name='Source/Pitch/PitchDetector.cpp';s=v1[name].decode();s=s.replace('auditHarmonicFraction=evidence.harmonicFraction;auditPhaseCoherence=evidence.phaseCoherence;auditSnr=evidence.snrDb;\n        ','');v1[name]=s.encode()
s=(ROOTS['real-repeat']/'trace.cpp').read_text().replace(',confidence,harmonic_fraction,phase_coherence,snr_db\\n','\\n');s=s.replace("<<','<<detector->view.confidence<<','<<detector->auditHarmonicFraction<<','<<detector->auditPhaseCoherence<<','<<detector->auditSnr",'');v1['trace.cpp']=s.encode()
for name,data in v1.items():add('reconstructed/real-trace-v1/'+name,data,'Reversed exact documented v1→v2 debug-only edits; retrospective, not a contemporaneous hash attestation')
jwrite(DEST/'trace-v1-reconstruction.json',dict(scope='Retrospective exact inverse of the recorded v2 debug extension; original v1 beforecompile hashes/binary were not retained',source_sha256={name:sha(data) for name,data in v1.items()},changes=['Remove three v2 evidence fields from PitchDetector.h','Remove v2 evidence assignment before minimumSnr in PitchDetector.cpp','Remove four v2 evidence columns from trace.cpp'],runtime_evidence='v1 and v2 all eight selected target event streams separately equal original baseline558 events; preserved raw files are authoritative'))
# The first mono runner later gained argv/output-name conveniences; preserve that earlier edition explicitly.
runner=(ROOTS['initial-mono']/'run_score.py').read_text()
old="for name in sys.argv[1:] or ['baseline','plain-phase','variance2','variance3']:"
assert runner.count(old)==1
runner=runner.replace(old,"for name in ['baseline','plain-phase','variance2','variance3']:")
old="(base/('experiment-summary-'+('-'.join(sys.argv[1:]) or 'initial')+'.json')).write_text"
assert runner.count(old)==1
runner=runner.replace(old,"(base/'experiment-summary.json').write_text")
add('reconstructed/initial-runner-v1/run_score.py',runner.encode(),'Retrospective inverse of two documented argv/output-name convenience edits; scoring body unchanged')
jwrite(DEST/'initial-runner-reconstruction.json',dict(scope='Retrospective exact inverse of two recorded convenience edits; not a contemporaneous before-run hash',sha256=sha(runner.encode()),changes=['Restore fixed four-variant loop instead of argv selection','Restore experiment-summary.json output name'],scoring_evaluator='Unchanged exact e235 guitar_evaluation_score.py/readiness score.py are preserved separately'))

# Index each complete Source snapshot, including debug overlays, with all bytes restored by the logical map.
sources={}
for name,data in logical_data.items():
 if '/Source/Pitch/' not in name:continue
 prefix=name.split('/Source/Pitch/')[0]+'/Source'
 sources.setdefault(prefix,{})[name[len(prefix)+1:]]=sha(data)
# Canonical Source additionally includes UI/Core files.
sources['canonical/e235/Source']={name[len('Source/'):]:digest for name,digest in canonical.items()}
jwrite(DEST/'source-snapshots.json',dict(schema=1,snapshots=[dict(logical_prefix=k,files_sha256=v,files=len(v),scope='Exact preserved snapshot, including trace-only access/fields where present') for k,v in sorted(sources.items())]))
# Runtime hash/command binding is explicit about what was captured then versus reconstructed now.
bindings=[]
for item in excluded:
 if item['reason']!='ELF runtime executable':continue
 p=Path(item['original_path']);tag='initial-mono' if str(p).startswith(str(ROOTS['initial-mono'])) else 'real-repeat';root=ROOTS[tag];rel=p.relative_to(root);parent=p.parent;source=None;driver=None;attestation='Retrospective replay binding from retained source/driver and actual executed command patterns; not an original beforecompile attestation';recorded=None
 if p.name=='RealPluckCapture':
  source=parent/'source';driver=parent/'capture.cpp';recorded=parent/'measurement-manifest.json';attestation='Original before-compile/measurement manifests retained with source snapshots, command and executable hash'
 elif p.name=='LegacyCapture':source=parent;driver=parent/'legacy_driver.cpp'
 elif p.name=='validation':
  source=ROOTS['initial-mono']/('plain-phase' if parent.name.startswith('plain-phase') else 'baseline');driver=REPO/'Tools/audio_validation.cpp'
 elif p.name=='TraceCapture':
  source=parent;driver=parent/'trace.cpp'
 elif p.name in ('LegacyEvidenceTrace','RawArmTrace','FloorArmTrace'):
  source=ROOTS['initial-mono']/'baseline' if p.name=='LegacyEvidenceTrace' else ROOTS['real-repeat']/('innovation-raw' if p.name=='RawArmTrace' else 'raw-onset-floor');driver=ROOTS['real-repeat']/('legacy_evidence_trace.cpp' if p.name=='LegacyEvidenceTrace' else 'raw_arm_trace.cpp')
 elif p.name=='Capture':
  if (parent/'counter-results.csv').exists():
   if parent.name=='quiet-counter':source=parent.parent
   elif parent.name=='baseline-quiet-counter':source=ROOTS['initial-mono']/'baseline'
   else:source=ROOTS['real-repeat']/parent.name.removesuffix('-quiet-counter')
   driver=ROOTS['real-repeat']/'quiet_repick_counter.cpp'
  else:source=parent;driver=parent/'capture.cpp'
 if source is None or driver is None or not (source/'Source/Pitch').is_dir() or not driver.is_file():raise RuntimeError('Unbound runtime '+str(p))
 source_abs=str(source/'Source');prefix=None
 for original,logical in original_to_logical.items():
  if original.startswith(source_abs+'/Pitch/'):
   prefix=logical.split('/Source/Pitch/')[0]+'/Source';break
 if prefix is None:raise RuntimeError('Missing source logical binding '+str(source))
 driverlogical=original_to_logical.get(str(driver))
 if driverlogical is None:raise RuntimeError('Missing driver logical binding '+str(driver))
 compilefiles=sorted([q for q in (source/'Source/Pitch').glob('*.cpp') if q.name!='MpeOutputRouter.cpp'])
 command=['g++','-std=c++17','-O2','-DNDEBUG','-I',str(source/'Source'),str(driver),*map(str,compilefiles),'-o',str(p)]
 # These explicitly enumerated trace compiles did not include the unused EarlyEstimator translation unit.
 if p.name in ('TraceCapture','LegacyEvidenceTrace','RawArmTrace','FloorArmTrace'):
  order=['PitchDetector.cpp','TimeDomainPursuit.cpp','YinEstimator.cpp','ArticulationTracker.cpp','NoiseConditioner.cpp','PolyphonicTracker.cpp','InputDynamics.cpp'];command=['g++','-std=c++17','-O2','-DNDEBUG','-I',str(source/'Source'),str(driver),*[str(source/'Source/Pitch'/x) for x in order],'-o',str(p)]
 row=dict(executable_logical_path=item['logical_path'],excluded_executable_sha256=item['sha256'],excluded_executable_bytes=item['bytes'],source_logical_prefix=prefix,driver_logical_path=driverlogical,driver_sha256=sha(driver.read_bytes()),replay_compile_command=command,binding_scope=attestation)
 if recorded:
  doc=json.loads(recorded.read_text());row['recorded_measurement_manifest']=original_to_logical[str(recorded)];row['recorded_executable_hash_matches_excluded']=doc.get('executable_sha256')==item['sha256'];row['recorded_compile_command']=doc.get('compile_command')
  if not row['recorded_executable_hash_matches_excluded']:raise RuntimeError('Recorded executable mismatch '+str(p))
 bindings.append(row)
jwrite(DEST/'runtime-bindings.json',dict(collected_at_utc=datetime.datetime.now(datetime.timezone.utc).isoformat(),scope='Bindings are retrospective except where original manifests explicitly exist. No file or attestation is backdated. Compiler/system/JUCE dependency closure is not claimed.',executables=bindings,current_reproduction_compiler=subprocess.check_output(['g++','--version'],text=True).splitlines()[0]))
jwrite(DEST/'external-inputs.json',dict(scope='Canonical metadata references only; no PCM/f32/audio/model payload is copied. Original per-recording audio hashes remain in the unchanged fixture manifests. Prospective reserved detector inputs were never captured here.',files=external))
jwrite(DEST/'exclusions.json',dict(scope='Excluded runtime bytes are recorded only as hashes/sizes; no executable/object/model/audio payload preserved',files=excluded))
jwrite(DEST/'storage-map.json',dict(schema=1,encoding='Lossless deterministic gzip for CSV; all other unique payloads stored raw; identical bytes globally deduplicated',original_roots={k:str(v) for k,v in ROOTS.items()},logical_files=len(entries),unique_payloads=len(blobs),logical_bytes=sum(x['bytes'] for x in entries),stored_payload_bytes=sum(len(v[2]) for v in blobs.values()),entries=entries))
# This script itself is reproduction/provenance for the preservation operation.
shutil.copyfile(Path(__file__),DEST/'preserve_studies.py')
print(json.dumps(dict(destination=str(DEST),logical_files=len(entries),unique_payloads=len(blobs),excluded=len(excluded),source_snapshots=len(sources),logical_bytes=sum(x['bytes'] for x in entries),stored_payload_bytes=sum(len(v[2]) for v in blobs.values())),indent=2))
