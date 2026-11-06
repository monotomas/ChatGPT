from pathlib import Path
import csv,collections,json
import sys
base=Path(sys.argv[1]) if len(sys.argv)>1 else Path(__file__).parent
truths={'single_normal':[40],'single_even':[40],'single_even_to_odd_morph':[40],'single_12_partials':[40],'late_added_octave':[40,52],'late_added_near_third':[45,64]}
def analyze(rows,keys,expected):
    grouped=collections.defaultdict(list)
    for row in rows:grouped[tuple(row[k] for k in keys)].append(row)
    cases=[];total=collections.Counter()
    for key,truth in expected:
        raw=grouped[key];ons=[r for r in raw if r['type']=='N'];counts=collections.Counter(int(r['note']) for r in ons)
        missing=[n for n in truth if not counts[n]];extras=[int(r['note']) for r in ons if int(r['note']) not in truth]
        repeats=sum(max(0,counts[n]-1) for n in truth)
        octaves=[n for n in extras if any(abs(n-t) in (12,24,36) for t in truth)]
        lower=[n for n in extras if any(t-n in (12,24,36) for t in truth)]
        metric=dict(runs=1,expected=len(truth),matched=len(truth)-len(missing),missed=len(missing),extra_note_ons=len(extras),nominal_repeats=repeats,false_octave_ons=len(octaves),false_lower_octave_ons=len(lower),onset_count_error_free=int(not(missing or extras or repeats)))
        total.update(metric)
        cases.append(dict(key=dict(zip(keys,key)),truth=truth,metrics=metric,misses=missing,extras=extras,onsets=[dict(note=int(r['note']),sample=int(r['sample'])) for r in ons]))
    return dict(counts=dict(total),cases=cases,scope='New disclosed diagnostic; onset count/error-free is not strict full-policy musical acceptance')
expected_open=[((str(c),str(t),str(d)),[40,45,50,55,59,64]) for c in (-40,-20,0,20,40) for t in range(3) for d in (0,1)]
baseline_open=list(csv.DictReader((base/'open-baseline-fresh.csv').open()))
candidate_open=list(csv.DictReader((base/'open-strings.csv').open()))
summary={'open_strings':{'baseline':analyze(baseline_open,['cents','timbre','dynamics'],expected_open),'spectral':analyze(candidate_open,['cents','timbre','dynamics'],expected_open)},'falsifier':{}}
for variant in ('baseline','spectral'):
    folder=base/('falsifier-'+variant);rows=list(csv.DictReader((folder/'events.csv').open()))
    wanted=[((name,str(d),str(p)),truth) for name,truth in truths.items() for d in (0,1) for p in (0,1)]
    result=analyze(rows,['case','dynamics','phase_variant'],wanted)
    callbacks=list(csv.DictReader((folder/'callbacks.csv').open()));result['callbacks']={'runs':len(callbacks),'deadline_misses':sum(int(r['deadline_misses']) for r in callbacks),'worst_per_case_p99_us':max(float(r['callback_p99_us']) for r in callbacks),'maximum_us':max(float(r['callback_max_us']) for r in callbacks),'budget_us':64/48000*1e6,'environment':'Shared cloud worker; serial paired runs, not certified realtime bounds'}
    summary['falsifier'][variant]=result
(base/'results.json').write_text(json.dumps(summary,indent=2)+'\n')
print(json.dumps({suite:{variant:value['counts'] for variant,value in values.items()} for suite,values in summary.items()},indent=2))
for variant,data in summary['falsifier'].items():
    print(variant,'callback',data['callbacks'])
    for name in truths:
        rows=[r for r in data['cases'] if r['key']['case']==name]
        print(name,dict(sum((collections.Counter(r['metrics']) for r in rows),collections.Counter())))
