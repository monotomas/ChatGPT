#!/usr/bin/env python3
"""Generate or verify the second sealed audio confirmation set.

SPDX-License-Identifier: AGPL-3.0-or-later
Generation needs numpy/FFmpeg. --verify uses Python standard library only.
No detector result, source parameter or tuned model is read during generation.
"""
from pathlib import Path
import argparse,functools,hashlib,json,math,subprocess,wave
ROOT=Path(__file__).resolve().parents[1]/'Tests/AudioFixtures/confirmation'
RATE=48000

def verify(root):
    source=json.loads((root/'tonejs/manifest.json').read_text())
    for item in source['files']:
        if hashlib.sha256((root/'tonejs'/item['local_file']).read_bytes()).hexdigest()!=item['sha256']:raise ValueError('Confirmation source hash mismatch: '+item['local_file'])
    corpus=json.loads((root/'fixtures.json').read_text())
    if corpus.get('scope')!='second-sealed-confirmation' or len(corpus['records'])!=25 or len(corpus['cases'])!=46:raise ValueError('Confirmation inventory changed')
    records={}
    for item in corpus['records']:
        p=root/item['file']
        if not p.resolve().is_relative_to(root.resolve()):raise ValueError('Unsafe fixture path')
        if hashlib.sha256(p.read_bytes()).hexdigest()!=item['sha256']:raise ValueError('Confirmation PCM hash mismatch: '+item['file'])
        with wave.open(str(p),'rb') as w:
            if (w.getnchannels(),w.getsampwidth(),w.getframerate(),w.getnframes())!=(1,2,RATE,item['frames']):raise ValueError('Confirmation PCM parameters changed')
        for t in item['truth']:
            if not(0<=t['note']<=127 and 0<=t['on_sample']<t['off_sample']<=item['frames']):raise ValueError('Invalid confirmation reference annotation')
        if item['file'] in records:raise ValueError('Duplicate confirmation waveform')
        records[item['file']]=item
    rows=(root/'fixtures.tsv').read_text().splitlines()[1:]
    if len(rows)!=46:raise ValueError('Confirmation TSV inventory changed')
    names=set()
    for row,c in zip(rows,corpus['cases']):
        truth=','.join(f"{t['note']}:{t['on_sample']}:{t['off_sample']}" for t in c['truth'])
        if row.split('\t')!=[c['name'],c['family'],c['mode'],c['file'],truth] or c['name'] in names or c['truth']!=records[c['file']]['truth']:raise ValueError('Confirmation case/annotation mismatch')
        names.add(c['name'])
    # All recorded source paths must be disjoint from both previous corpora.
    prior=set()
    for p in [root.parent/'tonejs/manifest.json',root.parent/'holdout/tonejs/manifest.json']:
        for item in json.loads(p.read_text())['files']:
            if item['upstream_path'].endswith('.ogg'):prior.add(item['upstream_path'])
    if any(item['upstream_path'] in prior for item in source['files'] if item['upstream_path'].endswith('.ogg')):raise ValueError('Confirmation reused a previous recorded note')
    print('Verified second sealed confirmation:25 waveforms/46 cases,9 disjoint recorded notes; no detector evaluated')

def generate(root):
    import numpy as np
    root.mkdir(parents=True,exist_ok=True);(root/'pcm').mkdir(exist_ok=True)
    records=[];cases=[]
    def write(name,family,audio,truth,modes,description):
        peak=float(np.max(np.abs(audio),initial=0))
        if not np.isfinite(audio).all() or peak>.99:raise ValueError('Invalid/clipped confirmation waveform:'+name)
        file='pcm/'+name+'.wav';pcm=np.round(audio*32767).astype('<i2')
        with wave.open(str(root/file),'wb') as w:w.setnchannels(1);w.setsampwidth(2);w.setframerate(RATE);w.writeframes(pcm.tobytes())
        item={'file':file,'sha256':hashlib.sha256((root/file).read_bytes()).hexdigest(),'sample_rate':RATE,'frames':len(audio),'family':family,'truth':truth,'description':description};records.append(item)
        for mode in modes:cases.append({'name':name+'_'+mode,'family':family,'mode':mode,'file':file,'truth':truth})
    def pluck(note,seed,duration=.6,gain=.19,beating=True,weak_root=False):
        rng=np.random.default_rng(seed);n=round(duration*RATE);t=np.arange(n)/RATE
        cents=float(rng.uniform(-11,11));f0=440*2**((note-69+cents/100)/12)
        position=float(rng.uniform(.075,.39));slope=float(rng.uniform(.85,1.65));inharm=float(rng.uniform(0,.000018));y=np.zeros(n)
        for h in range(1,min(27,int(RATE*.43/f0))+1):
            amplitude=(abs(np.sin(np.pi*position*h))+.1)/h**slope
            if h==1 and weak_root:amplitude*=.12
            hz=f0*h*np.sqrt(1+inharm*h*h);phase=float(rng.uniform(-np.pi,np.pi));decay=.8+.21*h
            harmonic=np.sin(2*np.pi*hz*t+phase)
            if beating and h<=6:harmonic+=.14*np.sin(2*np.pi*(hz+rng.uniform(.7,2.6))*t+rng.uniform(-np.pi,np.pi))
            y+=amplitude*harmonic*np.exp(-t*decay)
        y/=max(.1,float(np.max(np.abs(y))));y*=gain*(1-np.exp(-t/.00075));y+=rng.normal(0,.002,n)*np.exp(-t/.001)
        y[-round(.004*RATE):]*=np.linspace(1,0,round(.004*RATE))
        return y
    # Sustained changing partial balance and slight inharmonic beating are not rearticulations.
    for i,note in enumerate([43,48,55,62,70,81]):
        data=pluck(note,603140+i,duration=.65,weak_root=i==3);start=round(.05*RATE);audio=np.zeros(start+len(data)+round(.25*RATE));audio[start:start+len(data)]=data
        write(f'confirm_sustain_{i+1:02d}','confirmation-held-modal',audio,[{'note':note,'on_sample':start,'off_sample':start+len(data)}],('mono','poly'),'Single controlled pluck, evolving decays/weak beating, no later attack')
    for i,(note,kind) in enumerate([(48,'gap-repeat'),(57,'continuous-repeat'),(64,'quieter-repeat'),(52,'legato-change')]):
        width=round(.22*RATE);start=round(.05*RATE);audio=np.zeros(start+3*width+round(.25*RATE));truth=[];previous=None
        for j in range(3):
            current=note+[0,2,5][j] if kind=='legato-change' else note
            gain=[.18,.12,.15][j] if kind=='quieter-repeat' else .18
            full=pluck(current,714200+i*11+j,duration=.36,gain=gain,beating=False)
            chunk=full[:width].copy()
            if kind=='gap-repeat':
                cutoff=width-round(.045*RATE);fade=round(.004*RATE)
                chunk[cutoff-fade:cutoff]*=np.linspace(1,0,fade);chunk[cutoff:]=0
            elif j:
                blend=round(.0012*RATE);chunk[:blend]=previous[width:width+blend]*np.linspace(1,0,blend)+chunk[:blend]*np.linspace(0,1,blend)
            if j==2 or kind=='gap-repeat':chunk[-round(.004*RATE):]*=np.linspace(1,0,round(.004*RATE))
            at=start+j*width;audio[at:at+width]=chunk
            off=at+width-(round(.045*RATE) if kind=='gap-repeat' else 0)
            truth.append({'note':current,'on_sample':at,'off_sample':off});previous=full
        write(f'confirm_articulation_{i+1:02d}','confirmation-'+kind,audio,truth,('mono','poly'),'Three controlled attacks/fret changes, reference lifecycle known by construction')
    voicings=[[40,47,52,56,62,64],[50,57,62,65],[45,52,56,61,64],[43,65]]
    for i,notes in enumerate(voicings):
        audio=np.zeros(round(.95*RATE));truth=[]
        for j,note in enumerate(notes):
            onset=.05+(j*.005 if i<3 else (.25 if j else 0));gain=.08 if i<3 else (.22 if j==0 else .035)
            data=pluck(note,825310+i*17+j,duration=.58 if i<3 or j==0 else .28,gain=gain,beating=False);at=round(onset*RATE);audio[at:at+len(data)]+=data
            truth.append({'note':note,'on_sample':at,'off_sample':at+len(data)})
        write(f'confirm_voicing_{i+1:02d}','confirmation-polyphonic',audio,truth,('poly',),'New controlled voicing; final case adds a weak late independent pitch under a sustained strong note')
    for i in range(2):
        audio=np.zeros(round(.7*RATE));rng=np.random.default_rng(936410+i)
        if i==0:
            n=round(.12*RATE);noise=rng.normal(0,.022,n);noise*=np.exp(-np.arange(n)/(.018*RATE));audio[round(.07*RATE):round(.07*RATE)+n]=noise
        else:
            n=round(.42*RATE);noise=rng.normal(0,.008,n)
            for k in range(1,n):noise[k]=.6*noise[k-1]+.4*noise[k]
            fade=round(.01*RATE);noise[:fade]*=np.linspace(0,1,fade);noise[-fade:]*=np.linspace(1,0,fade);audio[round(.06*RATE):round(.06*RATE)+n]=noise
        write(f'confirm_unpitched_{i+1:02d}','confirmation-noise',audio,[],('mono','poly'),'Broadband/colored unpitched negative control')
    source=root/'tonejs';source_manifest=json.loads((source/'manifest.json').read_text());expected={x['local_file']:x['sha256'] for x in source_manifest['files']}
    originals=[('acoustic_F2',41),('acoustic_A3',57),('acoustic_C4',60),('electric_Fs3',54),('electric_C4',60),('electric_Ds4',63),('nylon_Cs3',49),('nylon_Gs4',68),('nylon_A4',69)];checks=[]
    for i,(name,note) in enumerate(originals):
        p=source/(name+'.ogg')
        if hashlib.sha256(p.read_bytes()).hexdigest()!=expected[p.name]:raise ValueError('Confirmation original changed')
        decoded=np.frombuffer(subprocess.check_output(['ffmpeg','-v','error','-i',str(p),'-f','f32le','-ac','1','-ar',str(RATE),'pipe:1']),dtype='<f4').astype(float)
        body=decoded[round(.08*RATE):min(len(decoded),round(1.1*RATE))];size=max(65536,2**math.ceil(math.log2(len(body))));spectrum=np.abs(np.fft.rfft(body*np.hanning(len(body)),n=size));peak=float(np.max(spectrum));f0=440*2**((note-69)/12);supported=[]
        for h in range(1,9):
            center=f0*h;lo=max(2,int(center*2**(-45/1200)*size/RATE));hi=min(len(spectrum)-2,int(math.ceil(center*2**(45/1200)*size/RATE)));index=lo+int(np.argmax(spectrum[lo:hi+1]));logs=np.log(spectrum[index-1:index+2]+1e-12);den=logs[0]-2*logs[1]+logs[2];delta=.5*(logs[0]-logs[2])/den if abs(den)>1e-10 else 0
            hz=(index+delta)*RATE/size;cents=1200*math.log2(hz/center);relative=float(spectrum[index])/peak
            if abs(cents)<35 and relative>=.025 and lo<index<hi:supported.append({'harmonic':h,'hz':float(hz),'cents':float(cents),'relative_peak':relative})
        if len(supported)<2 or functools.reduce(math.gcd,[h['harmonic'] for h in supported],0)!=1:raise ValueError('Uncertain confirmation pitch label:'+name)
        checks.append({'source':p.name,'expected_midi':note,'supported_harmonic_peaks':supported})
        data=decoded[:round(.65*RATE)]*.35;data[-round(.004*RATE):]*=np.linspace(1,0,round(.004*RATE));start=round(.05*RATE);audio=np.zeros(start+len(data)+round(.25*RATE));audio[start:start+len(data)]=data;onset=int(np.flatnonzero(np.abs(data)>=.001)[0])
        write(f'confirm_recorded_{i+1:02d}','confirmation-recorded-'+name.split('_')[0],audio,[{'note':note,'on_sample':start+onset,'off_sample':start+len(data)}],('mono','poly'),'Disjoint edited isolated recording,650ms crop/.35 gain/4ms controlled cutoff')
    corpus={'version':1,'scope':'second-sealed-confirmation','prepared_after':'first-blind-iteration14-audit','generator':'Tools/audio_validation_confirmation.py','detector_evaluated_during_preparation':False,'excluded_sources':[{'source':'nylon_D5.ogg','reason':'Upstreamfilename implies MIDI74 (587.33Hz), but independent sustained FFT peak is~621.8Hz (MIDI75); omitted before detector evaluation and replaced by independently verified A4. Originalfile retained for provenance.'}],'records':records,'cases':cases,'recorded_label_frequency_checks':checks}
    (root/'fixtures.json').write_text(json.dumps(corpus,indent=2)+'\n');lines=['name\tfamily\tmode\twav\ttruth_note_on_off_samples']
    for c in cases:lines.append('\t'.join([c['name'],c['family'],c['mode'],c['file'],','.join(f"{t['note']}:{t['on_sample']}:{t['off_sample']}" for t in c['truth'])]))
    (root/'fixtures.tsv').write_text('\n'.join(lines)+'\n');verify(root)

def main():
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('--verify',action='store_true');parser.add_argument('--root',type=Path,default=ROOT);args=parser.parse_args()
    if args.verify:verify(args.root)
    else:generate(args.root)
if __name__=='__main__':main()
