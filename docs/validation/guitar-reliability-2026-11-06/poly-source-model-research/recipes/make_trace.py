from pathlib import Path
import shutil,subprocess
b=Path(__file__).parent;v=b/'trace-time-atoms';shutil.copytree(b/'time-atoms-guitar-free-prediction/Source',v/'Source',dirs_exist_ok=True)
p=v/'Source/Pitch/PitchDetector.cpp';s=p.read_text().replace('#include <algorithm>','#include <algorithm>\n#include <iostream>')
needle='                emit(false,n+lowNote,0,offset,out);voice.active=false;'
s=s.replace(needle,'''                std::cerr<<"RETRIGGER sample "<<analysisClock<<" note "<<n+lowNote<<" admission "<<polyAdmission<<" novelty "<<independentNovelty[size_t(n)]<<" attackRemaining "<<attackRemaining<<" phaseAttack "<<phaseAttack<<" age "<<voice.age<<" detected "<<detected<<" baseline "<<retriggerBaseline[size_t(n)]<<" fast "<<fastEnvelope<<" slow "<<slowEnvelope<<"\\n";
'''+needle)
s=s.replace('                polyAdmission=int(analysisRate*.120);predictionCooldown=int(analysisRate*.075);','                std::cerr<<"PRED_ADMISSION sample "<<analysisClock<<" fastErr "<<predictionFast<<" slowErr "<<predictionSlow<<" observed "<<predictionObserved<<" expected "<<predictionExpected<<"\\n";\n                polyAdmission=int(analysisRate*.120);predictionCooldown=int(analysisRate*.075);')
s=s.replace('            attackRemaining=int(analysisRate*.045);attackCooldown=int(analysisRate*.075);phaseAttack=false;','            std::cerr<<"AMP_ATTACK sample "<<analysisClock<<" ratio "<<attackRatio<<" signalSamples "<<signalSamples<<"\\n";\n            attackRemaining=int(analysisRate*.045);attackCooldown=int(analysisRate*.075);phaseAttack=false;')
s=s.replace('if(signalSamples==0)polyAdmission=int(analysisRate*.140);','if(signalSamples==0){std::cerr<<"SIGNAL_START sample "<<analysisClock<<" background "<<noiseConditioner.backgroundRms()<<" rawclean "<<rawCleanEnvelope<<"\\n";polyAdmission=int(analysisRate*.140);}')
p.write_text(s)
probe=(b/'ablation_probe.cpp').read_text().replace('if(argc>1&&int(c)!=std::atoi(argv[1]))continue;','if(c!=10||cents!=0||timbre!=0||dynamics!=1)continue;');(v/'probe.cpp').write_text(probe)
src=[str(p) for p in (v/'Source/Pitch').glob('*.cpp') if p.name!='MpeOutputRouter.cpp']
r=subprocess.run(['g++','-std=c++17','-O2','-DNDEBUG','-I',str(v/'Source'),str(v/'probe.cpp'),*src,'-o',str(v/'probe')],stdout=(v/'compile.log').open('w'),stderr=subprocess.STDOUT);print('compile',r.returncode,flush=True)
if not r.returncode:
 with (v/'events.csv').open('w') as out,(v/'trace.txt').open('w') as err:subprocess.run([str(v/'probe')],stdout=out,stderr=err,check=True)
