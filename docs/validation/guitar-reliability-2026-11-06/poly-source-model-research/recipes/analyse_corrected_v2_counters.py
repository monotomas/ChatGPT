from pathlib import Path
import csv,collections,json
b=Path(__file__).parent
# Exact fixed source IDs and actual onset samples; sustained connected MIDI
# cannot satisfy the later authored repick reference without a fresh On.
rep={}
paths=[('baseline','chord-repicks-corrected-v2-baseline/events.csv'),('phase_v2','time-atoms-per-voice-phase-v2/chord-repicks-corrected-v2/events.csv'),('null_guard','time-atoms-phase-null-guard/chord-repicks-corrected-v2/events.csv')]
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
(b/'corrected-v2-chord-repick-analysis.json').write_text(json.dumps(rep,indent=2)+'\n');print('CHORD',json.dumps({n:d['totals'] for n,d in rep.items()},indent=2))
