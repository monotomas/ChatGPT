from pathlib import Path
import shutil,subprocess
b=Path(__file__).parent;v=b/'time-atoms-phase-null-owned';shutil.copytree(b/'time-atoms-phase-null-guard/Source',v/'Source',dirs_exist_ok=True)
p=v/'Source/Pitch/TimeDomainPursuit.h';s=p.read_text().replace('void copyExcitation(std::array<float,noteCount>&) const noexcept;','void copyExcitation(std::array<float,noteCount>&) const noexcept;\n    void copyHeldStrength(std::array<float,noteCount>&) const noexcept;');p.write_text(s)
p=v/'Source/Pitch/TimeDomainPursuit.cpp';s=p.read_text().replace('std::array<float,noteCount> sourceExcitation{};','std::array<float,noteCount> sourceExcitation{},heldStrength{};')
s=s.replace('data->sourcePhase={};data->sourceExcitation={};','data->sourcePhase={};data->sourceExcitation={};data->heldStrength={};')
needle='void TimeDomainPursuit::copyExcitation(std::array<float,noteCount>& out) const noexcept {out.fill(0);if(data)out=data->sourceExcitation;}'
assert needle in s;s=s.replace(needle,needle+'\nvoid TimeDomainPursuit::copyHeldStrength(std::array<float,noteCount>& out) const noexcept {out.fill(0);if(data)out=data->heldStrength;}')
s=s.replace('    if(data)data->predictionValid=false;','    if(data){data->predictionValid=false;data->heldStrength.fill(0);data->sourceExcitation.fill(0);}')
needle='    // The state below belongs to held physical-source hypotheses.'
insert='''    if(evidence)for(int j=0;j<predictionUsed;++j){const int n=predictionSelected[size_t(j)];if(!evidence->active[size_t(n)])continue;const auto &m=data->models[size_t(n)];double power=0;
        for(int c=0;c<columns;++c){const double a=predictionCoefficient[size_t(j)][size_t(c)]*m.weight[size_t(c)];power+=a*a;}data->heldStrength[size_t(n)]=float(std::sqrt(power*.5));}
''';assert needle in s;s=s.replace(needle,insert+needle);p.write_text(s)
p=v/'Source/Pitch/PitchDetector.h';s=p.read_text().replace('retriggerBaseline{},sourceExcitation{};','retriggerBaseline{},sourceExcitation{},sourceHeldStrength{};');p.write_text(s)
p=v/'Source/Pitch/PitchDetector.cpp';s=p.read_text().replace('sourceExcitation={};candidateAdmissible={};','sourceExcitation={};sourceHeldStrength={};candidateAdmissible={};')
needle='    strength.fill(0);scores.fill(0);candidateAdmissible.fill(false);';assert needle in s;s=s.replace(needle,'    sourceExcitation.fill(0);sourceHeldStrength.fill(0);\n'+needle)
s=s.replace('    pursuit.copyExcitation(sourceExcitation);','    pursuit.copyExcitation(sourceExcitation);\n    pursuit.copyHeldStrength(sourceHeldStrength);')
needle='        auto& voice=voices[size_t(n)];float detected=strength[size_t(n)];bool rearticulated=false;'
insert='''
        // A conditioned event for an ALREADY held oscillator is not another
        // fresh-note acquisition. Preserve its identity and commit Off/On
        // atomically once its own current support and input gate permit it.
        if(config.polyphonic&&canAdmit()&&voice.active&&sourceExcitation[size_t(n)]>.5f
            &&voice.age>analysisRate*.07&&sourceHeldStrength[size_t(n)]>gate*.8f){
            const float level=sourceHeldStrength[size_t(n)];const int velocity=std::clamp(int(std::lround(127.f*std::pow(std::min(level*2.f,1.f),.45f))),1,127);
            emit(false,n+lowNote,0,offset,out);emit(true,n+lowNote,velocity,offset,out);
            voice.age=voice.absent=0;voice.strength=level;retriggerBaseline[size_t(n)]=std::numeric_limits<float>::infinity();continue;
        }
''';assert needle in s;s=s.replace(needle,needle+insert)
# The old branch retains the legacy single-source pending-phase path; the
# multi-source event was already processed above using its own held evidence.
s=s.replace('config.polyphonic?(sourceExcitation[size_t(n)]>.5f||(phaseAttack&&attackRemaining>0&&fastEnvelope>slowEnvelope*.8f))','config.polyphonic?(phaseAttack&&attackRemaining>0&&fastEnvelope>slowEnvelope*.8f)')
p.write_text(s)
sources=[str(p) for p in (v/'Source/Pitch').glob('*.cpp') if p.name!='MpeOutputRouter.cpp'];out=v/'chord-repicks-corrected-v2';out.mkdir(exist_ok=True)
r=subprocess.run(['g++','-std=c++17','-O2','-DNDEBUG','-I',str(v/'Source'),str(b/'chord_repick_probe_v2.cpp'),*sources,'-o',str(out/'probe')],stdout=(out/'compile.log').open('w'),stderr=subprocess.STDOUT);print('compile',r.returncode,flush=True)
if not r.returncode:
 with (out/'events.csv').open('w') as f:subprocess.run([str(out/'probe')],stdout=f,check=True)
 print('capture complete',flush=True)
