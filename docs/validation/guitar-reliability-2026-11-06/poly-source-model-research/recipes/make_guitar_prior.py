from pathlib import Path
import shutil,subprocess
base=Path(__file__).parent
for name,removeanchor in [('guitar-prior',False),('guitar-prior-no-anchor',True)]:
 v=base/name;shutil.copytree(base/'Source',v/'Source',dirs_exist_ok=True)
 p=v/'Source/Pitch/TimeDomainPursuit.cpp';s=p.read_text()
 s=s.replace('const int anchor=evidence&&evidence->anchorNote>=0&&evidence->anchorNote<noteCount?evidence->anchorNote:-1;','const int anchor=evidence&&evidence->anchorNote>=40-lowNote&&evidence->anchorNote<=88-lowNote?evidence->anchorNote:-1;')
 helper="""    // Private explicit standard-six-string prior: each model must be
    // jointly assignable to a distinct string, before it can explain audio.
    auto guitarAssignable=[&](int note) noexcept {
        std::array<int,7> ordered{};
        for(int j=0;j<used;++j)ordered[size_t(j)]=selected[size_t(j)]+lowNote;
        ordered[size_t(used)]=note+lowNote;
        std::sort(ordered.begin(),ordered.begin()+used+1);
        constexpr std::array<int,6> tuning{{40,45,50,55,59,64}};
        int string=0;
        for(int j=0;j<=used;++j){
            while(string<6&&(ordered[size_t(j)]<tuning[size_t(string)]||ordered[size_t(j)]>tuning[size_t(string)]+24))++string;
            if(string>=6)return false;++string;
        }
        return true;
    };
"""
 s=s.replace('    const double cutoff=.06+.12*(1-sensitivity);',helper+'    const double cutoff=.06+.12*(1-sensitivity);')
 s=s.replace('        for(int note=0;note<noteCount;++note){\n            bool duplicate=false;','        for(int note=0;note<noteCount;++note){\n            if(!guitarAssignable(note))continue;\n            bool duplicate=false;')
 s=s.replace('if(!duplicate&&octaveEvidence[size_t(note)]>strongest)','if(!duplicate&&guitarAssignable(note)&&octaveEvidence[size_t(note)]>strongest)')
 if removeanchor:s=s.replace('if(anchor>=0&&anchor!=note&&octaveEvidence','if(false&&anchor>=0&&anchor!=note&&octaveEvidence')
 p.write_text(s)
 src=[str(p) for p in (v/'Source/Pitch').glob('*.cpp') if p.name!='MpeOutputRouter.cpp']
 r=subprocess.run(['g++','-std=c++17','-O2','-DNDEBUG','-I',str(v/'Source'),str(base/'ablation_probe.cpp'),*src,'-o',str(v/'probe')],stdout=(v/'compile.log').open('w'),stderr=subprocess.STDOUT);print(name,'compile',r.returncode,flush=True)
 if r.returncode:continue
 with (v/'openstrings.csv').open('w') as out:subprocess.run([str(v/'probe'),'10'],stdout=out,check=True)
