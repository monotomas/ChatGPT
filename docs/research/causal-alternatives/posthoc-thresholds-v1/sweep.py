#!/usr/bin/env python3
"""Post-hoc decoding of saved probabilities; no inference, audio or kernel execution."""
from pathlib import Path
import argparse,array,csv,datetime,gzip,hashlib,importlib.util,json,math,sys

HERE=Path(__file__).resolve().parent
ARCHIVE=HERE.parent
spec=importlib.util.spec_from_file_location('retained_event_scorer',ARCHIVE/'harness/score.py')
scorer=importlib.util.module_from_spec(spec);spec.loader.exec_module(scorer)

def sha(path):return hashlib.sha256(path.read_bytes()).hexdigest()
def aggregate(rows):
    keys=('expected','matched','missed','false_note_ons','noise_note_ons','octave_errors','same_pitch_retriggers','wrong_before_correct','early_offs','late_offs','hung','orphan_offs')
    result={key:sum(r[key] for r in rows) for key in keys}
    latencies=[v for r in rows for v in r['latencies_ms']]
    result.update(cases=len(rows),latency_matched_reference_count=len(latencies),latency_ms={key:scorer.quantile(latencies,q) for key,q in [('median',.5),('p95',.95),('maximum',1)]})
    return result

def decode(frames,on,off):
    state=[False]*88;events=[]
    for offset in range(0,len(frames),90):
        sample=math.floor(frames[offset+1]*48000+.5)
        for key in range(88):
            probability=frames[offset+2+key]
            if not math.isfinite(probability) or not 0<=probability<=1:raise ValueError('Invalid saved probability')
            next_state=probability>off if state[key] else probability>on
            if next_state!=state[key]:events.append({'sample':sample,'on':int(next_state),'note':key+21,'probability':probability})
            state[key]=next_state
    return events

def dominates(a,b,dimensions):
    better_or_equal=[];strict=[]
    for path,direction in dimensions:
        av=a;bv=b
        for key in path:av=av[key];bv=bv[key]
        better_or_equal.append(av>=bv if direction=='max' else av<=bv)
        strict.append(av>bv if direction=='max' else av<bv)
    return all(better_or_equal) and any(strict)

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--probability-root',type=Path,required=True,help='Original bench folder containing full-fast/full-piano matrices')
    parser.add_argument('--output',type=Path,required=True)
    args=parser.parse_args();output=args.output.resolve();output.mkdir(parents=True,exist_ok=True)
    if any(output.iterdir()):parser.error('Refuse existing nonempty result directory')
    policy_path=HERE/'policy.json';policy=json.loads(policy_path.read_text())
    if sha(policy_path)!=(HERE/'POLICY_SHA256.txt').read_text().strip():raise ValueError('Predeclared policy seal differs')
    omitted=json.loads((ARCHIVE/'provenance/omitted-artifacts.json').read_text())['artifacts']
    original_frames={}
    for item in omitted:
        if item['kind']=='raw_probability_matrix' and Path(item['original_path']).parent.name in ('full-fast','full-piano'):
            original_frames[(Path(item['original_path']).parent.name,Path(item['original_path']).name)]=item
    all_rows=[];points=[];raw={};frame_proof={};default_parity={}
    for model in ('fast','piano'):
        config=json.loads((ARCHIVE/'configurations'/(model+'.json')).read_text())
        capture=json.loads((ARCHIVE/'results'/model/'capture.json').read_text())
        inputs={item['id']:item for item in capture['inputs']};frames={};default_events={}
        for identity,item in inputs.items():
            path=args.probability_root/('full-'+model)/item['frames_file'];expected=original_frames[('full-'+model,item['frames_file'])]
            if sha(path)!=expected['sha256']:raise ValueError('Saved probability matrix hash differs:'+str(path))
            values=array.array('f');values.frombytes(path.read_bytes())
            if sys.byteorder!='little':values.byteswap()
            if len(values)!=item['frame_count']*90:raise ValueError('Saved probability matrix shape differs')
            frames[identity]=values;frame_proof[str(path)]={'sha256':expected['sha256'],'bytes':path.stat().st_size}
            default_events[identity]=json.loads((ARCHIVE/'results'/model/item['events_file']).read_text())
        published=policy['published_defaults'][model]
        settings=[{'label':'published-default','published_default':True,**published},*[{'label':'on-'+str(on),'published_default':False,'on':on,'off':.6*on} for on in policy['on_grid']]]
        cache={};raw[model]={};default_parity[model]=True
        for setting in settings:
            key=(setting['on'],setting['off'])
            if key not in cache:cache[key]={identity:decode(values,*key) for identity,values in frames.items()}
            decoded=cache[key];raw[model][setting['label']]={identity:events for identity,events in decoded.items()}
            if setting['published_default']:
                for identity,events in decoded.items():
                    signature=lambda es:[(e['sample'],e['on'],e['note']) for e in es]
                    if signature(events)!=signature(default_events[identity]):raise ValueError('Published default event transcript mismatch:'+identity)
            for scope in ('primary-all88','posthoc-MIDI28-100'):
                rows=[];excluded_ons=0
                for entry in config['cases']:
                    identity=entry['input_id'];events=decoded[identity]
                    if scope!='primary-all88':
                        excluded_ons+=sum(e['on'] for e in events if not 28<=e['note']<=100)
                        events=[e for e in events if 28<=e['note']<=100]
                    result=scorer.score(entry['case'],events,inputs[identity]['processed_frames'])
                    result.update(model=model,point=setting['label'],scope=scope,dataset=entry['dataset'])
                    rows.append(result);all_rows.append(result)
                known={mode:aggregate([r for r in rows if r['dataset']!='recorded_noise' and r['mode']==mode]) for mode in ('mono','poly')}
                recorded={mode:aggregate([r for r in rows if r['dataset']=='recorded_noise' and r['mode']==mode]) for mode in ('mono','poly')}
                # Neural decoding does not depend on the benchmark mode label;
                # report its unique quiet/noise subsets once, using poly rows.
                families={family:aggregate([r for r in rows if r['dataset']=='recorded_noise' and r['mode']=='poly' and r['family']==family]) for family in ('real-quiet-pluck','real-note-plus-recorded-noise','real-recorded-noise-only')}
                point={'model':model,'scope':scope,**setting,'known196':known,'recorded64_per_mode':recorded,'recorded_families':families,'excluded_note_ons_across_mode_cases':excluded_ons}
                requirements=policy['requested_criteria'];poly=known['poly'];positive=recorded['poly'];noise=families['real-recorded-noise-only']
                point['meets_known_poly_legacy_recall_and_extra_count']=poly['matched']>=requirements['known_poly_matched_min'] and poly['false_note_ons']<=requirements['known_poly_extra_note_ons_max']
                point['preserves_all48_recorded_positives_with_zero_noise_ons']=positive['matched']>=requirements['recorded_positive_matched_min'] and noise['noise_note_ons']<=requirements['recorded_noise_only_note_ons_max']
                point['meets_both_requested_conditions']=point['meets_known_poly_legacy_recall_and_extra_count'] and point['preserves_all48_recorded_positives_with_zero_noise_ons']
                points.append(point)
            print(model,setting['label'],'decoded',flush=True)
    for path,expected in frame_proof.items():
        if sha(Path(path))!=expected['sha256']:raise ValueError('Saved probabilities changed during study')
    dimensions=[(('known196','poly','matched'),'max'),(('known196','poly','false_note_ons'),'min')]
    joint=dimensions+[(('known196','mono','matched'),'max'),(('known196','mono','false_note_ons'),'min'),(('recorded64_per_mode','poly','matched'),'max'),(('recorded_families','real-recorded-noise-only','noise_note_ons'),'min')]
    for p in points:
        group=[q for q in points if q['scope']==p['scope']]
        p['known_poly_pareto']=not any(dominates(q,p,dimensions) for q in group)
        p['joint_recall_noise_pareto']=not any(dominates(q,p,joint) for q in group)
    payload=json.dumps(raw,separators=(',',':')).encode();(output/'decoded-events.json.gz').write_bytes(gzip.compress(payload,mtime=0))
    fields=['model','point','scope','dataset','case','family','mode','expected','matched','missed','false_note_ons','noise_note_ons','octave_errors','same_pitch_retriggers','wrong_before_correct','early_offs','late_offs','hung','orphan_offs','latency_median_ms','latency_p95_ms','latency_maximum_ms']
    with (output/'cases.csv').open('w',newline='') as handle:
        writer=csv.DictWriter(handle,fieldnames=fields);writer.writeheader()
        for row in all_rows:
            result={k:row[k] for k in fields if k in row};values=row['latencies_ms']
            result.update(latency_median_ms=scorer.quantile(values,.5),latency_p95_ms=scorer.quantile(values,.95),latency_maximum_ms=scorer.quantile(values,1));writer.writerow(result)
    report={'scope':policy['evaluation_role'],'policy_sha256':sha(policy_path),'probability_matrices_unchanged':True,'default_event_transcripts_reproduced':default_parity,'new_inference_executed':False,'latency_scope':'Actual original delivery times, matched references only; misses retained separately','pareto_scope':'Known-poly recall/extras and joint recall/noise precision; latency shown but not used to rescue lost references','points':points,'qualifying_points':{scope:[{'model':p['model'],'point':p['label']} for p in points if p['scope']==scope and p['meets_both_requested_conditions']] for scope in ('primary-all88','posthoc-MIDI28-100')}}
    (output/'summary.json').write_text(json.dumps(report,indent=2)+'\n')
    manifest={'scope':policy['evaluation_role'],'policy_sha256':sha(policy_path),'script_sha256':sha(Path(__file__)),'scorer_sha256':sha(ARCHIVE/'harness/score.py'),'completed_at_utc':datetime.datetime.now(datetime.timezone.utc).isoformat(),'input_probability_files':frame_proof,'output_sha256':{p.name:sha(p) for p in output.iterdir() if p.is_file()}}
    (output/'manifest.json').write_text(json.dumps(manifest,indent=2)+'\n')
    print('Qualifying points',json.dumps(report['qualifying_points']))

if __name__=='__main__':main()
