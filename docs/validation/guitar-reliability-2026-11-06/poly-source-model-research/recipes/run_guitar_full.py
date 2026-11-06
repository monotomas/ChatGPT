from pathlib import Path
import subprocess
b=Path(__file__).parent
with (b/'guitar-prior/events.csv').open('w') as out:subprocess.run([str(b/'guitar-prior/probe')],stdout=out,check=True)
print('guitar-prior330 complete',flush=True)
for name,source in [('counter-baseline','Source'),('counter-guitar-prior','guitar-prior/Source')]:
 v=b/name;v.mkdir(exist_ok=True);src=[str(p) for p in (b/source/'Pitch').glob('*.cpp') if p.name!='MpeOutputRouter.cpp']
 sourcepath=b/source
 sources=[str(p) for p in (sourcepath/'Pitch').glob('*.cpp') if p.name!='MpeOutputRouter.cpp']
 r=subprocess.run(['g++','-std=c++17','-O2','-DNDEBUG','-I',str(sourcepath),str(b/'counter_probe.cpp'),*sources,'-o',str(v/'probe')],stdout=(v/'compile.log').open('w'),stderr=subprocess.STDOUT)
 print(name,'compile',r.returncode,flush=True)
 if r.returncode:continue
 with (v/'events.csv').open('w') as out:subprocess.run([str(v/'probe')],stdout=out,check=True)
 print(name,'capture complete',flush=True)
