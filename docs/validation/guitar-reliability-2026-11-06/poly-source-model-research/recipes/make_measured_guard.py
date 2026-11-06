from pathlib import Path
import shutil,subprocess
b=Path(__file__).parent;v=b/'measured-root-guards';shutil.copytree(b/'time-atoms-held-prediction/Source',v/'Source',dirs_exist_ok=True)
p=v/'Source/Pitch/TimeDomainPursuit.cpp';s=p.read_text();original=(b/'Source/Pitch/TimeDomainPursuit.cpp').read_text()
a=original.index('    auto computeActiveUpperSupport=');z=original.index('    const double cutoff=',a);physicalExceptions=original[a:z]
needle='    std::array<double,6> amplitudes{};int used=0;double firstScore=0;';assert needle in s;s=s.replace(needle,needle+'\n    const int anchor=evidence&&evidence->anchorNote>=0&&evidence->anchorNote<noteCount?evidence->anchorNote:-1;\n'+physicalExceptions)
s=s.replace('    for(int iteration=0;iteration<maxVoices;++iteration){\n        int best=','    for(int iteration=0;iteration<maxVoices;++iteration){\n        activeSupport.fill(-1);\n        int best=')
needle='            if(observedPower<1e-15)continue;'
insert='''            // This is measured residual-wave evidence, before a profile
            // can insert a hypothesized fundamental into its own atom.
            if(observed[size_t(0)]*observed[size_t(0)]<observedPower*.05&&anchor!=note
                &&!activeUpperSupport(note)&&!heldOctaveSupport[size_t(note)])continue;
''';assert needle in s;s=s.replace(needle,needle+'\n'+insert)
needle='    for(int j=0;j<used;++j){const int note=selected[size_t(j)];const auto& model=data->models[size_t(note)];double magnitude=0;'
insert='''    // The final evidence fit is independent of the imposed envelope.
    // New candidates must not borrow a profile-imputed root as measurement.
    constexpr int measuredMaximum=6*columns;const int measuredDimension=used*columns;
    std::array<float,measuredMaximum*measuredMaximum> measuredFactor{};
    std::array<double,measuredMaximum> measuredRhs{},measuredSolution{};
    for(int i=0;i<measuredDimension;++i){const int n=selected[size_t(i/columns)],c=i%columns;measuredRhs[size_t(i)]=projection[size_t(n)][size_t(c)];
        for(int j=0;j<=i;++j){double value=data->cross(n,c,selected[size_t(j/columns)],j%columns)+(i==j?ridge*samples:0.);
            value-=dot(measuredFactor.data()+i*measuredMaximum,measuredFactor.data()+j*measuredMaximum,j);
            measuredFactor[size_t(i*measuredMaximum+j)]=float(i==j?std::sqrt(std::max(value,1e-10)):value/measuredFactor[size_t(j*measuredMaximum+j)]);}}
    for(int i=0;i<measuredDimension;++i){double value=measuredRhs[size_t(i)];for(int j=0;j<i;++j)value-=measuredFactor[size_t(i*measuredMaximum+j)]*measuredSolution[size_t(j)];measuredSolution[size_t(i)]=value/measuredFactor[size_t(i*measuredMaximum+i)];}
    for(int i=measuredDimension-1;i>=0;--i){double value=measuredSolution[size_t(i)];for(int j=i+1;j<measuredDimension;++j)value-=measuredFactor[size_t(j*measuredMaximum+i)]*measuredSolution[size_t(j)];measuredSolution[size_t(i)]=value/measuredFactor[size_t(i*measuredMaximum+i)];}
    std::array<bool,6> measuredSupported{};activeSupport.fill(-1);
    for(int j=0;j<used;++j){const int n=selected[size_t(j)];const auto &m=data->models[size_t(n)];double rootPower=0,totalPower=0;
        for(int c=0;c<columns;++c){const double a=measuredSolution[size_t(j*columns+c)]*m.weight[size_t(c)];totalPower+=a*a;if(c==0||c==harmonics)rootPower+=a*a;}
        measuredSupported[size_t(j)]=rootPower>=totalPower*.035||anchor==n||activeUpperSupport(n)||heldOctaveSupport[size_t(n)];}
''';assert needle in s;s=s.replace(needle,insert+needle+'\n        if(!measuredSupported[size_t(j)])continue;');p.write_text(s)
sources=[str(p) for p in (v/'Source/Pitch').glob('*.cpp') if p.name!='MpeOutputRouter.cpp']
r=subprocess.run(['g++','-std=c++17','-O2','-DNDEBUG','-I',str(v/'Source'),str(b/'ablation_probe.cpp'),*sources,'-o',str(v/'probe')],stdout=(v/'compile.log').open('w'),stderr=subprocess.STDOUT);print('compile',r.returncode,flush=True)
if not r.returncode:
 for argument,name in [('8','upper-triad'),('10','openstrings')]:
  with (v/(name+'.csv')).open('w') as out:subprocess.run([str(v/'probe'),argument],stdout=out,check=True)
 print('two30casecaptures complete',flush=True)
