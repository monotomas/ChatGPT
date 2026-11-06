from pathlib import Path
import shutil,subprocess
base=Path(__file__).parent
probe=(base/'probe.cpp').read_text().replace('#include <algorithm>','#include <algorithm>\n#include <cstdlib>').replace('int main(){','int main(int argc,char**argv){').replace(' det->prepare(sr);',' if(argc>1&&int(c)!=std::atoi(argv[1]))continue;\n det->prepare(sr);')
(base/'ablation_probe.cpp').write_text(probe)
for name,anchor,octave in [('no-anchor',True,False),('no-octave',False,True),('no-anchor-octave',True,True)]:
 v=base/name;shutil.copytree(base/'Source',v/'Source',dirs_exist_ok=True)
 p=v/'Source/Pitch/TimeDomainPursuit.cpp';s=p.read_text()
 if anchor:s=s.replace('if(anchor>=0&&anchor!=note&&octaveEvidence','if(false&&anchor>=0&&anchor!=note&&octaveEvidence')
 if octave:s=s.replace('auto refreshOctaveEvidence=[&]() noexcept {','auto refreshOctaveEvidence=[&]() noexcept { return;')
 p.write_text(s)
 src=[str(p) for p in (v/'Source/Pitch').glob('*.cpp') if p.name!='MpeOutputRouter.cpp']
 r=subprocess.run(['g++','-std=c++17','-O2','-DNDEBUG','-I',str(v/'Source'),str(base/'ablation_probe.cpp'),*src,'-o',str(v/'probe')],stdout=(v/'compile.log').open('w'),stderr=subprocess.STDOUT);print(name,'compile',r.returncode,flush=True)
 if r.returncode:continue
 with (v/'openstrings.csv').open('w') as out:subprocess.run([str(v/'probe'),'10'],stdout=out,check=True)
