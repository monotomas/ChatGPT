#!/usr/bin/env python3
"""Reproduce constructed calibration controls into a NEW folder, not recorded guitar.
SPDX-License-Identifier: AGPL-3.0-or-later
Usage: python generate.py /workspace/ChatGPT /tmp/new-trained-fixtures
"""
from pathlib import Path
import importlib.util,json,sys,wave
import numpy as np
repository=Path(sys.argv[1]);fx=Path(sys.argv[2]);fx.mkdir(exist_ok=False);(fx/'pcm').mkdir()
spec=importlib.util.spec_from_file_location('gen',repository/'Tests/AudioFixtures/readiness/generate.py');g=importlib.util.module_from_spec(spec);spec.loader.exec_module(g)
rate=48000;seconds=23.;size=round(seconds*rate);records=[];truth=[]
for tag,note,kind in [('quiet31',31,'note'),('quiet40',40,'note'),('quiet52',52,'note'),('nearF4',65,'note'),('nearE5',76,'note'),('handling',None,'handling')]:
 for common in [1,8]:
  rng=np.random.default_rng(0x43af)
  noise=g.mains(seconds,50,512403,.00010,drift=.08)+rng.normal(0,.00004,size)
  wanted=np.zeros(size)
  for j,on in enumerate(np.arange(3.25,19.,2.1)):
   count=round(1.5*rate);tone,_,_=g.make_string(g.timeline([(int([40,52,57,64][j%4]),0.,1.5,'pluck')]),1.5,0x43af+j,peak=[.006,.025,.0032,.012][j%4],morph=True)
   a=round(on*rate);wanted[a:a+count]+=tone
  if kind=='note':
   tone,_,_=g.make_string(g.timeline([(note,.5,1.55,'pluck')]),2.1,512303,peak=.0032 if tag.startswith('quiet') else .025,morph=True)
   wanted[round(20*rate):round(22.1*rate)]+=tone
  else:wanted[round(20*rate):round(22.1*rate)]+=g.handling(2.1,441201,.004)+g.squeaks(2.1,441202,.003)
  audio=(noise+wanted)*common;name=tag+f'_gain{common}';target=fx/'pcm'/(name+'.wav')
  with wave.open(str(target),'wb') as w:w.setnchannels(1);w.setsampwidth(2);w.setframerate(rate);w.writeframes(np.clip(np.rint(audio*32768),-32768,32767).astype('<i2').tobytes())
  for mode in ['mono','poly']:
   case=name+'_'+mode;records.append(f'{case}\ttrained-mechanism\t{mode}\tpcm/{name}.wav\t\t1');truth.append({'case':case,'anchor':note,'on_sample':round(20.5*rate) if note is not None else None,'off_sample':round(21.55*rate) if note is not None else None,'training':'3squiet+17srepresentativeplaying;then500msquietbeforetestnote;rawhumphasecontinuous','kind':kind})
(fx/'fixtures.tsv').write_text('case\tfamily\tmode\tfile\ttruth\tbend_enabled\n'+'\n'.join(records)+'\n');(fx/'truth.json').write_text(json.dumps(truth,indent=2)+'\n')
