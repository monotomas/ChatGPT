from pathlib import Path
import shutil,subprocess
b=Path(__file__).parent
for name,source,cutoff in [('guitar-low-cost','guitar-prior/Source',.006),('time-atoms-guitar-low-cost','time-envelope-atoms-guitar/Source',.006),('time-atoms-guitar-mid-cost','time-envelope-atoms-guitar/Source',.02)]:
 v=b/name;shutil.copytree(b/source,v/'Source',dirs_exist_ok=True)
 p=v/'Source/Pitch/TimeDomainPursuit.cpp';s=p.read_text().replace('const double cutoff=.06+.12*(1-sensitivity);',f'const double cutoff={cutoff};');p.write_text(s)
 src=[str(p) for p in (v/'Source/Pitch').glob('*.cpp') if p.name!='MpeOutputRouter.cpp']
 r=subprocess.run(['g++','-std=c++17','-O2','-DNDEBUG','-I',str(v/'Source'),str(b/'ablation_probe.cpp'),*src,'-o',str(v/'probe')],stdout=(v/'compile.log').open('w'),stderr=subprocess.STDOUT);print(name,'compile',r.returncode,flush=True)
 if not r.returncode:
  with (v/'openstrings.csv').open('w') as out:subprocess.run([str(v/'probe'),'10'],stdout=out,check=True)
