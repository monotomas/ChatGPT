from pathlib import Path
import csv,collections,json,math
import sys
base=Path(sys.argv[1]) if len(sys.argv)>1 else Path(__file__).parent
truths={'single_normal':[40],'single_even':[40],'single_even_to_odd_morph':[40],'single_12_partials':[40],'late_added_octave':[40,52],'late_added_near_third':[45,64]}
output={}
for label in ('baseline','spectral'):
    grouped=collections.defaultdict(list)
    for r in csv.DictReader((base/f'falsifier-{label}/events.csv').open()):grouped[(r['case'],r['dynamics'],r['phase_variant'])].append(r)
    cases=[];counts=collections.Counter();all_latency=[]
    for name,truth in truths.items():
        for d in ('0','1'):
            for p in ('0','1'):
                raw=grouped[(name,d,p)];intervals=[];held={}
                for row in raw:
                    n=int(row['note']);at=int(row['sample'])
                    if row['type']=='N':
                        item={'note':n,'on':at,'off':None};intervals.append(item);held[n]=item
                    elif n in held:held.pop(n)['off']=at
                refs=[]
                for j,n in enumerate(truth):
                    on=round((.28 if j else .08)*48000);cut=on+28800
                    candidates=[r for r in intervals if r['note']==n and on<=r['on']<cut and min(r['off'] or 48000,cut)-r['on']>=960]
                    chosen=min(candidates,key=lambda r:r['on']) if candidates else None
                    row={'note':n,'reference_on':on,'reference_cut':cut,'matched_useful_20ms':bool(chosen),'first_interval':chosen}
                    if chosen:
                        latency=(chosen['on']-on)/48.;all_latency.append(latency);row['decision_ms']=latency
                        row['release_ms']=(chosen['off']-cut)/48. if chosen['off'] is not None else None
                        counts['early_first_releases']+=chosen['off'] is not None and chosen['off']<cut-480
                        counts['late_first_releases_over80ms']+=chosen['off'] is None or chosen['off']>cut+3840
                    if j:
                        useful_in_actual_overlap=bool(chosen and min(chosen['off'] or 48000,32640)-chosen['on']>=960)
                        row['new_voice_usable_before_old_actual_cut']=useful_in_actual_overlap
                        counts['late_additions_expected']+=1;counts['late_additions_found_during_overlap']+=useful_in_actual_overlap
                    counts['expected']+=1;counts['matched_useful_20ms']+=bool(chosen);refs.append(row)
                cases.append({'case':name,'dynamics':int(d),'phase_variant':int(p),'references':refs})
    all_latency.sort();output[label]={'counts':dict(counts),'conditional_latency_ms':{'median':all_latency[len(all_latency)//2],'p95':all_latency[math.ceil(.95*(len(all_latency)-1))],'max':max(all_latency)},'cases':cases}
document={'scope':'Additional actual-time audit of unchanged raw falsifier events. No timing alignment/backdating. Existing nominal onset-count totals remain distinct from simultaneous acquisition/release success. Synthetic physical cut is known here; this is not a hardware or real-guitar contact measurement.','prediction_confound':'The spectral-power replacement cannot provide an analytic phase-resolved waveform prediction and leaves predictionValid=false. Its lower repeated-On count is not attributable solely to the envelope prior; phase-innovation admission is also disabled. No candidate adoption is recommended.', 'results':output}
(base/'timing-release-audit.json').write_text(json.dumps(document,indent=2)+'\n')
print(json.dumps({k:{'counts':v['counts'],'conditional_latency_ms':v['conditional_latency_ms']} for k,v in output.items()},indent=2))
