from pathlib import Path
base=Path(__file__).parent/'Source/Pitch'
h=base/'TimeDomainPursuit.h';s=h.read_text();s=s.replace('bool sortedPeaks=false;','bool sortedPeaks=false;\n        const float* powerSpectrum=nullptr;int spectrumBins=0;');h.write_text(s)
p=base/'PitchDetector.cpp';s=p.read_text();s=s.replace('pursuit.analyse(frame.data(),512,strength,scores,config.polyphony,config.sensitivity,&evidence);','evidence.powerSpectrum=spectrum.data();evidence.spectrumBins=int(spectrum.size());\n    pursuit.analyse(frame.data(),512,strength,scores,config.polyphony,config.sensitivity,&evidence);');p.write_text(s)
p=base/'TimeDomainPursuit.cpp';s=p.read_text();s=s.replace('struct TimeDomainPursuit::Data {','struct TimeDomainPursuit::Data {\n    static constexpr int bins=1025,profiles=12;\n    std::vector<float> envelopeTemplates;\n    std::array<double,noteCount*profiles> envelopeNorm{},envelopePower{};')
marker='    data=std::move(next);'
prep=r'''
    // Private research dictionary: four spectral slopes times three odd/even
    // profiles, fixed before any measured result. Amplitude/power templates
    // do not receive note labels, a chord or a desired voice count.
    next->envelopeTemplates.resize(size_t(noteCount)*Data::profiles*Data::bins);
    const auto windowPower=[&](double phase) {
        const auto sum=sums(phase,samples),minus=sums(phase-2*pi/(samples-1),samples),plus=sums(phase+2*pi/(samples-1),samples);
        const double re=.5*sum.cosine-.25*(minus.cosine+plus.cosine),im=.5*sum.sine-.25*(minus.sine+plus.sine);
        return re*re+im*im;
    };
    for(int note=0;note<noteCount;++note){
        const double f=440*std::exp2(double(note+lowNote-69)/12.);
        std::array<std::array<double,Data::bins>,harmonics> partial{};
        for(int h=1;h<=harmonics;++h)if(f*h<analysisRate*.45)
            for(int k=0;k<Data::bins;++k){
                const double bin=analysisRate*k/(2*(Data::bins-1));
                partial[size_t(h-1)][size_t(k)]=.25*(windowPower(2*pi*(bin-f*h)/analysisRate)+windowPower(2*pi*(bin+f*h)/analysisRate));
            }
        for(int profile=0;profile<Data::profiles;++profile){
            const double slope=std::array<double,4>{{.8,1.2,1.8,2.6}}[size_t(profile/3)];
            const double even=std::array<double,3>{{.15,1.,1./.15}}[size_t(profile%3)];
            const size_t id=size_t(note*Data::profiles+profile);auto* target=next->envelopeTemplates.data()+id*Data::bins;
            for(int h=1;h<=harmonics;++h)if(f*h<analysisRate*.45){
                const double amplitude=(h%2?1.:even)/std::pow(double(h),slope);
                next->envelopePower[id]+=amplitude*amplitude;
                for(int k=0;k<Data::bins;++k)target[k]+=float(amplitude*amplitude*partial[size_t(h-1)][size_t(k)]);
            }
            next->envelopeNorm[id]=dot(target,target,Data::bins);
        }
    }
'''
s=s.replace(marker,prep+marker)
start=s.index('void TimeDomainPursuit::analyse(')
s=s[:start]+r'''
void TimeDomainPursuit::analyse(const float*,int count,std::array<float,noteCount>& strength,
                              std::array<float,noteCount>& confidence,int maxVoices,float sensitivity,const SpectralEvidence* evidence) noexcept {
    strength.fill(0);confidence.fill(0);if(data)data->predictionValid=false;
    if(!data||count<data->samples||!evidence||!evidence->powerSpectrum||evidence->spectrumBins!=Data::bins)return;
    maxVoices=std::clamp(maxVoices,1,6);(void)sensitivity;
    constexpr int bins=Data::bins,profiles=Data::profiles;
    std::array<float,bins> observed{},residual{};double energy=0;
    // DC is excluded; all other observed bins retain the actual frontend FFT.
    for(int k=1;k<bins;++k){observed[size_t(k)]=std::max(0.f,evidence->powerSpectrum[k]);energy+=double(observed[size_t(k)])*observed[size_t(k)];}
    if(energy<1e-16)return;residual=observed;
    std::array<int,6> selected{};selected.fill(-1);std::array<double,6> coefficient{};
    int used=0;
    for(int iter=0;iter<maxVoices;++iter){
        int best=-1;double bestImprovement=0;
        for(int note=0;note<noteCount;++note){
            bool duplicate=false;for(int j=0;j<used;++j)duplicate|=selected[size_t(j)]/profiles==note;if(duplicate)continue;
            for(int profile=0;profile<profiles;++profile){
                const int id=note*profiles+profile;const auto* t=data->envelopeTemplates.data()+size_t(id)*bins;
                const double correlation=dot(t,residual.data(),bins),norm=data->envelopeNorm[size_t(id)];
                const double improvement=correlation>0&&norm>0?correlation*correlation/norm:0;
                if(improvement>bestImprovement){bestImprovement=improvement;best=id;}
            }
        }
        // A fixed complexity cost, not a desired/chord-derived note count.
        if(best<0||bestImprovement<energy*.006)break;
        selected[size_t(used++)]=best;
        // Bounded coordinate NNLS over at most six selected source templates.
        for(int sweep=0;sweep<8;++sweep)for(int j=0;j<used;++j){
            const int id=selected[size_t(j)];const auto* t=data->envelopeTemplates.data()+size_t(id)*bins;
            const double old=coefficient[size_t(j)],correlation=dot(t,residual.data(),bins);
            const double next=std::max(0.,old+correlation/std::max(data->envelopeNorm[size_t(id)],1e-30));
            coefficient[size_t(j)]=next;const float change=float(next-old);
            for(int k=0;k<bins;++k)residual[size_t(k)]-=change*t[k];
        }
    }
    for(int j=0;j<used;++j){
        const int id=selected[size_t(j)],note=id/profiles;const double a=coefficient[size_t(j)];
        if(a<=0)continue;strength[size_t(note)]=float(std::sqrt(a*data->envelopePower[size_t(id)]*.5));
        confidence[size_t(note)]=float(std::clamp(a*a*data->envelopeNorm[size_t(id)]/energy,0.,1.));
    }
}
}
'''
p.write_text(s)
