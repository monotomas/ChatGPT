from pathlib import Path
import shutil,subprocess
b=Path(__file__).parent;v=b/'time-atoms-per-voice-phase-v2';shutil.copytree(b/'time-atoms-per-voice-phase-v1/Source',v/'Source',dirs_exist_ok=True)
p=v/'Source/Pitch/TimeDomainPursuit.cpp';s=p.read_text()
needle='    const double variance=residual/std::max(samples-dimension,1);'
insert='''
    std::array<double,maxJoint*maxJoint> inverseLower{};
    std::array<double,maxJoint> conditionalVariance{};
    for(int row=0;row<dimension;++row)for(int col=0;col<=row;++col){double value=row==col?1.:0.;
        for(int k=col;k<row;++k)value-=factor[size_t(row*maxJoint+k)]*inverseLower[size_t(k*maxJoint+col)];
        inverseLower[size_t(row*maxJoint+col)]=value/factor[size_t(row*maxJoint+row)];}
    for(int col=0;col<dimension;++col)for(int row=col;row<dimension;++row){const double x=inverseLower[size_t(row*maxJoint+col)];conditionalVariance[size_t(col)]+=x*x;}
''';assert needle in s;s=s.replace(needle,needle+insert)
needle='            const double noisePower=variance/std::max(samples*.5,1.)/std::max(1-coherence*coherence,.01);'
replace='''            const int si=j*columns+h,ci=j*columns+h+8;
            const double conditioning=std::max(conditionalVariance[size_t(si)]*data->cross(note,h,note,h),conditionalVariance[size_t(ci)]*data->cross(note,h+8,note,h+8));
            const double noisePower=variance*(conditionalVariance[size_t(si)]+conditionalVariance[size_t(ci)])*model.weight[size_t(h)]*model.weight[size_t(h)];
            const double phaseUncertainty=std::sqrt(noisePower/std::max(power[size_t(h)],1e-30));''';assert needle in s;s=s.replace(needle,replace)
s=s.replace('const bool reliable=model.valid[size_t(h)]&&coherence<.80&&','const bool reliable=model.valid[size_t(h)]&&coherence<.80&&conditioning<8&&')
needle='''                if(state.warmup<4){state.trend[size_t(h)]=difference/dt;state.departure[size_t(h)]=0;}
                else{'''
replace='''                if(state.warmup<4||clock-state.lastEvent<data->rate*.075){state.trend[size_t(h)]=.5*state.trend[size_t(h)]+.5*difference/dt;state.departure[size_t(h)]=0;}
                else{''';assert needle in s;s=s.replace(needle,replace)
s=s.replace('if(distance>.30)++changed;','if(distance>.30+3*phaseUncertainty)++changed;');p.write_text(s)
p=v/'Source/Pitch/PitchDetector.cpp';s=p.read_text().replace('&&fastEnvelope>slowEnvelope*.35f) {','&&((config.polyphonic&&rearticulated)||fastEnvelope>slowEnvelope*.35f)) {');p.write_text(s)
sources=[str(p) for p in (v/'Source/Pitch').glob('*.cpp') if p.name!='MpeOutputRouter.cpp']
for driver,output,arg in [('ablation_probe.cpp','openstrings','10'),('chord_repick_probe.cpp','chord-repicks',None)]:
 out=v/output;out.mkdir(exist_ok=True);r=subprocess.run(['g++','-std=c++17','-O2','-DNDEBUG','-I',str(v/'Source'),str(b/driver),*sources,'-o',str(out/'probe')],stdout=(out/'compile.log').open('w'),stderr=subprocess.STDOUT);print(output,'compile',r.returncode,flush=True)
 if not r.returncode:
  with (out/'events.csv').open('w') as f:subprocess.run([str(out/'probe')]+([arg] if arg else []),stdout=f,check=True)
 print(output,'capture complete',flush=True)
