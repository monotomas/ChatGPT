from pathlib import Path
import shutil,subprocess,hashlib,json,datetime,csv
b=Path('/tmp/flysnapper-v4-real-repeat-audit');old=Path('/tmp/flysnapper-v4-mono-audit/baseline');root=Path('/workspace/ChatGPT');driver=Path('/tmp/flysnapper-v4-latency-overlap/legacy_mono_capture.cpp')
summary={}
for name in ['reference-clear-tracked']:
 out=b/name;shutil.copytree(old/'Source',out/'Source',dirs_exist_ok=True)
 f=out/'Source/Pitch/PitchDetector.cpp';s=f.read_text()
 if name in ['phase-raw','articulation-raw']:
  match='?monoPartialPhase.observe(frame.data(),count,float(analysisRate/measuredPeriod),analysisClock):0;';assert s.count(match)==1;s=s.replace(match,'?monoPartialPhase.observe(rawReference.data(),count,float(analysisRate/measuredPeriod),analysisClock):0;')
 if name in ['innovation-raw','articulation-raw']:
  match='innovationEnvelope+=innovationCoefficient*(std::abs(analysisSample-predicted)-innovationEnvelope);';assert s.count(match)==1;s=s.replace(match,'''if(!config.polyphonic)predicted=rawReferenceRing[size_t((writeIndex-whole+windowSize)%windowSize)]*(1-fraction)
                    +rawReferenceRing[size_t((writeIndex-whole-1+windowSize)%windowSize)]*fraction;
                const float innovationSample=config.polyphonic?analysisSample:rawReferenceRing[size_t(writeIndex)];
                innovationEnvelope+=innovationCoefficient*(std::abs(innovationSample-predicted)-innovationEnvelope);''')
 if name=='reference-clear-tracked':
  match='if(reference.valid&&reference.confidence>.95f&&std::abs(reference.pitch-estimate.pitch)<=.5f)estimate=reference;';assert s.count(match)==1;s=s.replace(match,'if(reference.valid&&reference.confidence>.95f&&std::abs(reference.pitch-estimate.pitch)<=.5f){estimate=reference;monoTrackedOnly=false;}')
 f.write_text(s);shutil.copyfile(driver,out/'legacy_driver.cpp');cpp=[p for p in sorted((out/'Source/Pitch').glob('*.cpp')) if p.name!='MpeOutputRouter.cpp'];cmd=['g++','-std=c++17','-O2','-DNDEBUG','-I',str(out/'Source'),str(out/'legacy_driver.cpp'),*map(str,cpp),'-o',str(out/'LegacyCapture')]
 before={p.relative_to(out).as_posix():hashlib.sha256(p.read_bytes()).hexdigest() for p in (out/'Source/Pitch').glob('*') if p.suffix in ('.h','.cpp')}
 with (out/'legacy-compile.txt').open('w') as log:r=subprocess.run(cmd,stdout=log,stderr=subprocess.STDOUT)
 assert r.returncode==0,name
 info={};summary[name]=info
 for dynamics in ['off','default']:
  counts={k:0 for k in ['expected','matched','missed','false_note_ons','octave_errors','wrong_before_correct','early_offs']};differ=[]
  for suite,fixtures in [('public',root/'Tests/AudioFixtures'),('holdout',root/'Tests/AudioFixtures/holdout'),('confirmation',root/'Tests/AudioFixtures/confirmation')]:
   target=out/'legacy107'/dynamics/suite;target.mkdir(parents=True,exist_ok=True)
   with (target/'console.txt').open('w') as log:r=subprocess.run([str(out/'LegacyCapture'),str(fixtures),str(target),'quick',dynamics],stdout=log,stderr=subprocess.STDOUT)
   assert r.returncode==0,(name,suite)
   rows=list(csv.DictReader((target/'audio-cases.csv').open()));base=Path('/tmp/flysnapper-v4-latency-overlap/legacy107/baseline')/dynamics/suite
   new=list(csv.DictReader((target/'audio-events.csv').open()));orig=list(csv.DictReader((base/'audio-events.csv').open()));
   for row in rows:
    for key in counts:counts[key]+=int(row[key])
   if new!=orig:differ.append(suite)
  info[dynamics]={'counts':counts,'changed_event_suites_vs_paired_baseline':differ};print(name,dynamics,info[dynamics],flush=True)
 assert before=={p.relative_to(out).as_posix():hashlib.sha256(p.read_bytes()).hexdigest() for p in (out/'Source/Pitch').glob('*') if p.suffix in ('.h','.cpp')}
 (out/'topology-manifest.json').write_text(json.dumps({'source_sha256':before,'topology':name,'driver_sha256':hashlib.sha256(driver.read_bytes()).hexdigest(),'command':cmd,'results':info,'source_frozen':True,'production_source_changed':False},indent=2)+'\n')
(b/'reference-flag-legacy107-summary.json').write_text(json.dumps(summary,indent=2)+'\n')
