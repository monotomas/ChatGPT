#!/usr/bin/env python3
"""Prepare/verify independent unseen timbres after freezing the basic audio gate.

SPDX-License-Identifier: AGPL-3.0-or-later
Generation requires numpy and FFmpeg; --verify uses only Python's standard library.
Do not tune an algorithm against this holdout before recording its frozen hash.
"""
from pathlib import Path
import argparse,functools,hashlib,importlib.util,json,math,subprocess,wave
ROOT=Path(__file__).resolve().parents[1]/'Tests/AudioFixtures/holdout'
RATE=48000

def verify(root):
    source=json.loads((root/'tonejs/manifest.json').read_text())
    for item in source['files']:
        if hashlib.sha256((root/'tonejs'/item['local_file']).read_bytes()).hexdigest()!=item['sha256']:raise ValueError('Source hash mismatch: '+item['local_file'])
    manifest=json.loads((root/'fixtures.json').read_text())
    if manifest.get('scope')!='independent-holdout' or len(manifest['records'])!=39 or len(manifest['cases'])!=69:raise ValueError('Holdout inventory changed')
    by_file={}
    for item in manifest['records']:
        path=root/item['file']
        if not path.resolve().is_relative_to(root.resolve()):raise ValueError('Unsafe holdout path')
        if hashlib.sha256(path.read_bytes()).hexdigest()!=item['sha256']:raise ValueError('PCM hash mismatch: '+item['file'])
        with wave.open(str(path),'rb') as w:
            if (w.getnchannels(),w.getsampwidth(),w.getframerate(),w.getnframes())!=(1,2,RATE,item['frames']):raise ValueError('PCM parameters changed')
        for t in item['truth']:
            if not(0<=t['note']<=127 and 0<=t['on_sample']<t['off_sample']<=item['frames']):raise ValueError('Invalid holdout truth')
        by_file[item['file']]=item
    names=set();rows=(root/'fixtures.tsv').read_text().splitlines()[1:]
    if len(rows)!=69:raise ValueError('TSV inventory changed')
    for row,item in zip(rows,manifest['cases']):
        if item['name'] in names or item['truth']!=by_file[item['file']]['truth']:raise ValueError('Holdout case mismatch')
        names.add(item['name'])
        truth=','.join(f"{t['note']}:{t['on_sample']}:{t['off_sample']}" for t in item['truth'])
        if row.split('\t')!=[item['name'],item['family'],item['mode'],item['file'],truth]:raise ValueError('TSV/JSON mismatch')
    print('Verified sealed holdout:39waveforms/69cases,12additionalguitar recordings and independent random timbres')

def generate(root):
    import numpy as np
    root.mkdir(parents=True,exist_ok=True);(root/'pcm').mkdir(exist_ok=True)
    records=[];cases=[]
    def write(name,family,audio,truth,modes):
        if np.max(np.abs(audio),initial=0)>.99:raise ValueError('Clipped holdout:'+name)
        file='pcm/'+name+'.wav';pcm=np.round(audio*32767).astype('<i2')
        with wave.open(str(root/file),'wb') as w:
            w.setnchannels(1);w.setsampwidth(2);w.setframerate(RATE);w.writeframes(pcm.tobytes())
        record={'file':file,'sha256':hashlib.sha256((root/file).read_bytes()).hexdigest(),'sample_rate':RATE,'frames':len(audio),'family':family,'truth':truth};records.append(record)
        for mode in modes:cases.append({'name':name+'_'+mode,'family':family,'mode':mode,'file':file,'truth':truth})
    def note_wave(note,seed,duration=.4,gain=.2):
        rng=np.random.default_rng(seed);n=round(duration*RATE);t=np.arange(n)/RATE
        cents=rng.uniform(-21,21);freq=440*2**((note-69+cents/100)/12)
        position=rng.uniform(.08,.41);brightness=rng.uniform(.8,1.85);attack=rng.uniform(.00035,.0035)
        y=np.zeros(n)
        for h in range(1,min(28,int(RATE*.44/freq))+1):
            # Random pluck position and weak detuning avoid the development corpus's fixed comb.
            amplitude=(abs(np.sin(np.pi*position*h))+.06)*rng.uniform(.75,1.2)/h**brightness
            if seed%7==0 and h==1:amplitude*=.03
            y+=amplitude*np.sin(2*np.pi*freq*h*t+rng.uniform(-np.pi,np.pi))*np.exp(-t*(.7+rng.uniform(.12,.45)*h))
        y/=max(.1,float(np.max(np.abs(y))));y*=gain*(1-np.exp(-t/attack))
        y+=rng.normal(0,.002,n)*np.exp(-t/.0015)
        y[-round(.004*RATE):]*=np.linspace(1,0,round(.004*RATE))
        return y
    for i,note in enumerate([40,42,45,47,50,52,54,57,59,60,64,67,69,72,76,79,83,88]):
        wave_data=note_wave(note,171900+i);start=round(.05*RATE);audio=np.zeros(start+len(wave_data)+round(.25*RATE));audio[start:start+len(wave_data)]=wave_data
        write(f'hidden_modal_{i+1:02d}','holdout-random-modal',audio,[{'note':note,'on_sample':start,'off_sample':start+len(wave_data)}],('mono','poly'))
    voicings=[[40,47,52,56,59,64],[50,57,62,66],[41,48,53,57,60,65],[43,47,50,55,59,67],[47,54,59,62,66],[48,52,55,60,64],[59,60],[45,57,69],[45,64]]
    for i,voicing in enumerate(voicings):
        audio=np.zeros(round(.85*RATE));truth=[]
        for j,note in enumerate(voicing):
            start=round((.05+(j*.004 if i%2 else 0))*RATE)
            gain=.12 if len(voicing)<=3 else .07
            if i==8 and j==1:gain=.025
            wave_data=note_wave(note,272000+i*13+j,duration=.5,gain=gain);audio[start:start+len(wave_data)]+=wave_data
            truth.append({'note':note,'on_sample':start,'off_sample':start+len(wave_data)})
        write(f'hidden_poly_{i+1:02d}','holdout-random-poly',audio,truth,('poly',))
    # Evaluate label frequency on the sustained body rather than blindly trusting filenames.
    source=root/'tonejs';source_manifest=json.loads((source/'manifest.json').read_text());expected={i['local_file']:i['sha256'] for i in source_manifest['files']}
    files=[('electric_E2',40),('electric_Fs2',42),('electric_A2',45),('electric_C3',48),('electric_Ds3',51),('electric_A3',57),('nylon_E2',40),('nylon_A2',45),('nylon_D3',50),('nylon_Fs3',54),('nylon_B3',59),('nylon_E4',64)]
    checks=[]
    for i,(name,note) in enumerate(files):
        p=source/(name+'.ogg')
        if hashlib.sha256(p.read_bytes()).hexdigest()!=expected[p.name]:raise ValueError('Source hash changed')
        decoded=np.frombuffer(subprocess.check_output(['ffmpeg','-v','error','-i',str(p),'-f','f32le','-ac','1','-ar',str(RATE),'pipe:1']),dtype='<f4').astype(float)
        body=decoded[round(.08*RATE):min(len(decoded),round(1.1*RATE))]
        size=max(65536,2**math.ceil(math.log2(len(body))));spectrum=np.abs(np.fft.rfft(body*np.hanning(len(body)),n=size));peak=float(np.max(spectrum));f0=440*2**((note-69)/12);supported=[]
        for h in range(1,9):
            center=f0*h;lo=max(2,int(center*2**(-45/1200)*size/RATE));hi=min(len(spectrum)-2,int(math.ceil(center*2**(45/1200)*size/RATE)))
            index=lo+int(np.argmax(spectrum[lo:hi+1]));logs=np.log(spectrum[index-1:index+2]+1e-12);den=logs[0]-2*logs[1]+logs[2];fraction=.5*(logs[0]-logs[2])/den if abs(den)>1e-10 else 0
            freq=(index+fraction)*RATE/size;cents=1200*math.log2(freq/center);relative=float(spectrum[index])/peak
            if abs(cents)<35 and relative>=.025 and index>lo and index<hi:supported.append({'harmonic':h,'hz':float(freq),'cents':float(cents),'relative_peak':relative})
        gcd=functools.reduce(math.gcd,[v['harmonic'] for v in supported],0)
        if len(supported)<2 or gcd!=1:raise ValueError(f'Uncertain recording label {name}:supported={supported}')
        checks.append({'source':p.name,'expected_midi':note,'supported_harmonic_peaks':supported})
        data=decoded[:round(.65*RATE)]*.35;data[-round(.004*RATE):]*=np.linspace(1,0,round(.004*RATE));onset=int(np.flatnonzero(np.abs(data)>=.001)[0]);start=round(.05*RATE);audio=np.zeros(start+len(data)+round(.25*RATE));audio[start:start+len(data)]=data
        write(f'hidden_recorded_{i+1:02d}','holdout-recorded-electric' if i<6 else 'holdout-recorded-nylon',audio,[{'note':note,'on_sample':start+onset,'off_sample':start+len(data)}],('mono','poly'))
    manifest={'version':1,'scope':'independent-holdout','generator':'Tools/audio_validation_holdout.py','records':records,'cases':cases,'recorded_label_frequency_checks':checks}
    (root/'fixtures.json').write_text(json.dumps(manifest,indent=2)+'\n');rows=['name\tfamily\tmode\twav\ttruth_note_on_off_samples']
    for c in cases:rows.append('\t'.join([c['name'],c['family'],c['mode'],c['file'],','.join(f"{t['note']}:{t['on_sample']}:{t['off_sample']}" for t in c['truth'])]))
    (root/'fixtures.tsv').write_text('\n'.join(rows)+'\n');verify(root)

def main():
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('--verify',action='store_true');parser.add_argument('--root',type=Path,default=ROOT);args=parser.parse_args()
    if args.verify:verify(args.root)
    else:generate(args.root)

if __name__=='__main__':main()
