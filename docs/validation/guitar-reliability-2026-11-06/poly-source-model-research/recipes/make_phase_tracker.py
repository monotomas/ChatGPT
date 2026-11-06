from pathlib import Path
import shutil,subprocess
b=Path(__file__).parent;v=b/'time-atoms-per-voice-phase';shutil.copytree(b/'time-atoms-held-prediction/Source',v/'Source',dirs_exist_ok=True)
p=v/'Source/Pitch/TimeDomainPursuit.h';s=p.read_text().replace('#include <memory>','#include <memory>\n#include <cstdint>');s=s.replace('bool copyPrediction(std::array<float,predictionSamples>&) const noexcept;','bool copyPrediction(std::array<float,predictionSamples>&) const noexcept;\n    void setObservationClock(int64_t) noexcept;\n    void copyExcitation(std::array<float,noteCount>&) const noexcept;');p.write_text(s)
p=v/'Source/Pitch/TimeDomainPursuit.cpp';s=p.read_text();s=s.replace('std::array<Model,noteCount> models{};','''std::array<Model,noteCount> models{};
    struct PhaseState {std::array<double,8> phase{},omega{},trend{},departure{},power{};std::array<bool,8> valid{};int warmup=0;int64_t sample=0,lastEvent=-1000000;};
    std::array<PhaseState,noteCount> sourcePhase{};
    std::array<float,noteCount> sourceExcitation{};
    int64_t observationClock=0,lastObservationClock=0;''')
needle='namespace {\n// Actual detector peaks'
insert='''void TimeDomainPursuit::setObservationClock(int64_t sample) noexcept {if(data)data->observationClock=sample;}
void TimeDomainPursuit::copyExcitation(std::array<float,noteCount>& out) const noexcept {out.fill(0);if(data)out=data->sourceExcitation;}
''';assert needle in s;s=s.replace(needle,insert+needle)
needle='    data->prediction.fill(float(mean));'
insert='''    // The state below belongs to held physical-source hypotheses. Instant
    // candidate-order churn is excluded by the separate held-set joint fit.
    const int64_t clock=data->observationClock>0?data->observationClock:data->lastObservationClock+predictionSamples;
    if(clock<=data->lastObservationClock)data->sourcePhase={};
    data->lastObservationClock=clock;data->sourceExcitation.fill(0);
    double explained=0,regularised=0;
    for(int i=0;i<dimension;++i){explained+=solution[size_t(i)]*rhs[size_t(i)];regularised+=solution[size_t(i)]*solution[size_t(i)];}
    const double residual=std::max(0.,energy-explained-ridge*samples*regularised);
    const double variance=residual/std::max(samples-dimension,1);
    for(int j=0;j<predictionUsed;++j){
        const int note=predictionSelected[size_t(j)];const auto &model=data->models[size_t(note)];auto &state=data->sourcePhase[size_t(note)];
        const int64_t elapsed=clock-state.sample;
        if(elapsed<=0||elapsed>predictionSamples*4){state={};state.lastEvent=-1000000;}
        const double dt=state.sample>0?double(clock-state.sample):0;
        std::array<double,8> power{},angle{};double maximum=0;
        for(int h=0;h<8;++h){const double a=predictionCoefficient[size_t(j)][size_t(h)],c=predictionCoefficient[size_t(j)][size_t(h+8)];power[size_t(h)]=(a*a+c*c)*model.weight[size_t(h)]*model.weight[size_t(h)];maximum=std::max(maximum,power[size_t(h)]);angle[size_t(h)]=std::remainder(std::atan2(c,a)+model.phase[size_t(h)]*(samples-1)*.5,2*pi);}
        int supported=0,changed=0;double largest=0;
        for(int h=0;h<8;++h){double coherence=0;
            for(int other=0;other<predictionUsed;++other)if(other!=j){const auto &m=data->models[size_t(predictionSelected[size_t(other)])];
                for(int k=0;k<8;++k)if(m.valid[size_t(k)]){const double delta=model.phase[size_t(h)]-m.phase[size_t(k)],denominator=samples*std::sin(delta*.5);const double value=std::abs(denominator)<1e-10?1.:std::abs(std::sin(samples*delta*.5)/denominator);coherence=std::max(coherence,value);}}
            const double noisePower=variance/std::max(samples*.5,1.)/std::max(1-coherence*coherence,.01);
            const bool reliable=model.valid[size_t(h)]&&coherence<.80&&power[size_t(h)]>std::max(maximum*.003,noisePower*9.)&&power[size_t(h)]>1e-16;
            if(reliable&&state.valid[size_t(h)]&&dt>0&&dt<=predictionSamples*4){
                const double expected=.5*(model.phase[size_t(h)]+state.omega[size_t(h)])*dt;
                const double difference=std::remainder(angle[size_t(h)]-state.phase[size_t(h)]-expected,2*pi);
                if(state.warmup<4){state.trend[size_t(h)]=difference/dt;state.departure[size_t(h)]=0;}
                else{const double innovation=std::remainder(difference-state.trend[size_t(h)]*dt,2*pi);state.departure[size_t(h)]=state.departure[size_t(h)]*std::exp(-dt/(data->rate*.040))+innovation;
                    const double distance=std::abs(state.departure[size_t(h)]);largest=std::max(largest,distance);++supported;if(distance>.30)++changed;
                    if(distance<.15)state.trend[size_t(h)]=.9*state.trend[size_t(h)]+.1*difference/dt;}
            }else{state.trend[size_t(h)]=0;state.departure[size_t(h)]=0;}
            state.phase[size_t(h)]=angle[size_t(h)];state.omega[size_t(h)]=model.phase[size_t(h)];state.power[size_t(h)]=power[size_t(h)];state.valid[size_t(h)]=reliable;
        }
        if(supported>=2&&changed>=2&&clock-state.lastEvent>data->rate*.075){state.lastEvent=clock;state.departure={};}
        if(clock-state.lastEvent<data->rate*.060)data->sourceExcitation[size_t(note)]=1;
        state.warmup=std::min(state.warmup+1,8);state.sample=clock;
    }
''';assert needle in s;s=s.replace(needle,insert+needle);p.write_text(s)
p=v/'Source/Pitch/PitchDetector.h';s=p.read_text().replace('std::array<float,noteCount> frequency{},scores{},strength{},retriggerBaseline{};','std::array<float,noteCount> frequency{},scores{},strength{},retriggerBaseline{},sourceExcitation{};');p.write_text(s)
p=v/'Source/Pitch/PitchDetector.cpp';s=p.read_text().replace('retriggerBaseline={};candidateAdmissible={};','retriggerBaseline={};sourceExcitation={};candidateAdmissible={};')
needle='    pursuit.analyse(frame.data(),512,strength,scores,config.polyphony,config.sensitivity,&evidence);';assert needle in s;s=s.replace(needle,'    pursuit.setObservationClock(analysisClock);\n'+needle+'\n    pursuit.copyExcitation(sourceExcitation);')
old='''if(canAdmit()&&voice.active&&ready&&(config.polyphonic||!monoTrackedOnly)&&(!config.polyphonic||phaseAttack||polyRepickEvidence>0)&&attackRemaining>0&&voice.age>analysisRate*.07&&((phaseAttack&&fastEnvelope>slowEnvelope*.8f)||(!phaseAttack&&detected>retriggerBaseline[size_t(n)]*1.35f)))'''
new='''if(canAdmit()&&voice.active&&ready&&(config.polyphonic||!monoTrackedOnly)&&voice.age>analysisRate*.07
                &&(config.polyphonic?(sourceExcitation[size_t(n)]>.5f||(phaseAttack&&attackRemaining>0&&fastEnvelope>slowEnvelope*.8f))
                    :attackRemaining>0&&((phaseAttack&&fastEnvelope>slowEnvelope*.8f)||(!phaseAttack&&detected>retriggerBaseline[size_t(n)]*1.35f))))'''
assert old in s;s=s.replace(old,new);p.write_text(s)
sources=[str(p) for p in (v/'Source/Pitch').glob('*.cpp') if p.name!='MpeOutputRouter.cpp']
for driver,output,arg in [('ablation_probe.cpp','openstrings','10'),('chord_repick_probe.cpp','chord-repicks',None)]:
 out=v/output;out.mkdir(exist_ok=True);r=subprocess.run(['g++','-std=c++17','-O2','-DNDEBUG','-I',str(v/'Source'),str(b/driver),*sources,'-o',str(out/'probe')],stdout=(out/'compile.log').open('w'),stderr=subprocess.STDOUT);print(output,'compile',r.returncode,flush=True)
 if not r.returncode:
  with (out/'events.csv').open('w') as f:subprocess.run([str(out/'probe')]+([arg] if arg else []),stdout=f,check=True)
 print(output,'capture complete',flush=True)
