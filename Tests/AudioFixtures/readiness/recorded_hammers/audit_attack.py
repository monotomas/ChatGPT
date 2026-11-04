#!/usr/bin/env python3
"""Offline acoustic annotation audit; does not relabel sealed acceptance.
SPDX-License-Identifier: AGPL-3.0-or-later
Reads original PCM24 recordings, not DSP-filtered/normalized fixture output.
Short-window spectral estimates are diagnostics, not instantaneous F0 truth.
"""
import argparse,csv,hashlib,json,math,wave
from pathlib import Path
import numpy as np
HERE=Path(__file__).resolve().parent

def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def pcm24(path):
    with wave.open(str(path)) as w:
        assert w.getnchannels()==1 and w.getsampwidth()==3
        rate=w.getframerate();b=np.frombuffer(w.readframes(w.getnframes()),dtype=np.uint8).reshape(-1,3).astype(np.int32)
        value=b[:,0]+(b[:,1]<<8)+(b[:,2]<<16);value=np.where(value>=2**23,value-2**24,value)
    return rate,value.astype(float)/2**23
def midi(hz):return float(69+12*np.log2(hz/440))
def measure(audio,rate,anchor,start_ms,end_ms):
    begin=round(start_ms*rate/1000);end=round(end_ms*rate/1000);n=end-begin;hop=16
    assert begin>=0 and n>=32 and end+hop<=len(audio)
    raw=audio[begin:end];window=np.hanning(n);fft_size=262144
    spectrum=np.fft.rfft((raw-np.mean(raw))*window,fft_size);power=abs(spectrum)**2
    peaks=np.flatnonzero((power[1:-1]>power[:-2])&(power[1:-1]>=power[2:]))+1
    f0=440*2**((anchor-69)/12);rows=[]
    for harmonic in range(1,7):
        low=f0*harmonic*2**(-1.7/12);high=f0*harmonic*2**(1.7/12)
        candidates=[k for k in peaks if low<k*rate/fft_size<high]
        if not candidates:continue
        k=max(candidates,key=lambda k:power[k]);a,b,c=np.log(np.maximum(power[k-1:k+2],1e-100));delta=.5*(a-c)/(a-2*b+c)
        frequency=(k+delta)*rate/fft_size
        oscillator=np.exp(-2j*np.pi*frequency*np.arange(n)/rate)
        z0=np.sum(window*audio[begin:end]*oscillator);z1=np.sum(window*audio[begin+hop:end+hop]*oscillator)
        phase_frequency=float(np.angle(z1*np.conj(z0))*rate/(2*np.pi*hop))
        # All searched partials are below the phase hop's ambiguity frequency.
        phase_midi=midi(phase_frequency/harmonic) if phase_frequency>0 else None
        rows.append(dict(harmonic=harmonic,peak_hz=float(frequency),derived_midi=midi(frequency/harmonic),power_db_below_window_peak=float(10*np.log10(power[k]/max(power))),phase_advance_derived_midi=phase_midi))
    selected=[r for r in rows if 2<=r['harmonic']<=5 and r['power_db_below_window_peak']>=-25]
    pitches=[r['derived_midi'] for r in selected];phase=[r['phase_advance_derived_midi'] for r in selected if r['phase_advance_derived_midi'] is not None]
    return dict(start_ms=start_ms,end_ms=end_ms,energy_window_center_ms=(start_ms+end_ms)/2,fft_bin_spacing_hz=rate/fft_size,actual_window_ms=1000*n/rate,partials=rows,selected_harmonics=[r['harmonic'] for r in selected],partial_median_midi=float(np.median(pitches)) if pitches else None,partial_range_midi=[min(pitches),max(pitches)] if pitches else None,phase_median_midi=float(np.median(phase)) if phase else None,warning='Windowed local spectral/phase evidence, not instantaneous acoustic pitch ground truth. Sparse/wide partial consensus is especially uncertain.')
def main():
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('--output',type=Path,required=True);parser.add_argument('--capture',type=Path);args=parser.parse_args()
    assert not args.output.exists(),'Refuse existing audit output'
    provenance=json.loads((HERE/'source/provenance.json').read_text());source_seal=(HERE/'SEALED_SHA256.txt').read_text().strip();result=[]
    events=list(csv.DictReader((args.capture/'events.csv').open())) if args.capture else []
    for item in provenance['recordings']:
        if item['role']!='hammer-on positive':continue
        path=HERE/item['original_relative_path'];assert sha(path)==item['sha256'];rate,audio=pcm24(path);anchor=item['midi_pitch_from_author_sfz']
        fixed=[measure(audio,rate,anchor,a,b) for a,b in ((0,20),(0,28),(5,35),(15,45),(30,60),(50,100),(100,200),(300,500))]
        case=path.stem+'_peak25_mono';first=next((e for e in events if e['case']==case and e['type']=='N'),None);comparison=None
        if first:
            previous=[e for e in events if e['case']==case and e['type']=='P' and int(e['sample'])<=int(first['sample'])]
            event_ms=1000*(int(first['sample'])-16800)/48000
            comparison=dict(case=case,emitted_note=int(first['note']),emitted_pitch=float(previous[-1]['pitch']) if previous else int(first['note']),event_sample=int(first['sample']),sample_playback_relative_ms=event_ms,historical_window=measure(audio,rate,anchor,0,event_ms),centered_20ms=measure(audio,rate,anchor,event_ms-10,event_ms+10),centered_30ms=measure(audio,rate,anchor,event_ms-15,event_ms+15),causal_last_20ms=measure(audio,rate,anchor,event_ms-20,event_ms),timing_warning='Historical-window energy center precedes event time. Centered windows use future source samples only for offline annotation, never as detector latency evidence.')
        result.append(dict(source=item['original_relative_path'],sha256=item['sha256'],nominal_author_sfz_anchor=anchor,original_rate=rate,fixed_windows=fixed,capture_comparison=comparison))
    report=dict(scope='Post hoc acoustic annotation audit of already-seen real isolated hammers; sealed labels/policy/results remain unchanged',script_sha256=sha(Path(__file__)),fixture_seal_before=source_seal,fixture_seal_after=(HERE/'SEALED_SHA256.txt').read_text().strip(),source_repository=provenance['repository'],source_commit=provenance['commit'],method=dict(original_pcm='Direct signed PCM24 decode, original44.1kHz, no filtering/resampling/gain or DSP imports',spectral='Hann-window FFT, local maxima only, log-quadratic interpolation; zero padding improves peak interpolation, not actual time/frequency resolution',frequency_search_prior='Author nominal anchor ±1.7semitones per harmonic; candidate harmonic identity is assumed and must be treated as a limitation',phase='16-source-sample shifted complex projection at each spectral peak; independent phase-advance diagnostic within same audio window',aggregation='Median/range of partials2..5 no lower than25dB beneath window peak; not a certified F0 oracle'),capture_sha256={name:sha(args.capture/name) for name in ('events.csv','capture.csv','measurement-manifest.json')} if args.capture else None,recordings=result)
    args.output.mkdir(parents=True);(args.output/'report.json').write_text(json.dumps(report,indent=2)+'\n');print(args.output/'report.json')
if __name__=='__main__':main()
