#!/usr/bin/env python3
"""Apply the frozen additional-source experiment to actual detector histories.
SPDX-License-Identifier: AGPL-3.0-or-later
Only a previously emitted note is given to the model. Desired companions are
not passed to proposal, fitting, covariance or selection.
"""
from pathlib import Path
import csv,hashlib,importlib.util,json,re
import numpy as np
ROOT=Path(__file__).resolve().parent
method=ROOT/'probe_v5_fixed_uncertainty.py'
spec=importlib.util.spec_from_file_location('physical_admission',method)
probe=importlib.util.module_from_spec(spec);spec.loader.exec_module(probe)
rows=[]
for mode in ('manual','auto'):
    raw=ROOT/f'frontend-{mode}.csv';notes=ROOT/f'frontend-{mode}-notes.txt'
    match=re.search(r'On (\d+) @(\d+)',notes.read_text())
    if not match:raise RuntimeError('Actual detector supplied no anchor')
    anchor,first_sample=map(int,match.groups());frames={}
    for row in csv.DictReader(raw.open()):frames.setdefault(int(row['analysis_sample']),[]).append(float(row['value']))
    for tick in (768,1024,1280,1536,2048,3072):
        assert tick/12000>=first_sample/48000
        frame=np.asarray(frames[tick],dtype=np.float64);assert len(frame)==512
        # Replace only the input renderer, with no note/count truth supplied.
        probe.render=lambda *args,frame=frame,anchor=anchor,**kwargs:(frame,anchor,[])
        result=probe.run('actual-processed-frame',512,tick-512,0.,True)
        result.update(mode=mode,analysis_end_sample=tick,anchor_actual_first_host_sample=first_sample,
                      expected_notes_for_diagnostic_only=[45,64])
        rows.append(result);(ROOT/'frontend-results.json').write_text(json.dumps(rows,indent=2)+'\n')
        print(mode,tick,'proposed',result['proposed'],'admitted',result['admitted_companion'],flush=True)
manifest={'scope':'Actual mainline conditioned histories; additional-source probe only, no integrated MIDI/admission change',
          'method_sha256':hashlib.sha256(method.read_bytes()).hexdigest(),
          'harness_sha256':hashlib.sha256(Path(__file__).read_bytes()).hexdigest(),
          'inputs_sha256':{p.name:hashlib.sha256(p.read_bytes()).hexdigest() for p in ROOT.glob('frontend-*.csv')},
          'results_sha256':hashlib.sha256((ROOT/'frontend-results.json').read_bytes()).hexdigest()}
(ROOT/'frontend-manifest.json').write_text(json.dumps(manifest,indent=2)+'\n')
