#!/usr/bin/env python3
"""Independently recompute a frozen evaluator report from raw events/annotations.
SPDX-License-Identifier: AGPL-3.0-or-later
Does not execute or tune the detector, and never edits fixtures/original reports.
"""
from pathlib import Path
import argparse,collections,csv,hashlib,json,math

def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def nearest(v):return math.floor(v+.5)
def quantile(v,q):
    if not v:return None
    ordered=sorted(v);return ordered[math.ceil(q*(len(v)-1))]
def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--fixtures',type=Path,required=True);parser.add_argument('--report',type=Path,required=True)
    parser.add_argument('--evaluation-role',choices=['development','first-blind','seen-regression'],help='Explicit role for legacy reports without role metadata')
    args=parser.parse_args();folder=args.report;manifest_path=folder/'measurement-manifest.json';manifest=json.loads(manifest_path.read_text());fixture_path=args.fixtures/'fixtures.json'
    if sha(fixture_path)!=manifest['fixtures_sha256']:raise ValueError('Frozen fixture manifest differs from recorded run')
    fixture=json.loads(fixture_path.read_text());cases={c['name']:c for c in fixture['cases']};records={r['file']:r for r in fixture['records']}
    rows=list(csv.DictReader((folder/'audio-cases.csv').open()));event_rows=list(csv.DictReader((folder/'audio-events.csv').open()));events=collections.defaultdict(list)
    keys=[(r['case'],int(r['rate']),int(r['block'])) for r in rows]
    configurations=sorted({(rate,block) for _,rate,block in keys})
    expected_keys={(name,rate,block) for name in cases for rate,block in configurations}
    if not configurations or len(keys)!=len(set(keys)) or set(keys)!=expected_keys:raise ValueError('Report does not contain each annotated case exactly once per recorded configuration')
    rates=sorted({rate for rate,_ in configurations})
    blocks_by_rate={rate:sorted(block for r,block in configurations if r==rate) for rate in rates}
    reference_block={rate:64 if 64 in blocks_by_rate[rate] else blocks_by_rate[rate][0] for rate in rates}
    for e in event_rows:
        key=(e['case'],int(e['rate']),int(e['block']))
        if key not in expected_keys:raise ValueError('Raw event does not belong to a recorded case/configuration')
        group=events[key]
        if group and int(e['sample'])<int(group[-1]['sample']):raise ValueError('Raw events are not in causal sample order')
        group.append(e)
    differences=[];recomputed={};false_details=[];matched_truth_notes={};matched_truth_latency={}
    counts=['expected','matched','missed','false_note_ons','octave_errors','wrong_before_correct','early_offs','hung','overflow']
    for row in rows:
        name=row['case'];rate=int(row['rate']);block=int(row['block']);key=(name,rate,block);record=records[cases[name]['file']];base=record['sample_rate'];end=nearest(record['frames']*rate/base)
        truth=[{'note':t['note'],'on':nearest(t['on_sample']*rate/base),'off':nearest(t['off_sample']*rate/base)} for t in cases[name]['truth']]
        intervals=[];held=collections.defaultdict(collections.deque)
        for e in events[key]:
            note=int(e['note']);sample=int(e['sample'])
            if int(e['on']):intervals.append({'note':note,'on':sample,'off':end,'matched':False});held[note].append(len(intervals)-1)
            elif held[note]:intervals[held[note].popleft()]['off']=sample
        chosen=[];latency=[];release=[];early=0
        for t in truth:
            minimum=min(nearest(.020*rate),t['off']-t['on']);selected=None
            for j,i in enumerate(intervals):
                overlap=min(i['off'],t['off'])-max(i['on'],t['on'])
                if not i['matched'] and i['note']==t['note'] and t['on']-2<=i['on']<=t['off']+2 and overlap>=minimum:
                    selected=j;i['matched']=True;break
            chosen.append(selected)
            if selected is not None:
                i=intervals[selected];latency.append((i['on']-t['on'])*1000/rate)
                if i['off']<t['off']-int(.01*rate):early+=1
                else:release.append((i['off']-t['off'])*1000/rate)
        wrong=0
        for t,selected in zip(truth,chosen):
            correct=truth and (intervals[selected]['on'] if selected is not None else t['off'])
            for i in intervals:
                if t['on']<=i['on']<correct and not any(o['note']==i['note'] and o['on']-2<=i['on']<=o['off']+2 for o in truth):wrong+=1;break
        false=[i for i in intervals if not i['matched']];octaves=0
        for i in false:
            active=[t for t in truth if t['on']<=i['on']<=t['off']]
            octave=any(abs(t['note']-i['note']) in (12,24,36) for t in active);octaves+=int(octave)
            same=any(t['note']==i['note'] for t in active)
            false_details.append({'case':name,'family':row['family'],'mode':row['mode'],'rate':rate,'block':block,'note':i['note'],'on_sample':i['on'],'off_sample':i['off'],'on_ms':i['on']*1000/rate,'same_pitch_retrigger':same,'octave_error':octave})
        calculated={'expected':len(truth),'matched':sum(c is not None for c in chosen),'missed':sum(c is None for c in chosen),'false_note_ons':len(false),'octave_errors':octaves,'wrong_before_correct':wrong,'early_offs':early,'hung':sum(len(q) for q in held.values()),'overflow':int(row['overflow'])}
        for field in counts:
            if int(row[field])!=calculated[field]:differences.append({'case':name,'rate':rate,'block':block,'field':field,'recorded':row[field],'recomputed':calculated[field]})
        for field,v,q in [('latency_median_ms',latency,.5),('latency_p95_ms',latency,.95),('latency_p99_ms',latency,.99),('latency_max_ms',latency,1),('release_median_ms',release,.5),('release_p95_ms',release,.95)]:
            value=quantile(v,q);original=float(row[field]) if row[field] else None
            if (value is None)!=(original is None) or value is not None and abs(value-original)>1e-6:differences.append({'case':name,'rate':rate,'block':block,'field':field,'recorded':original,'recomputed':value})
        recomputed[key]=calculated
        matched_truth_notes[key]=[t['note'] if selected is not None else None for t,selected in zip(truth,chosen)]
        matched_truth_latency[key]=[(intervals[selected]['on']-t['on'])*1000/rate if selected is not None else None for t,selected in zip(truth,chosen)]
    # Exact sample-position/velocity/note invariance across chunk sizes.
    block_differences=[];cross_rate_differences=[];cross_rate_note_sequence_differences=[];cross_rate_velocity_differences=[];cross_rate_recall_identity_differences=[];time_spreads=[];correct_latency_spreads=[]
    for name in cases:
        for rate in rates:
            signatures=[]
            for block in blocks_by_rate[rate]:signatures.append([(e['on'],e['note'],e['velocity'],e['sample']) for e in events[(name,rate,block)]])
            if signatures[1:]!=signatures[:-1]:block_differences.append({'case':name,'rate':rate})
        groups=[recomputed[(name,rate,reference_block[rate])] for rate in rates]
        if groups[1:]!=groups[:-1]:cross_rate_differences.append(name)
        recall=[matched_truth_notes[(name,rate,reference_block[rate])] for rate in rates]
        if recall[1:]!=recall[:-1]:cross_rate_recall_identity_differences.append(name)
        correct=[matched_truth_latency[(name,rate,reference_block[rate])] for rate in rates]
        for values in zip(*correct):
            if all(v is not None for v in values):correct_latency_spreads.append(max(values)-min(values))
        rate_events=[events[(name,rate,reference_block[rate])] for rate in rates]
        shapes=[[(e['on'],e['note']) for e in group] for group in rate_events]
        velocities=[[e['velocity'] for e in group] for group in rate_events]
        if shapes[1:]!=shapes[:-1]:cross_rate_note_sequence_differences.append(name)
        if velocities[1:]!=velocities[:-1]:cross_rate_velocity_differences.append(name)
        if shapes[1:]==shapes[:-1]:
            for es in zip(*rate_events):time_spreads.append(max(int(e['sample'])/int(e['rate']) for e in es)-min(int(e['sample'])/int(e['rate']) for e in es))
    preserved={p.name:sha(p) for p in [folder/'audio-cases.csv',folder/'audio-events.csv',folder/'audio-summary.txt',manifest_path]}
    role=args.evaluation_role or manifest.get('evaluation_role','unspecified-legacy-role')
    summary={'audit':'independent Python recomputation from raw MIDI events and frozen reference annotations','rows':len(rows),'case_count':len(cases),'configuration_count':len(configurations),'configurations':[{'rate':rate,'block':block} for rate,block in configurations],'block_invariance_assessed':any(len(blocks)>1 for blocks in blocks_by_rate.values()),'cross_rate_invariance_assessed':len(rates)>1,'evaluation_role':role,'scoring_differences':differences,'block_event_invariance_differences':block_differences,'cross_rate_outcome_invariance_differences':cross_rate_differences,'cross_rate_recall_identity_differences':cross_rate_recall_identity_differences,'cross_rate_note_sequence_differences':cross_rate_note_sequence_differences,'cross_rate_velocity_differences':cross_rate_velocity_differences,'maximum_cross_rate_matching_event_time_spread_ms':max(time_spreads,default=0)*1000,'maximum_cross_rate_correct_response_spread_ms':max(correct_latency_spreads,default=0),'fixture_manifest_sha256':sha(fixture_path),'original_report_sha256':preserved,'source_changed_during_measurement':manifest.get('source_changed_during_measurement'),'detector_binary_sha256':manifest['binary_sha256'],'heldout_first_audit':role=='first-blind'}
    (folder/'scoring-audit.json').write_text(json.dumps(summary,indent=2)+'\n')
    with (folder/'false-onset-details.csv').open('w',newline='') as f:
        fields=['case','family','mode','rate','block','note','on_sample','off_sample','on_ms','same_pitch_retrigger','octave_error'];writer=csv.DictWriter(f,fieldnames=fields);writer.writeheader();writer.writerows(false_details)
    by_case=collections.defaultdict(list)
    for row in rows:by_case[row['case']].append(row)
    with (folder/'errors-by-case.csv').open('w',newline='') as f:
        fields=['case','family','mode','configurations',*counts,'latency_max_ms','deadline_misses'];writer=csv.DictWriter(f,fieldnames=fields);writer.writeheader()
        for name,group in by_case.items():
            totals={field:sum(int(r[field]) for r in group) for field in counts}
            if any(totals[k] for k in counts if k not in ('expected','matched')):
                writer.writerow({'case':name,'family':group[0]['family'],'mode':group[0]['mode'],'configurations':len(group),**totals,'latency_max_ms':max((float(r['latency_max_ms']) for r in group if r['latency_max_ms']),default=''),'deadline_misses':sum(int(r['deadline_misses']) for r in group)})
    print(json.dumps({k:v for k,v in summary.items() if k not in ['original_report_sha256']},indent=2))
    if differences or block_differences or cross_rate_differences or cross_rate_note_sequence_differences:raise SystemExit(1)

if __name__=='__main__':main()
