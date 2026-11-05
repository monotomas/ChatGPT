from pathlib import Path
from collections import defaultdict,deque
import csv,json
BASE=Path(__file__).resolve().parent
QUIET_BASE=BASE.parent

def read(path):return list(csv.DictReader(path.open()))
def parse(raw):
    output=[];held=defaultdict(deque);orphan=duplicate=0
    for r in raw:
        key=(int(r['voice_id']),int(r['note']));at=int(r['sample'])
        if r['type']=='N':
            duplicate+=bool(held[key]);i=dict(note=key[1],voice=key[0],on=at,off=None,used=False);held[key].append(i);output.append(i)
        elif r['type']=='F':
            if held[key]:held[key].popleft()['off']=at
            else:orphan+=1
    return output,orphan,duplicate,sum(len(v)for v in held.values())

def score(raw,refs,frames):
    output,orphan,duplicate,unclosed=parse(raw);result=[]
    for ref in refs:
        on=int(ref['on_sample']);off=int(ref['off_sample']);note=int(ref['note'])
        choices=[i for i in output if not i['used'] and i['note']==note and on-2<=i['on']<off and min(i['off']if i['off']is not None else frames,off)-max(i['on'],on)>=960]
        i=min(choices,key=lambda x:(abs(x['on']-on),x['on']))if choices else None
        if i:i['used']=True
        release=1000*(i['off']-off)/48000 if i and i['off']is not None else None
        result.append(dict(note=note,reference_on=on,reference_off=off,matched=i is not None,decision_sample=i['on']if i else None,note_off_sample=i['off']if i else None,latency_ms=1000*(i['on']-on)/48000 if i else None,release_ms=release,early=release is not None and release< -10,late=bool(i and(i['off']is None or release>80))))
    extras=[i for i in output if not i['used']]
    return dict(expected=len(refs),matched=sum(r['matched']for r in result),missed=sum(not r['matched']for r in result),extra_ons=len(extras),early=sum(r['early']for r in result),late=sum(r['late']for r in result),orphan_offs=orphan,duplicates=duplicate,unclosed=unclosed,references=result,extras=extras,all_intervals=output)

def main():
    quiet={};raw={}
    for v,directory in [('baseline',QUIET_BASE/'results'),('candidate',BASE/'candidate/quiet-results')]:
        grouped=defaultdict(list)
        for r in read(directory/'events.csv'):grouped[(r['case'],int(r['mono_bend']),int(r['dynamics_enabled']))].append(r)
        raw[v]=grouped;rows=[]
        for metric in read(directory/'capture.csv'):
            key=(metric['case'],int(metric['mono_bend']),int(metric['dynamics_enabled']));refs=[dict(note=metric['note'],on_sample=24000,off_sample=74400)]
            measured=score(grouped[key],refs,int(metric['frames']));rows.append(dict(case=key[0],mono_bend=key[1],dynamics_enabled=key[2],overflow=int(metric['overflow']),active_at_end=int(metric['active_at_end']),**measured))
        quiet[v]=rows
    quiet['raw_comparison']=[]
    for key,rows in raw['baseline'].items():
        after=raw['candidate'][key]
        quiet['raw_comparison'].append(dict(case=key[0],mono_bend=key[1],dynamics_enabled=key[2],all_events_identical=rows==after,note_on_off_identical=[r for r in rows if r['type']!='P']==[r for r in after if r['type']!='P'],baseline_event_count=len(rows),candidate_event_count=len(after)))
    (BASE/'quiet-comparison.json').write_text(json.dumps(quiet,indent=2)+'\n')
    counters={}
    for v in ['baseline','candidate']:
        directory=BASE/v/'counter-results';grouped=defaultdict(list);refs=defaultdict(list)
        for r in read(directory/'events.csv'):grouped[r['case']].append(r)
        for r in read(directory/'references.csv'):refs[r['case']].append(r)
        rows=[]
        for metric in read(directory/'capture.csv'):
            measured=score(grouped[metric['case']],refs[metric['case']],int(metric['frames']))
            rows.append(dict(case=metric['case'],root=int(metric['root']),family=metric['family'],mono_bend=int(metric['mono_bend']),overflow=int(metric['overflow']),active_at_end=int(metric['active_at_end']),**measured))
        counters[v]={'cases':rows,'totals':{k:sum(r[k]for r in rows)for k in ['expected','matched','missed','extra_ons','early','late','orphan_offs','duplicates','unclosed','overflow','active_at_end']}}
    changes=[]
    for a,b in zip(counters['baseline']['cases'],counters['candidate']['cases']):
        assert a['case']==b['case'];diff={k:[a[k],b[k]]for k in ['matched','missed','extra_ons','early','late']if a[k]!=b[k]}
        refdiff=[]
        for x,y in zip(a['references'],b['references']):
            d={k:[x[k],y[k]]for k in ['matched','decision_sample','note_off_sample','latency_ms','release_ms','early','late']if x[k]!=y[k]}
            if d:refdiff.append(dict(note=x['note'],reference_on=x['reference_on'],changes=d))
        if diff or refdiff:changes.append(dict(case=a['case'],counts=diff,references=refdiff))
    counters['changes']=changes
    (BASE/'counter-comparison.json').write_text(json.dumps(counters,indent=2)+'\n')
    for v in ['baseline','candidate']:
        print('QUIET',v,{k:sum(r[k]for r in quiet[v])for k in ['expected','matched','missed','extra_ons','early','late','unclosed','overflow','active_at_end']})
        print('COUNTERS',v,counters[v]['totals'])
        for r in counters[v]['cases']:
            print(v,r['case'],'matched',r['matched'],'/',r['expected'],'extra',r['extra_ons'],'early/late',r['early'],r['late'],'refs',[(n['note'],n['decision_sample'],n['note_off_sample'])for n in r['references']])
    print('QUIET RAW CHANGED',[r for r in quiet['raw_comparison']if not r['all_events_identical']])
    print('COUNTER CHANGES',json.dumps(changes,indent=2))
if __name__=='__main__':main()
