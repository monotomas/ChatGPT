#!/usr/bin/env python3
"""Private paired v4 latency investigation; disclosed regressions, not acceptance."""
from pathlib import Path
import csv,hashlib,importlib.util,json,subprocess,sys,datetime
ROOT=Path('/workspace/ChatGPT')
HERE=Path(__file__).resolve().parent
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def loadmod(name,p):
    spec=importlib.util.spec_from_file_location(name,p);m=importlib.util.module_from_spec(spec);spec.loader.exec_module(m);return m
score=loadmod('v4_guitar_score',ROOT/'Tools/guitar_evaluation_score.py')
verify=loadmod('v4_readiness_verify',ROOT/'Tests/AudioFixtures/readiness/verify.py')
policy=json.loads(score.POLICY.read_text())
driver=HERE/'capture_mono.cpp'
s=(ROOT/'Tools/guitar_evaluation_capture.cpp').read_text()
s=s.replace('args.size()<=5','args.size()<=6')
s=s.replace('const bool full=args.size()>=4','const bool plain=args.size()>=6&&args[5]=="plain";const bool full=args.size()>=4')
s=s.replace('const auto original=wave(item.file);','if(item.mode!="mono")continue;const auto original=wave(item.file);')
s=s.replace('expression(config,item.bend);','expression(config,plain?false:item.bend);')
s=s.replace('native_integrated_dynamics\\n','native_integrated_dynamics,mono_bend_enabled\\n')
s=s.replace('HasIntegratedDynamics<flysnapper::pitch::Settings>::value<<\'\\n\'','HasIntegratedDynamics<flysnapper::pitch::Settings>::value<<\',\'<<config.monoBend<<\'\\n\'')
driver.write_text(s)
fixture_roots={'controlled':ROOT/'Tests/AudioFixtures/readiness','recorded-hammers':ROOT/'Tests/AudioFixtures/readiness/recorded_hammers','recorded-noise':ROOT/'Tests/AudioFixtures/readiness/recorded_noise'}
for folder in fixture_roots.values():verify.verify(folder)
scoring_hash={str(p.relative_to(ROOT)):sha(p) for p in (ROOT/'Tools/guitar_evaluation_score.py',ROOT/'Tests/AudioFixtures/readiness/score.py',score.POLICY)}
for variant in ['adaptive_held']:
    snap=HERE/variant
    files=sorted(p for p in (snap/'Source/Pitch').glob('*') if p.suffix in ('.h','.cpp') and p.name not in ('MpeOutputRouter.h','MpeOutputRouter.cpp'))
    source_hash={str(p.relative_to(snap)):sha(p) for p in files}
    exe=HERE/(variant+'-mono-capture')
    cmd=['g++','-std=c++17','-O2','-DNDEBUG','-I',str(snap/'Source'),str(driver),*[str(p) for p in files if p.suffix=='.cpp'],'-o',str(exe)]
    with (HERE/(variant+'-mono-compile.txt')).open('w') as log:subprocess.run(cmd,stdout=log,stderr=subprocess.STDOUT,check=True)
    for mode in ['plain','bend']:
        for group,fixtures in fixture_roots.items():
            out=HERE/'mono-paired'/variant/mode/group;out.mkdir(parents=True,exist_ok=False)
            cmd=[str(exe),str(fixtures),str(out),'quick','default',mode]
            with (out/'capture-console.txt').open('w') as log:subprocess.run(cmd,stdout=log,stderr=subprocess.STDOUT,check=True)
            cases={c['name']:c for c in json.loads((fixtures/'fixtures.json').read_text())['cases']}
            metrics=list(csv.DictReader((out/'capture.csv').open()));events=list(csv.DictReader((out/'events.csv').open()));curves=score.original.read_curves(fixtures)
            by={};rows=[]
            for event in events:by.setdefault((event['case'],event['rate'],event['block']),[]).append(event)
            assert {m['case'] for m in metrics}=={n for n,c in cases.items() if c['mode']=='mono'}
            for metric in metrics:
                assert metric['mono_bend_enabled']==('0' if mode=='plain' else '1')
                key=(metric['case'],metric['rate'],metric['block'])
                row=score.evaluate(cases[key[0]],metric,by.get(key,[]),curves,policy)
                row['capture_mono_bend_enabled']=mode=='bend'
                # Keep original truth/failures. Plain quantized bend cases are
                # diagnostics of a different output requirement, never passes.
                row['plain_quantized_bend_truth_mismatch']=mode=='plain' and cases[key[0]]['family']=='continuous-bend'
                rows.append(row)
            summary=score.summarize(rows,[])
            summary['scope']='Mono-only paired disclosed-regression latency experiment; default integrated dynamics; '+mode+' output. Plain continuous-bend cases retain historical expression truth and are diagnostics, not acceptance.'
            summary['mono_bend_enabled']=mode=='bend'
            (out/'cases.json').write_text(json.dumps(rows,indent=2)+'\n');(out/'summary.json').write_text(json.dumps(summary,indent=2)+'\n')
            manifest={'recorded_at_utc':datetime.datetime.now(datetime.timezone.utc).isoformat(),'source_snapshot':str(snap),'base_commit':'e235c8a098dadce6e0a4b8bd6ca9de789a5d174d','variant':variant,'driver_sha256':sha(driver),'scoring_sha256':scoring_hash,'source_sha256':source_hash,'binary_sha256':sha(exe),'command':cmd,'mono_bend_enabled':mode=='bend','frontend':'default integrated input dynamics','selection':'all mono cases from the unchanged sealed fixture root','fixture_root':str(fixtures),'fixture_seal':(fixtures/'SEALED_SHA256.txt').read_text().strip(),'source_changed':source_hash!={str(p.relative_to(snap)):sha(p) for p in files},'files_sha256':{p.name:sha(p) for p in out.iterdir() if p.is_file()}}
            assert not manifest['source_changed']
            (out/'manifest.json').write_text(json.dumps(manifest,indent=2)+'\n')
            print(variant,mode,group,summary['passed_runs'],summary['runs'],summary['counts'],flush=True)
