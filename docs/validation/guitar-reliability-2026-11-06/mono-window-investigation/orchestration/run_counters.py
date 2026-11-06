from pathlib import Path
import csv,hashlib,json,shutil,subprocess,datetime
H=Path(__file__).resolve().parent
R=Path('/workspace/ChatGPT')
old=Path('/tmp/flysnapper-native3-quiet-alias-audit/raw-physical-candidate-validation')
for name,p in [('counter_capture.cpp',old/'counter_capture.cpp'),('quiet_capture.cpp',old/'candidate/quiet_capture.cpp')]:shutil.copyfile(p,H/name)
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
for variant in ['baseline','adaptive_held']:
 snap=H/variant;obj=H/(variant+'-counter-objects');obj.mkdir()
 inputs=sorted(p for p in (snap/'Source/Pitch').glob('*') if p.suffix in ('.h','.cpp') and p.name not in ('MpeOutputRouter.h','MpeOutputRouter.cpp'))
 bindings={str(p.relative_to(snap)):sha(p) for p in inputs}
 with (obj/'compile.txt').open('w') as log:
  for p in inputs:
   if p.suffix=='.cpp':subprocess.run(['g++','-std=c++17','-O2','-DNDEBUG','-I',str(snap/'Source'),'-c',str(p),'-o',str(obj/(p.stem+'.o'))],stdout=log,stderr=subprocess.STDOUT,check=True)
  for kind in ['counter','quiet']:
   exe=H/(variant+'-'+kind+'-capture');subprocess.run(['g++','-std=c++17','-O2','-DNDEBUG','-I',str(snap/'Source'),str(H/(kind+'_capture.cpp')),*[str(p) for p in obj.glob('*.o')],'-o',str(exe)],stdout=log,stderr=subprocess.STDOUT,check=True)
   out=H/'counters'/variant/kind;out.mkdir(parents=True)
   command=[str(exe),str(R/'Tests/AudioFixtures/readiness'),str(out)] if kind=='quiet' else [str(exe),str(out)]
   with (out/'console.txt').open('w') as console:subprocess.run(command,stdout=console,stderr=subprocess.STDOUT,check=True)
   manifest={'recorded_at_utc':datetime.datetime.now(datetime.timezone.utc).isoformat(),'variant':variant,'base_commit':'e235c8a098dadce6e0a4b8bd6ca9de789a5d174d','source_sha256':bindings,'driver_sha256':sha(H/(kind+'_capture.cpp')),'binary_sha256':sha(exe),'command':command,'source_changed':bindings!={str(p.relative_to(snap)):sha(p) for p in inputs},'files_sha256':{p.name:sha(p) for p in out.iterdir() if p.is_file()}}
   assert not manifest['source_changed'];(out/'manifest.json').write_text(json.dumps(manifest,indent=2)+'\n')
   print(variant,kind,'completed',flush=True)
summary=[]
for variant in ['baseline','adaptive_held']:
 out=H/'counters'/variant/'counter'
 refs=list(csv.DictReader((out/'references.csv').open()));events=list(csv.DictReader((out/'events.csv').open()));metrics=list(csv.DictReader((out/'capture.csv').open()))
 for metric in metrics:
  name=metric['case'];r=[x for x in refs if x['case']==name];e=[x for x in events if x['case']==name and x['type'] in ('N','F')]
  ons=[x for x in e if x['type']=='N'];offs=[x for x in e if x['type']=='F'];wanted=[int(x['note']) for x in r]
  valid=[int(x['note']) for x in ons]==wanted and [int(x['note']) for x in offs]==wanted and not int(metric['overflow']) and not int(metric['active_at_end'])
  delays=[(int(x['sample'])-int(y['on_sample']))/48 for x,y in zip(ons,r)] if valid else []
  releases=[(int(x['sample'])-int(y['off_sample']))/48 for x,y in zip(offs,r)] if valid else []
  valid=valid and all(0<=x<=80 for x in delays) and all(-10<=x<=80 for x in releases)
  summary.append({'variant':variant,'case':name,'passed':valid,'references':len(r),'ons':[{'note':int(x['note']),'sample':int(x['sample'])} for x in ons],'offs':[{'note':int(x['note']),'sample':int(x['sample'])} for x in offs],'on_delays_ms':delays,'off_delays_ms':releases})
 out=H/'counters'/variant/'quiet';events=list(csv.DictReader((out/'events.csv').open()));metrics=list(csv.DictReader((out/'capture.csv').open()))
 for metric in metrics:
  key=tuple(metric[k] for k in ('case','mono_bend','dynamics_enabled'));rows=[x for x in events if tuple(x[k] for k in ('case','mono_bend','dynamics_enabled'))==key and x['type'] in ('N','F')]
  valid=len(rows)==2 and rows[0]['type']=='N' and rows[1]['type']=='F' and all(int(x['note'])==int(metric['note']) for x in rows) and int(rows[1]['sample'])>=73920 and int(rows[1]['sample'])<=78240 and not int(metric['active_at_end']) and not int(metric['overflow'])
  summary.append({'variant':variant,'quiet_case':key,'passed':valid,'events':rows})
(H/'counter-summary.json').write_text(json.dumps(summary,indent=2)+'\n')
for variant in ['baseline','adaptive_held']:
 x=[r for r in summary if r['variant']==variant];print(variant,'passed',sum(r['passed'] for r in x),'of',len(x),flush=True)
