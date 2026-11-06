from pathlib import Path
import csv,collections,json
H=Path(__file__).resolve().parent
CHORDS330=[[40],[45],[52],[57],[64],[40,52],[45,64],[48,52,55],[52,55,59],[40,47,52,55,59,64],[40,45,50,55,59,64]]
CHORDS132=[[40,52,64],[41,48,53,56,60,65],[47,54,59,63,66,71],[64,69,74,79,83,88],[45,52,57,60,64],[43,47,50,55,59,67],[50,57,62,66],[40,52],[45,64],[38,45,50,55,59,64],[28,35,40,45]]
def score(path,suite):
 rows=list(csv.DictReader(path.open()));groups=collections.defaultdict(list)
 for r in rows:groups[tuple(int(r[k]) for k in ('case','cents','timbre','dynamics'))].append(r)
 chords=CHORDS330 if suite==330 else CHORDS132;totals=collections.defaultdict(collections.Counter);details=[]
 for c,truth in enumerate(chords):
  for cents in ([-40,-20,0,20,40] if suite==330 else [-20,0,20]):
   for timbre in (range(3) if suite==330 else range(2)):
    for dynamics in (0,1):
     key=(c,cents,timbre,dynamics);events=groups[key];ons=[r for r in events if r['type']=='N'];counts=collections.Counter(int(r['note']) for r in ons)
     missing=[n for n in truth if counts[n]==0];extras=[r for r in ons if int(r['note']) not in truth];repeat=sum(max(0,counts[n]-1) for n in truth)
     lower=sum(any(t-int(r['note']) in (12,24,36) for t in truth) for r in extras)
     octave=sum(any(abs(t-int(r['note'])) in (12,24,36) for t in truth) for r in extras)
     stat=dict(runs=1,expected=len(truth),misses=len(missing),extras=len(extras),repeats=repeat,false_octave_extras=octave,lower_octave_extras=lower,perfect=int(not(missing or extras or repeat)))
     # Independent useful-interval audit retains real causal decisions. This
     # does not replace the paired legacy ever-On comparison above.
     intervals=[];active={};orphan=0
     for r in events:
      note=int(r['note']);sample=int(r['sample'])
      if r['type']=='N':
       if note in active:orphan+=1
       intervals.append(dict(note=note,on=sample,off=48000,used=False));active[note]=len(intervals)-1
      elif note in active:intervals[active.pop(note)]['off']=sample
      else:orphan+=1
     references=[];early=late=0
     for j,note in enumerate(truth):
      on=round((.08+(j*.007 if suite==330 or c not in (7,8) else (0 if j==0 else .2)))*48000);off=on+28800
      choices=[i for i in intervals if not i['used'] and i['note']==note and on-2<=i['on']<off and min(i['off'],off)-max(i['on'],on)>=960]
      chosen=min(choices,key=lambda i:i['on']) if choices else None
      if chosen:
       chosen['used']=True;early+=chosen['off']<off-480;late+=chosen['off']>off+3840
      references.append(dict(note=note,on=on,off=off,matched=chosen is not None,actual_on=chosen['on'] if chosen else None,actual_off=chosen['off'] if chosen else None,latency_ms=(chosen['on']-on)/48 if chosen else None))
     stat.update(useful_matched=sum(r['matched'] for r in references),useful_missed=sum(not r['matched'] for r in references),early_first_offs=early,late_first_offs=late,active_at_end=len(active),orphan_or_overlapping_events=orphan)
     totals['total'].update(stat);totals[str(cents)].update(stat);totals['dynamics-'+str(dynamics)].update(stat)
     if suite==132:totals['standard-profile' if c<9 else 'outside-standard-profile'].update(stat)
     details.append(dict(case=c,cents=cents,timbre=timbre,dynamics=dynamics,truth=truth,misses=missing,extras=extras,repeats=repeat,references=references,stats=stat,events=events))
 return dict(legacy_counts_definition='Same preexisting any-On misses/extras/repeats; all outputs retained. Extra useful-interval audit is independent and never backdates events.',totals={k:dict(v) for k,v in totals.items()},cases=details)
output={}
for variant in ['observed-f0','base']:
 for suite in [330,132]:
  folder=H/variant/('runs'+str(suite))
  if not (folder/'measurement-manifest.json').exists():continue
  result=score(folder/'events.csv',suite);manifest=json.loads((folder/'measurement-manifest.json').read_text());result['execution']={k:manifest[k] for k in ['max_callback_us','deadline_misses','capture_configurations','frequency_evidence']}
  output[variant+'-'+str(suite)]=result
(H/'scores.json').write_text(json.dumps(output,indent=2)+'\n')
print(json.dumps({k:dict(totals=v['totals'],execution=v['execution']) for k,v in output.items()},indent=2))
