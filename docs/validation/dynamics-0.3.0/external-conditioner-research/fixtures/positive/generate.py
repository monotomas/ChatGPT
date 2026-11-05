#!/usr/bin/env python3
"""Reproduce five deterministic positive controls into a NEW directory.
SPDX-License-Identifier: AGPL-3.0-or-later
These are constructed signals, not recorded guitar performances.
"""
from pathlib import Path
import shutil,sys,wave
import numpy as np
out=Path(sys.argv[1]);out.mkdir(exist_ok=False);(out/'pcm').mkdir()
rate=48000;seconds=4.;t=np.arange(round(rate*seconds))/rate;rng=np.random.default_rng(0x46eaf43)
hum=.002*(np.sin(2*np.pi*50.03*t)+.4*np.sin(2*np.pi*100.06*t+.3)+.2*np.sin(2*np.pi*150.09*t+.7))
noise=rng.uniform(-1,1,len(t))*.00003
for base,note,start,end,with_hum,bend in [('G1_bend50',31,.1,1.8,False,True),('G1_bend50_afterhum',31,2.,3.5,True,True),('F4_near350_afterhum',65,2.,3.5,True,False),('E5_near660_afterhum',76,2.,3.5,True,False),('quiet_E2_afterhum',40,2.,3.5,True,False)]:
 pitch=np.full(len(t),440*2**((note-69)/12));age=np.maximum(0,t-start)
 if bend:pitch+=np.clip((age-.3)/.3,0,1)*(50-pitch)
 phase=2*np.pi*np.cumsum(pitch)/rate
 envelope=(1-np.exp(-age/.0015))*np.exp(-age/2)*(t>=start)*(t<end)
 amplitude=.0032 if base.startswith('quiet') else .025
 tone=amplitude*envelope*(np.sin(phase)+.35*np.sin(2*phase+.2)+.15*np.sin(3*phase+.4)+.07*np.sin(5*phase+.6))
 audio=tone+noise+(hum if with_hum else 0)
 with wave.open(str(out/'pcm'/f'{base}.wav'),'wb') as w:w.setnchannels(1);w.setsampwidth(2);w.setframerate(rate);w.writeframes(np.clip(np.rint(audio*32768),-32768,32767).astype('<i2').tobytes())
for name in ('fixtures.tsv','truth.json'):shutil.copy2(Path(__file__).with_name(name),out/name)
