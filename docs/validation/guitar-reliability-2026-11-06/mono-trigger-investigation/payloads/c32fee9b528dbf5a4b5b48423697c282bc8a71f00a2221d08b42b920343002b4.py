from pathlib import Path
import subprocess,csv,json,hashlib
b=Path('/tmp/flysnapper-v4-real-repeat-audit');root=Path('/workspace/ChatGPT');summary={}
for name in ['raw-onset-floor']:
 src=b/name;cpp=[p for p in sorted((src/'Source/Pitch').glob('*.cpp')) if p.name!='MpeOutputRouter.cpp'];counter=src/'quiet-counter';counter.mkdir(exist_ok=True)
 sources={p.relative_to(src).as_posix():hashlib.sha256(p.read_bytes()).hexdigest() for p in (src/'Source/Pitch').glob('*') if p.suffix in('.cpp','.h')}
 cmd=['g++','-std=c++17','-O2','-DNDEBUG','-I',str(src/'Source'),str(b/'quiet_repick_counter.cpp'),*map(str,cpp),'-o',str(counter/'Capture')]
 with (counter/'compile.txt').open('w') as log:r=subprocess.run(cmd,stdout=log,stderr=subprocess.STDOUT)
 assert r.returncode==0
 with (counter/'run.txt').open('w') as log:r=subprocess.run([str(counter/'Capture'),str(counter)],stdout=log,stderr=subprocess.STDOUT)
 assert r.returncode==0
 rows=list(csv.DictReader((counter/'counter-results.csv').open()));good=sum(int(r['note_ons'])==2 and int(r['correct'])==2 and int(r['note_offs'])==2 and r['active']=='0' and int(r['second_decision'])>=21600 for r in rows)
 info={'quiet_postdrop_good':good,'quiet_total':len(rows),'legacy107':{}};summary[name]=info;print(name,'quiet',good,'/',len(rows),flush=True)
 cmd=['g++','-std=c++17','-O2','-DNDEBUG','-I',str(src/'Source'),str(src/'legacy_driver.cpp'),*map(str,cpp),'-o',str(src/'LegacyCapture')]
 with (src/'legacy-compile.txt').open('w') as log:r=subprocess.run(cmd,stdout=log,stderr=subprocess.STDOUT)
 assert r.returncode==0
 for dynamics in ['off','default']:
  counts={k:0 for k in ['expected','matched','missed','false_note_ons','octave_errors','wrong_before_correct','early_offs']}
  for suite,fixtures in [('public',root/'Tests/AudioFixtures'),('holdout',root/'Tests/AudioFixtures/holdout'),('confirmation',root/'Tests/AudioFixtures/confirmation')]:
   out=src/'legacy107'/dynamics/suite;out.mkdir(parents=True,exist_ok=True)
   with (out/'console.txt').open('w') as log:r=subprocess.run([str(src/'LegacyCapture'),str(fixtures),str(out),'quick',dynamics],stdout=log,stderr=subprocess.STDOUT)
   assert r.returncode==0
   for row in csv.DictReader((out/'audio-cases.csv').open()):
    for key in counts:counts[key]+=int(row[key])
  info['legacy107'][dynamics]=counts;print(name,dynamics,counts,flush=True)
 assert sources=={p.relative_to(src).as_posix():hashlib.sha256(p.read_bytes()).hexdigest() for p in (src/'Source/Pitch').glob('*') if p.suffix in('.cpp','.h')}
 (src/'normalization-source-proof.json').write_text(json.dumps({'source_sha256':sources,'counter_good':good,'counter_total':len(rows),'legacy107':info['legacy107'],'source_unchanged':True},indent=2)+'\n')
(b/'normalization-floor-summary.json').write_text(json.dumps(summary,indent=2)+'\n')
