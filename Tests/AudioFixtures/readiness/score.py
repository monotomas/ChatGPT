#!/usr/bin/env python3
"""Independent note/articulation/continuous-pitch scoring of causal raw events.

SPDX-License-Identifier: AGPL-3.0-or-later
No output delay alignment or onset backdating. A passing synthetic score alone
cannot satisfy the real-guitar evidence or original physical-latency requirements.
"""
from __future__ import annotations
import argparse
from bisect import bisect_right
from collections import defaultdict
import csv
import hashlib
import json
import math
from pathlib import Path

HERE=Path(__file__).resolve().parent

def digest(path):return hashlib.sha256(Path(path).read_bytes()).hexdigest()
def nearest_sample(value):return int(math.floor(value+.5))
def quantile(values,q):
    if not values:return None
    values=sorted(values);return values[math.ceil(q*(len(values)-1))]
def triplet(values):return dict(median=quantile(values,.5),p95=quantile(values,.95),maximum=quantile(values,1))
def read_csv(path):return list(csv.DictReader(Path(path).open(newline='')))

def read_curves(folder):
    curves=defaultdict(lambda:[[],[]])
    for row in csv.DictReader((folder/'pitch-curves.tsv').open(newline=''),delimiter='\t'):
        curve=curves[(row['record'],row['note_id'])];curve[0].append(int(row['sample']));curve[1].append(float(row['pitch']))
    for key,(times,values) in curves.items():
        if times!=sorted(set(times)) or not all(math.isfinite(v) for v in values):raise ValueError('Invalid truth curve '+str(key))
    return curves

def true_pitch(curve,sample):
    times,values=curve;i=bisect_right(times,sample)-1
    if i<0:return values[0]
    if i>=len(times)-1:return values[-1]
    fraction=(sample-times[i])/(times[i+1]-times[i]);return values[i]+fraction*(values[i+1]-values[i])

def intervals(raw):
    result=[];held={};pending={};orphan=0;overlapping_same_voice=0
    for row in raw:
        at=int(row['sample']);voice=int(row['voice_id']);anchor=int(row['note']);kind=row['type']
        if kind=='P':
            value=float(row['pitch'])
            if not math.isfinite(value):raise ValueError('Nonfinite emitted pitch')
            pending[voice]=(at,value)
            if voice in held:held[voice]['pitch'].append((at,value))
        elif kind=='N':
            if voice in held:
                overlapping_same_voice+=1
                held[voice]['off']=at
            previous=pending.get(voice)
            initial=previous[1] if previous and previous[0]<=at else float(anchor)
            item=dict(on=at,off=None,anchor=anchor,voice=voice,pitch=[(at,initial)],used=False)
            result.append(item);held[voice]=item
        elif kind=='F':
            if voice in held and held[voice]['anchor']==anchor:
                held.pop(voice)['off']=at;pending.pop(voice,None)
            else:orphan+=1
        else:raise ValueError('Unknown event '+kind)
    return result,orphan,overlapping_same_voice

def evaluate(case,metric,raw,curves,policy):
    rate=int(metric['rate']);block=int(metric['block']);reference=[]
    for note in case['reference_notes']:
        copied=dict(note);copied['on']=nearest_sample(note['on_sample']*rate/48000);copied['off']=nearest_sample(note['off_sample']*rate/48000);reference.append(copied)
    output,orphans,overlap=intervals(raw);failures=[];paired=[];latencies=[];discrete_latencies=[];release=[];pitch_errors=[]
    strict=case['snr_db'] is None or case['snr_db']>=12
    rules=policy['mandatory_clean_and_12dB_SNR_controls'] if strict else policy['6dB_SNR_challenge_controls']
    for ref in reference:
        duration_ms=1000*(ref['off']-ref['on'])/rate
        useful_ms=policy['scoring']['minimum_useful_ms_short_note' if duration_ms<policy['scoring']['short_note_duration_ms_max'] else 'minimum_useful_ms_long_note']
        useful=nearest_sample(useful_ms*rate/1000)
        candidates=[i for i in output if not i['used'] and i['anchor']==ref['anchor'] and i['on']>=ref['on']-2 and i['on']<ref['off'] and min(i['off'] if i['off'] is not None else int(metric['frames']),ref['off'])-max(i['on'],ref['on'])>=useful]
        if not candidates:paired.append((ref,None));continue
        chosen=min(candidates,key=lambda i:(abs(i['on']-ref['on']),i['on']));chosen['used']=True;paired.append((ref,chosen))
        latencies.append(1000*(chosen['on']-ref['on'])/rate)
        if ref['technique'] in ('fretted_slide','hammer_on','pull_off'):discrete_latencies.append(latencies[-1])
        if chosen['off'] is not None:release.append(1000*(chosen['off']-ref['off'])/rate)
    matched=sum(out is not None for _,out in paired);extra=[out for out in output if not out['used']]
    early=0;late=0;wrong=0;octaves=0;discrete_misses=0;bend_retriggers=0;curve_rows=[]
    for out in extra:
        active=[r for r in reference if r['on']<=out['on']<r['off']]
        if not any(r['anchor']==out['anchor'] for r in active) and any(abs(r['anchor']-out['anchor']) in (12,24,36) for r in active):octaves+=1
        if case['family'] in ('continuous-bend','poly-independent-bend'):bend_retriggers+=1
    for ref,out in paired:
        if out is None:
            if ref['technique'] in ('fretted_slide','hammer_on','pull_off'):discrete_misses+=1
            continue
        if out['off'] is None:late+=1
        else:
            if out['off']<ref['off']-nearest_sample(policy['mandatory_clean_and_12dB_SNR_controls']['early_release_tolerance_ms']*rate/1000):early+=1
            if out['off']>ref['off']+nearest_sample(rules['release_after_physical_damping_ms_max']*rate/1000):late+=1
        for other in output:
            if ref['on']<=other['on']<out['on'] and not any(r['anchor']==other['anchor'] and r['on']-2<=other['on']<r['off'] for r in reference):wrong+=1;break
        curve=curves[(case['record'],ref['id'])];changes=out['pitch'];times=[p[0] for p in changes]
        errors=[];steady=[]
        for source_sample in curve[0]:
            at=nearest_sample(source_sample*rate/48000)
            if at<out['on'] or (out['off'] is not None and at>=out['off']):continue
            index=bisect_right(times,at)-1
            if index<0:continue
            expected=true_pitch(curve,source_sample);error=abs(changes[index][1]-expected)*100
            errors.append(error)
            if case.get('pitch_curve_ground_truth',True):pitch_errors.append(error)
            before=true_pitch(curve,source_sample-128);after=true_pitch(curve,source_sample+128)
            if abs(after-before)<.002:steady.append(error)
        pitch_changes=sum(row['type']=='P' and int(row['voice_id'])==out['voice'] and out['on']<=int(row['sample'])<ref['off'] for row in raw)
        variable=max(curve[1])-min(curve[1])>.05
        row=dict(fractional_ground_truth=case.get('pitch_curve_ground_truth',True),note_id=ref['id'],implementation_voice=out['voice'],anchor=ref['anchor'],technique=ref['technique'],samples_scored=len(errors),pitch_events=pitch_changes,error_cents=triplet(errors),steady_max_cents=max(steady,default=None))
        curve_rows.append(row)
        if variable and pitch_changes<10:failures.append('missing-continuous-expression:'+ref['id'])
        if strict and errors and case.get('pitch_curve_ground_truth',True):
            if quantile(errors,.95)>rules['pitch_error_p95_cents_max']:failures.append('pitch-p95:'+ref['id'])
            if max(errors)>rules['pitch_error_max_cents_max']:failures.append('pitch-max:'+ref['id'])
            if steady and max(steady)>rules['steady_pitch_error_cents_max']:failures.append('steady-pitch:'+ref['id'])
    precision=matched/len(output) if output else (1.0 if not reference else 0.0)
    recall=matched/len(reference) if reference else 1.0
    if precision<rules.get('note_precision_min',1):failures.append('note-precision')
    if recall<rules.get('note_recall_min',1):failures.append('note-recall')
    if not reference and output:failures.append('noise-only-note-ons')
    for value,label in ((octaves,'false-octave'),(early,'premature-release'),(late,'late-release'),(wrong,'wrong-before-correct'),(discrete_misses,'missed-discrete-articulation'),(bend_retriggers,'bend-retrigger'),(orphans,'orphan-off'),(overlap,'duplicate-active-voice'),(int(metric['active_at_end']),'hanging-after-supplied-silence'),(int(metric['overflow']),'overflow')):
        if value:failures.append(label)
    if case['mode']=='poly' and case['bend_enabled'] and int(metric['native_poly_bend'])==0:failures.append('poly-bend-not-implemented')
    if reference and any(int(row['voice_id'])<0 for row in raw if row['type']=='N'):failures.append('missing-stable-voice-id')
    return dict(case=case['name'],family=case['family'],mode=case['mode'],rate=rate,block=block,snr_db=case['snr_db'],expected=len(reference),matched=matched,missed=len(reference)-matched,extra_note_ons=len(extra),noise_note_ons=len(output) if not reference else 0,octave_errors=octaves,wrong_before_correct=wrong,early_offs=early,late_offs=late,discrete_misses=discrete_misses,bend_retriggers=bend_retriggers,hung=int(metric['active_at_end']),overflow=int(metric['overflow']),precision=precision,recall=recall,latency_ms=triplet(latencies),latencies_ms=latencies,discrete_latencies_ms=discrete_latencies,release_ms=triplet(release),pitch_error_cents=triplet(pitch_errors),curves=curve_rows,failures=sorted(set(failures)),passed=not failures)

def main():
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('--fixtures',type=Path,default=HERE);parser.add_argument('--capture',type=Path,required=True);parser.add_argument('--output',type=Path,required=True);parser.add_argument('--research-only',action='store_true',help='Return checkpoint status only; this does not grant musical acceptance');args=parser.parse_args()
    manifest=json.loads((args.fixtures/'fixtures.json').read_text());policy=json.loads((args.fixtures/'policy.json').read_text());curves=read_curves(args.fixtures);by_case={c['name']:c for c in manifest['cases']}
    metrics=read_csv(args.capture/'capture.csv');events=read_csv(args.capture/'events.csv');grouped=defaultdict(list)
    for event in events:grouped[(event['case'],int(event['rate']),int(event['block']))].append(event)
    rows=[evaluate(by_case[m['case']],m,grouped[(m['case'],int(m['rate']),int(m['block']))],curves,policy) for m in metrics]
    if len({(r['case'],r['rate'],r['block']) for r in rows})!=len(rows):raise ValueError('Duplicate capture configuration')
    configurations=sorted({(r['rate'],r['block']) for r in rows})
    for rate,block in configurations:
        if {r['case'] for r in rows if r['rate']==rate and r['block']==block}!=set(by_case):raise ValueError('Incomplete readiness configuration')
    summary=dict(scope='Independent causal detector research scoring; not physical latency or musical readiness proof',policy_sha256=digest(args.fixtures/'policy.json'),fixture_seal=(args.fixtures/'SEALED_SHA256.txt').read_text().strip(),capture_sha256={n:digest(args.capture/n) for n in ('capture.csv','events.csv')},runs=len(rows),passed_runs=sum(r['passed'] for r in rows),failed_runs=sum(not r['passed'] for r in rows),real_articulation_requirement_satisfied=set(policy['required_real_techniques']).issubset(set(manifest.get('real_technique_evidence',[]))),original_physical_latency_goals_verified=False,musical_readiness_eligible=False,release_status='BLOCKED: real-technique and original physical-latency evidence remain required',families={})
    for family in sorted({r['family'] for r in rows}):
        subset=[r for r in rows if r['family']==family];summary['families'][family]={k:sum(r[k] for r in subset) for k in ('expected','matched','missed','extra_note_ons','noise_note_ons','octave_errors','wrong_before_correct','early_offs','late_offs','discrete_misses','bend_retriggers','hung','overflow')}
        summary['families'][family].update(runs=len(subset),passed_runs=sum(r['passed'] for r in subset))
    summary['latency_checkpoints']={}
    for mode in ('mono','poly'):
        values=[v for r in rows if r['mode']==mode for v in r['latencies_ms']];observed=triplet(values);limits=policy['detector_research_checkpoints_not_original_goal_fulfillment'][mode+'_first_correct']
        summary['latency_checkpoints'][mode]=dict(observed_ms=observed,passed=bool(values) and all(observed[k]<=limits[k+'_ms_max'] for k in ('median','p95','maximum') if k!='maximum') and observed['maximum']<=limits['max_ms_max'])
    values=[v for r in rows for v in r['discrete_latencies_ms']];observed=triplet(values);limits=policy['detector_research_checkpoints_not_original_goal_fulfillment']['fretted_or_hammer_transition']
    summary['latency_checkpoints']['discrete_articulation']=dict(observed_ms=observed,passed=bool(values) and observed['median']<=limits['median_ms_max'] and observed['p95']<=limits['p95_ms_max'] and observed['maximum']<=limits['max_ms_max'])
    for checkpoint in summary['latency_checkpoints'].values():
        checkpoint['status']='unrun' if checkpoint['observed_ms']['median'] is None else 'passed' if checkpoint['passed'] else 'failed'
    summary['configuration_count']=len(configurations)
    summary['research_checkpoints_passed']=summary['failed_runs']==0 and all(c['passed'] for c in summary['latency_checkpoints'].values())
    args.output.mkdir(parents=True,exist_ok=True);(args.output/'cases.json').write_text(json.dumps(rows,indent=2)+'\n');(args.output/'summary.json').write_text(json.dumps(summary,indent=2)+'\n');print(json.dumps(summary,indent=2))
    if not summary['research_checkpoints_passed']:return 1
    if not summary['musical_readiness_eligible'] and not args.research_only:return 3
    return 0

if __name__=='__main__':raise SystemExit(main())
