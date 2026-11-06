from pathlib import Path
import subprocess,json,hashlib,datetime,csv
H=Path(__file__).resolve().parent
O=Path('/tmp/flysnapper-v4-poly-f0')
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
source={p.relative_to(H).as_posix():sha(p) for p in sorted((H/'Source').rglob('*')) if p.is_file()}
(H/'source-inventory.json').write_text(json.dumps({'base':'../flysnapper-v4-poly-f0/uncertainty-f0/Source','source_is_private_research_not_production':True,'files_sha256':source},indent=2)+'\n')
obj=H/'objects';obj.mkdir(exist_ok=True)
with (H/'compile.txt').open('w') as log:
 for p in sorted((H/'Source/Pitch').glob('*.cpp')):
  if p.name=='MpeOutputRouter.cpp':continue
  subprocess.run(['g++','-std=c++17','-O2','-DNDEBUG','-I',str(H/'Source'),'-c',str(p),'-o',str(obj/(p.stem+'.o'))],stdout=log,stderr=subprocess.STDOUT,check=True)
 for suite,stem in [(330,'ablation_probe'),(132,'counter_probe')]:
  for cold in [True,False]:
   label=('cold' if cold else 'warm')+str(suite);driver=O/(stem+('-cold-timed.cpp' if cold else '-timed.cpp'));exe=H/('capture-'+label)
   subprocess.run(['g++','-std=c++17','-O2','-DNDEBUG','-I',str(H/'Source'),str(driver),*[str(p) for p in sorted(obj.glob('*.o'))],'-o',str(exe)],stdout=log,stderr=subprocess.STDOUT,check=True)
print('all four captures compiled',flush=True)
score_source=(O/'score_frequency_experiment.py').read_text().split('output={}')[0];scope={'__file__':str(O/'score_frequency_experiment.py')};exec(compile(score_source,str(O/'score_frequency_experiment.py'),'exec'),scope);score=scope['score']
for suite in [330,132]:
 for cold in [True,False]:
  label=('cold' if cold else 'warm')+str(suite);out=H/label;out.mkdir();exe=H/('capture-'+label);driver=O/(('ablation_probe' if suite==330 else 'counter_probe')+('-cold-timed.cpp' if cold else '-timed.cpp'))
  with (out/'events.csv').open('w') as stdout,(out/'timing.csv').open('w') as stderr:subprocess.run([str(exe)],stdout=stdout,stderr=stderr,check=True)
  assert source=={p.relative_to(H).as_posix():sha(p) for p in sorted((H/'Source').rglob('*')) if p.is_file()}
  rows=list(csv.DictReader((out/'timing.csv').open()));assert len(rows)==suite
  manifest={'recorded_at_utc':datetime.datetime.now(datetime.timezone.utc).isoformat(),'variant':'pure-current-evidence-nominal-fine-lifecycle','files_sha256':source,'driver_sha256':sha(driver),'binary_sha256':sha(exe),'fresh_detector_per_configuration':cold,'same_rate_prepare_reset_between_configurations':not cold,'no_real_or_reserved_audio_used':True,'output_sha256':{p.name:sha(p) for p in sorted(out.iterdir()) if p.is_file()},'wall_deadline_misses':sum(int(r['deadline_misses']) for r in rows),'wall_max_callback_us':max(float(r['cpu_max_us']) for r in rows)}
  (out/'manifest.json').write_text(json.dumps(manifest,indent=2)+'\n');result=score(out/'events.csv',suite);(H/(label+'-scores.json')).write_text(json.dumps(result,indent=2)+'\n');print(label,json.dumps(result['totals']['total']),flush=True)
 parity=(H/('cold'+str(suite))/'events.csv').read_bytes()==(H/('warm'+str(suite))/'events.csv').read_bytes();print('warm/cold event parity',suite,parity,flush=True);assert parity
(H/'parity.json').write_text(json.dumps({'cold_warm330_events_equal':True,'cold_warm132_events_equal':True},indent=2)+'\n')
