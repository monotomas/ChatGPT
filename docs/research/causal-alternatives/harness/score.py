#!/usr/bin/env python3
"""Score actual-emission integer note events without delay fitting/backdating."""
from pathlib import Path
import argparse,json,math,collections,hashlib,csv

def quantile(values,q):
    return sorted(values)[math.ceil(q*(len(values)-1))] if values else None

def score(case,events,end):
    truth=[dict(note=r.get('note',r.get('anchor')),on=r['on_sample'],off=r['off_sample']) for r in case.get('truth',case.get('reference_notes',[]))]
    intervals=[];held={};orphans=0
    for e in events:
        note=e['note'];at=e['sample']
        if e['on']:
            if note in held:raise ValueError('Duplicate Note On without off')
            item=dict(note=note,on=at,off=end,matched=False,hung=True);intervals.append(item);held[note]=item
        elif note in held:
            item=held.pop(note);item['off']=at;item['hung']=False
        else:orphans+=1
    chosen=[];latency=[];release=[];early=late=wrong=0
    for ref in truth:
        selected=None;minimum=min(960,ref['off']-ref['on'])
        for item in intervals:
            if not item['matched'] and item['note']==ref['note'] and ref['on']-2<=item['on']<=ref['off']+2 and min(item['off'],ref['off'])-max(item['on'],ref['on'])>=minimum:
                selected=item;item['matched']=True;break
        chosen.append(selected)
        if selected is not None:
            latency.append((selected['on']-ref['on'])/48);release.append((selected['off']-ref['off'])/48)
            if selected['off']<ref['off']-480:early+=1
            if selected['off']>ref['off']+3840:late+=1
        correct=selected['on'] if selected else ref['off']
        if any(ref['on']<=item['on']<correct and not any(t['note']==item['note'] and t['on']-2<=item['on']<=t['off']+2 for t in truth) for item in intervals):wrong+=1
    false=[item for item in intervals if not item['matched']]
    octaves=sum(any(t['on']<=i['on']<=t['off'] and abs(t['note']-i['note']) in (12,24,36) for t in truth) for i in false)
    same=sum(any(t['on']<=i['on']<=t['off'] and t['note']==i['note'] for t in truth) for i in false)
    matched=sum(i is not None for i in chosen)
    return dict(case=case['name'],family=case['family'],mode=case['mode'],expected=len(truth),matched=matched,missed=len(truth)-matched,false_note_ons=len(false),noise_note_ons=len(intervals) if not truth else 0,octave_errors=octaves,same_pitch_retriggers=same,wrong_before_correct=wrong,early_offs=early,late_offs=late,hung=len(held),orphan_offs=orphans,latencies_ms=latency,releases_ms=release,passed=matched==len(truth) and not(false or early or late or held or orphans))

def main():
    parser=argparse.ArgumentParser();parser.add_argument('configuration',type=Path);parser.add_argument('capture',type=Path);args=parser.parse_args()
    config=json.loads(args.configuration.read_text());capture=json.loads((args.capture/'capture.json').read_text());byid={row['id']:row for row in capture['inputs']}
    rows=[]
    for entry in config['cases']:
        item=byid[entry['input_id']];events=json.loads((args.capture/item['events_file']).read_text());row=score(entry['case'],events,item['processed_frames']);row['dataset']=entry['dataset'];rows.append(row)
    summary={'scope':'Seen-regression integer-note admission only; no continuous bends, MPE, DAW or musical-readiness claim','model':capture['model'],'block':capture['block'],'case_count':len(rows),'unique_audio_inputs':len({entry['input_id'] for entry in config['cases']}),'datasets':{}}
    for dataset in sorted({r['dataset'] for r in rows}):
        modes={}
        for mode in ('mono','poly'):
            group=[r for r in rows if r['dataset']==dataset and r['mode']==mode]
            keys=('expected','matched','missed','false_note_ons','noise_note_ons','octave_errors','same_pitch_retriggers','wrong_before_correct','early_offs','late_offs','hung','orphan_offs')
            totals={key:sum(r[key] for r in group) for key in keys};values=[v for r in group for v in r['latencies_ms']]
            totals.update(cases=len(group),passed_cases=sum(r['passed'] for r in group),latency_ms={key:quantile(values,q) for key,q in [('median',.5),('p95',.95),('maximum',1)]});modes[mode]=totals
        summary['datasets'][dataset]=modes
    # Re-scoring retained events does not require the omitted third-party
    # inference kernel. Report absence explicitly instead of implying that a
    # recorded hash proves those bytes are present or were executed now.
    paths=(Path(__file__),Path(__file__).with_name('run.mjs'))
    summary['source_sha256']={p.name:hashlib.sha256(p.read_bytes()).hexdigest() for p in paths}
    kernel=Path(__file__).with_name('incport.mjs')
    summary['external_kernel_present_for_rescoring']=kernel.is_file()
    if kernel.is_file():summary['source_sha256'][kernel.name]=hashlib.sha256(kernel.read_bytes()).hexdigest()
    summary['rescoring_does_not_execute_inference']=True
    (args.capture/'scored-cases.json').write_text(json.dumps(rows,indent=2)+'\n');(args.capture/'scoring-summary.json').write_text(json.dumps(summary,indent=2)+'\n');print(json.dumps(summary,indent=2))
if __name__=='__main__':main()
