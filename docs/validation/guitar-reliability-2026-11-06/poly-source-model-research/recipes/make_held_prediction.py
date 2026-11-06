from pathlib import Path
import shutil,subprocess
b=Path(__file__).parent;v=b/'time-atoms-held-prediction';shutil.copytree(b/'time-atoms-guitar-free-prediction/Source',v/'Source',dirs_exist_ok=True)
p=v/'Source/Pitch/TimeDomainPursuit.cpp';s=p.read_text();a=s.index('    // Acquisition/output keep the soft envelope hypothesis.');z=s.index('    (void)evidence;',a)
part=s[a:z]
part=part.replace('    std::array<std::array<double,columns>,6> predictionCoefficient=coefficient;','''    std::array<int,6> predictionSelected=selected;int predictionUsed=used;
    if(evidence){int held=0;std::array<int,6> active{};active.fill(-1);
        for(int note=0;note<noteCount;++note)if(evidence->active[size_t(note)]&&held<6)active[size_t(held++)]=note;
        if(held>0){predictionSelected=active;predictionUsed=held;}}
    std::array<std::array<double,columns>,6> predictionCoefficient{};''')
part=part.replace('dimension=used*columns','dimension=predictionUsed*columns').replace('selected[size_t(','predictionSelected[size_t(').replace('j<used','j<predictionUsed').replace('data->predictionValid=used>0','data->predictionValid=predictionUsed>0')
s=s[:a]+part+s[z:];p.write_text(s)
p=v/'Source/Pitch/PitchDetector.h';s=p.read_text().replace('int polyAdmission=0,polyAnchor=-1;','int polyAdmission=0,polyAnchor=-1;\n    int polyRepickEvidence=0;');p.write_text(s)
p=v/'Source/Pitch/PitchDetector.cpp';s=p.read_text().replace('polyAdmission=0;polyAnchor=-1;','polyAdmission=0;polyRepickEvidence=0;polyAnchor=-1;').replace('        if(polyAdmission>0)--polyAdmission;','        if(polyAdmission>0)--polyAdmission;\n        if(polyRepickEvidence>0)--polyRepickEvidence;')
s=s.replace('polyAdmission=int(analysisRate*.120);predictionCooldown=int(analysisRate*.075);','polyAdmission=int(analysisRate*.120);polyRepickEvidence=int(analysisRate*.060);predictionCooldown=int(analysisRate*.075);')
old='(!config.polyphonic||polyAdmission>0||independentNovelty[size_t(n)])&&attackRemaining>0&&voice.age>'
new='(!config.polyphonic||phaseAttack||polyRepickEvidence>0)&&attackRemaining>0&&voice.age>'
assert old in s;s=s.replace(old,new);p.write_text(s)
src=[str(p) for p in (v/'Source/Pitch').glob('*.cpp') if p.name!='MpeOutputRouter.cpp']
r=subprocess.run(['g++','-std=c++17','-O2','-DNDEBUG','-I',str(v/'Source'),str(b/'ablation_probe.cpp'),*src,'-o',str(v/'probe')],stdout=(v/'compile.log').open('w'),stderr=subprocess.STDOUT);print('compile',r.returncode,flush=True)
if not r.returncode:
 with (v/'openstrings.csv').open('w') as out:subprocess.run([str(v/'probe'),'10'],stdout=out,check=True)
