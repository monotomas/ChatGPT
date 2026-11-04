#!/usr/bin/env python3
# Additional seen controls only; no tuning to these cases and no timing alignment.
from pathlib import Path
import math,wave,struct,json,csv,subprocess,hashlib,argparse
p=argparse.ArgumentParser();p.add_argument('executable',type=Path);p.add_argument('out',type=Path);a=p.parse_args();out=a.out.resolve();out.mkdir(exist_ok=False);f=out/'fixtures';f.mkdir();sr=48000;total=1.1
cases=[]
for note in (40,57):
 for ms in (25,80,220):cases.append(dict(name=f'bend_return_N{note}_{ms}ms',kind='bend',note=note,ms=ms,expected=[note]))
for jump in (2,12):cases.append(dict(name=f'hammer_pull_{jump}semi',kind='hammer',note=45,jump=jump,expected=[45,45+jump,45]))
cases.append(dict(name='fretted_slide_35ms',kind='slide',note=45,step=.035,expected=list(range(45,51))))
cases.append(dict(name='same_anchor_independent_repick',kind='repick',note=40,expected=[40,40,40]))
with (f/'fixtures.tsv').open('w') as inventory:
 inventory.write('case\tfamily\tmode\tfile\ttruth\tbend_enabled\n')
 for c in cases:
  phase=0.;samples=[]
  for i in range(int(sr*total)):
   t=i/sr;note=c['note'];j=0
   if c['kind']=='bend':
    dur=c['ms']/1000;up=max(0,min(1,(t-.30)/dur));down=max(0,min(1,(t-.68)/dur));note+=2*(up-down)
   if c['kind']=='hammer':note+=c['jump'] if .32<=t<.62 else 0
   if c['kind']=='slide':note+=max(0,min(5,1+int((t-.30)/c['step']))) if t>=.30 else 0
   if c['kind']=='repick':j=sum(t>=v for v in (.36,.62))
   hz=440*2**((note-69)/12);phase+=2*math.pi*hz/sr
   since=t-.10 if j==0 else t-(.36 if j==1 else .62)
   env=0 if t<.10 or t>=.92 else .15*min(1,max(0,since)/.002)*math.exp(-.65*max(0,since))
   phases=[.13,.91,2.31,.47,4.2,1.1,5.5,.82]
   value=sum(math.cos(h*phase+phases[h-1]+j*((h*h*.731)%(2*math.pi)))/h**1.15 for h in range(1,9))*env
   samples.append(round(max(-.999,min(.999,value))*32767))
  with wave.open(str(f/(c['name']+'.wav')),'wb') as wav:wav.setnchannels(1);wav.setsampwidth(2);wav.setframerate(sr);wav.writeframes(struct.pack('<'+'h'*len(samples),*samples))
  inventory.write(f"{c['name']}\tadditional-seen-falsification\tmono\t{c['name']}.wav\t\t1\n")
(f/'truth.json').write_text(json.dumps(cases,indent=2)+'\n')
with (out/'console.txt').open('w') as log:subprocess.run([str(a.executable.resolve()),str(f),str(out)],stdout=log,stderr=subprocess.STDOUT,check=True)
rows=list(csv.DictReader((out/'events.csv').open()));summary=[]
for c in cases:
 events=[r for r in rows if r['case']==c['name']];ons=[r for r in events if r['type']=='N'];offs=[r for r in events if r['type']=='F']
 summary.append(dict(case=c['name'],expected_notes=c['expected'],emitted_notes=[int(e['note']) for e in ons],on_samples=[int(e['sample']) for e in ons],off_samples=[int(e['sample']) for e in offs],on_sequence_matches=[int(e['note']) for e in ons]==c['expected']))
manifest=dict(scope='Additional seen causal falsification, synthetic harmonic input, not musical readiness or physical latency',executable_sha256=hashlib.sha256(a.executable.read_bytes()).hexdigest(),script_sha256=hashlib.sha256(Path(__file__).read_bytes()).hexdigest(),cases=summary)
(out/'summary.json').write_text(json.dumps(manifest,indent=2)+'\n');print(json.dumps(manifest,indent=2))
