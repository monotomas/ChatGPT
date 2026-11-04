#!/usr/bin/env python3
"""Causal, oracle-free *additional-source* hypothesis probe, not a detector.
SPDX-License-Identifier: AGPL-3.0-or-later

The first source comes from a pre-existing detector. Candidate companions are
proposed from three observed spectral lines, never from a desired voice count.
All fits use only the supplied trailing frame. This is an expensive Python
mechanism/falsification experiment, not callback code or musical acceptance.
"""
from pathlib import Path
import argparse, hashlib, json, math, time
import numpy as np
from scipy.optimize import minimize

RATE=12000
ROOT=Path(__file__).resolve().parent
TWOPI=2*np.pi
def hz(p):return 440*2**((p-69)/12)
def midi(f):return 69+12*np.log2(f/440)

def render(kind,n,start,seed=17,noise=0.):
    # Render absolute-time source to retain one continuously running phase.
    t=(np.arange(n)+start)/RATE
    old=45.;upper=64.
    if kind=='body_resonance':
        return .12*np.sin(TWOPI*110*t)+.30*np.sin(TWOPI*165*t)+.18*np.sin(TWOPI*330*t)+.09*np.sin(TWOPI*495*t),52,[52]
    if kind.startswith('missing_root'):old=52.
    if kind.startswith('stiff'):stiff=.0004
    else:stiff=0.
    # Every source's partials share physical F0 evolution. Stiffness only
    # stretches partials above the root; it cannot lower the third harmonic.
    def source(note,level,harmonics,stiff=0.,third=1.,missing=False,phase=.0):
        p=hz(note)
        f=p*t
        if kind=='vibrato_single':f=p*t-p*.013/(TWOPI*6)*np.cos(TWOPI*6*t)
        elif kind=='chirp_single':f=p*t+70*t*t/2
        out=np.zeros(n)
        for h in range(1,harmonics+1):
            if missing and h==1:continue
            ratio=h*np.sqrt((1+stiff*h*h)/(1+stiff))
            if kind.startswith('pair') and harmonics==3:gain=[1.,.20,.08][h-1]
            else:gain=h**-1.3
            if h%3==0:gain*=third
            angle=TWOPI*ratio*f+phase+.17*h*h
            out+=level*gain*np.sin(angle)
        return out*np.exp(-.18*t)
    pair=kind.startswith('pair')
    if pair:
        x=source(old,.15,3)+source(upper,.11,3,phase=.37)
        truth=[45,64]
    else:
        third=5. if kind in ('third_boost_single','stiff_third_single') else 1.
        x=source(old,.15,12,stiff,third,kind.startswith('missing_root'))
        truth=[int(old)]
    if noise:
        rng=np.random.default_rng(seed);x+=rng.normal(0,noise,n)
    return x,old,truth

def candidates(x,anchor):
    n=len(x);window=np.hanning(n);s=np.abs(np.fft.rfft(x*window,n=8192))
    peak=np.flatnonzero((s[1:-1]>s[:-2])&(s[1:-1]>=s[2:]))+1
    peak=peak[(peak*RATE/8192>35)&(peak*RATE/8192<5000)&(s[peak]>s.max()*.008)]
    order=peak[np.argsort(s[peak])[::-1]][:24]
    frequencies=order*RATE/8192;poss=set()
    for f in frequencies:
        note=int(round(midi(f)))
        if 28<=note<=88 and note!=anchor:
            fundamental=hz(note)
            supported=sum(np.any(np.abs(frequencies-fundamental*h)<max(fundamental*h*.025,RATE/n*.30)) for h in (1,2,3))
            if supported==3:poss.add(note)
    # Noise estimate from the median high spectral power, not known fixture
    # noise. A Hann periodogram's exponential median is log(2) times its mean.
    raw=np.abs(np.fft.rfft(x*window))**2
    variance=float(np.median(raw[3:])/max(np.log(2)*np.sum(window*window),1e-30))
    return sorted(poss),max(variance,1e-12)

def design(parameters,notes,n,dynamic):
    t=(np.arange(n)-(n-1)/2)/RATE
    blocks=[]
    stride=5 if dynamic else 3
    for j,note in enumerate(notes):
        q=parameters[j*stride:(j+1)*stride]
        f=hz(note+q[0]);stiff=q[1]*.001
        h=np.arange(1,13)
        valid=f*h<5400;h=h[valid]
        ratio=h*np.sqrt((1+stiff*h*h)/(1+stiff))
        cycles=f*t
        if dynamic:cycles+=.5*(q[3]*100)*t*t+(q[4]*2000)*t*t*t/6
        phase=TWOPI*cycles[:,None]*ratio[None,:]
        amplitude=np.exp(-q[2]*5*t)[:,None]
        blocks.extend([amplitude*np.sin(phase),amplitude*np.cos(phase)])
    a=np.column_stack(blocks)
    return a-a.mean(axis=0)

def fit(x,notes,dynamic=False):
    n=len(x);y=x-x.mean();stride=5 if dynamic else 3
    initial=np.zeros(len(notes)*stride)
    bounds=([(-.5,.5),(0,1),(0,3),(-3,3),(-3,3)] if dynamic else [(-.5,.5),(0,1),(0,3)])*len(notes)
    # Tiny numerical stabilization, not an envelope prior. Report covariance
    # of the actual fitted system, so unresolved duplicates cannot look sure.
    ridge=n*1e-12
    def solve(p,details=False):
        a=design(p,notes,n,dynamic);g=a.T@a+ridge*np.eye(a.shape[1]);b=a.T@y
        try:beta=np.linalg.solve(g,b)
        except np.linalg.LinAlgError:return (1e12,None,None,None) if details else 1e12
        r=y-a@beta;cost=float(r@r+ridge*(beta@beta))
        return (cost,beta,a,g) if details else cost
    before=time.perf_counter()
    result=minimize(solve,initial,method='L-BFGS-B',bounds=bounds,
                    options={'maxiter':150,'ftol':1e-14,'gtol':1e-10,'maxls':30,'finite_diff_rel_step':1e-5})
    cost,beta,a,g=solve(result.x,True)
    return dict(cost=cost,parameters=result.x.tolist(),columns=a.shape[1],beta=beta,a=a,g=g,
                seconds=time.perf_counter()-before,success=bool(result.success),message=result.message,iterations=result.nit)

def run(kind,n,start,noise,dynamic):
    x,anchor,truth=render(kind,n,start,noise=noise)
    proposed,variance=candidates(x,anchor)
    one=fit(x,[anchor],dynamic);rows=[]
    for candidate in proposed:
        two=fit(x,[anchor,candidate],dynamic)
        delta=(one['cost']-two['cost'])/variance
        penalty=(two['columns']-one['columns']+(5 if dynamic else 3))*math.log(n)
        # A pseudoinverse would silently erase unidentifiable/null directions
        # and give a falsely small variance for exactly shared harmonics.
        _,singular,vt=np.linalg.svd(two['a'],full_matrices=False)
        covariance=(vt.T*(variance/np.maximum(singular,1e-12)**2))@vt
        # Coefficients are ordered sin/cos per source, with 12 partials/source.
        shift=24;upper_h=min(12,int(5400/hz(candidate)))
        z=[]
        for h in range(min(3,upper_h)):
            aa=two['beta'][shift+h];bb=two['beta'][shift+upper_h+h]
            av=covariance[shift+h,shift+h];bv=covariance[shift+upper_h+h,shift+upper_h+h]
            z.append(float(math.sqrt((aa*aa+bb*bb)/max(av+bv,1e-30))))
        root_energy=two['beta'][shift]**2+two['beta'][shift+upper_h]**2
        total_energy=float(two['beta'][shift:]@two['beta'][shift:])
        root_fraction=float(root_energy/max(total_energy,1e-30))
        accepted=delta>penalty and len(z)>=3 and min(z)>3 and root_fraction>.035
        rows.append(dict(note=candidate,accepted=accepted,likelihood_gain=delta,bic_penalty=penalty,
                         coefficient_z_first3=z,root_fraction=root_fraction,cost=two['cost'],parameters=two['parameters'],success=two['success'],iterations=two['iterations'],seconds=two['seconds']))
    best=max((r for r in rows if r['accepted']),key=lambda r:r['likelihood_gain']-r['bic_penalty'],default=None)
    return dict(kind=kind,samples=n,history_ms=1000*n/RATE,frame_end_ms=1000*(start+n)/RATE,
                dynamic=dynamic,generated_noise_std=noise,estimated_noise_variance=variance,
                prior_anchor=anchor,truth_notes=truth,proposed=proposed,admitted_companion=best['note'] if best else None,
                baseline_cost=one['cost'],baseline_parameters=one['parameters'],baseline_success=one['success'],baseline_seconds=one['seconds'],comparisons=rows)

def main():
    p=argparse.ArgumentParser();p.add_argument('--output',type=Path,required=True);p.add_argument('--dynamic',action='store_true');p.add_argument('--quick',action='store_true');args=p.parse_args()
    kinds=['pair_clean','third_boost_single','stiff_third_single','missing_root_single','body_resonance','vibrato_single','chirp_single']
    configs=[(512,700,0.)] if args.quick else [(n,700,noise) for n in (256,512,1024) for noise in (0.,.003,.02)]
    plan={'scope':'Additional-source hypothesis falsification; not full detector/latency/acceptance','kinds':kinds,'configurations':configs,'dynamic':args.dynamic,'root_prior':'Existing detector anchor only; companions from three FFT peaks','selection':'BIC gain against measured median spectral noise; each of first3 companion partials >3 covariance-normalized amplitude'}
    args.output.mkdir(parents=True,exist_ok=False)
    (args.output/'plan.json').write_text(json.dumps(plan,indent=2)+'\n')
    rows=[]
    for kind in kinds:
        for n,start,noise in configs:
            r=run(kind,n,start,noise,args.dynamic);rows.append(r)
            (args.output/'results.json').write_text(json.dumps(rows,indent=2)+'\n')
            print(kind,n,noise,'candidates',r['proposed'],'admit',r['admitted_companion'],'truth',r['truth_notes'],flush=True)
    manifest={'source_sha256':hashlib.sha256(Path(__file__).read_bytes()).hexdigest(),'plan_sha256':hashlib.sha256((args.output/'plan.json').read_bytes()).hexdigest(),'results_sha256':hashlib.sha256((args.output/'results.json').read_bytes()).hexdigest(),'succeeded':False,'note':'Research method; inspect every false admission/miss and runtime, no acceptance claim'}
    (args.output/'manifest.json').write_text(json.dumps(manifest,indent=2)+'\n')
if __name__=='__main__':main()
