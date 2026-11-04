#!/usr/bin/env python3
"""Seal independent six-string tracking inputs before any prototype execution.
SPDX-License-Identifier: AGPL-3.0-or-later
Only physical-inspired synthesis/reference data; no detector imports or outputs.
"""
from pathlib import Path
import csv,hashlib,json,math
import numpy as np
HERE=Path(__file__).resolve().parent
RATE=12000
SEED=202611041337023

def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def smooth_curve(time,knots):
    value=np.zeros_like(time)
    for (a,x),(b,y) in zip(knots,knots[1:]):
        active=(time>=a)&(time<b);u=(time[active]-a)/(b-a)
        value[active]=x+(y-x)*(.5-.5*np.cos(np.pi*u))
    value[time>=knots[-1][0]]=knots[-1][1]
    return value
def main():
    assert not (HERE/'SEALED_SHA256.txt').exists(),'Refuse to overwrite sealed edition'
    rng=np.random.default_rng(SEED);count=round(1.48*RATE);time=np.arange(count)/RATE
    definitions=[
      ('g_major_static',[43,47,50,55,59,67],'static',False,False),
      ('c_major7_opposed',[43,48,52,55,59,64],'opposed',False,False),
      ('d_add9_vibrato',[42,45,50,57,62,64],'vibrato',False,True),
      ('b7_pickup_nodes',[42,47,51,57,59,66],'opposed',True,False),
      ('e7_reversal_windows',[40,47,50,56,59,65],'reversal',False,True),
      ('a_minor6_weak_tail',[40,45,52,55,60,66],'weak-tail',True,False),
      ('d7_neighbour_attack',[42,45,50,57,60,66],'neighbour',False,False),
      ('g13_stiff_strum',[43,47,53,57,64,69],'strum',True,True)
    ]
    cases=[];inventory=[];seeds=[];truth=[];(HERE/'pcm').mkdir(exist_ok=True)
    for name,notes,kind,coloured,varied in definitions:
        permutation=rng.permutation(6);voices=[];mix=np.zeros(count)
        for string,anchor in enumerate(notes):
            slot=int(permutation[string]);detune=float(rng.uniform(-.075,.075));knots=[(0.,0.),(1.48,0.)]
            if kind=='opposed' and string in (2,5):
                amount=1.35 if string==2 else -.85;knots=[(0.,0.),(.31,0.),(.58,amount),(.73,amount),(.99,-amount*.35),(1.22,0.),(1.48,0.)]
            elif kind=='vibrato' and string in (3,5):knots=[(0.,0.),(.30,0.),(.48,.85 if string==3 else -.55),(.69,-.45 if string==3 else .65),(.91,.70 if string==3 else -.35),(1.20,0.),(1.48,0.)]
            elif kind=='reversal' and string in (1,4):
                sign=1 if string==1 else -1;knots=[(0.,0.),(.32,0.),(.41,sign*.95),(.52,-sign*.45),(.68,sign*.75),(.88,-sign*.60),(1.20,0.),(1.48,0.)]
            elif kind in ('weak-tail','neighbour') and string in (2,5):knots=[(0.,0.),(.35,0.),(.67,1.15 if string==2 else -.65),(1.05,-.25 if string==2 else .30),(1.28,0.),(1.48,0.)]
            elif kind=='strum' and string in (1,4):knots=[(0.,0.),(.37,0.),(.69,1.10 if string==1 else -.60),(1.06,-.25 if string==1 else .35),(1.30,0.),(1.48,0.)]
            pitch=anchor+detune+smooth_curve(time,knots)
            vibrato_depth=.10 if kind=='vibrato' and string in (3,5) else .025 if kind!='static' and string==4 else 0.
            if vibrato_depth:
                gate=np.clip((time-.45)/.07,0,1);pitch+=vibrato_depth*gate*np.sin(2*np.pi*(4.8+.17*string)*time+string*.7)
            start=.006*string if kind=='strum' else 0.
            age=np.maximum(0,time-start);envelope=np.minimum(1,age/.003)*np.exp(-age/(2.4+.23*string));envelope[time<start]=0
            if kind=='weak-tail' and string==4:envelope*=.10+ .90*np.exp(-np.maximum(0,time-.43)/.13)
            if kind=='neighbour' and string==3:
                pulse=np.exp(-((time-.66)/.055)**2)+.8*np.exp(-((time-1.04)/.040)**2);envelope*=1+3*pulse
            level=float(rng.uniform(.035,.075));stiffness=float(rng.uniform(.00001,.00030)) if coloured else 0.
            partials=12 if kind in ('neighbour','strum') else 5 if kind=='weak-tail' and string==4 else 8
            pluck=float(rng.uniform(.13,.36));pickup=float(rng.uniform(.055,.21));phase_offsets=rng.uniform(-np.pi,np.pi,partials)
            fundamental=440*2**((pitch-69)/12);signal=np.zeros(count);amplitudes=[]
            for h in range(1,partials+1):
                harmonic_hz=h*fundamental*np.sqrt((1+stiffness*h*h)/(1+stiffness));phase=np.cumsum(2*np.pi*harmonic_hz/RATE)+phase_offsets[h-1]
                reset=kind=='neighbour' and string==3
                if reset:phase[time>=.66]+=1.47*h
                spectral=(1 if not coloured else max(.11,abs(np.sin(np.pi*h*pluck)*np.cos(np.pi*h*pickup))))/h**(1.18+.035*string)
                if coloured and h==1:spectral*=.22 if string in (1,4) else .65
                amplitude=level*spectral*np.exp(-age*h*.035)
                # Avoid aliasing source partials rather than claiming aliased
                # transients are guitar evidence.
                amplitude=np.where(harmonic_hz<RATE*.43,amplitude,0)
                signal+=amplitude*np.sin(phase);amplitudes.append(float(level*spectral))
            signal*=envelope;mix+=signal
            voices.append(dict(physical_string=string,slot=slot,voice_id=slot,anchor=anchor,seed_pitch=float(pitch[1023]),detune_semitones=detune,level=level,onset_seconds=start,stiffness=stiffness,partials=partials,partial_initial_amplitudes=amplitudes,pluck_fraction=pluck,pickup_fraction=pickup,control_knots=knots,vibrato_depth=vibrato_depth,phase_reset_seconds=.66 if kind=='neighbour' and string==3 else None))
            seeds.append([name,slot,slot,anchor,f'{pitch[1023]:.10f}'])
            for sample in range(127,count,32):truth.append([name,sample,string,slot,anchor,f'{pitch[sample]:.10f}',int(2879<=sample<=16511)])
        assert max(abs(mix))<.95 and np.isfinite(mix).all()
        file='pcm/'+name+'.f32';(HERE/file).write_bytes(mix.astype('<f4').tobytes());observation='varied' if varied else 'full512'
        cases.append(dict(name=name,file=file,frames=count,sha256=sha(HERE/file),kind=kind,voices=voices,observation=observation,peak=float(max(abs(mix))),rms=float(np.sqrt(np.mean(mix*mix)))))
        inventory.append([name,file,count,observation,1023,2879])
    for filename,header,rows in [('cases.tsv',['case','file','frames','observation','seed_sample','warmup_end'],inventory),('seeds.tsv',['case','slot','voice_id','anchor','pitch'],seeds),('truth.tsv',['case','sample','physical_string','voice_id','anchor','pitch','scored'],truth)]:
        with (HERE/filename).open('w',newline='') as f:w=csv.writer(f,delimiter='\t',lineterminator='\n');w.writerow(header);w.writerows(rows)
    manifest=dict(schema=1,edition='independent-continuous-poly-oracle-v1',seed=SEED,sample_rate=RATE,hop_samples=32,oracle_seed_sample=1023,scored_sample_interval=[2879,16511],generator_sha256=sha(Path(__file__)),policy_sha256=sha(HERE/'policy.json'),detector_used_during_generation=False,real_audio=False,note_acquisition_tested=False,musical_readiness_eligible=False,cases=cases)
    (HERE/'fixtures.json').write_text(json.dumps(manifest,indent=2)+'\n');seal=hashlib.sha256(b''.join((HERE/f).read_bytes() for f in ('fixtures.json','cases.tsv','seeds.tsv','truth.tsv','policy.json'))).hexdigest();(HERE/'SEALED_SHA256.txt').write_text(seal+'\n');print('SEALED',len(cases),'six-source controls:',seal)
if __name__=='__main__':main()
