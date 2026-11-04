#!/usr/bin/env python3
"""Post-hoc common MIDI28..100 scope; all excluded raw events remain retained."""
from pathlib import Path
import argparse,json,collections
from score import score,quantile
parser=argparse.ArgumentParser();parser.add_argument('configuration',type=Path);parser.add_argument('capture',type=Path);args=parser.parse_args()
config=json.loads(args.configuration.read_text());capture=json.loads((args.capture/'capture.json').read_text());inputs={i['id']:i for i in capture['inputs']};events={};excluded={}
for identity,item in inputs.items():
    original=json.loads((args.capture/item['events_file']).read_text());events[identity]=[e for e in original if 28<=e['note']<=100];excluded[identity]=[e for e in original if not 28<=e['note']<=100]
rows=[]
for entry in config['cases']:
    identity=entry['input_id'];case=entry['case'];item=inputs[identity]
    truth=case.get('truth',case.get('reference_notes',[]))
    if any(not 28<=r.get('note',r.get('anchor'))<=100 for r in truth):raise ValueError('Reference outside pre-existing declared range')
    row=score(case,events[identity],item['processed_frames']);row['dataset']=entry['dataset'];row['excluded_raw_note_ons']=sum(e['on'] for e in excluded[identity]);rows.append(row)
summary=dict(scope='POST HOC shared declared MIDI28..100 scope only; primary all88key results preserved',model=capture['model'],bounds=[28,100],thresholds_changed=False,top_voice_limit=None,excluded_unique_input_note_ons=sum(e['on'] for es in excluded.values() for e in es),datasets={})
for ds in sorted({r['dataset'] for r in rows}):
    summary['datasets'][ds]={}
    for mode in ('mono','poly'):
        group=[r for r in rows if r['dataset']==ds and r['mode']==mode];keys=('expected','matched','missed','false_note_ons','noise_note_ons','octave_errors','same_pitch_retriggers','wrong_before_correct','early_offs','late_offs','hung','excluded_raw_note_ons')
        data={k:sum(r[k] for r in group) for k in keys};latencies=[v for r in group for v in r['latencies_ms']];data.update(cases=len(group),passed_cases=sum(r['passed'] for r in group),latency_ms={k:quantile(latencies,q) for k,q in [('median',.5),('p95',.95),('maximum',1)]});summary['datasets'][ds][mode]=data
(args.capture/'range28-100-summary.json').write_text(json.dumps(summary,indent=2)+'\n');(args.capture/'range28-100-cases.json').write_text(json.dumps(rows,indent=2)+'\n');(args.capture/'range-excluded-raw-events.json').write_text(json.dumps(excluded,indent=2)+'\n');print(json.dumps(summary,indent=2))
