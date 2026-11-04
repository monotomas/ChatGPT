#!/usr/bin/env python3
"""Score causal estimates against original oracle-seeded identities.
SPDX-License-Identifier: AGPL-3.0-or-later
No nearest-note rescue, fitted delay alignment or acquisition/readiness claim.
"""
from pathlib import Path
from collections import defaultdict,Counter
import argparse,csv,hashlib,json,math
HERE=Path(__file__).resolve().parent
def sha(path):return hashlib.sha256(path.read_bytes()).hexdigest()
def csvrows(path,delimiter=','):return list(csv.DictReader(path.open(newline=''),delimiter=delimiter))
def quantile(value,q):
    if not value:return None
    value=sorted(value);return value[math.ceil(q*(len(value)-1))]
def stats(value):return dict(median=quantile(value,.5),p95=quantile(value,.95),maximum=quantile(value,1))
def score_case(case,truth,frames,voices,policy):
    grouped=defaultdict(list)
    for r in voices:grouped[int(r['sample'])].append(r)
    reference=defaultdict(dict)
    for r in truth:
        if r['scored']=='1':reference[int(r['sample'])][int(r['voice_id'])]=r
    expected_frames=set(reference);actual_frames={int(r['sample']) for r in frames if int(r['sample']) in expected_frames}
    assert expected_frames==actual_frames,'Missing observation in capture'
    errors=[];per_id=defaultdict(list);details=[];missing=extra=duplicates=swaps=octaves=unobservable=bad_frames=0
    wall=[];cpu=[];raw_frame_by_sample={int(f['sample']):f for f in frames}
    for at,refs in sorted(reference.items()):
        observed=grouped[at];counts=Counter(int(v['id']) for v in observed);by_id={int(v['id']):v for v in observed}
        local_missing=len(set(refs)-set(by_id));local_extra=len(set(by_id)-set(refs));local_duplicate=sum(n-1 for n in counts.values() if n>1)
        missing+=local_missing;extra+=local_extra;duplicates+=local_duplicate;local_bad=bool(local_missing or local_extra or local_duplicate)
        frame=raw_frame_by_sample[at];assert int(frame['output_count'])==len(observed)
        wall.append(float(frame['wall_us']));cpu.append(float(frame['cpu_us']))
        for identifier,ref in sorted(refs.items()):
            output=by_id.get(identifier);expected=float(ref['pitch'])
            if output is None:
                details.append(dict(case=case,sample=at,id=identifier,physical_string=int(ref['physical_string']),expected_pitch=expected,actual_pitch=None,error_cents=None,missing=True));continue
            pitch=float(output['pitch']);assert math.isfinite(pitch);error=abs(pitch-expected)*100
            errors.append(error);per_id[identifier].append(error);unobservable+=not int(output['phase_observable'])
            nearest=min(refs,key=lambda i:abs(float(refs[i]['pitch'])-pitch))
            # Nearest identities are DIAGNOSTIC ONLY; every score uses the
            # original seed mapping, even when another reference is closer.
            possible_swap=nearest!=identifier and error>75 and abs(float(refs[nearest]['pitch'])-pitch)*100<25
            swaps+=possible_swap;octaves+=any(abs(error-octave)<100 for octave in (1200,2400))
            local_bad|=error>policy['mandatory_tracking_checks']['pitch_error_max_cents_max']
            details.append(dict(case=case,sample=at,id=identifier,physical_string=int(ref['physical_string']),expected_pitch=expected,actual_pitch=pitch,error_cents=error,missing=False,phase_observable=bool(int(output['phase_observable'])),possible_identity_swap=possible_swap))
        bad_frames+=local_bad
    mandatory=policy['mandatory_tracking_checks'];summary=stats(errors);failures=[]
    for value,label,limit in ((missing,'missing-original-identity',mandatory['missing_seeded_voice_frames_max']),(extra,'extra-voice',mandatory['extra_voice_frames_max']),(duplicates,'duplicate-id',mandatory['duplicate_id_frames_max'])):
        if value>limit:failures.append(label)
    for key in ('median','p95','maximum'):
        rule='pitch_error_'+('max' if key=='maximum' else key)+'_cents_max'
        if summary[key] is None or summary[key]>mandatory[rule]:failures.append('pitch-'+key)
    per_voice={str(i):dict(estimated_frames=len(per_id[i]),expected_frames=len(reference),error_cents=stats(per_id[i])) for i in sorted(next(iter(reference.values())))}
    for identifier,item in per_voice.items():
        e=item['error_cents']
        if e['p95'] is None or e['p95']>mandatory['per_voice_pitch_error_p95_cents_max']:failures.append('voice-'+identifier+'-p95')
        if e['maximum'] is None or e['maximum']>mandatory['per_voice_pitch_error_max_cents_max']:failures.append('voice-'+identifier+'-max')
    return dict(case=case,assessed_observations=len(reference),expected_source_frames=sum(len(r) for r in reference.values()),estimated_source_frames=len(errors),missing_source_frames=missing,extra_source_frames=extra,duplicate_source_frames=duplicates,possible_identity_swap_frames=swaps,octave_error_frames=octaves,phase_unobservable_source_frames=unobservable,observations_with_missing_duplicate_extra_or_error_over_75c=bad_frames,error_cents=summary,per_voice=per_voice,wall_us=stats(wall),thread_cpu_us=stats(cpu),failures=failures,passed=not failures),details
def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--fixtures',type=Path,default=HERE);p.add_argument('--capture',type=Path,required=True);p.add_argument('--output',type=Path,required=True);a=p.parse_args()
    manifest=json.loads((a.fixtures/'fixtures.json').read_text());policy=json.loads((a.fixtures/'policy.json').read_text());truth=csvrows(a.fixtures/'truth.tsv','\t');frames=csvrows(a.capture/'frames.csv');voices=csvrows(a.capture/'voices.csv')
    expected={c['name'] for c in manifest['cases']};assert {r['case'] for r in frames}==expected
    rows=[];details=[]
    for case in manifest['cases']:
        result,raw=score_case(case['name'],[r for r in truth if r['case']==case['name']],[r for r in frames if r['case']==case['name']],[r for r in voices if r['case']==case['name']],policy);rows.append(result);details+=raw
    summary=dict(scope=policy['scope'],fixture_seal=(a.fixtures/'SEALED_SHA256.txt').read_text().strip(),policy_sha256=sha(a.fixtures/'policy.json'),capture_sha256={f:sha(a.capture/f) for f in ('frames.csv','voices.csv')},cases=len(rows),passed_cases=sum(r['passed'] for r in rows),failed_cases=sum(not r['passed'] for r in rows),causal_error_not_delay_aligned=True,identity_matching='Original seeded physical-string-to-ID mapping, never nearest-reference reassignment',oracle_seeded_all_true_voices=True,note_acquisition_tested=False,real_audio=False,musical_readiness_eligible=False,results=rows)
    a.output.mkdir(parents=True,exist_ok=True);(a.output/'summary.json').write_text(json.dumps(summary,indent=2)+'\n')
    with (a.output/'errors.csv').open('w',newline='') as f:w=csv.DictWriter(f,fieldnames=['case','sample','id','physical_string','expected_pitch','actual_pitch','error_cents','missing','phase_observable','possible_identity_swap']);w.writeheader();w.writerows(details)
    print(json.dumps(dict(cases=summary['cases'],passed_cases=summary['passed_cases'],failed_cases=summary['failed_cases'],musical_readiness_eligible=False),indent=2));return int(bool(summary['failed_cases']))
if __name__=='__main__':raise SystemExit(main())
