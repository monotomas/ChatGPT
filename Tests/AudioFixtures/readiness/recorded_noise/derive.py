#!/usr/bin/env python3
"""Independent CC0 microphone/pickup guitar noise and quiet-note controls.

SPDX-License-Identifier: AGPL-3.0-or-later
Preserves recorded gestures; neither renders guitar nor consults a detector.
No file-edit timestamp is represented as an unknown physical guitar gesture.
"""
from pathlib import Path
import hashlib,json,subprocess,wave
import numpy as np
HERE=Path(__file__).resolve().parent
RATE=48000

def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def rms(x):return float(np.sqrt(np.mean(x*x)))
def main():
    source=json.loads((HERE/'source/provenance.json').read_text())
    decoded={}
    for item in source['recordings']:
        p=HERE/item['original_relative_path'];assert sha(p)==item['sha256']
        decoded[p.stem]=np.frombuffer(subprocess.check_output(['ffmpeg','-v','error','-i',str(p),'-f','f32le','-ac','1','-ar',str(RATE),'pipe:1']),dtype='<f4').astype(float)
    (HERE/'pcm').mkdir(exist_ok=True);records=[];cases=[];curves=[]
    def save(name,audio,notes,origin,sources,snr=None,metadata=None):
        assert np.isfinite(audio).all() and max(abs(audio))<1
        relative='pcm/'+name+'.wav'
        with wave.open(str(HERE/relative),'wb') as w:
            w.setnchannels(1);w.setsampwidth(2);w.setframerate(RATE);w.writeframes(np.round(audio*32767).astype('<i2').tobytes())
        records.append(dict(name=name,file=relative,frames=len(audio),sha256=sha(HERE/relative),origin=origin,source=sources[0]['original_relative_path'],source_sha256=sources[0]['sha256'],all_sources=[dict(path=s['original_relative_path'],sha256=s['sha256']) for s in sources],derivation=metadata))
        family='real-recorded-noise-only' if not notes else 'real-quiet-pluck' if snr is None else 'real-note-plus-recorded-noise'
        for mode in ('mono','poly'):cases.append(dict(name=name+'_'+mode,record=name,file=relative,mode=mode,bend_enabled=True,family=family,snr_db=snr,reference_notes=notes,pitch_curve_ground_truth=False))
        for n in notes:
            for at in list(range(n['on_sample'],n['off_sample'],64))+[n['off_sample']-1]:curves.append(f"{name}\t{n['id']}\t{at}\t{n['anchor']:.9f}")
    noises=[s for s in source['recordings'] if s['role']=='author-labelled noise']
    tones=[s for s in source['recordings'] if s['role']=='quiet plucked note']
    for s in noises:
        body=decoded[Path(s['original_relative_path']).stem];body=body/max(abs(body))
        for peak in (.025,.15):
            audio=np.zeros(round(1.60*RATE))
            for at in (.10,.80):audio[round(at*RATE):round(at*RATE)+len(body)]+=body*peak
            save(Path(s['original_relative_path']).stem+'_noise_peak'+str(round(peak*1000)),audio,[],'repeated-real-noise-sample',[s],metadata=dict(noise_peak=peak,sample_starts_seconds=[.10,.80],author_label='muted strings or percussion; assigned suppression intent for these controls'))
    for s in tones:
        body=decoded[Path(s['original_relative_path']).stem][:round(1.10*RATE)].copy();body[-192:]*=np.linspace(1,0,192);body/=max(abs(body))
        for start in (0,.35):
            for peak in (.025,.15):
                on=round(start*RATE);off=on+len(body);audio=np.zeros(off+round(.30*RATE));audio[on:off]=body*peak
                n=dict(id='s0-n0',string=0,anchor=s['midi_pitch_from_author_sfz'],pitch=s['midi_pitch_from_author_sfz'],on_sample=on,off_sample=off,technique='plucked_note_isolated')
                save(Path(s['original_relative_path']).stem+'_quiet_start'+str(round(start*1000))+'_peak'+str(round(peak*1000)),audio,[n],'derived-real-recording',[s],metadata=dict(peak=peak,controlled_damping_samples=192,onset_definition='Authored sample playback start; physical gesture timing before source edit unknown'))
        selected=[n for n in noises if n['source_capture']==s['source_capture'] and any(t in n['original_relative_path'] for t in ('mute_strings1','perc_4'))]
        for noise in selected:
            nb=decoded[Path(noise['original_relative_path']).stem];on=round(.25*RATE);off=on+len(body);tone=np.zeros(off+round(.30*RATE));tone[on:off]=body
            unwanted=np.zeros_like(tone)
            for at in (.45,.95):
                begin=round(at*RATE);end=min(begin+len(nb),len(unwanted));unwanted[begin:end]+=nb[:end-begin]
            for snr in (6,12):
                scale=rms(tone[on:off])/max(rms(unwanted[on:off]),1e-20)/10**(snr/20)
                base=tone+unwanted*scale
                for gain in (.04,.16):
                    audio=base*gain
                    n=dict(id='s0-n0',string=0,anchor=s['midi_pitch_from_author_sfz'],pitch=s['midi_pitch_from_author_sfz'],on_sample=on,off_sample=off,technique='plucked_note_isolated')
                    name=Path(s['original_relative_path']).stem+'_plus_'+Path(noise['original_relative_path']).stem+'_snr'+str(snr)+'_gain'+str(round(gain*1000))
                    save(name,audio,[n],'real-recording-mixture',[s,noise],snr,dict(noise_starts_seconds=[.45,.95],nominal_snr_db=snr,measured_snr_db=20*np.log10(rms(tone[on:off])/rms((unwanted*scale)[on:off])),snr_definition='RMS desired/noise over complete known note-active interval, before common gain',common_gain=gain,controlled_damping_samples=192))
    policy=(HERE.parent/'policy.json').read_bytes();(HERE/'policy.json').write_bytes(policy)
    manifest=dict(schema=1,edition='real-muted-percussive-and-quiet-controls-v1',sample_rate=RATE,baseline_source_commit='c5b05337641b13ae259836f5346e7c1850b7382c',detector_used_during_generation=False,recorded_real_articulations_included=True,real_technique_evidence=['muted_percussive_noise','plucked_note_isolated'],missing_required_real_techniques=['continuous_fretted_slide','hammer_transition','pull_off','continuous_bend','vibrato','independent_poly_bend','real_fret_squeak','real_mains_hum'],original_physical_latency_groundtruth_available=False,derive_script_sha256=sha(Path(__file__)),policy_sha256=hashlib.sha256(policy).hexdigest(),source_repository=source['repository'],source_commit=source['commit'],source_license=source['license'],paired_capture_warning=source['paired_capture_warning'],semantic_limit=source['intent_limit'],records=records,cases=cases)
    (HERE/'fixtures.json').write_text(json.dumps(manifest,indent=2)+'\n');rows=['case\tfamily\tmode\tfile\ttruth\tbend_enabled']
    for c in cases:rows.append('\t'.join([c['name'],c['family'],c['mode'],c['file'],','.join(f"{n['anchor']}:{n['on_sample']}:{n['off_sample']}" for n in c['reference_notes']),'1']))
    (HERE/'fixtures.tsv').write_text('\n'.join(rows)+'\n');(HERE/'pitch-curves.tsv').write_text('record\tnote_id\tsample\tpitch\n'+'\n'.join(curves)+'\n')
    seal=hashlib.sha256(b''.join((HERE/n).read_bytes() for n in ('fixtures.json','fixtures.tsv','pitch-curves.tsv','policy.json'))).hexdigest();(HERE/'SEALED_SHA256.txt').write_text(seal+'\n');print(f'SEALED {len(records)} waveforms / {len(cases)} mode cases: {seal}')
if __name__=='__main__':main()
