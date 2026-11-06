from pathlib import Path
import csv,datetime,hashlib,json,subprocess
H=Path(__file__).resolve().parent;R=Path('/workspace/ChatGPT')
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
driver=H/'legacy_mono_capture.cpp';original=R/'Tools/audio_validation.cpp'
s=original.read_text().replace('namespace {\nstruct Truth','namespace {\nbool integratedDynamicsEnabled=false;\nstruct Truth')
s=s.replace('settings.polyphonic=fixture.mode=="poly";detector.configure(settings);','settings.polyphonic=fixture.mode=="poly";settings.inputDynamics.enabled=integratedDynamicsEnabled;detector.configure(settings);')
s=s.replace('args.size()>4','args.size()>5').replace('auto fixtures=readFixtures(args[1]);','integratedDynamicsEnabled=args.size()>=5&&args[4]=="default";auto fixtures=readFixtures(args[1]);')
s=s.replace('for(const auto& f:fixtures){auto wave=readWave(f.file);','for(const auto& f:fixtures){if(f.mode!="mono")continue;auto wave=readWave(f.file);')
driver.write_text(s)
summaries=[]
for variant in ['baseline','local_secondary128']:
 snap=H/variant;obj=H/(variant+'-counter-objects');exe=H/(variant+'-legacy-mono')
 sources=sorted(p for p in (snap/'Source/Pitch').glob('*') if p.suffix in ('.h','.cpp') and p.name not in ('MpeOutputRouter.h','MpeOutputRouter.cpp'))
 bindings={str(p.relative_to(snap)):sha(p) for p in sources}
 with (H/(variant+'-legacy-link.txt')).open('w') as log:subprocess.run(['g++','-std=c++17','-O2','-DNDEBUG','-I',str(snap/'Source'),str(driver),*[str(p) for p in obj.glob('*.o')],'-o',str(exe)],stdout=log,stderr=subprocess.STDOUT,check=True)
 for profile in ['off','default']:
  for group,fixtures in [('public',R/'Tests/AudioFixtures'),('holdout',R/'Tests/AudioFixtures/holdout'),('confirmation',R/'Tests/AudioFixtures/confirmation')]:
   out=H/'legacy107'/variant/profile/group;out.mkdir(parents=True,exist_ok=False)
   command=[str(exe),str(fixtures),str(out),'quick',profile]
   with (out/'console.txt').open('w') as log:subprocess.run(command,stdout=log,stderr=subprocess.STDOUT,check=True)
   rows=list(csv.DictReader((out/'audio-cases.csv').open()))
   counts={k:sum(int(row[k]) for row in rows) for k in ['expected','matched','missed','false_note_ons','octave_errors','wrong_before_correct','early_offs','hung','overflow','deadline_misses']}
   summary={'variant':variant,'profile':profile,'group':group,'cases':len(rows),'counts':counts}
   summaries.append(summary)
   manifest={'recorded_at_utc':datetime.datetime.now(datetime.timezone.utc).isoformat(),'variant':variant,'base_commit':'e235c8a098dadce6e0a4b8bd6ca9de789a5d174d','source_sha256':bindings,'driver_sha256':sha(driver),'original_driver_sha256':sha(original),'driver_changes':'Select all mono cases; explicitly switch integrated dynamics off/default; unchanged independent matching, timing, waveform, controls and numerical tolerances','binary_sha256':sha(exe),'command':command,'settings':'plain mono, autoInput=true, integrated dynamics '+profile,'fixture_metadata_sha256':{n:sha(fixtures/n) for n in ('fixtures.json','fixtures.tsv')},'source_changed':bindings!={str(p.relative_to(snap)):sha(p) for p in sources},'summary':summary,'files_sha256':{p.name:sha(p) for p in out.iterdir() if p.is_file()}}
   assert not manifest['source_changed'];(out/'manifest.json').write_text(json.dumps(manifest,indent=2)+'\n')
   print(summary,flush=True)
(H/'legacy107-summary.json').write_text(json.dumps(summaries,indent=2)+'\n')
