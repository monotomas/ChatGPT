from pathlib import Path
import csv,collections,json
b=Path(__file__).parent;chords=[[40,52,64],[41,48,53,56,60,65],[47,54,59,63,66,71],[64,69,74,79,83,88],[45,52,57,60,64],[43,47,50,55,59,67],[50,57,62,66],[40,52],[45,64],[38,45,50,55,59,64],[28,35,40,45]]
r={}
for name,path in [('baseline','counter-baseline/events.csv'),('guitar-prior','counter-guitar-prior/events.csv'),('candidate','time-atoms-held-prediction/physical-counters/events.csv')]:
 rows=list(csv.DictReader((b/path).open()));gs=collections.defaultdict(list)
 for x in rows:gs[tuple(x[k] for k in ('case','cents','timbre','dynamics'))].append(x)
 totals=collections.defaultdict(collections.Counter);results=[]
 for c,truth in enumerate(chords):
  for cents in (-20,0,20):
   for timbre in (0,1):
    for dyn in (0,1):
     k=tuple(map(str,(c,cents,timbre,dyn)));ons=[int(x['note']) for x in gs[k] if x['type']=='N'];co=collections.Counter(ons)
     miss=[n for n in truth if n not in ons];extra=[n for n in ons if n not in truth];repeat=sum(max(0,co[n]-1) for n in truth)
     stat=dict(runs=1,miss=len(miss),extra=len(extra),repeat=repeat,onset_complete=int(not(miss or extra or repeat)))
     totals['standard-guitar' if c<9 else 'outside-standard-profile'].update(stat);totals[str(c)].update(stat);results.append(dict(case=c,cents=cents,timbre=timbre,dynamics=dyn,truth=truth,miss=miss,extra=extra,repeat=repeat,onsets=[x for x in gs[k] if x['type']=='N']))
 r[name]=dict(totals=totals,cases=results)
(b/'held-counter-analysis.json').write_text(json.dumps(r,indent=2)+'\n')
print('PHYSICAL',json.dumps({n:{k:v for k,v in d['totals'].items() if not k.isdigit()} for n,d in r.items()},indent=2))
# Exact fixed source IDs and actual onset samples; sustained connected MIDI
# cannot satisfy the later authored repick reference without a fresh On.
rep={}
paths=[('candidate','time-atoms-held-prediction/chord-repicks/events.csv')]
if (b/'chord-repicks-baseline/events.csv').exists():paths.insert(0,('baseline','chord-repicks-baseline/events.csv'))
for name,path in paths:
 rows=list(csv.DictReader((b/path).open()));gs=collections.defaultdict(list)
 for x in rows:gs[tuple(x[k] for k in ('wave','phase','new_gain','dynamics'))].append(x)
 totals=collections.defaultdict(collections.Counter);details=[]
 for kind in range(4):
  notes=[45,52,57,60,64] if kind==2 else [40,45,50,55,59,64]
  for phase in ('0','0.9','2.1'):
   for gain in ('0.5','1'):
    for dyn in ('0','1'):
     key=tuple(map(str,(kind,phase,gain,dyn)));events=gs[key];ons=[x for x in events if x['type']=='N'];matched=set();missing=[];latencies=[]
     for gesture in range(2 if kind in (1,2) else 1):
      for j,note in enumerate(notes):
       on=round((.08+.32*gesture+j*.007)*48000);off=round((.4+j*.007)*48000) if gesture==0 and kind in (1,2) else on+28800
       choices=[(index,x) for index,x in enumerate(ons) if index not in matched and int(x['note'])==note and on<=int(x['sample'])<off]
       if not choices:missing.append(dict(note=note,gesture=gesture,on=on,off=off));continue
       idx,x=choices[0];matched.add(idx);latencies.append((int(x['sample'])-on)/48)
     extras=[x for idx,x in enumerate(ons) if idx not in matched]
     stats=dict(runs=1,expected=len(notes)*(2 if kind in (1,2) else 1),matched=len(matched),missed=len(missing),extra=len(extras),second_gesture_missed=sum(x['gesture']==1 for x in missing),onset_complete=int(not(missing or extras)))
     totals[str(kind)].update(stats);details.append(dict(kind=kind,phase=phase,gain=gain,dynamics=dyn,missing=missing,extras=extras,latencies_ms=latencies,events=events))
 rep[name]=dict(totals=totals,cases=details)
(b/'held-chord-repick-analysis.json').write_text(json.dumps(rep,indent=2)+'\n');print('CHORD',json.dumps({n:d['totals'] for n,d in rep.items()},indent=2))
