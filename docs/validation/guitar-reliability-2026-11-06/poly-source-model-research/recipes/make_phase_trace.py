from pathlib import Path
import shutil,subprocess
b=Path(__file__).parent;v=b/'trace-phase-v2';shutil.copytree(b/'time-atoms-per-voice-phase-v2/Source',v/'Source',dirs_exist_ok=True)
p=v/'Source/Pitch/PitchDetector.cpp';s=p.read_text().replace('#include <algorithm>','#include <algorithm>\n#include <iostream>')
needle='                emit(false,n+lowNote,0,offset,out);voice.active=false;rearticulated=true;'
replace='''                std::cerr<<"REARTIC clock "<<analysisClock<<" note "<<n+lowNote<<" sourcePhase "<<sourceExcitation[size_t(n)]<<" globalPhase "<<phaseAttack<<" freshWindow "<<polyRepickEvidence<<" admission "<<polyAdmission<<" ready "<<ready<<" age "<<voice.age<<" strength "<<detected<<" fast "<<fastEnvelope<<" slow "<<slowEnvelope<<"\\n";
'''+needle;assert needle in s;s=s.replace(needle,replace)
needle='        if(detected>0) {'
insert='''        if(config.polyphonic&&analysisClock>=int(analysisRate*.38)&&analysisClock<=int(analysisRate*.65)&&voices[size_t(n)].active){
            std::cerr<<"VOICE clock "<<analysisClock<<" note "<<n+lowNote<<" phase "<<sourceExcitation[size_t(n)]<<" global "<<phaseAttack<<" attack "<<attackRemaining<<" ready "<<ready<<" admit "<<canAdmit()<<" age "<<voice.age<<" strength "<<detected<<"\\n";}
''';assert needle in s;s=s.replace(needle,insert+needle);p.write_text(s)
p=v/'Source/Pitch/TimeDomainPursuit.cpp';s=p.read_text().replace('#include <algorithm>','#include <algorithm>\n#include <iostream>')
needle='        if(supported>=2&&changed>=2&&clock-state.lastEvent>data->rate*.075){state.lastEvent=clock;state.departure={};}'
replace='''        if(supported>=2&&changed>=2&&clock-state.lastEvent>data->rate*.075){std::cerr<<"PHASE_EVENT clock "<<clock<<" note "<<note+lowNote<<" support "<<supported<<" changed "<<changed<<" largest "<<largest<<" departures";
            for(int h=0;h<8;++h)std::cerr<<" "<<h+1<<":"<<state.departure[size_t(h)]<<":"<<state.valid[size_t(h)];std::cerr<<"\\n";
            state.lastEvent=clock;state.departure={};}''';assert needle in s;s=s.replace(needle,replace);p.write_text(s)
probe=(b/'chord_repick_probe.cpp').read_text().replace('#include <algorithm>','#include <algorithm>\n#include <cstdlib>').replace('int main(){','int main(int argc,char**argv){\n const int chosen=argc>1?std::atoi(argv[1]):3;')
needle='  std::vector<int> notes=kind==2?';assert needle in probe;probe=probe.replace(needle,'  if(kind!=chosen||phase!=0||newGain!=1||dynamics!=1)continue;\n'+needle)
(v/'probe.cpp').write_text(probe);src=[str(p) for p in (v/'Source/Pitch').glob('*.cpp') if p.name!='MpeOutputRouter.cpp']
r=subprocess.run(['g++','-std=c++17','-O2','-DNDEBUG','-I',str(v/'Source'),str(v/'probe.cpp'),*src,'-o',str(v/'probe')],stdout=(v/'compile.log').open('w'),stderr=subprocess.STDOUT);print('compile',r.returncode,flush=True)
if not r.returncode:
 for case,name in [('3','gain-ramp'),('1','real-repick')]:
  with (v/(name+'.csv')).open('w') as out,(v/(name+'.txt')).open('w') as err:subprocess.run([str(v/'probe'),case],stdout=out,stderr=err,check=True)
