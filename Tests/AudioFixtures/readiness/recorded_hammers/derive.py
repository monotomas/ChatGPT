#!/usr/bin/env python3
"""Derive pinned CC0 hammer-on recording controls, without any detector output.
SPDX-License-Identifier: AGPL-3.0-or-later
This preserves real recorded attack/timbre. It does not create a real continuous
legato transition, nor infer physical gesture-onset time before the sample edit.
"""
from pathlib import Path
import hashlib,json,subprocess,wave
import numpy as np
HERE=Path(__file__).resolve().parent
RATE=48000

def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def main():
    manifest=json.loads((HERE/'source/provenance.json').read_text());(HERE/'pcm').mkdir(exist_ok=True)
    records=[];cases=[];curves=[];start=round(.35*RATE);length=round(1.05*RATE);tail=round(.30*RATE)
    for item in manifest['recordings']:
        if item['role']!='hammer-on positive':continue
        original=HERE/item['original_relative_path'];assert sha(original)==item['sha256']
        decoded=np.frombuffer(subprocess.check_output(['ffmpeg','-v','error','-i',str(original),'-f','f32le','-ac','1','-ar',str(RATE),'pipe:1']),dtype='<f4').astype(float)
        body=decoded[:length].copy();body[-192:]*=np.linspace(1,0,192)
        for peak in (.025,.15):
            name=original.stem+'_peak'+str(round(peak*1000));audio=np.zeros(start+length+tail);audio[start:start+length]=body*(peak/max(abs(body)))
            relative='pcm/'+name+'.wav'
            with wave.open(str(HERE/relative),'wb') as w:
                w.setnchannels(1);w.setsampwidth(2);w.setframerate(RATE);w.writeframes(np.round(audio*32767).astype('<i2').tobytes())
            note=dict(id='s0-n0',string=0,anchor=item['midi_pitch_from_author_sfz'],pitch=item['midi_pitch_from_author_sfz'],on_sample=start,off_sample=start+length,technique='hammer_on_isolated')
            records.append(dict(name=name,file=relative,frames=len(audio),sha256=sha(HERE/relative),origin='derived-real-recording',source=item['original_relative_path'],source_sha256=item['sha256'],peak=peak,crop_source_frames=[0,length],controlled_damping_samples=192,onset_definition='Start of authored sample playback, not unknown original physical gesture before the audio edit',fractional_pitch_groundtruth=False,description='Real isolated CC0 hammer articulation, scalar gain and controlled4ms cutoff; not concatenated/synthesized legato.'))
            for mode in ('mono','poly'):
                cases.append(dict(name=name+'_'+mode,record=name,file=relative,mode=mode,bend_enabled=True,family='real-isolated-hammer',snr_db=None,reference_notes=[note],gain_pair=original.stem,pitch_curve_ground_truth=False))
            for sample in list(range(start,start+length,64))+[start+length-1]:curves.append(f"{name}\t{note['id']}\t{sample}\t{note['pitch']:.9f}")
    policy=(HERE.parent/'policy.json').read_bytes();(HERE/'policy.json').write_bytes(policy)
    result=dict(schema=1,edition='real-isolated-hammers-v1',sample_rate=RATE,baseline_source_commit='c5b05337641b13ae259836f5346e7c1850b7382c',detector_used_during_generation=False,recorded_real_articulations_included=True,real_technique_evidence=['hammer_on_isolated'],missing_required_real_techniques=['continuous_fretted_slide','hammer_transition','pull_off','continuous_bend','vibrato','independent_poly_bend','non_tonal_handling_or_squeak'],original_physical_latency_groundtruth_available=False,derive_script_sha256=sha(Path(__file__)),policy_sha256=hashlib.sha256(policy).hexdigest(),source_repository=manifest['repository'],source_commit=manifest['commit'],source_license=manifest['license'],records=records,cases=cases)
    (HERE/'fixtures.json').write_text(json.dumps(result,indent=2)+'\n');rows=['case\tfamily\tmode\tfile\ttruth\tbend_enabled']
    for c in cases:
        n=c['reference_notes'][0];rows.append('\t'.join([c['name'],c['family'],c['mode'],c['file'],f"{n['anchor']}:{n['on_sample']}:{n['off_sample']}",'1']))
    (HERE/'fixtures.tsv').write_text('\n'.join(rows)+'\n');(HERE/'pitch-curves.tsv').write_text('record\tnote_id\tsample\tpitch\n'+'\n'.join(curves)+'\n')
    seal=hashlib.sha256(b''.join((HERE/n).read_bytes() for n in ('fixtures.json','fixtures.tsv','pitch-curves.tsv','policy.json'))).hexdigest();(HERE/'SEALED_SHA256.txt').write_text(seal+'\n')
    print(f'SEALED {len(records)} derived real hammer waveforms / {len(cases)} mode cases: {seal}')
if __name__=='__main__':main()
