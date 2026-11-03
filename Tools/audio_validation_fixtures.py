#!/usr/bin/env python3
"""Regenerate the independent audio validation corpus.

SPDX-License-Identifier: AGPL-3.0-or-later
Original synthetic fixtures: Flysnapper contributors, 2026.
Recorded guitar material: CC-BY-3.0, see Tests/AudioFixtures/README.md.
Requires numpy and ffmpeg only when regenerating. Packaged PCM fixtures need neither.
"""
from __future__ import annotations
import argparse, hashlib, json, math, subprocess, wave
from pathlib import Path
import numpy as np

ROOT=Path(__file__).resolve().parents[1]/'Tests'/'AudioFixtures'
RATE=48000

def midi_freq(note: float) -> float:
    return 440*2**((note-69)/12)

def write_wav(path: Path,audio: np.ndarray) -> str:
    audio=np.nan_to_num(audio)
    if np.max(np.abs(audio),initial=0)>0.99:
        raise ValueError(f'Unexpected clipping in {path.name}; change test source gain explicitly')
    pcm=np.round(np.clip(audio,-1,1)*32767).astype('<i2')
    with wave.open(str(path),'wb') as f:
        f.setnchannels(1);f.setsampwidth(2);f.setframerate(RATE);f.writeframes(pcm.tobytes())
    return hashlib.sha256(path.read_bytes()).hexdigest()

def modal(note: int, duration: float=.35, gain: float=.3, seed: int=1,
          missing_fundamental: bool=False,cents: float=0,vibrato: float=0,
          attack_noise: float=.01, brightness: float=1.25) -> np.ndarray:
    """Damped string modes with pluck-position comb and frequency-dependent decay."""
    n=round(duration*RATE);t=np.arange(n)/RATE;rng=np.random.default_rng(seed)
    base=midi_freq(note+cents/100)
    phase=2*np.pi*base*(t+(vibrato/100*math.log(2)/12)/(2*np.pi*5)*(1-np.cos(2*np.pi*5*t)))
    audio=np.zeros(n)
    for harmonic in range(1,min(32,int(RATE*.46/base))+1):
        if harmonic==1 and missing_fundamental:continue
        coefficient=(abs(np.sin(harmonic*np.pi*.19))+.12)/(harmonic**brightness)
        audio+=coefficient*np.sin(harmonic*phase+rng.uniform(-np.pi,np.pi))*np.exp(-t*(1.1+.30*harmonic))
    audio/=max(.1,float(np.max(np.abs(audio))))
    attack=1-np.exp(-t/.0006)
    audio=audio*attack*gain+rng.normal(0,attack_noise,n)*np.exp(-t/.0015)*attack
    # Defined physical damping at the known note-off (4ms taper).
    fade=min(n,round(.004*RATE));audio[-fade:]*=np.linspace(1,0,fade)
    return audio

def karplus(note: int,duration: float=.35,gain: float=.3,seed: int=1) -> np.ndarray:
    """Independent noise-excited delay-loop string, not a harmonic-template rendering."""
    rng=np.random.default_rng(seed);delay=max(4,round(RATE/midi_freq(note)-.5))
    ring=rng.uniform(-1,1,delay);ring-=np.mean(ring)
    n=round(duration*RATE);audio=np.zeros(n)
    for i in range(n):
        p=i%delay;old=ring[p];audio[i]=old
        ring[p]=.996*.5*(old+ring[(p+1)%delay])
    audio/=max(.1,float(np.max(np.abs(audio))))
    audio*=gain
    fade=min(n,round(.004*RATE));audio[-fade:]*=np.linspace(1,0,fade)
    return audio

def main() -> None:
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('--root',type=Path,default=ROOT)
    args=parser.parse_args();root=args.root;root.mkdir(parents=True,exist_ok=True);(root/'pcm').mkdir(exist_ok=True)
    records=[];cases=[]
    def add(name: str,family: str,notes: list[tuple[int,float,np.ndarray]],modes=('mono',),background=0.0):
        frames=max([round((on+.0)*RATE)+len(a) for _,on,a in notes]+[round(.4*RATE)])+round(.25*RATE)
        audio=np.zeros(frames)
        if background:
            audio+=np.random.default_rng(775).normal(0,background,frames)
        truths=[]
        for note,on,data in notes:
            start=round(on*RATE);audio[start:start+len(data)]+=data
            truths.append({'note':note,'on_sample':start,'off_sample':start+len(data)})
        name_file='pcm/'+name+'.wav';sha=write_wav(root/name_file,audio)
        records.append({'file':name_file,'sha256':sha,'sample_rate':RATE,'frames':frames,'family':family,'truth':truths})
        for mode in modes:cases.append({'name':name+'_'+mode,'family':family,'mode':mode,'file':name_file,'truth':truths})
    # Causal attacks and known MIDI labels, no source reused from detector code.
    for note in (40,41,45,48,52,57,60,64,69,76,84):
        add(f'modal_{note}','synthetic-modal',[(note,.05,modal(note,seed=note))],('mono','poly'))
    for note in (40,52,64,76):
        add(f'karplus_{note}','synthetic-karplus',[(note,.05,karplus(note,seed=note))],('mono','poly'))
    add('missing_fundamental_52','synthetic-missing-fundamental',[(52,.05,modal(52,missing_fundamental=True))],('mono','poly'))
    add('detuned_52','synthetic-detuning',[(52,.05,modal(52,cents=17))],('mono','poly'))
    add('vibrato_64','synthetic-vibrato',[(64,.05,modal(64,duration=.6,vibrato=22))],('mono','poly'))
    add('soft_attack_52','synthetic-soft',[(52,.05,modal(52,gain=.025,attack_noise=.001))],('mono','poly'),background=.0003)
    add('weak_fundamental_40','synthetic-bright',[(40,.05,modal(40,brightness=.55,gain=.25))],('mono','poly'))
    add('repeated_52','synthetic-repeated',[(52,t,modal(52,duration=.16,seed=i+8)) for i,t in enumerate((.05,.26,.47))],('mono','poly'))
    # Equal-pitch repicks replace one vibrating string before its old decay is silent.
    segments=[modal(52,duration=.35,seed=710+i) for i in range(3)]
    width=round(.2*RATE);whole=np.zeros(3*width)
    for i,segment in enumerate(segments):
        chunk=segment[:width].copy()
        if i:
            fade=round(.001*RATE)
            chunk[:fade]=segments[i-1][width:width+fade]*np.linspace(1,0,fade)+chunk[:fade]*np.linspace(0,1,fade)
        whole[i*width:(i+1)*width]=chunk
    whole[-round(.004*RATE):]*=np.linspace(1,0,round(.004*RATE))
    add('continuous_repick_52','synthetic-repick',[(52,.05,whole)],('mono','poly'))
    repick_truth=[{'note':52,'on_sample':round((.05+i*.2)*RATE),'off_sample':round((.05+(i+1)*.2)*RATE)} for i in range(3)]
    records[-1]['truth']=repick_truth
    cases[-1]['truth']=repick_truth;cases[-2]['truth']=repick_truth
    add('chromatic_up','synthetic-run',[(n,.05+i*.15,modal(n,duration=.12,seed=400+i)) for i,n in enumerate(range(52,60))],('mono',))
    add('chromatic_down','synthetic-run',[(n,.05+i*.15,modal(n,duration=.12,seed=500+i)) for i,n in enumerate(range(59,51,-1))],('mono',))
    for label,notes,gains,onsets in [
        ('major_triad',[48,52,55],[.16,.16,.16],[.05]*3),
        ('open_e_minor',[40,47,52,55,59,64],[.08]*6,[.05]*6),
        ('strummed_a_minor',[45,52,57,60,64],[.09]*5,[.05,.055,.061,.068,.076]),
        ('adjacent_semitone',[52,53],[.17,.17],[.05]*2),
        ('octaves',[40,52,64],[.12]*3,[.05]*3),
        ('weak_upper',[48,64],[.27,.035],[.05]*2),
        ('staggered',[48,55,64],[.14]*3,[.05,.16,.28]),
    ]:
        add(label,'synthetic-poly',[(n,t,modal(n,duration=.45,gain=g,seed=100+j)) for j,(n,g,t) in enumerate(zip(notes,gains,onsets))],('poly',))
    # Silence and broadband transients are negative controls, not pitched truth.
    add('digital_silence','silence',[],('mono','poly'))
    noise=np.random.default_rng(394).normal(0,.04,round(.025*RATE))*np.exp(-np.arange(round(.025*RATE))/(.004*RATE))
    add('unpitched_click','noise',[],('mono','poly'))
    # Replace click waveform while preserving empty reference annotation.
    click=records[-1];audio=np.zeros(click['frames']);audio[round(.05*RATE):round(.05*RATE)+len(noise)]=noise
    click['sha256']=write_wav(root/click['file'],audio)
    add('noise_floor','noise',[],('mono','poly'),background=.0003)
    for name,colored in [('broadband_noise',False),('colored_noise',True)]:
        add(name,'noise',[],('mono','poly'))
        item=records[-1];audio=np.zeros(item['frames']);rng=np.random.default_rng(937 if colored else 938)
        values=rng.normal(0,.025,round(.3*RATE))
        if colored:
            for i in range(1,len(values)):values[i]=.85*values[i-1]+.15*values[i]
        fade=round(.003*RATE);values[:fade]*=np.linspace(0,1,fade);values[-fade:]*=np.linspace(1,0,fade)
        audio[round(.05*RATE):round(.05*RATE)+len(values)]=values
        item['sha256']=write_wav(root/item['file'],audio)
    # Source recordings preserve their original attack, not resynthesized guitar.
    source=root/'tonejs';manifest=json.loads((source/'manifest.json').read_text())
    expected={f['local_file']:f['sha256'] for f in manifest['files']}
    recorded={}
    for name,note in [('E2',40),('A2',45),('D3',50),('G3',55),('B3',59),('E3',52),('E4',64),('Fs3',54)]:
        p=source/(name+'.ogg');assert hashlib.sha256(p.read_bytes()).hexdigest()==expected[p.name]
        decoded=np.frombuffer(subprocess.check_output(['ffmpeg','-v','error','-i',str(p),'-f','f32le','-ac','1','-ar',str(RATE),'pipe:1']),dtype='<f4').astype(float)
        # Preserve 700ms from original file start incl. leading codec silence.
        audio=decoded[:round(.7*RATE)]*.45
        fade=round(.004*RATE);audio[-fade:]*=np.linspace(1,0,fade)
        onset=int(np.flatnonzero(np.abs(audio)>=.001)[0])
        recorded[name]=(note,audio,onset)
        # Metadata onset is first |sample|>=.001 (-60dBFS), not file start.
        add('recorded_'+name,'recorded-single',[(note,.05,audio)],('mono','poly'))
        for mode in ('mono','poly'):cases[-(1 if mode=='poly' else 2)]['truth'][0]['on_sample']=round(.05*RATE)+onset
        records[-1]['truth'][0]['on_sample']=round(.05*RATE)+onset
    for label,names,offsets in [('recorded_E_octaves',['E2','E3','E4'],[.05]*3),('recorded_strings',['E2','A2','D3','G3','B3','E4'],[.05,.057,.064,.071,.078,.085])]:
        mixtures=[]
        for name,offset in zip(names,offsets):
            note,audio,onset=recorded[name];mixtures.append((note,offset,audio*.35))
        add(label,'recorded-note-mixture',mixtures,('poly',))
        for truth,name in zip(records[-1]['truth'],names):truth['on_sample']+=recorded[name][2]
    (root/'fixtures.json').write_text(json.dumps({'version':2,'generator':'Tools/audio_validation_fixtures.py','rate':RATE,'records':records,'cases':cases},indent=2)+'\n')
    rows=['name\tfamily\tmode\twav\ttruth_note_on_off_samples']
    for case in cases:
        truth=','.join(f"{t['note']}:{t['on_sample']}:{t['off_sample']}" for t in case['truth'])
        rows.append('\t'.join([case['name'],case['family'],case['mode'],case['file'],truth]))
    (root/'fixtures.tsv').write_text('\n'.join(rows)+'\n')
    print(f'{len(records)} waveforms / {len(cases)} cases written under {root}')

if __name__=='__main__':main()
