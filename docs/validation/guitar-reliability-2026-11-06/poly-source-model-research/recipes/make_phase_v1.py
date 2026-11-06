from pathlib import Path
import shutil,subprocess
b=Path(__file__).parent;v=b/'time-atoms-per-voice-phase-v1';shutil.copytree(b/'time-atoms-per-voice-phase/Source',v/'Source',dirs_exist_ok=True)
p=v/'Source/Pitch/TimeDomainPursuit.cpp';s=p.read_text().replace('int64_t observationClock=0,lastObservationClock=0;','int64_t observationClock=0,lastObservationClock=0;\n    std::array<bool,noteCount> previousHeld{};')
needle='    data->lastObservationClock=clock;data->sourceExcitation.fill(0);';replace='''    std::array<bool,noteCount> held{};if(evidence)held=evidence->active;
    if(held!=data->previousHeld)data->sourcePhase={};
    data->previousHeld=held;
    data->lastObservationClock=clock;data->sourceExcitation.fill(0);''';assert needle in s;s=s.replace(needle,replace)
needle='        const int note=predictionSelected[size_t(j)];const auto &model=data->models[size_t(note)];auto &state=data->sourcePhase[size_t(note)];';replace='''        const int note=predictionSelected[size_t(j)];if(!held[size_t(note)])continue;
        const auto &model=data->models[size_t(note)];auto &state=data->sourcePhase[size_t(note)];''';assert needle in s;s=s.replace(needle,replace);p.write_text(s)
p=v/'Source/Pitch/PitchDetector.cpp';s=p.read_text();needle='        auto& voice=voices[size_t(n)];float detected=strength[size_t(n)];';assert needle in s;s=s.replace(needle,needle+'bool rearticulated=false;')
s=s.replace('                emit(false,n+lowNote,0,offset,out);voice.active=false;','                emit(false,n+lowNote,0,offset,out);voice.active=false;rearticulated=true;')
# The fresh-source admission test remains unchanged for every OTHER note.
needle='&&(!config.polyphonic||polyAdmission>0||independentNovelty[size_t(n)])&&fastEnvelope>slowEnvelope*.35f)';replace='&&(!config.polyphonic||rearticulated||polyAdmission>0||independentNovelty[size_t(n)])&&fastEnvelope>slowEnvelope*.35f)';assert needle in s;s=s.replace(needle,replace);p.write_text(s)
sources=[str(p) for p in (v/'Source/Pitch').glob('*.cpp') if p.name!='MpeOutputRouter.cpp']
for driver,output,arg in [('ablation_probe.cpp','openstrings','10'),('chord_repick_probe.cpp','chord-repicks',None)]:
 out=v/output;out.mkdir(exist_ok=True);r=subprocess.run(['g++','-std=c++17','-O2','-DNDEBUG','-I',str(v/'Source'),str(b/driver),*sources,'-o',str(out/'probe')],stdout=(out/'compile.log').open('w'),stderr=subprocess.STDOUT);print(output,'compile',r.returncode,flush=True)
 if not r.returncode:
  with (out/'events.csv').open('w') as f:subprocess.run([str(out/'probe')]+([arg] if arg else []),stdout=f,check=True)
 print(output,'capture complete',flush=True)
