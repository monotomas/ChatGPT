from pathlib import Path
import shutil,subprocess
b=Path(__file__).parent;v=b/'time-atoms-guitar-free-prediction';shutil.copytree(b/'time-atoms-guitar-mid-cost/Source',v/'Source',dirs_exist_ok=True)
p=v/'Source/Pitch/TimeDomainPursuit.cpp';s=p.read_text()
needle='    data->prediction.fill(float(mean));\n    for(int j=0;j<used;++j){const auto &model=data->models[size_t(selected[size_t(j)])];for(int c=0;c<columns;++c)for(int i=0;i<predictionSamples;++i)data->prediction[size_t(i)]+=float(coefficient[size_t(j)][size_t(c)])*model.continuation[size_t(c*predictionSamples+i)];}'
replacement='''    // Acquisition/output keep the soft envelope hypothesis. The separate
    // waveform predictor can jointly explain all already selected harmonics
    // without manufacturing a new note from a benign envelope mismatch.
    std::array<std::array<double,columns>,6> predictionCoefficient=coefficient;
    constexpr int maxJoint=6*columns;const int dimension=used*columns;
    std::array<float,maxJoint*maxJoint> factor{};
    std::array<double,maxJoint> rhs{},solution{};
    for(int i=0;i<dimension;++i){const int ni=selected[size_t(i/columns)],ci=i%columns;rhs[size_t(i)]=projection[size_t(ni)][size_t(ci)];
        for(int j=0;j<=i;++j){double value=data->cross(ni,ci,selected[size_t(j/columns)],j%columns)+(i==j?ridge*samples:0.);
            value-=dot(factor.data()+i*maxJoint,factor.data()+j*maxJoint,j);
            factor[size_t(i*maxJoint+j)]=float(i==j?std::sqrt(std::max(value,1e-10)):value/factor[size_t(j*maxJoint+j)]);}}
    for(int i=0;i<dimension;++i){double value=rhs[size_t(i)];for(int j=0;j<i;++j)value-=factor[size_t(i*maxJoint+j)]*solution[size_t(j)];solution[size_t(i)]=value/factor[size_t(i*maxJoint+i)];}
    for(int i=dimension-1;i>=0;--i){double value=solution[size_t(i)];for(int j=i+1;j<dimension;++j)value-=factor[size_t(j*maxJoint+i)]*solution[size_t(j)];solution[size_t(i)]=value/factor[size_t(i*maxJoint+i)];}
    for(int j=0;j<used;++j)for(int c=0;c<columns;++c)predictionCoefficient[size_t(j)][size_t(c)]=solution[size_t(j*columns+c)];
    data->prediction.fill(float(mean));
    for(int j=0;j<used;++j){const auto &model=data->models[size_t(selected[size_t(j)])];for(int c=0;c<columns;++c)for(int i=0;i<predictionSamples;++i)data->prediction[size_t(i)]+=float(predictionCoefficient[size_t(j)][size_t(c)])*model.continuation[size_t(c*predictionSamples+i)];}'''
assert needle in s;s=s.replace(needle,replacement);p.write_text(s)
src=[str(p) for p in (v/'Source/Pitch').glob('*.cpp') if p.name!='MpeOutputRouter.cpp']
r=subprocess.run(['g++','-std=c++17','-O2','-DNDEBUG','-I',str(v/'Source'),str(b/'ablation_probe.cpp'),*src,'-o',str(v/'probe')],stdout=(v/'compile.log').open('w'),stderr=subprocess.STDOUT);print('compile',r.returncode,flush=True)
if not r.returncode:
 with (v/'openstrings.csv').open('w') as out:subprocess.run([str(v/'probe'),'10'],stdout=out,check=True)
