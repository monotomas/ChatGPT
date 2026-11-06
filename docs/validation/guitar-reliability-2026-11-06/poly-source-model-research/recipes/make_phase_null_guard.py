from pathlib import Path
import shutil,subprocess
b=Path(__file__).parent;v=b/'time-atoms-phase-null-guard';shutil.copytree(b/'time-atoms-per-voice-phase-v2/Source',v/'Source',dirs_exist_ok=True)
p=v/'Source/Pitch/TimeDomainPursuit.h';s=p.read_text().replace('void setObservationClock(int64_t) noexcept;','void setObservationClock(int64_t) noexcept;\n    void resetTracking() noexcept;');p.write_text(s)
p=v/'Source/Pitch/TimeDomainPursuit.cpp';s=p.read_text().replace('#include <algorithm>','#include <algorithm>\n#ifdef FLYSNAPPER_RESEARCH_TRACE_NULL\n#include <iostream>\n#include <iomanip>\n#endif')
s=s.replace('std::array<bool,noteCount> previousHeld{};','''std::array<bool,noteCount> previousHeld{};
    std::array<std::array<double,columns>,noteCount> nullPreviousCoefficient{};
    std::array<std::array<double,8>,noteCount> nullPreviousOmega{};
    std::array<bool,noteCount> nullPreviousHeld{};
    int64_t nullPreviousSample=0;
    double nullPrecision=0;
    unsigned diagnosticEpoch=0;''')
needle='void TimeDomainPursuit::setObservationClock(int64_t sample) noexcept {if(data)data->observationClock=sample;}'
insert='''void TimeDomainPursuit::resetTracking() noexcept {if(!data)return;
    data->sourcePhase={};data->sourceExcitation={};data->previousHeld={};data->observationClock=data->lastObservationClock=0;
    data->nullPreviousCoefficient={};data->nullPreviousOmega={};data->nullPreviousHeld={};data->nullPreviousSample=0;data->nullPrecision=0;
    data->prediction={};data->predictionValid=false;++data->diagnosticEpoch;
}
''';assert needle in s;s=s.replace(needle,insert+needle)
needle='    const double variance=residual/std::max(samples-dimension,1);'
insert='''
    const double freeRelativePrecision=std::max(residual/std::max(energy,1e-30),1e-8);
    bool nullExplained=false;double nullRelativeResidual=-1,gain0=0,gain1=0,gain2=0;
    const bool stableNull=held==data->nullPreviousHeld&&data->nullPreviousSample>0&&clock>data->nullPreviousSample
        &&clock-data->nullPreviousSample<=predictionSamples*4&&predictionUsed>0;
    if(!stableNull)data->nullPrecision=freeRelativePrecision;
    else{
        std::array<std::array<double,columns>,6> prior{};const double dt=double(clock-data->nullPreviousSample),center=(samples-1)*.5;
        for(int j=0;j<predictionUsed;++j){const int n=predictionSelected[size_t(j)];const auto &model=data->models[size_t(n)];
            for(int h=0;h<8;++h){const double old=data->nullPreviousOmega[size_t(n)][size_t(h)],now=model.phase[size_t(h)],rotation=(old-now)*center+.5*(old+now)*dt,c=std::cos(rotation),si=std::sin(rotation);
                const double a=data->nullPreviousCoefficient[size_t(n)][size_t(h)],z=data->nullPreviousCoefficient[size_t(n)][size_t(h+8)];prior[size_t(j)][size_t(h)]=a*c-z*si;prior[size_t(j)][size_t(h+8)]=z*c+a*si;}}
        std::array<float,maxSamples> priorWave{};double matrix[4][5]{};
        for(int i=0;i<samples;++i){double w=0;for(int j=0;j<predictionUsed;++j){const auto &model=data->models[size_t(predictionSelected[size_t(j)])];for(int c=0;c<columns;++c)w+=prior[size_t(j)][size_t(c)]*model.basis[size_t(c*maxSamples+i)];}
            priorWave[size_t(i)]=float(w);const double u=(i-center)/samples,row[4]{w,u*w,u*u*w,1.};
            for(int a=0;a<4;++a){matrix[a][4]+=row[a]*frame[size_t(i)];for(int c=0;c<4;++c)matrix[a][c]+=row[a]*row[c];}}
        bool solved=true;for(int pivot=0;pivot<4;++pivot){int best=pivot;for(int row=pivot+1;row<4;++row)if(std::abs(matrix[row][pivot])>std::abs(matrix[best][pivot]))best=row;
            if(std::abs(matrix[best][pivot])<1e-16){solved=false;break;}if(best!=pivot)for(int c=pivot;c<5;++c)std::swap(matrix[pivot][c],matrix[best][c]);
            const double divisor=matrix[pivot][pivot];for(int c=pivot;c<5;++c)matrix[pivot][c]/=divisor;
            for(int row=0;row<4;++row)if(row!=pivot){const double scale=matrix[row][pivot];for(int c=pivot;c<5;++c)matrix[row][c]-=scale*matrix[pivot][c];}}
        gain0=matrix[0][4];gain1=matrix[1][4];gain2=matrix[2][4];
        if(solved&&gain0>1e-8){double nullError=0;
            for(int i=0;i<samples;++i){const double u=(i-center)/samples,gain=std::clamp(gain0+gain1*u+gain2*u*u,gain0*.10,gain0*10.);
                const double error=frame[size_t(i)]-gain*priorWave[size_t(i)]-matrix[3][4];nullError+=error*error;}
            nullRelativeResidual=nullError/std::max(energy,1e-30);
            // Existing factor-four innovation margin; learn only from frames
            // already explained by the old held wave plus a positive gain.
            nullExplained=nullRelativeResidual<=4*data->nullPrecision;
            if(nullExplained)data->nullPrecision+=(1-std::exp(-dt/(data->rate*.25)))*(freeRelativePrecision-data->nullPrecision);
        }
    }
#ifdef FLYSNAPPER_RESEARCH_TRACE_NULL
    std::cerr<<std::setprecision(9)<<"NULL,"<<data->diagnosticEpoch<<','<<clock<<','<<nullRelativeResidual<<','<<data->nullPrecision<<','<<freeRelativePrecision<<','<<nullExplained<<','<<gain0<<','<<gain1<<','<<gain2<<'\\n';
#endif
''';assert needle in s;s=s.replace(needle,needle+insert)
s=s.replace('if(state.warmup<4||clock-state.lastEvent<data->rate*.075)','if(state.warmup<4||clock-state.lastEvent<data->rate*.075||nullExplained)')
s=s.replace('if(supported>=2&&changed>=2&&clock-state.lastEvent>data->rate*.075)','if(!nullExplained&&supported>=2&&changed>=2&&clock-state.lastEvent>data->rate*.075)')
needle='    data->prediction.fill(float(mean));'
insert='''    data->nullPreviousHeld=held;data->nullPreviousSample=clock;
    for(int j=0;j<predictionUsed;++j){const int n=predictionSelected[size_t(j)];data->nullPreviousCoefficient[size_t(n)]=predictionCoefficient[size_t(j)];
        for(int h=0;h<8;++h)data->nullPreviousOmega[size_t(n)][size_t(h)]=data->models[size_t(n)].phase[size_t(h)];}
''';assert needle in s;s=s.replace(needle,insert+needle);p.write_text(s)
p=v/'Source/Pitch/PitchDetector.cpp';s=p.read_text();needle='    polyTracker.reset();publishedPoly={};polyShadowMode=false;';assert needle in s;s=s.replace(needle,'    pursuit.resetTracking();\n'+needle);p.write_text(s)
sources=[str(p) for p in (v/'Source/Pitch').glob('*.cpp') if p.name!='MpeOutputRouter.cpp'];out=v/'chord-repicks-corrected-v2';out.mkdir(exist_ok=True)
r=subprocess.run(['g++','-std=c++17','-O2','-DNDEBUG','-DFLYSNAPPER_RESEARCH_TRACE_NULL','-I',str(v/'Source'),str(b/'chord_repick_probe_v2.cpp'),*sources,'-o',str(out/'probe')],stdout=(out/'compile.log').open('w'),stderr=subprocess.STDOUT);print('compile',r.returncode,flush=True)
if not r.returncode:
 with (out/'events.csv').open('w') as f,(out/'null-trace.csv').open('w') as err:subprocess.run([str(out/'probe')],stdout=f,stderr=err,check=True)
 print('capture complete',flush=True)
