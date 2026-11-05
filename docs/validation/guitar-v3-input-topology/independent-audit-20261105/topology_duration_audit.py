from pathlib import Path
from collections import defaultdict,deque
import csv,json,hashlib,math

BASE=Path(__file__).resolve().parent.parent
OUT=Path(__file__).resolve().parent
ROOT=Path('/workspace/ChatGPT')
VARIANTS=['host-before-nc','analysis-after-nc-raw-reference','analysis-after-nc-shared-protection']
SUITES={'controlled':ROOT/'Tests/AudioFixtures/readiness','recorded-noise':ROOT/'Tests/AudioFixtures/readiness/recorded_noise'}
POLICY_PATH=ROOT/'Tests/AudioFixtures/evaluation_2026_11_05/policy.json'

def sha(path):return hashlib.sha256(path.read_bytes()).hexdigest()
def nearest(x):return int(math.floor(x+.5))

def intervals(raw,frames):
    held=defaultdict(deque);out=[];orphans=duplicates=0
    for r in raw:
        kind=r['type'];at=int(r['sample']);note=int(r['note']);voice=int(r['voice_id'])
        if kind=='P':continue
        key=(voice,note)
        if kind=='N':
            duplicates+=bool(held[key]);i={'note':note,'voice':voice,'on':at,'off':None,'used':False};held[key].append(i);out.append(i)
        elif kind=='F':
            if held[key]:held[key].popleft()['off']=at
            else:orphans+=1
        else:raise ValueError(kind)
    return out,orphans,duplicates,sum(len(q) for q in held.values())

def duration(intervals,rate,frames):
    pieces=[(i['on'],i['off'] if i['off'] is not None else frames)for i in intervals]
    assert all(b>=a for a,b in pieces)
    voice=sum(b-a for a,b in pieces);union=0;end=0
    for a,b in sorted(pieces):union+=max(0,b-max(a,end));end=max(end,b)
    return dict(voice_ms=1000*voice/rate,union_ms=1000*union/rate,max_interval_ms=max([1000*(b-a)/rate for a,b in pieces],default=0))

def measure(variant,suite,fixtures,policy):
    path=BASE/variant/suite
    summary=json.loads((path/'scoring/summary.json').read_text())
    assert summary['policy_sha256']==sha(POLICY_PATH)
    scored={r['case']:r for r in json.loads((path/'scoring/cases.json').read_text())}
    cases={c['name']:c for c in json.loads((fixtures/'fixtures.json').read_text())['cases']}
    grouped=defaultdict(list)
    for r in csv.DictReader((path/'events.csv').open()):grouped[(r['case'],int(r['rate']),int(r['block']))].append(r)
    results=[];mismatches=[]
    for metric in csv.DictReader((path/'capture.csv').open()):
        name=metric['case']
        if name not in scored:continue
        case=cases[name];score=scored[name];rate=int(metric['rate']);frames=int(metric['frames']);source_frames=int(metric['source_frames'])
        output,orphans,duplicates,unclosed=intervals(grouped[(name,rate,int(metric['block']))],frames)
        refs=[];early=late=0
        rules=policy['mandatory_clean_and_12dB_SNR_controls'] if case['snr_db'] is None or case['snr_db']>=12 else policy['6dB_SNR_challenge_controls']
        for ref in case['reference_notes']:
            on=nearest(ref['on_sample']*rate/48000);off=nearest(ref['off_sample']*rate/48000)
            useful=nearest(policy['scoring']['minimum_useful_ms_short_note' if 1000*(off-on)/rate<policy['scoring']['short_note_duration_ms_max'] else 'minimum_useful_ms_long_note']*rate/1000)
            choices=[i for i in output if not i['used'] and i['note']==ref['anchor'] and i['on']>=on-2 and i['on']<off and min(i['off'] if i['off'] is not None else frames,off)-max(i['on'],on)>=useful]
            chosen=min(choices,key=lambda i:(abs(i['on']-on),i['on'])) if choices else None
            first_off=chosen['off'] if chosen else None
            is_early=bool(chosen and first_off is not None and first_off<off-nearest(policy['mandatory_clean_and_12dB_SNR_controls']['early_release_tolerance_ms']*rate/1000))
            is_late=bool(chosen and(first_off is None or first_off>off+nearest(rules['release_after_physical_damping_ms_max']*rate/1000)))
            if chosen:chosen['used']=True
            early+=is_early;late+=is_late
            row=dict(note_id=ref['id'],anchor=ref['anchor'],reference_on=on,reference_off=off,matched=chosen is not None,decision_sample=chosen['on']if chosen else None,note_off_sample=first_off,early=is_early,late=is_late,release_ms=1000*(first_off-off)/rate if first_off is not None else None)
            refs.append(row)
        derived=dict(matched=sum(r['matched']for r in refs),missed=sum(not r['matched']for r in refs),extra_note_ons=sum(not i['used']for i in output),early_offs=early,late_offs=late)
        if any(derived[k]!=score[k]for k in derived):mismatches.append(dict(case=name,derived=derived,scored={k:score[k]for k in derived}))
        stored={r['note_id']:r for r in score['references']}
        for ref in refs:
            prev=stored[ref['note_id']]
            if any(ref[k]!=prev[k] for k in ['matched','decision_sample','note_off_sample']):mismatches.append(dict(case=name,note_id=ref['note_id'],derived_ref=ref,stored_ref=prev))
        noise=not case['reference_notes'];d=duration(output,rate,frames)if noise else dict(voice_ms=0,union_ms=0,max_interval_ms=0)
        results.append(dict(case=name,family=case['family'],mode=case['mode'],rate=rate,block=int(metric['block']),source_frames=source_frames,processed_frames=frames,supplied_flush_frames=frames-source_frames,noise_only=noise,noise_ons=len(output)if noise else 0,noise_voice_ms=d['voice_ms'],noise_union_ms=d['union_ms'],max_noise_interval_ms=d['max_interval_ms'],orphan_offs=orphans,duplicate_note_voice_ons=duplicates,unclosed_intervals=unclosed,expected=len(refs),**derived,passed=score['passed'],references=refs,intervals=[{k:v for k,v in i.items()if k!='used'}for i in output]))
    totals={}
    for mode in ['all','mono','poly']:
        rows=[r for r in results if mode=='all' or r['mode']==mode]
        keys=['noise_ons','noise_voice_ms','noise_union_ms','orphan_offs','duplicate_note_voice_ons','unclosed_intervals','expected','matched','missed','extra_note_ons','early_offs','late_offs']
        totals[mode]={k:sum(r[k]for r in rows)for k in keys};totals[mode]['runs']=len(rows);totals[mode]['passed']=sum(r['passed']for r in rows);totals[mode]['noise_cases']=sum(r['noise_only']for r in rows)
    return dict(totals=totals,cases=results,score_reconstruction_mismatches=mismatches,input_sha256={n:sha(path/n)for n in ['capture.csv','events.csv','scoring/cases.json','scoring/summary.json']})

def main():
    policy=json.loads(POLICY_PATH.read_text());result={'scope':'Independent native(note,voice) interval/duration audit, all scoped failures retained; no topology adoption','policy_sha256':sha(POLICY_PATH),'variants':{},'pairwise_case_changes':{},'pairwise_reference_changes':{}}
    for variant in VARIANTS:
        result['variants'][variant]={suite:measure(variant,suite,fixtures,policy)for suite,fixtures in SUITES.items()}
    keys=['matched','missed','extra_note_ons','early_offs','late_offs','noise_ons','noise_voice_ms','noise_union_ms']
    for a,b in [(VARIANTS[0],VARIANTS[1]),(VARIANTS[1],VARIANTS[2]),(VARIANTS[0],VARIANTS[2])]:
        pair=a+' -> '+b;result['pairwise_case_changes'][pair]={};result['pairwise_reference_changes'][pair]={}
        for suite in SUITES:
            before={r['case']:r for r in result['variants'][a][suite]['cases']};after={r['case']:r for r in result['variants'][b][suite]['cases']};assert before.keys()==after.keys()
            changed=[];ref_changed=[]
            for name,row in before.items():
                other=after[name];diff={k:[row[k],other[k]]for k in keys if row[k]!=other[k]}
                if diff:changed.append(dict(case=name,mode=row['mode'],changes=diff))
                oldrefs={r['note_id']:r for r in row['references']};newrefs={r['note_id']:r for r in other['references']}
                for id,old in oldrefs.items():
                    new=newrefs[id];delta={k:[old[k],new[k]]for k in ['matched','early','late','decision_sample','note_off_sample','release_ms']if old[k]!=new[k]}
                    if delta:ref_changed.append(dict(case=name,note_id=id,anchor=old['anchor'],changes=delta))
            result['pairwise_case_changes'][pair][suite]=changed;result['pairwise_reference_changes'][pair][suite]=ref_changed
    (OUT/'topology-duration-audit.json').write_text(json.dumps(result,indent=2)+'\n')
    for v,suites in result['variants'].items():
        for suite,value in suites.items():print(v,suite,value['totals'],'score-mismatches',len(value['score_reconstruction_mismatches']))
    for pair,suites in result['pairwise_reference_changes'].items():
        print('REFERENCE_STATUS_CHANGES',pair)
        for suite,rows in suites.items():
            for r in rows:
                if any(k in r['changes']for k in ['matched','early','late']):print(suite,r)
if __name__=='__main__':main()
