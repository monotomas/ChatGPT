from pathlib import Path
import subprocess
b=Path(__file__).parent;v=b/'time-atoms-held-prediction'
with (v/'events.csv').open('w') as out:subprocess.run([str(v/'probe')],stdout=out,check=True)
print('330capture complete',flush=True)
for filename,name in [('counter_probe.cpp','physical-counters'),('chord_repick_probe.cpp','chord-repicks')]:
 out=v/name;out.mkdir(exist_ok=True);sources=[str(p) for p in (v/'Source/Pitch').glob('*.cpp') if p.name!='MpeOutputRouter.cpp']
 cmd=['g++','-std=c++17','-O2','-DNDEBUG','-I',str(v/'Source'),str(b/filename),*sources,'-o',str(out/'probe')]
 r=subprocess.run(cmd,stdout=(out/'compile.log').open('w'),stderr=subprocess.STDOUT);print(name,'compile',r.returncode,flush=True)
 if not r.returncode:
  with (out/'events.csv').open('w') as f:subprocess.run([str(out/'probe')],stdout=f,check=True)
 print(name,'capture complete',flush=True)
