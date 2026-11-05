from pathlib import Path
import csv, json, wave
BASE=Path(__file__).resolve().parent
ROOT=Path('/workspace/ChatGPT')
SUITES={'public':ROOT/'Tests/AudioFixtures','holdout':ROOT/'Tests/AudioFixtures/holdout','confirmation':ROOT/'Tests/AudioFixtures/confirmation','controls':ROOT/'Tests/AudioFixtures/readiness','recorded_noise':ROOT/'Tests/AudioFixtures/readiness/recorded_noise'}

def one(variant,suite,fixtures):
    rows=list(csv.DictReader((BASE/variant/'results'/suite/'audio-events.csv').open()))
    grouped={}
    for r in rows:grouped.setdefault(r['case'],[]).append(r)
    results=[]
    for f in csv.DictReader((fixtures/'fixtures.tsv').open(),delimiter='\t'):
        if f['mode']!='poly':continue
        name=f.get('name',f.get('case'))
        # Inventory column names vary only in the first field.
        if name is None:name=next(iter(f.values()))
        relative=f.get('file',f.get('path'))
        if relative is None:relative=list(f.values())[3]
        with wave.open(str(fixtures/relative),'rb') as w:total=w.getnframes();rate=w.getframerate()
        truth_text=list(f.values())[4] if len(f)>4 else ''
        truth=[tuple(map(int,t.split(':'))) for t in truth_text.split(',') if t]
        intervals=[];held={}
        for r in grouped.get(name,[]):
            note=int(r['note']);sample=int(r['sample'])
            if int(r['on']):
                i=len(intervals);intervals.append({'note':note,'on':sample,'off':total,'closed':False,'matched':False});held.setdefault(note,[]).append(i)
            elif held.get(note):
                i=held[note].pop(0);intervals[i]['off']=sample;intervals[i]['closed']=True
        early=late=matched=0;release=[]
        for note,on,off in truth:
            for interval in intervals:
                if interval['matched'] or interval['note']!=note:continue
                if not(on-2<=interval['on']<=off+2):continue
                if min(interval['off'],off)-max(interval['on'],on)<min(round(.020*rate),off-on):continue
                interval['matched']=True;matched+=1;delta=1000*(interval['off']-off)/rate;release.append(delta)
                early+=delta < -10;late+=delta > (100 if 'snr6_' in name else 80)
                break
        noise_voice_ms=sum(1000*(i['off']-i['on'])/rate for i in intervals) if not truth else 0
        union=0
        if not truth:
            end=0
            for interval in sorted(intervals,key=lambda x:x['on']):
                union+=max(0,interval['off']-max(interval['on'],end));end=max(end,interval['off'])
        results.append({'case':name,'expected':len(truth),'matched':matched,'early':early,'late':late,'release_ms':release,'noise_voice_ms':noise_voice_ms,'noise_union_ms':1000*union/rate,'noise_ons':len(intervals) if not truth else 0})
    return results

if __name__=='__main__':
    output={}
    for variant in ['baseline','candidate','candidate2','candidate3']:
        if not(BASE/variant/'results/recorded_noise/audio-events.csv').exists():continue
        output[variant]={}
        for suite,fixtures in SUITES.items():
            rows=one(variant,suite,fixtures)
            output[variant][suite]={'totals':{k:sum(r[k] for r in rows) for k in ['expected','matched','early','late','noise_voice_ms','noise_union_ms','noise_ons']},'cases':rows}
    (BASE/'release-duration-audit.json').write_text(json.dumps(output,indent=2)+'\n')
    for variant,suites in output.items():
        for suite,value in suites.items():print(variant,suite,value['totals'])
