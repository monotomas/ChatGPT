from pathlib import Path
import shutil,subprocess
base=Path(__file__).parent
original=(base/'tuning-bank/Source/Pitch/PitchDetector.cpp').read_text()
a=original.index('    // Private audit: observed-spectrum tuning consensus')
b=original.index("    // A chord's common divisor",a)
insert=original[a:b]
insert=insert.replace('    int tuningBest=tunedPursuitIndex;','    int tuningBest=tunedPursuitIndex;').replace('    if(tuningVotes[size_t(tuningBest)]>tuningVotes[size_t(tunedPursuitIndex)]*1.10)tunedPursuitIndex=tuningBest;','    if(view.activeNotes==0&&tuningVotes[size_t(tuningBest)]>tuningVotes[size_t(tunedPursuitIndex)]*1.10)tunedPursuitIndex=tuningBest;')
for name,source in [('stable-tuning','Source'),('stable-tuning-guitar','guitar-prior/Source')]:
 v=base/name;shutil.copytree(base/source,v/'Source',dirs_exist_ok=True)
 h=v/'Source/Pitch/TimeDomainPursuit.h';s=h.read_text().replace('int samples=512);','int samples=512,float tuningCents=0);');h.write_text(s)
 p=v/'Source/Pitch/TimeDomainPursuit.cpp';s=p.read_text().replace('double analysisRate,int samples){','double analysisRate,int samples,float tuningCents){').replace('double(note+lowNote-69)/12.);','(double(note+lowNote-69)+tuningCents*.01)/12.);');p.write_text(s)
 h=v/'Source/Pitch/PitchDetector.h';s=h.read_text().replace('TimeDomainPursuit pursuit;','std::array<TimeDomainPursuit,5> tunedPursuits;\n    int tunedPursuitIndex=2;');h.write_text(s)
 p=v/'Source/Pitch/PitchDetector.cpp';s=p.read_text().replace('pursuit.prepare(analysisRate,512);preparedPursuitRate=analysisRate;','for(int bank=0;bank<5;++bank)tunedPursuits[size_t(bank)].prepare(analysisRate,512,float((bank-2)*20));preparedPursuitRate=analysisRate;').replace('view={};needsReset=false;','view={};needsReset=false;tunedPursuitIndex=2;')
 needle="    // A chord's common divisor can be periodic without being a played note."
 assert needle in s;s=s.replace(needle,insert+needle);p.write_text(s)
 src=[str(p) for p in (v/'Source/Pitch').glob('*.cpp') if p.name!='MpeOutputRouter.cpp']
 r=subprocess.run(['g++','-std=c++17','-O2','-DNDEBUG','-I',str(v/'Source'),str(base/'ablation_probe.cpp'),*src,'-o',str(v/'probe')],stdout=(v/'compile.log').open('w'),stderr=subprocess.STDOUT);print(name,'compile',r.returncode,flush=True)
 if r.returncode:continue
 with (v/'events.csv').open('w') as out:subprocess.run([str(v/'probe')],stdout=out,check=True)
