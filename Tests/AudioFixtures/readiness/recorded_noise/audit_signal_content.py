#!/usr/bin/env python3
"""Independent signal-content audit; never changes sealed labels or runs DSP.

SPDX-License-Identifier: AGPL-3.0-or-later
Separated components are available only because these controls are constructed
mixtures. Their offline knowledge is not information available to the detector.
"""
from pathlib import Path
import argparse,csv,hashlib,json,math,subprocess,wave
import numpy as np

HERE=Path(__file__).resolve().parent
RATE=48000
CASE='Samples-acoustic-a3_vl1_rr1_1_plus_Samples-acoustic-mute_strings1_rr1_1_snr12_gain40_mono'

def digest(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def rms(x):return float(np.sqrt(np.mean(np.square(x))))
def ratio_db(a,b):return 20*math.log10(a/b) if a>0 and b>0 else None
def decode(p):
    return np.frombuffer(subprocess.check_output(['ffmpeg','-v','error','-i',str(p),'-f','f32le','-ac','1','-ar',str(RATE),'pipe:1']),dtype='<f4').astype(float)
def raw24(p):
    with wave.open(str(p)) as w:
        if w.getnchannels()!=1 or w.getsampwidth()!=3:raise ValueError('Expected original mono PCM24')
        rate=w.getframerate();b=np.frombuffer(w.readframes(w.getnframes()),dtype=np.uint8).reshape(-1,3)
    value=b[:,0].astype(np.int32)|(b[:,1].astype(np.int32)<<8)|(b[:,2].astype(np.int32)<<16)
    value=np.where(value&0x800000,value-0x1000000,value)
    return rate,value.astype(float)/8388608
def projection(x,frequency,rate):
    n=np.arange(len(x));window=np.hanning(len(x))
    return 2*np.sum(x*window*np.exp(-2j*np.pi*frequency*n/rate))/np.sum(window)
def peaks(x,rate,limit=1200):
    window=np.hanning(len(x));nfft=max(131072,2**math.ceil(math.log2(len(x))))
    power=abs(np.fft.rfft((x-np.mean(x))*window,nfft))**2
    selected=np.where((power[1:-1]>power[:-2])&(power[1:-1]>=power[2:]))[0]+1
    selected=[int(k) for k in selected if 30<k*rate/nfft<limit]
    selected=sorted(selected,key=lambda k:power[k],reverse=True)[:10]
    output=[]
    for k in selected:
        a,b,c=np.log(np.maximum(power[k-1:k+2],1e-30));curvature=a-2*b+c
        delta=.5*(a-c)/curvature if curvature<0 else 0
        output.append(dict(frequency_hz=float((k+delta)*rate/nfft),relative_db=float(10*math.log10(power[k]/max(power)))))
    return output

def audit(out):
    source=json.loads((HERE/'source/provenance.json').read_text());fixtures=json.loads((HERE/'fixtures.json').read_text())
    source_by_path={r['original_relative_path']:r for r in source['recordings']}
    for item in source['recordings']:
        if digest(HERE/item['original_relative_path'])!=item['sha256']:raise ValueError('Original source hash differs')
    record=next(r for r in fixtures['records'] if r['name']==CASE[:-5])
    desired_path=HERE/record['all_sources'][0]['path'];noise_path=HERE/record['all_sources'][1]['path']
    desired=decode(desired_path)[:round(1.10*RATE)].copy();desired[-192:]*=np.linspace(1,0,192);desired/=max(abs(desired))
    noise=decode(noise_path);on=round(.25*RATE);off=on+len(desired);wanted=np.zeros(off+round(.30*RATE));unwanted=np.zeros_like(wanted);wanted[on:off]=desired
    for at in record['derivation']['noise_starts_seconds']:
        start=round(at*RATE);end=min(start+len(noise),len(unwanted));unwanted[start:end]+=noise[:end-start]
    scale=rms(wanted[on:off])/rms(unwanted[on:off])/10**(record['derivation']['nominal_snr_db']/20)
    gain=record['derivation']['common_gain'];wanted*=gain;unwanted*=scale*gain
    with wave.open(str(HERE/record['file'])) as w:
        if w.getframerate()!=RATE or w.getsampwidth()!=2:raise ValueError('Unexpected derived PCM')
        retained=np.frombuffer(w.readframes(w.getnframes()),dtype='<i2').astype(float)/32767
    reconstruction=float(max(abs(retained-wanted-unwanted)))
    if reconstruction>.501/32767:raise ValueError('Independent component reconstruction differs from retained PCM')
    events=list(csv.DictReader((HERE/'candidate-mono-v19/events.csv').open()))
    ons=[r for r in events if r['case']==CASE and r['type']=='N'];at=int(ons[1]['sample'])+1
    frame_results=[]
    for milliseconds in (16,24,32,48):
        count=round(RATE*milliseconds/1000);a=wanted[at-count:at];b=unwanted[at-count:at]
        rows=[]
        for harmonic in range(1,9):
            f=220*harmonic;target=projection(a,f,RATE);interference=projection(b,f,RATE);observed=target+interference
            rows.append(dict(harmonic=harmonic,projection_frequency_hz=f,target_amplitude=abs(target),interference_amplitude=abs(interference),projected_component_ratio_db=ratio_db(abs(target),abs(interference)),phase_perturbation_degrees=float(np.degrees(np.angle(observed*np.conj(target))))))
        frame_results.append(dict(trailing_ms=milliseconds,received_last_sample_inclusive=int(ons[1]['sample']),window_start_seconds=(at-count)/RATE,window_end_exclusive_seconds=at/RATE,hann_energy_center_seconds=(at-count+(count-1)/2)/RATE,local_time_domain_snr_db=ratio_db(rms(a),rms(b)),harmonics=rows))
    local_snr=[]
    for end in np.arange(.90,1.151,.01):
        stop=round(end*RATE);start=stop-round(.032*RATE)
        local_snr.append(dict(window_end_seconds=float(end),trailing_ms=32,snr_db=ratio_db(rms(wanted[start:stop]),rms(unwanted[start:stop]))))
    perc_path=HERE/'source/Samples-electric-perc_4_rr1_2.wav';perc_rate,perc=raw24(perc_path)
    noise_case='Samples-electric-perc_4_rr1_2_noise_peak25_mono'
    perc_on=next(r for r in events if r['case']==noise_case and r['type']=='N')
    age=int(perc_on['sample'])/RATE-.8
    spectral=[]
    for start,end,role in ((.02,age,'past-only source prefix available at false On'),(.05,.35,'offline tail characterization, includes future relative to false On')):
        values=perc[round(start*perc_rate):round(end*perc_rate)]
        spectral.append(dict(start_seconds=start,end_seconds=end,role=role,hann_energy_center_seconds=(start+end)/2,peaks=peaks(values,perc_rate)))
    result=dict(scope='Independent annotation/signal audit, not detector validation or relabeling',sealed_labels_unchanged=True,detector_executed=False,source_repository=source['repository'],source_commit=source['commit'],mixture_case=CASE,mixture_pcm_sha256=digest(HERE/record['file']),component_source_sha256={str(p.relative_to(HERE)):digest(p) for p in (desired_path,noise_path,perc_path)},component_reconstruction_max_abs_error=reconstruction,quantization_half_step=.5/32767,global_note_interval_snr_db=ratio_db(rms(wanted[on:off]),rms(unwanted[on:off])),false_rearticulation_sample=int(ons[1]['sample']),false_rearticulation_seconds=int(ons[1]['sample'])/RATE,causal_prefix_windows=frame_results,local_snr_trajectory=local_snr,electric_perc4=dict(original_rate=perc_rate,original_pcm_bits=24,original_duration_seconds=len(perc)/perc_rate,false_note=int(perc_on['note']),false_on_seconds=int(perc_on['sample'])/RATE,source_age_at_false_on_seconds=age,author_program='Programs/electric_noises.sfz',spectral_windows=spectral),limitations=['The suppression intent is an authored benchmark label, not proof that a gesture is acoustically non-tonal.','An approximately harmonic ringing tail does not establish a performer intended the corresponding note.','Source-separated components are known only from benchmark construction; a real detector receives their sum.','Historical Hann windows estimate an older weighted signal interval, not instantaneous pitch/phase at their final sample.','Projected component ratios are exact deterministic component comparisons, not statistical confidence intervals for correlated percussion noise.','Fractional pitch ground truth and original physical gesture timing are unavailable.'])
    out.mkdir(parents=True,exist_ok=True);(out/'signal-audit.json').write_text(json.dumps(result,indent=2,allow_nan=False)+'\n')
    inventory=[HERE/'source/provenance.json',HERE/'fixtures.json',HERE/'SEALED_SHA256.txt',HERE/'candidate-mono-v19/events.csv',Path(__file__).resolve(),desired_path,noise_path,perc_path,HERE/record['file']]
    manifest=dict(scope=result['scope'],inputs_sha256={str(p.relative_to(HERE)):digest(p) for p in inventory},output_sha256=digest(out/'signal-audit.json'),ffmpeg_version=subprocess.check_output(['ffmpeg','-version'],text=True).splitlines()[0],numpy_version=np.__version__)
    (out/'audit-manifest.json').write_text(json.dumps(manifest,indent=2)+'\n')
    print(json.dumps({'reconstruction_error':reconstruction,'false_rearticulation_seconds':result['false_rearticulation_seconds'],'global_snr_db':result['global_note_interval_snr_db'],'h1_h2_phase_32ms':[r['phase_perturbation_degrees'] for r in frame_results[2]['harmonics'][:2]],'perc_false_note':result['electric_perc4']['false_note']},indent=2))

if __name__=='__main__':
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--output',type=Path,default=HERE/'annotation_audit');args=p.parse_args();audit(args.output)
