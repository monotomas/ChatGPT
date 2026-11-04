#!/usr/bin/env python3
from pathlib import Path
import csv,json,collections,argparse,importlib.util
p=argparse.ArgumentParser();p.add_argument('fixtures',type=Path);p.add_argument('events',type=Path);p.add_argument('capture',type=Path);p.add_argument('out',type=Path);a=p.parse_args()
module=Path('/workspace/ChatGPT/Tests/AudioFixtures/readiness/score.py');spec=importlib.util.spec_from_file_location('score',module);m=importlib.util.module_from_spec(spec);spec.loader.exec_module(m)
manifest=json.loads((a.fixtures/'fixtures.json').read_text());policy=json.loads((a.fixtures/'policy.json').read_text());cases={c['name']:c for c in manifest['cases'] if c['mode']=='mono'};groups=collections.defaultdict(list)
for row in csv.DictReader(a.events.open()):
 if row['case'] in cases:groups[row['case'],int(row['rate']),int(row['block'])].append(row)
rows=[]
for metric in csv.DictReader(a.capture.open()):
 if metric['case'] not in cases:continue
 case=cases[metric['case']];rate=int(metric['rate']);raw=groups[case['name'],rate,int(metric['block'])];output,orphan,overlap=m.intervals(raw);refs=[]
 for note in case['reference_notes']:
  r=dict(note);r['on']=m.nearest_sample(r['on_sample']*rate/48000);r['off']=m.nearest_sample(r['off_sample']*rate/48000);refs.append(r)
 for ref in refs:
  duration=1000*(ref['off']-ref['on'])/rate;useful=m.nearest_sample(policy['scoring']['minimum_useful_ms_short_note' if duration<policy['scoring']['short_note_duration_ms_max'] else 'minimum_useful_ms_long_note']*rate/1000)
  candidates=[i for i in output if not i['used'] and i['anchor']==ref['anchor'] and i['on']>=ref['on']-2 and i['on']<ref['off'] and min(i['off'] if i['off'] is not None else int(metric['frames']),ref['off'])-max(i['on'],ref['on'])>=useful]
  if candidates:min(candidates,key=lambda i:(abs(i['on']-ref['on']),i['on']))['used']=True
 for i,out in enumerate(output):
  if out['used']:continue
  active=[r for r in refs if r['on']<=out['on']<r['off']];same=[r for r in active if r['anchor']==out['anchor']];prior=output[i-1] if i else None;gap=None if prior is None or prior['off'] is None else out['on']-prior['off'];correctBefore=any(o['used'] and o['on']<out['on'] for o in output)
  category='noise_only' if not refs else ('same_anchor_direct_retrigger' if same and gap==0 else 'same_anchor_after_gap' if same and gap is not None and gap>0 else 'same_anchor_other' if same else 'wrong_pitch_during_truth' if active else 'outside_truth')
  rows.append(dict(case=case['name'],on_sample=out['on'],off_sample=out['off'],note=out['anchor'],truth_anchors=[r['anchor'] for r in active],category=category,preceding_note=prior['anchor'] if prior else None,preceding_off_sample=prior['off'] if prior else None,gap_samples=gap,gap_ms=None if gap is None else gap*1000/rate,after_matched_correct_on=correctBefore))
counts=dict(collections.Counter(r['category'] for r in rows));gaps=[r['gap_ms'] for r in rows if r['category']=='same_anchor_after_gap'];summary=dict(scope='Disjoint raw extra interval audit of unchanged readiness matching; direct same-time Off/On distinguished from release/reacquisition gaps',total_extras=len(rows),categories=counts,wrong_pitch_after_correct=sum(r['category']=='wrong_pitch_during_truth' and r['after_matched_correct_on'] for r in rows),same_anchor_gap_ms_range=[min(gaps),max(gaps)] if gaps else None,rows=rows)
a.out.write_text(json.dumps(summary,indent=2)+'\n');print(json.dumps({k:v for k,v in summary.items() if k!='rows'},indent=2))
