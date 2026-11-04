#!/usr/bin/env python3
"""Explicitly partial, seen-corpus mono diagnostics; never full readiness."""
import argparse
import collections
import hashlib
import importlib.util
import json
from pathlib import Path
import subprocess

ROOT=Path('/workspace/ChatGPT')
def digest(path):return hashlib.sha256(path.read_bytes()).hexdigest()
def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('snapshot',type=Path);p.add_argument('fixtures',type=Path);p.add_argument('output',type=Path)
    a=p.parse_args();source=a.snapshot.resolve();fixtures=a.fixtures.resolve();out=a.output.resolve()
    if out.exists() and any(out.iterdir()):p.error('Refuse existing nonempty output')
    out.mkdir(parents=True,exist_ok=True)
    original=ROOT/'Tools/readiness_capture.cpp';driver=out/'partial-mono-capture.cpp'
    text=original.read_text();needle='const auto fixtures=cases(args[1]);'
    assert text.count(needle)==1
    text=text.replace(needle,'auto fixtures=cases(args[1]); fixtures.erase(std::remove_if(fixtures.begin(),fixtures.end(),[](const auto& item){return item.mode!="mono";}),fixtures.end());')
    driver.write_text(text)
    cpp=[f for f in sorted((source/'Source/Pitch').glob('*.cpp')) if f.name!='MpeOutputRouter.cpp']
    files=cpp+[f for f in sorted((source/'Source/Pitch').glob('*.h')) if f.name!='MpeOutputRouter.h']
    before={f.relative_to(source).as_posix():digest(f) for f in files}
    exe=out/'PartialMonoCapture';command=['g++','-std=c++17','-O2','-DNDEBUG','-I',str(source/'Source'),str(driver),*[str(f) for f in cpp],'-o',str(exe)]
    with (out/'compile.txt').open('w') as log:subprocess.run(command,stdout=log,stderr=subprocess.STDOUT,check=True)
    with (out/'capture-console.txt').open('w') as log:subprocess.run([str(exe),str(fixtures),str(out)],stdout=log,stderr=subprocess.STDOUT,check=True)
    assert before=={f.relative_to(source).as_posix():digest(f) for f in files},'Source changed during diagnostic'
    scorer=ROOT/'Tests/AudioFixtures/readiness/score.py';spec=importlib.util.spec_from_file_location('pristine_score',scorer)
    module=importlib.util.module_from_spec(spec);spec.loader.exec_module(module)
    manifest=json.loads((fixtures/'fixtures.json').read_text());policy=json.loads((fixtures/'policy.json').read_text())
    curves=module.read_curves(fixtures);by_case={c['name']:c for c in manifest['cases'] if c['mode']=='mono'}
    metrics=module.read_csv(out/'capture.csv');events=module.read_csv(out/'events.csv');group=collections.defaultdict(list)
    for e in events:group[e['case'],int(e['rate']),int(e['block'])].append(e)
    assert {m['case'] for m in metrics}==set(by_case)
    rows=[module.evaluate(by_case[m['case']],m,group[m['case'],int(m['rate']),int(m['block'])],curves,policy) for m in metrics]
    summary=dict(scope='PARTIAL MONO-ONLY SEEN-REGRESSION DIAGNOSTIC; not full readiness or physical latency acceptance',
        full_readiness_test=False,original_inventory_cases=len(manifest['cases']),cases_measured=len(rows),passed_cases=sum(r['passed'] for r in rows),
        source_snapshot=str(source),source_sha256=before,fixture_seal=(fixtures/'SEALED_SHA256.txt').read_text().strip(),
        scorer_sha256=digest(scorer),policy_sha256=digest(fixtures/'policy.json'),driver_sha256=digest(driver),original_driver_sha256=digest(original))
    summary['totals']={k:sum(r[k] for r in rows) for k in ('expected','matched','missed','extra_note_ons','noise_note_ons','octave_errors','wrong_before_correct','early_offs','late_offs','hung','overflow')}
    (out/'partial-cases.json').write_text(json.dumps(rows,indent=2)+'\n');(out/'partial-summary.json').write_text(json.dumps(summary,indent=2)+'\n')
    print(json.dumps({k:v for k,v in summary.items() if k not in ('source_sha256',)},indent=2))
if __name__=='__main__':main()
