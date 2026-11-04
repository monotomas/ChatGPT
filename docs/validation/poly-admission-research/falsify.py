#!/usr/bin/env python3
"""Additional single-string physical/model-mismatch falsification.
SPDX-License-Identifier: AGPL-3.0-or-later
The input's one-source label is retained only after inference, never passed to
candidate construction, fitting or model selection.
"""
from pathlib import Path
import hashlib,importlib.util,json
import numpy as np
ROOT=Path(__file__).resolve().parent
method=ROOT/'probe_v5_fixed_uncertainty.py'
spec=importlib.util.spec_from_file_location('physical_admission_falsify',method)
probe=importlib.util.module_from_spec(spec);spec.loader.exec_module(probe)
cases=[]
for note in (40,45,52):
    for stiffness in (0.,.0004):
        for boost in (1.,5.):
            cases.append(dict(name=f'partial_decay_{note}_{stiffness}_{boost}',kind='partial_decay',note=note,stiffness=stiffness,boost=boost))
for kind in ('same_note_phase_reset','odd_partial_morph','gain_swell','gain_withdrawal'):
    cases.append(dict(name=kind,kind=kind,note=45,stiffness=.0003,boost=1.))
out=ROOT/'physical-mismatch';out.mkdir(exist_ok=False)
(out/'plan.json').write_text(json.dumps({'scope':'Post-development synthetic falsification, not unseen acceptance','cases':cases,'samples':512,'rate':12000,'frame_start_sample':700},indent=2)+'\n')
rows=[]
for case in cases:
    n=512;t=(np.arange(n)+700)/12000;f=probe.hz(case['note']);x=np.zeros(n)
    for h in range(1,13):
        ratio=h*np.sqrt((1+case['stiffness']*h*h)/(1+case['stiffness']))
        phase=2*np.pi*f*ratio*t+.23*h*h
        amp=.15/h**1.3*(case['boost'] if h%3==0 else 1.)
        env=np.exp(-(.18+(.03*h*h if case['kind']=='partial_decay' else 0))*t)
        if case['kind']=='same_note_phase_reset':phase+=np.where(t>=.08,.85*h+.2*h*h,0)
        if case['kind']=='odd_partial_morph':env*=1+((.08 if h%2 else 2.)-1)*np.clip((t-.072)/.015,0,1)
        if case['kind']=='gain_swell':env*=np.exp(20*(t-t[0]))
        if case['kind']=='gain_withdrawal':env*=np.where(t<.08,1,.03)
        x+=amp*env*np.sin(phase)
    probe.render=lambda *args,x=x,note=case['note'],**kwargs:(x,note,[])
    row=probe.run(case['name'],512,700,0.,True)
    row.update(generation=case,expected_source_count_for_scoring_only=1)
    rows.append(row);(out/'results.json').write_text(json.dumps(rows,indent=2)+'\n')
    print(case['name'],'proposed',row['proposed'],'admit',row['admitted_companion'],flush=True)
(out/'manifest.json').write_text(json.dumps({'method_sha256':hashlib.sha256(method.read_bytes()).hexdigest(),'driver_sha256':hashlib.sha256(Path(__file__).read_bytes()).hexdigest(),'results_sha256':hashlib.sha256((out/'results.json').read_bytes()).hexdigest()},indent=2)+'\n')
