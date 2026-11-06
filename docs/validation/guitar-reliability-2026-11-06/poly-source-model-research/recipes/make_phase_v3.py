from pathlib import Path
import shutil,subprocess
b=Path(__file__).parent;v=b/'time-atoms-per-voice-phase-v3';shutil.copytree(b/'time-atoms-per-voice-phase-v2/Source',v/'Source',dirs_exist_ok=True)
p=v/'Source/Pitch/TimeDomainPursuit.cpp';s=p.read_text().replace('std::array<bool,noteCount> previousHeld{};','''std::array<bool,noteCount> previousHeld{};
    std::array<std::array<double,columns>,noteCount> previousPhaseCoefficient{};
    std::array<std::array<double,8>,noteCount> previousPhaseOmega{};
    std::array<bool,noteCount> previousCoefficientHeld{};
    int64_t previousCoefficientSample=0;''')
needle='    double explained=0,regularised=0;'
insert='''    // A positive common gain envelope is a nuisance parameter. It changes
    // no source phase and must not masquerade as another played note.
    auto phaseCoefficient=predictionCoefficient;
    double phaseVariance=0;
    const bool stableHeld=held==data->previousCoefficientHeld&&data->previousCoefficientSample>0
        &&clock>data->previousCoefficientSample&&clock-data->previousCoefficientSample<=predictionSamples*4;
    if(stableHeld&&predictionUsed>0){
        std::array<std::array<double,columns>,6> prior{};
        const double dt=double(clock-data->previousCoefficientSample),center=(samples-1)*.5;
        for(int j=0;j<predictionUsed;++j){const int note=predictionSelected[size_t(j)];const auto &m=data->models[size_t(note)];
            for(int h=0;h<8;++h){const double omega=data->previousPhaseOmega[size_t(note)][size_t(h)],now=m.phase[size_t(h)];
                const double rotation=(omega-now)*center+.5*(omega+now)*dt,c=std::cos(rotation),si=std::sin(rotation);
                const double a=data->previousPhaseCoefficient[size_t(note)][size_t(h)],z=data->previousPhaseCoefficient[size_t(note)][size_t(h+8)];
                prior[size_t(j)][size_t(h)]=a*c-z*si;prior[size_t(j)][size_t(h+8)]=z*c+a*si;}}
        std::array<float,maxSamples> previousWave{},normalised{};
        double gainSystem[4][5]{};
        for(int i=0;i<samples;++i){double w=0;for(int j=0;j<predictionUsed;++j){const auto &m=data->models[size_t(predictionSelected[size_t(j)])];for(int c=0;c<columns;++c)w+=prior[size_t(j)][size_t(c)]*m.basis[size_t(c*maxSamples+i)];}
            previousWave[size_t(i)]=float(w);const double u=(i-center)/samples,basis[4]{w,u*w,u*u*w,1.};
            for(int a=0;a<4;++a){gainSystem[a][4]+=basis[a]*frame[size_t(i)];for(int c=0;c<4;++c)gainSystem[a][c]+=basis[a]*basis[c];}}
        bool invertible=true;
        for(int pivot=0;pivot<4;++pivot){int best=pivot;for(int row=pivot+1;row<4;++row)if(std::abs(gainSystem[row][pivot])>std::abs(gainSystem[best][pivot]))best=row;
            if(std::abs(gainSystem[best][pivot])<1e-16){invertible=false;break;}
            if(best!=pivot)for(int c=pivot;c<5;++c)std::swap(gainSystem[pivot][c],gainSystem[best][c]);
            const double d=gainSystem[pivot][pivot];for(int c=pivot;c<5;++c)gainSystem[pivot][c]/=d;
            for(int row=0;row<4;++row)if(row!=pivot){const double scale=gainSystem[row][pivot];for(int c=pivot;c<5;++c)gainSystem[row][c]-=scale*gainSystem[pivot][c];}}
        const double g0=invertible?gainSystem[0][4]:1.,g1=invertible?gainSystem[1][4]:0.,g2=invertible?gainSystem[2][4]:0.,dc=invertible?gainSystem[3][4]:0.;
        double average=0;
        for(int i=0;i<samples;++i){const double u=(i-center)/samples;
            const double gain=g0>1e-8?std::clamp(g0+g1*u+g2*u*u,g0*.10,g0*10.):1.;
            normalised[size_t(i)]=invertible&&g0>1e-8?float((frame[size_t(i)]-dc)/gain):frame[size_t(i)];average+=normalised[size_t(i)];}
        average/=samples;double normalisedEnergy=0;
        for(int i=0;i<samples;++i){normalised[size_t(i)]-=float(average);normalisedEnergy+=double(normalised[size_t(i)])*normalised[size_t(i)];}
        std::array<double,maxJoint> phaseProjection{},phaseRhs{},phaseSolution{};
        for(int j=0;j<predictionUsed;++j){const auto &m=data->models[size_t(predictionSelected[size_t(j)])];for(int c=0;c<columns;++c){const int index=j*columns+c;
            phaseProjection[size_t(index)]=dot(m.basis.data()+c*maxSamples,normalised.data(),samples);
            phaseRhs[size_t(index)]=phaseProjection[size_t(index)]+ridge*samples*prior[size_t(j)][size_t(c)];}}
        for(int i=0;i<dimension;++i){double value=phaseRhs[size_t(i)];for(int j=0;j<i;++j)value-=factor[size_t(i*maxJoint+j)]*phaseSolution[size_t(j)];phaseSolution[size_t(i)]=value/factor[size_t(i*maxJoint+i)];}
        for(int i=dimension-1;i>=0;--i){double value=phaseSolution[size_t(i)];for(int j=i+1;j<dimension;++j)value-=factor[size_t(j*maxJoint+i)]*phaseSolution[size_t(j)];phaseSolution[size_t(i)]=value/factor[size_t(i*maxJoint+i)];}
        double phaseResidual=normalisedEnergy;
        for(int i=0;i<dimension;++i){phaseResidual-=phaseSolution[size_t(i)]*phaseProjection[size_t(i)];phaseResidual+=phaseSolution[size_t(i)]*(phaseRhs[size_t(i)]-phaseProjection[size_t(i)]);phaseResidual-=ridge*samples*phaseSolution[size_t(i)]*phaseSolution[size_t(i)];}
        phaseVariance=std::max(0.,phaseResidual)/std::max(samples-dimension,1);
        for(int j=0;j<predictionUsed;++j)for(int c=0;c<columns;++c)phaseCoefficient[size_t(j)][size_t(c)]=phaseSolution[size_t(j*columns+c)];
    }
''';assert needle in s;s=s.replace(needle,insert+needle)
needle='    const double variance=residual/std::max(samples-dimension,1);';assert needle in s;s=s.replace(needle,needle+'\n    if(!stableHeld)phaseVariance=variance;')
a=s.index('    for(int j=0;j<predictionUsed;++j){\n        const int note=predictionSelected[size_t(j)];if(!held[size_t(note)])continue;')
z=s.index('    data->prediction.fill(float(mean));',a)
part=s[a:z].replace('predictionCoefficient[size_t(j)]','phaseCoefficient[size_t(j)]').replace('const double noisePower=variance*','const double noisePower=phaseVariance*')
part+='''    data->previousCoefficientHeld=held;data->previousCoefficientSample=clock;
    for(int j=0;j<predictionUsed;++j){const int note=predictionSelected[size_t(j)];data->previousPhaseCoefficient[size_t(note)]=phaseCoefficient[size_t(j)];
        for(int h=0;h<8;++h)data->previousPhaseOmega[size_t(note)][size_t(h)]=data->models[size_t(note)].phase[size_t(h)];}
'''
s=s[:a]+part+s[z:];p.write_text(s)
sources=[str(p) for p in (v/'Source/Pitch').glob('*.cpp') if p.name!='MpeOutputRouter.cpp']
for driver,output,arg in [('ablation_probe.cpp','openstrings','10'),('chord_repick_probe.cpp','chord-repicks',None)]:
 out=v/output;out.mkdir(exist_ok=True);r=subprocess.run(['g++','-std=c++17','-O2','-DNDEBUG','-I',str(v/'Source'),str(b/driver),*sources,'-o',str(out/'probe')],stdout=(out/'compile.log').open('w'),stderr=subprocess.STDOUT);print(output,'compile',r.returncode,flush=True)
 if not r.returncode:
  with (out/'events.csv').open('w') as f:subprocess.run([str(out/'probe')]+([arg] if arg else []),stdout=f,check=True)
 print(output,'capture complete',flush=True)
