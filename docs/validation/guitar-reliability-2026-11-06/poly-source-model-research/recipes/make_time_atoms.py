from pathlib import Path
import shutil,subprocess
b=Path(__file__).parent;v=b/'time-envelope-atoms';shutil.copytree(b/'Source',v/'Source',dirs_exist_ok=True)
p=v/'Source/Pitch/TimeDomainPursuit.cpp';s=p.read_text()
s=s.replace('std::array<Model,noteCount> models{};','std::array<Model,noteCount> models{};\n    std::array<std::array<float,8>,27> profiles{};')
needle='    next->gram.resize(size_t(components)*components);'
s=s.replace(needle,needle+'''
    int profile=0;
    for(double slope:{.7,1.2,1.8})for(double odd:{.15,1.,4.})for(double pluck:{0.,.22,.37}){
        for(int h=1;h<=8;++h)next->profiles[size_t(profile)][size_t(h-1)]=float((h%2?odd:1.)/std::pow(double(h),slope)*(pluck==0?1.:std::abs(std::sin(pi*h*pluck))));
        ++profile;
    }
''')
a=s.index('void TimeDomainPursuit::analyse(')
s=s[:a]+'''void TimeDomainPursuit::analyse(const float* wave,int count,std::array<float,noteCount>& strength,
                              std::array<float,noteCount>& confidence,int maxVoices,float sensitivity,const SpectralEvidence* evidence) noexcept {
    strength.fill(0);confidence.fill(0);
    if(data)data->predictionValid=false;
    if(!data||!wave||count<data->samples)return;
    maxVoices=std::clamp(maxVoices,1,6);sensitivity=std::clamp(sensitivity,0.f,1.f);
    const int samples=data->samples;
    std::array<float,maxSamples> frame{};wave+=count-samples;
    double mean=0,energy=0;
    for(int i=0;i<samples;++i){frame[size_t(i)]=std::isfinite(wave[i])?std::clamp(wave[i],-16.f,16.f):0;mean+=frame[size_t(i)];}
    mean/=samples;
    for(int i=0;i<samples;++i){frame[size_t(i)]-=float(mean);energy+=double(frame[size_t(i)])*frame[size_t(i)];}
    if(energy<1e-12)return;
    std::array<std::array<double,columns>,noteCount> projection{};
    for(int n=0;n<noteCount;++n)for(int c=0;c<columns;++c)
        if(data->models[size_t(n)].valid[size_t(c)])projection[size_t(n)][size_t(c)]=dot(data->models[size_t(n)].basis.data()+c*maxSamples,frame.data(),samples);
    std::array<int,6> selected{};selected.fill(-1);
    std::array<std::array<double,columns>,6> shape{},coefficient{};
    std::array<double,6> amplitudes{};int used=0;double firstScore=0;
    const double cutoff=.06+.12*(1-sensitivity);
    auto crossShape=[&](int a,const std::array<double,columns>& x,int b,const std::array<double,columns>& y) noexcept {
        double result=0;for(int i=0;i<columns;++i)for(int j=0;j<columns;++j)result+=x[size_t(i)]*data->cross(a,i,b,j)*y[size_t(j)];return result;
    };
    for(int iteration=0;iteration<maxVoices;++iteration){
        int best=-1;double bestScore=0;
        std::array<double,columns> bestShape{};
        for(int note=0;note<noteCount;++note){
            bool duplicate=false;for(int j=0;j<used;++j)duplicate|=selected[size_t(j)]==note;if(duplicate)continue;
            const auto &model=data->models[size_t(note)];
            std::array<double,columns> correlation=projection[size_t(note)],beta{};
            for(int r=0;r<columns;++r)for(int j=0;j<used;++j)for(int c=0;c<columns;++c)
                correlation[size_t(r)]-=data->cross(note,r,selected[size_t(j)],c)*coefficient[size_t(j)][size_t(c)];
            for(int r=0;r<columns;++r)for(int c=0;c<columns;++c)beta[size_t(r)]+=model.inv[size_t(r*columns+c)]*correlation[size_t(c)];
            std::array<double,8> observed{},phaseSin{},phaseCos{};double observedPower=0;
            for(int h=0;h<8;++h){const double magnitude=std::hypot(beta[size_t(h)],beta[size_t(h+8)]);observed[size_t(h)]=magnitude*model.weight[size_t(h)];observedPower+=observed[size_t(h)]*observed[size_t(h)];
                if(magnitude>1e-15){phaseSin[size_t(h)]=beta[size_t(h)]/magnitude;phaseCos[size_t(h)]=beta[size_t(h+8)]/magnitude;}}
            if(observedPower<1e-15)continue;
            for(auto &a:observed)a/=std::sqrt(observedPower);
            for(const auto& profile:data->profiles){
                double power=0;for(int h=0;h<8;++h)if(model.valid[size_t(h)])power+=double(profile[size_t(h)])*profile[size_t(h)];
                if(power<=0)continue;const double scale=1/std::sqrt(power);
                std::array<double,columns> atom{};
                for(int h=0;h<8;++h)if(model.valid[size_t(h)]){const double a=(.5*observed[size_t(h)]+.5*profile[size_t(h)]*scale)/model.weight[size_t(h)];atom[size_t(h)]=a*phaseSin[size_t(h)];atom[size_t(h+8)]=a*phaseCos[size_t(h)];}
                double numerator=0,penalty=0;for(int c=0;c<columns;++c){numerator+=correlation[size_t(c)]*atom[size_t(c)];penalty+=atom[size_t(c)]*atom[size_t(c)];}
                const double denominator=crossShape(note,atom,note,atom)+ridge*samples*penalty;
                const double score=numerator>0&&denominator>1e-12?numerator*numerator/denominator:0;
                if(score>bestScore){bestScore=score;best=note;bestShape=atom;}
            }
        }
        if(iteration==0)firstScore=bestScore;
        if(best<0||bestScore<energy*.006||bestScore<firstScore*cutoff)break;
        selected[size_t(used)]=best;shape[size_t(used)]=bestShape;++used;
        // Scalar source amplitudes are refitted jointly; free partial
        // coefficients cannot undo the explicit soft envelope model.
        double gram[6][6]{},rhs[6]{};
        for(int i=0;i<used;++i){
            for(int c=0;c<columns;++c)rhs[i]+=projection[size_t(selected[size_t(i)])][size_t(c)]*shape[size_t(i)][size_t(c)];
            for(int j=0;j<=i;++j){gram[i][j]=gram[j][i]=crossShape(selected[size_t(i)],shape[size_t(i)],selected[size_t(j)],shape[size_t(j)]);}
            double norm=0;for(double value:shape[size_t(i)])norm+=value*value;gram[i][i]+=ridge*samples*norm;
        }
        for(int sweep=0;sweep<12;++sweep)for(int i=0;i<used;++i){double value=rhs[i];for(int j=0;j<used;++j)if(j!=i)value-=gram[i][j]*amplitudes[size_t(j)];amplitudes[size_t(i)]=std::max(0.,value/std::max(gram[i][i],1e-12));}
        for(int j=0;j<used;++j)for(int c=0;c<columns;++c)coefficient[size_t(j)][size_t(c)]=shape[size_t(j)][size_t(c)]*amplitudes[size_t(j)];
    }
    for(int j=0;j<used;++j){const int note=selected[size_t(j)];const auto& model=data->models[size_t(note)];double magnitude=0;
        for(int c=0;c<columns;++c){const double a=coefficient[size_t(j)][size_t(c)]*model.weight[size_t(c)];magnitude+=a*a;}
        strength[size_t(note)]=float(std::sqrt(magnitude*.5));confidence[size_t(note)]=float(std::clamp(crossShape(note,coefficient[size_t(j)],note,coefficient[size_t(j)])/energy,0.,1.));
    }
    data->prediction.fill(float(mean));
    for(int j=0;j<used;++j){const auto &model=data->models[size_t(selected[size_t(j)])];for(int c=0;c<columns;++c)for(int i=0;i<predictionSamples;++i)data->prediction[size_t(i)]+=float(coefficient[size_t(j)][size_t(c)])*model.continuation[size_t(c*predictionSamples+i)];}
    data->predictionValid=used>0;
    (void)evidence;
}
}
'''
p.write_text(s)
src=[str(p) for p in (v/'Source/Pitch').glob('*.cpp') if p.name!='MpeOutputRouter.cpp']
r=subprocess.run(['g++','-std=c++17','-O2','-DNDEBUG','-I',str(v/'Source'),str(b/'ablation_probe.cpp'),*src,'-o',str(v/'probe')],stdout=(v/'compile.log').open('w'),stderr=subprocess.STDOUT);print('compile',r.returncode,flush=True)
if not r.returncode:
 with (v/'openstrings.csv').open('w') as out:subprocess.run([str(v/'probe'),'10'],stdout=out,check=True)
