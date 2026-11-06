from pathlib import Path
import csv,datetime,hashlib,json,subprocess
H=Path(__file__).resolve().parent
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
for name in ['ablation_probe','counter_probe']:
 src=(H/(name+'-timed.cpp')).read_text()
 old=' auto det=std::make_unique<flysnapper::pitch::PitchDetector>();';assert old in src;src=src.replace(old,'')
 old=' det->prepare(sr);';assert old in src;src=src.replace(old,' auto det=std::make_unique<flysnapper::pitch::PitchDetector>();det->prepare(sr);')
 (H/(name+'-cold-timed.cpp')).write_text(src)
for variant in ['uncertainty-f0','observed-f0','base']:
 snap=H/variant;inputs=sorted(p for p in (snap/'Source').rglob('*') if p.is_file());before={p.relative_to(snap).as_posix():sha(p) for p in inputs};objects=sorted((snap/'objects').glob('*.o'))
 expected=json.loads((snap/'source-inventory.json').read_text())['files_sha256'];assert before==expected
 for suite,stem in [(330,'ablation_probe'),(132,'counter_probe')]:
  driver=H/(stem+'-cold-timed.cpp');exe=snap/('capture'+str(suite)+'-cold')
  with (snap/('cold'+str(suite)+'-link.txt')).open('w') as log:subprocess.run(['g++','-std=c++17','-O2','-DNDEBUG','-I',str(snap/'Source'),str(driver),*[str(p) for p in objects],'-o',str(exe)],stdout=log,stderr=subprocess.STDOUT,check=True)
  out=snap/('cold'+str(suite));out.mkdir()
  with (out/'events.csv').open('w') as stdout,(out/'timing.csv').open('w') as stderr:result=subprocess.run([str(exe)],stdout=stdout,stderr=stderr)
  assert before=={p.relative_to(snap).as_posix():sha(p) for p in inputs}
  rows=list(csv.DictReader((out/'timing.csv').open()));assert len(rows)==suite
  manifest={'recorded_at_utc':datetime.datetime.now(datetime.timezone.utc).isoformat(),'variant':variant,'base_git_commit':'e235c8a098dadce6e0a4b8bd6ca9de789a5d174d','actual_source_snapshot':str(snap),'files_sha256':before,'source_changed':False,'source_is_private_research_not_root_current_commit':True,'driver_sha256':sha(driver),'binary_sha256':sha(exe),'linked_objects_sha256':{p.name:sha(p) for p in objects},'runner_exit_code':result.returncode,'settings':'48000Hz64samples; static poly; explicit dynamics off/default rows','fresh_detector_instance_per_configuration':True,'cache_reuse_between_configurations':False,'prior_warm_sequence':'../runs'+str(suite),'frequency_evidence':'original fixed MIDI-frequency basis' if variant=='base' else 'independent cleaned pre-dynamics frequency peaks, processed fit/admission unchanged; '+('unshared per-harmonic consensus with physical dispersion uncertainty' if variant=='uncertainty-f0' else 'magnitude-weighted peak/h median'),'physical_latency_measured':False,'reserved_audio_used':False,'capture_configurations':len(rows),'wall_max_callback_us':max(float(r['cpu_max_us']) for r in rows),'wall_deadline_misses':sum(int(r['deadline_misses']) for r in rows),'output_sha256':{p.name:sha(p) for p in out.iterdir() if p.is_file()}}
  (out/'measurement-manifest.json').write_text(json.dumps(manifest,indent=2)+'\n')
  print(variant,suite,'independent cold capture complete',flush=True)
