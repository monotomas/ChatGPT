#!/usr/bin/env python3
"""Independent controlled readiness inputs, generated without detector output.

SPDX-License-Identifier: AGPL-3.0-or-later
These are synthesized controls, never substitutes for recorded technique data.
Regeneration needs NumPy. Normal measurement uses retained PCM16 waveforms.
"""
from __future__ import annotations
import argparse
import hashlib
import json
from pathlib import Path
import wave
import numpy as np

HERE = Path(__file__).resolve().parent
RATE = 48000
CURVE_HOP = 64


def sha(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def smoothstep(x):
    x = np.clip(x, 0, 1)
    return x * x * (3 - 2 * x)


def timeline(items, string=0):
    return [dict(id=f's{string}-n{i}', string=string, anchor=int(round(pitch)),
                 on_sample=round(on * RATE), off_sample=round(off * RATE),
                 pitch=float(pitch), technique=technique)
            for i, (pitch, on, off, technique) in enumerate(items)]


def make_string(notes, seconds, seed, peak=.12, stiffness=.000055,
                curve=None, weak_contact=False, morph=False):
    rng = np.random.default_rng(seed)
    n = round(seconds * RATE)
    t = np.arange(n) / RATE
    pitch = np.full(n, float(notes[0]['pitch']))
    amplitude = np.zeros(n)
    active = np.zeros(n, dtype=bool)
    for index, note in enumerate(notes):
        a, b = note['on_sample'], note['off_sample']
        age = np.arange(b - a) / RATE
        if curve is None:
            pitch[a:b] = note['pitch']
        else:
            pitch[a:b] = curve(t[a:b] - notes[0]['on_sample'] / RATE)
        # Hammer and pull transitions deliberately have no amplitude attack.
        if index == 0 or note['technique'] in ('pluck', 'repick'):
            env = (1 - np.exp(-age / .0009)) * np.exp(-age / 1.2)
        else:
            total_age = (np.arange(a, b) - notes[0]['on_sample']) / RATE
            env = np.exp(-total_age / 1.2) * (.83 ** index)
        amplitude[a:b] = env
        active[a:b] = True
    # The phase remains continuous over abrupt fretted pitch changes. Neither
    # an amplitude surge nor a fresh global phase is required for a new note.
    frequency = 440 * 2 ** ((pitch - 69) / 12)
    phase = np.cumsum(2 * np.pi * frequency / RATE)
    audio = np.zeros(n)
    for harmonic in range(1, min(20, int(RATE * .43 / np.max(frequency))) + 1):
        partial = harmonic * np.sqrt((1 + stiffness * harmonic * harmonic) / (1 + stiffness))
        weight = (abs(np.sin(.173 * np.pi * harmonic)) + .13) / harmonic ** 1.16
        envelope = np.exp(-t * (.15 + harmonic * .11))
        if morph:
            transition = smoothstep((t - .58) / .30)
            envelope *= (1 - transition) + transition * (.22 if harmonic % 2 else 1.55)
        audio += weight * envelope * np.sin(phase * partial + rng.uniform(-np.pi, np.pi))
    audio *= amplitude
    if weak_contact:
        for note in notes[1:]:
            a = note['on_sample']; count = min(96, n - a)
            burst = rng.normal(0, .002, count) * np.exp(-np.arange(count) / 18)
            audio[a:a + count] += burst
    for note in notes:
        b = note['off_sample']
        if b == notes[-1]['off_sample']:
            width = min(96, b - note['on_sample'])
            audio[b - width:b] *= np.linspace(1, 0, width)
    maximum = np.max(np.abs(audio), initial=0)
    if maximum:
        audio *= peak / maximum
    return audio, pitch, active


def mains(seconds, base, seed, rms, drift=.0):
    rng = np.random.default_rng(seed)
    t = np.arange(round(seconds * RATE)) / RATE
    phase = 2 * np.pi * (base * t + .5 * drift * t * t / max(seconds, .001))
    result = np.zeros(len(t))
    for harmonic in range(1, 11):
        result += np.sin(phase * harmonic + rng.uniform(-np.pi, np.pi)) / harmonic ** 1.38
    return result * (rms / max(1e-12, np.sqrt(np.mean(result * result))))


def handling(seconds, seed, level=.008):
    rng = np.random.default_rng(seed)
    result = np.zeros(round(seconds * RATE))
    # Damped, detuned/inharmonic contact resonances, not fixed-note templates.
    for at in (.16, .37, .61, .94, 1.17, 1.43):
        count = round(rng.uniform(.018, .045) * RATE)
        age = np.arange(count) / RATE
        value = np.zeros(count)
        root = rng.uniform(88, 190)
        for ratio in (1., 1.39, 2.21, 3.73, 5.18):
            value += rng.uniform(.15, 1) * np.sin(2 * np.pi * root * ratio * age + rng.uniform(-np.pi, np.pi))
        value += rng.normal(0, .6, count)
        value *= np.exp(-age / rng.uniform(.003, .008))
        value *= level / max(1e-12, np.max(np.abs(value)))
        start = round(at * RATE); result[start:start + count] += value[:len(result) - start]
    return result


def squeaks(seconds, seed, level=.006):
    rng = np.random.default_rng(seed)
    result = np.zeros(round(seconds * RATE))
    for at, duration in ((.22, .12), (.57, .18), (1.02, .15), (1.40, .11)):
        count = round(duration * RATE); age = np.arange(count) / RATE
        start_hz, end_hz = rng.uniform(600, 1100), rng.uniform(2400, 3900)
        instantaneous = np.linspace(start_hz, end_hz, count)
        instantaneous += 95 * np.sin(2 * np.pi * 47 * age)
        phase = np.cumsum(2 * np.pi * instantaneous / RATE)
        value = np.sin(phase) + .45 * np.sin(phase * 1.37 + .3)
        value += rng.normal(0, .27, count)
        value *= np.sin(np.pi * np.arange(count) / count) ** 1.3
        value *= level / max(1e-12, np.max(np.abs(value)))
        start = round(at * RATE);result[start:start + count] += value[:len(result) - start]
    return result


def make(output):
    output.mkdir(parents=True, exist_ok=True);(output / 'pcm').mkdir(exist_ok=True)
    records=[];cases=[];curves=[]
    def write(name, audio, notes, pitches, family, *, modes=('mono','poly'), snr=None, detail='', gain_pair=None):
        if not np.isfinite(audio).all() or np.max(np.abs(audio), initial=0) >= .98:
            raise ValueError('Invalid/nonfinite/clipped control '+name)
        relative='pcm/'+name+'.wav';pcm=np.round(np.clip(audio,-1,1)*32767).astype('<i2')
        with wave.open(str(output/relative),'wb') as stream:
            stream.setnchannels(1);stream.setsampwidth(2);stream.setframerate(RATE);stream.writeframes(pcm.tobytes())
        records.append(dict(name=name,file=relative,frames=len(audio),sha256=sha(output/relative),origin='controlled-synthesis',family=family,description=detail))
        for mode in modes:
            cases.append(dict(name=name+'_'+mode,record=name,file=relative,mode=mode,bend_enabled=True,
                              family=family,snr_db=snr,reference_notes=notes,gain_pair=gain_pair))
        for note in notes:
            pitch=pitches[note['string']]
            points=sorted(set(range(note['on_sample'],note['off_sample'],CURVE_HOP))|{note['on_sample'],note['off_sample']-1})
            curves.extend(f"{name}\t{note['id']}\t{sample}\t{pitch[sample]:.9f}" for sample in points)
    seconds=2.1;size=round(seconds*RATE)
    # Pure noises and pitched electrical interference are separate labelled families.
    for base in (50,60):
        for level in (.00045,.0036):
            noise=mains(seconds,base,120300+base,rms=level,drift=.13)
            write(f'hum{base}_{round(level*1e6)}',noise,[],{},'stationary-mains',detail='Electrical mains comb, present from input start; no played string.')
    write('handling_damped_contacts',handling(seconds,441201,.018),[],{},'non-tonal-handling',detail='Synthesized damped inharmonic half-muted string/contact resonances; not a recording.')
    write('fret_squeaks',squeaks(seconds,441202,.016),[],{},'non-tonal-squeak',detail='Synthesized sliding friction ridges with no harmonic fundamental; not recorded fret noise.')
    rng=np.random.default_rng(441203)
    noise=rng.normal(0,.003,size);write('broadband_noise',noise,[],{},'broadband-noise',detail='Stationary Gaussian noise, no pitched source.')
    combined=handling(seconds,441204,.012)+squeaks(seconds,441205,.010)+mains(seconds,50,441206,.001)
    write('handling_squeak_hum',combined,[],{},'mixed-unwanted',detail='Known electrical and non-tonal interference, no intended tonal voice.')
    # Quiet desired notes with pre-existing hum. Gain variants share exact SNR.
    for index,note_number in enumerate((31,40,52,64)):
        notes=timeline([(note_number,.50,1.55,'pluck')]);clean,pitch,active=make_string(notes,seconds,512300+index,peak=.0032,morph=True)
        source_noise=mains(seconds,50,512400+index,.00010,drift=.08)
        source_noise+=np.random.default_rng(512500+index).normal(0,.00004,size)
        for gain in (1,8):
            name=f'quiet_hum_{note_number}_gain{gain}'
            write(name,(clean+source_noise)*gain,notes,{0:pitch},'quiet-guitar-mains',snr=round(10*np.log10(np.mean(clean[active]**2)/np.mean(source_noise[active]**2)),3),gain_pair=f'quiet_hum_{note_number}',detail='A desired weak pluck near/beside mains after500ms interference; exact common-gain pair, no compression.')
    # Music + string handling at fixed independent SNR, with ordinary/boosted input.
    for index,snr_db in enumerate((6,12,24)):
        notes=timeline([(57,.45,1.53,'pluck')]);clean,pitch,active=make_string(notes,seconds,614100+index,peak=.025,morph=True)
        interference=handling(seconds,614200+index,.5)+squeaks(seconds,614300+index,.5)
        interference+=mains(seconds,60,614400+index,.09)
        scale=np.sqrt(np.mean(clean[active]**2)/(np.mean(interference[active]**2)*10**(snr_db/10)))
        interference*=scale
        for gain in (1,4):
            write(f'music_handling_snr{snr_db}_gain{gain}',(clean+interference)*gain,notes,{0:pitch},'music-plus-handling',snr=snr_db,gain_pair=f'music_handling_snr{snr_db}',detail='Pitched desired string plus independent non-tonal handling/squeak/mains; common-gain/SNR pair.')
    # Fretted slides have discrete plateaus, with no amplitude attack per fret.
    for step_seconds in (.035,.065,.105):
        start=.25;sequence=[57,58,59,60,61,62,63,64]
        notes=timeline([(note,start+i*step_seconds,start+(i+1)*step_seconds,'pluck' if i==0 else 'fretted_slide') for i,note in enumerate(sequence)])
        notes[-1]['off_sample']+=round(.25*RATE)
        audio,pitch,_=make_string(notes,seconds,720100+round(step_seconds*1000),peak=.12,weak_contact=True)
        write(f'fretted_slide_up_{round(step_seconds*1000)}ms',audio,notes,{0:pitch},'fretted-slide',detail='One physical string; stepwise semitone plateaus, phase continuous, no per-fret amplitude attack.')
        down=timeline([(note,start+i*step_seconds,start+(i+1)*step_seconds,'pluck' if i==0 else 'fretted_slide') for i,note in enumerate(reversed(sequence))]);down[-1]['off_sample']+=round(.25*RATE)
        audio,pitch,_=make_string(down,seconds,720200+round(step_seconds*1000),peak=.12,weak_contact=True)
        write(f'fretted_slide_down_{round(step_seconds*1000)}ms',audio,down,{0:pitch},'fretted-slide',detail='Descending discrete frets; no bottleneck/continuous pitch interpolation.')
    for contact in (False,True):
        notes=timeline([(52,.25,.57,'pluck'),(55,.57,.88,'hammer_on'),(59,.88,1.17,'hammer_on'),(55,1.17,1.48,'pull_off'),(52,1.48,1.77,'pull_off')])
        audio,pitch,_=make_string(notes,seconds,830100+contact,peak=.10,weak_contact=contact)
        write('hammer_pull_'+('tiny_contact' if contact else 'no_amplitude_attack'),audio,notes,{0:pitch},'hammer-pull',detail='Legato pitch steps, decreasing amplitude; new intended notes without picking attack.')
    # Matched continuous bends do not create new note lifecycles at semitones.
    for duration in (.08,.22,.42):
        notes=timeline([(57,.25,1.65,'pluck')])
        def curve(t,duration=duration):
            return 57+2*smoothstep((t-.20)/duration)-2*smoothstep((t-.88)/duration)+.09*np.sin(2*np.pi*5.2*t)*smoothstep((t-.6)/.15)
        audio,pitch,_=make_string(notes,seconds,940100+round(duration*1000),peak=.10,curve=curve,morph=True)
        write(f'smooth_bend_{round(duration*1000)}ms',audio,notes,{0:pitch},'continuous-bend',detail='A phase-continuous bent string; smooth rise/return through all intermediate pitches and later vibrato.')
    # Independently bending polyphonic voices, strums and voice withdrawals.
    for index,anchors in enumerate(((52,59),(45,52,57),(40,47,52,56),(52,55,59,62))):
        all_notes=[];all_pitches={};audio=np.zeros(size)
        for string,note_number in enumerate(anchors):
            onset=.35+string*.018;off=1.70-string*.07
            notes=timeline([(note_number,onset,off,'pluck')],string=string)
            if string==0:
                def curve(t,note_number=note_number):return note_number+1.7*smoothstep((t-.27)/.30)-.8*smoothstep((t-.91)/.23)
            elif string==1:
                def curve(t,note_number=note_number):return note_number-.9*smoothstep((t-.59)/.25)+.055*np.sin(2*np.pi*4.7*t)*smoothstep((t-.2)/.1)
            else:curve=None
            part,pitch,_=make_string(notes,seconds,105010+index*37+string,peak=.11/len(anchors),curve=curve,stiffness=.000045+string*.000012)
            audio+=part;all_notes+=notes;all_pitches[string]=pitch
        write(f'poly_independent_bends_{len(anchors)}voices_{index}',audio,all_notes,all_pitches,'poly-independent-bend',modes=('poly',),detail='Independently curved/steady physical strings; strummed starts and separate damping; no shared global bend truth.')
    # A real quiet new tone must remain eligible after noise and a decayed tone.
    notes=timeline([(45,.30,.78,'pluck'),(57,1.02,1.56,'pluck')]);audio,pitch,active=make_string(notes,seconds,118012,peak=.06)
    audio[round(.98*RATE):]*=.14
    audio+=handling(seconds,118013,.0025)+mains(seconds,50,118014,.00015)
    write('quiet_new_note_after_decay_noise',audio,notes,{0:pitch},'quiet-rearticulation',detail='A quieter intended new pluck after a damped old string and interference; no long fixed gate lockout is allowed.')
    manifest=dict(schema=1,edition='controlled-readiness-v1',sample_rate=RATE,baseline_source_commit='c5b05337641b13ae259836f5346e7c1850b7382c',
                  detector_used_during_generation=False,recorded_real_articulations_included=False,
                  synthetic_controls_do_not_establish_musical_readiness=True,generator_sha256=sha(Path(__file__)),policy_sha256=sha(HERE/'policy.json'),records=records,cases=cases)
    (output/'fixtures.json').write_text(json.dumps(manifest,indent=2)+'\n')
    rows=['case\tfamily\tmode\tfile\ttruth\tbend_enabled']
    for case in cases:
        truth=','.join(f"{n['anchor']}:{n['on_sample']}:{n['off_sample']}" for n in case['reference_notes'])
        rows.append('\t'.join([case['name'],case['family'],case['mode'],case['file'],truth,'1']))
    (output/'fixtures.tsv').write_text('\n'.join(rows)+'\n')
    (output/'pitch-curves.tsv').write_text('record\tnote_id\tsample\tpitch\n'+'\n'.join(curves)+'\n')
    for filename in ('policy.json',):
        if output.resolve()!=HERE.resolve():(output/filename).write_bytes((HERE/filename).read_bytes())
    seal=hashlib.sha256(b''.join((output/n).read_bytes() for n in ('fixtures.json','fixtures.tsv','pitch-curves.tsv','policy.json'))).hexdigest()
    (output/'SEALED_SHA256.txt').write_text(seal+'\n')
    print(f'SEALED {len(records)} controlled PCM files / {len(cases)} mode cases: {seal}')


if __name__=='__main__':
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('--output',type=Path,default=HERE)
    make(parser.parse_args().output)
