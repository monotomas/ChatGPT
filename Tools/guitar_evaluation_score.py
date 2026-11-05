#!/usr/bin/env python3
"""Dated Nov5 scope adapter around the unchanged independent acoustic scorer.

SPDX-License-Identifier: AGPL-3.0-or-later
Fixed-semitone polyphony, monophonic bend, and descriptive causal latency.
Original fixture bytes, policies and historical scores remain unchanged.
"""
from __future__ import annotations
import argparse
from collections import defaultdict
import copy
import importlib.util
import json
from pathlib import Path

ROOT=Path(__file__).resolve().parents[1]
POLICY=ROOT/'Tests/AudioFixtures/evaluation_2026_11_05/policy.json'
_spec=importlib.util.spec_from_file_location('original_readiness_score',ROOT/'Tests/AudioFixtures/readiness/score.py')
original=importlib.util.module_from_spec(_spec);_spec.loader.exec_module(original)

def in_scope(case,policy):
    return not(case['mode']=='poly' and case['family'] in policy['out_of_scope']['poly_families'])

def prepare(case,raw,curves):
    """Build a documented scoring view, without modifying any stored input."""
    view=copy.deepcopy(case);events=copy.deepcopy(raw);view_curves=dict(curves)
    if case['mode']=='poly':
        view['bend_enabled']=False
        view['pitch_curve_ground_truth']=False
        # Static MIDI has note identity, not a required physical-string ID.
        # Original raw IDs are still kept in events.csv and source manifests.
        for event in events:event['voice_id']=str(int(event['note']))
        for note in case['reference_notes']:
            key=(case['record'],note['id']);times,_=curves[key]
            view_curves[key]=(times,[float(note['anchor'])]*len(times))
    return view,events,view_curves

def reference_decisions(case,metric,raw,policy):
    """Retain each expected note, including missed notes, for per-pitch timing."""
    rate=int(metric['rate']);output,_,_=original.intervals(raw);rows=[]
    for note in case['reference_notes']:
        on=original.nearest_sample(note['on_sample']*rate/48000)
        off=original.nearest_sample(note['off_sample']*rate/48000)
        duration_ms=1000*(off-on)/rate
        key='minimum_useful_ms_short_note' if duration_ms<policy['scoring']['short_note_duration_ms_max'] else 'minimum_useful_ms_long_note'
        useful=original.nearest_sample(policy['scoring'][key]*rate/1000)
        candidates=[i for i in output if not i['used'] and i['anchor']==note['anchor'] and i['on']>=on-2 and i['on']<off and min(i['off'] if i['off'] is not None else int(metric['frames']),off)-max(i['on'],on)>=useful]
        chosen=min(candidates,key=lambda i:(abs(i['on']-on),i['on'])) if candidates else None
        if chosen:chosen['used']=True
        rows.append(dict(note_id=note['id'],anchor=note['anchor'],technique=note['technique'],reference_on_sample=on,reference_off_sample=off,matched=chosen is not None,decision_sample=chosen['on'] if chosen else None,note_off_sample=chosen['off'] if chosen else None,latency_ms=1000*(chosen['on']-on)/rate if chosen else None))
    return rows

def evaluate(case,metric,raw,curves,policy):
    view,events,view_curves=prepare(case,raw,curves)
    scored=original.evaluate(view,metric,events,view_curves,policy)
    scored['deadline_misses']=int(metric['deadline_misses'])
    scored['cpu_p99_us']=float(metric['cpu_p99_us'])
    scored['cpu_max_us']=float(metric['cpu_max_us'])
    if case['mode']=='poly' and any(row['type']=='P' for row in raw):
        scored['failures'].append('unexpected-poly-continuous-expression')
        scored['passed']=False
    scored['references']=reference_decisions(view,metric,events,policy)
    if sum(r['matched'] for r in scored['references'])!=scored['matched']:raise ValueError('Reference timing and acoustic matching disagree')
    scored['scope']='static-poly' if case['mode']=='poly' else 'mono-expression'
    return scored

def summarize(rows,excluded,policy_path=POLICY):
    counts=('expected','matched','missed','extra_note_ons','noise_note_ons','octave_errors','wrong_before_correct','early_offs','late_offs','discrete_misses','bend_retriggers','hung','overflow','deadline_misses')
    summary=dict(schema=1,scope='Nov5 fixed-semitone poly / mono bend evaluation; no physical interface or universal musical-readiness claim',policy_sha256=original.digest(policy_path),runs=len(rows),passed_runs=sum(r['passed'] for r in rows),failed_runs=sum(not r['passed'] for r in rows),out_of_scope_runs=len(excluded),out_of_scope=excluded,absolute_latency_gate=False,latency_alignment_or_backdating=False,physical_latency_verified=False,counts={k:sum(r[k] for r in rows) for k in counts},latency_by_pitch=[])
    for mode in ('mono','poly'):
        for anchor in sorted({ref['anchor'] for row in rows if row['mode']==mode for ref in row['references']}):
            refs=[ref for row in rows if row['mode']==mode for ref in row['references'] if ref['anchor']==anchor]
            timing=[ref['latency_ms'] for ref in refs if ref['matched']]
            summary['latency_by_pitch'].append(dict(mode=mode,anchor=anchor,expected=len(refs),matched=sum(r['matched'] for r in refs),missed=sum(not r['matched'] for r in refs),conditional_on_matched=True,decision_ms=original.triplet(timing)))
    return summary

def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--fixtures',type=Path,required=True);p.add_argument('--capture',type=Path,required=True);p.add_argument('--output',type=Path,required=True);a=p.parse_args()
    policy=json.loads(POLICY.read_text());manifest=json.loads((a.fixtures/'fixtures.json').read_text());curves=original.read_curves(a.fixtures);cases={c['name']:c for c in manifest['cases']}
    metrics=original.read_csv(a.capture/'capture.csv');raw=original.read_csv(a.capture/'events.csv');grouped=defaultdict(list)
    for row in raw:grouped[(row['case'],int(row['rate']),int(row['block']))].append(row)
    seen=set();rows=[];excluded=[]
    for metric in metrics:
        key=(metric['case'],int(metric['rate']),int(metric['block']))
        if key in seen:raise ValueError('Duplicate capture configuration')
        seen.add(key);case=cases[key[0]]
        if not in_scope(case,policy):excluded.append(dict(case=key[0],rate=key[1],block=key[2],reason=policy['out_of_scope']['reason']));continue
        rows.append(evaluate(case,metric,grouped[key],curves,policy))
    for rate,block in sorted({k[1:] for k in seen}):
        if {k[0] for k in seen if k[1:]==(rate,block)}!=set(cases):raise ValueError('Incomplete evaluation configuration')
    summary=summarize(rows,excluded);summary['fixture_seal']=(a.fixtures/'SEALED_SHA256.txt').read_text().strip();summary['capture_sha256']={n:original.digest(a.capture/n) for n in ('capture.csv','events.csv')}
    a.output.mkdir(parents=True,exist_ok=True);(a.output/'cases.json').write_text(json.dumps(rows,indent=2)+'\n');(a.output/'summary.json').write_text(json.dumps(summary,indent=2)+'\n');print(json.dumps(summary,indent=2))
    return 1 if summary['failed_runs'] else 0
if __name__=='__main__':raise SystemExit(main())
