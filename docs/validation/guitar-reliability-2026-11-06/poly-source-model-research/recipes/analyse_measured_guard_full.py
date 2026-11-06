from pathlib import Path
import csv,collections,json
chords=[[40],[45],[52],[57],[64],[40,52],[45,64],[48,52,55],[52,55,59],[40,47,52,55,59,64],[40,45,50,55,59,64]]
base=Path(__file__).parent; result={}
for filename in ('baseline.csv','tuning-bank/events.csv','stable-tuning/events.csv','stable-tuning-guitar/events.csv','guitar-prior/events.csv','time-atoms-held-prediction/events.csv','measured-root-guards/events.csv'):
 rows=list(csv.DictReader((base/filename).open()));gs=collections.defaultdict(list)
 for r in rows:gs[tuple(r[k] for k in ('case','cents','timbre','dynamics'))].append(r)
 agg=collections.defaultdict(lambda:collections.Counter());cases=[]
 for c,truth in enumerate(chords):
  for cents in (-40,-20,0,20,40):
   for t in range(3):
    for dyn in (0,1):
     k=(str(c),str(cents),str(t),str(dyn));v=gs[k];ons=[int(r['note']) for r in v if r['type']=='N'];co=collections.Counter(ons)
     miss=[n for n in truth if n not in ons];extras=[n for n in ons if n not in truth];repeats=sum(max(0,co[n]-1) for n in truth)
     stats={'runs':1,'misses':len(miss),'extras':len(extras),'repeats':repeats,'perfect':int(not(miss or extras or repeats))}
     agg[cents].update(stats);agg['total'].update(stats);cases.append(dict(case=c,cents=cents,timbre=t,dynamics=dyn,truth=truth,misses=miss,extras=extras,repeats=repeats,onsets=[dict(note=int(r['note']),sample=int(r['sample'])) for r in v if r['type']=='N']))
 result[filename]={'by_detune':dict(agg),'cases':cases}
(base/'measured-guard-full-analysis.json').write_text(json.dumps(result,indent=2)+'\n')
print(json.dumps({n:r['by_detune'] for n,r in result.items()},indent=2))
