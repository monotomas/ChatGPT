#!/usr/bin/env python3
from pathlib import Path
import subprocess,importlib.util,json,collections,argparse,hashlib
p=argparse.ArgumentParser();p.add_argument('executable',type=Path);p.add_argument('fixtures',type=Path);p.add_argument('output',type=Path);a=p.parse_args()
root=Path('/workspace/ChatGPT');fixtures=a.fixtures.resolve();out=a.output.resolve();out.mkdir(exist_ok=False);exe=a.executable.resolve()
with (out/'console.txt').open('w') as log:subprocess.run([str(exe),str(fixtures),str(out)],stdout=log,stderr=subprocess.STDOUT,check=True)
spec=importlib.util.spec_from_file_location('readiness_score',root/'Tests/AudioFixtures/readiness/score.py');m=importlib.util.module_from_spec(spec);spec.loader.exec_module(m)
manifest=json.loads((fixtures/'fixtures.json').read_text());policy=json.loads((fixtures/'policy.json').read_text());curves=m.read_curves(fixtures);cases={c['name']:c for c in manifest['cases'] if c['mode']=='mono'};group=collections.defaultdict(list)
for e in m.read_csv(out/'events.csv'):group[e['case'],int(e['rate']),int(e['block'])].append(e)
rows=[m.evaluate(cases[r['case']],r,group[r['case'],int(r['rate']),int(r['block'])],curves,policy) for r in m.read_csv(out/'capture.csv')]
summary={'scope':'PARTIAL MONO seen-control diagnostic, not readiness','cases':len(rows),'passed':sum(r['passed'] for r in rows),'totals':{k:sum(r[k] for r in rows) for k in ('expected','matched','missed','extra_note_ons','early_offs','late_offs','noise_note_ons','hung','overflow')},'failures':[{k:r[k] for k in ('case','matched','missed','extra_note_ons','failures')} for r in rows if not r['passed']],'binary_sha256':hashlib.sha256(exe.read_bytes()).hexdigest(),'fixture_seal':(fixtures/'SEALED_SHA256.txt').read_text().strip()}
(out/'partial-cases.json').write_text(json.dumps(rows,indent=2)+'\n');(out/'partial-summary.json').write_text(json.dumps(summary,indent=2)+'\n');print(json.dumps(summary,indent=2))
