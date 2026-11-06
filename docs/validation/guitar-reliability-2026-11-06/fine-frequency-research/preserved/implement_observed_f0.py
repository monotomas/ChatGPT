from pathlib import Path
P=Path(__file__).resolve().parent/'observed-f0/Source/Pitch/TimeDomainPursuit.cpp'
s=P.read_text()
s=s.replace('void inverse(std::array<double,columns*columns>& matrix) {','bool invertBounded(std::array<double,columns*columns>& matrix) noexcept {')
s=s.replace('if(std::abs(augmented[size_t(best*columns*2+p)])<1e-12)throw std::runtime_error("Singular harmonic regression table");','if(std::abs(augmented[size_t(best*columns*2+p)])<1e-12)return false;')
needle='''    for(int r=0;r<columns;++r)for(int c=0;c<columns;++c)matrix[size_t(r*columns+c)]=augmented[size_t(r*columns*2+c+columns)];
}
}
struct TimeDomainPursuit::Data {'''
replace='''    for(int r=0;r<columns;++r)for(int c=0;c<columns;++c)matrix[size_t(r*columns+c)]=augmented[size_t(r*columns*2+c+columns)];
    return true;
}
void inverse(std::array<double,columns*columns>& matrix) {
    if(!invertBounded(matrix))throw std::runtime_error("Singular harmonic regression table");
}
}
struct TimeDomainPursuit::Data {'''
assert needle in s;s=s.replace(needle,replace)
s=s.replace('''    struct Model {
        std::array<float,columns*maxSamples> basis{};''','''    struct Geometry {double hs=0,hc=1,ns=0,nc=1,cs=0,cc=1;};
    struct Model {
        double frequency=0;
        std::array<Geometry,harmonics> geometry{};
        std::array<float,columns*maxSamples> basis{};''')
needle='''    float cross(int noteA,int colA,int noteB,int colB) const noexcept {
        return gram[size_t((noteA*columns+colA)*components+noteB*columns+colB)];
    }
};'''
replace=needle[:-3]+'''
    static void cacheGeometry(Model& model,double frequency,double rate,int samples) noexcept {
        model.frequency=frequency;
        const double half=pi*frequency/rate;
        const Geometry step{std::sin(half),std::cos(half),std::sin(samples*half),std::cos(samples*half),std::sin((samples-1)*half),std::cos((samples-1)*half)};
        Geometry phase=step;
        for(int h=0;h<harmonics;++h){
            model.geometry[size_t(h)]=phase;
            const auto multiply=[](double as,double ac,double bs,double bc,double& os,double& oc) noexcept {os=as*bc+ac*bs;oc=ac*bc-as*bs;};
            Geometry next;
            multiply(phase.hs,phase.hc,step.hs,step.hc,next.hs,next.hc);
            multiply(phase.ns,phase.nc,step.ns,step.nc,next.ns,next.nc);
            multiply(phase.cs,phase.cc,step.cs,step.cc,next.cs,next.cc);
            phase=next;
        }
    }
    static Pair combinedSums(const Geometry& a,const Geometry& b,bool plus,int samples) noexcept {
        const double sign=plus?1.:-1.;
        const double denominator=a.hs*b.hc+sign*a.hc*b.hs;
        const double numerator=a.ns*b.nc+sign*a.nc*b.ns;
        const double factor=std::abs(denominator)>1e-10?numerator/denominator:double(samples);
        return {factor*(a.cs*b.cc+sign*a.cc*b.cs),factor*(a.cc*b.cc-sign*a.cs*b.cs)};
    }
    static double analyticCross(const Model& a,int ca,const Model& b,int cb,int samples) noexcept {
        if(!a.valid[size_t(ca)]||!b.valid[size_t(cb)])return 0;
        const auto minus=combinedSums(a.geometry[size_t(ca%harmonics)],b.geometry[size_t(cb%harmonics)],false,samples);
        const auto plus=combinedSums(a.geometry[size_t(ca%harmonics)],b.geometry[size_t(cb%harmonics)],true,samples);
        const bool ac=ca>=harmonics,bc=cb>=harmonics;
        const double value=ac?(bc?.5*(minus.cosine+plus.cosine):.5*(plus.sine-minus.sine)):(bc?.5*(plus.sine+minus.sine):.5*(minus.cosine-plus.cosine));
        return (value-samples*a.mean[size_t(ca)]*b.mean[size_t(cb)])*a.weight[size_t(ca)]*b.weight[size_t(cb)];
    }
    bool rebuildModel(int note,double frequency) noexcept {
        Model next;
        cacheGeometry(next,frequency,rate,samples);
        for(int h=0;h<harmonics;++h){
            const auto& g=next.geometry[size_t(h)];
            const double omega=2*pi*frequency*(h+1)/rate,weight=1/std::sqrt(double(h+1));
            next.phase[size_t(h)]=next.phase[size_t(h+harmonics)]=omega;
            next.weight[size_t(h)]=next.weight[size_t(h+harmonics)]=weight;
            next.valid[size_t(h)]=next.valid[size_t(h+harmonics)]=frequency*(h+1)<rate*.45;
            if(!next.valid[size_t(h)])continue;
            const double factor=std::abs(g.hs)>1e-10?g.ns/g.hs:double(samples);
            next.mean[size_t(h)]=factor*g.cs/samples;next.mean[size_t(h+harmonics)]=factor*g.cc/samples;
            const double ds=2*g.hs*g.hc,dc=g.hc*g.hc-g.hs*g.hs;
            double sn=0,cs=1;
            for(int i=0;i<samples+predictionSamples;++i){
                const float a=float((sn-next.mean[size_t(h)])*weight),b=float((cs-next.mean[size_t(h+harmonics)])*weight);
                if(i<samples){next.basis[size_t(h*maxSamples+i)]=a;next.basis[size_t((h+harmonics)*maxSamples+i)]=b;}
                else{next.continuation[size_t(h*predictionSamples+i-samples)]=a;next.continuation[size_t((h+harmonics)*predictionSamples+i-samples)]=b;}
                const double ns=sn*dc+cs*ds;cs=cs*dc-sn*ds;sn=ns;
            }
        }
        for(int r=0;r<columns;++r)for(int c=0;c<columns;++c)next.inv[size_t(r*columns+c)]=float(analyticCross(next,r,next,c,samples))+(r==c?ridge*samples:0);
        if(!invertBounded(next.inv))return false;
        models[size_t(note)]=next;
        const auto& a=models[size_t(note)];
        for(int other=0;other<noteCount;++other){const auto& b=models[size_t(other)];
            for(int h=0;h<harmonics;++h)for(int k=0;k<harmonics;++k){
                const auto minus=combinedSums(a.geometry[size_t(h)],b.geometry[size_t(k)],false,samples);
                const auto plus=combinedSums(a.geometry[size_t(h)],b.geometry[size_t(k)],true,samples);
                const double product=a.weight[size_t(h)]*b.weight[size_t(k)];
                const std::array<double,4> value{{.5*(minus.cosine-plus.cosine),.5*(plus.sine+minus.sine),.5*(plus.sine-minus.sine),.5*(minus.cosine+plus.cosine)}};
                for(int qa=0;qa<2;++qa)for(int qb=0;qb<2;++qb){const int ca=h+qa*harmonics,cb=k+qb*harmonics;
                    const float crossValue=a.valid[size_t(ca)]&&b.valid[size_t(cb)]?float((value[size_t(qa*2+qb)]-samples*a.mean[size_t(ca)]*b.mean[size_t(cb)])*product):0;
                    gram[size_t((note*columns+ca)*components+other*columns+cb)]=crossValue;
                    gram[size_t((other*columns+cb)*components+note*columns+ca)]=crossValue;
                }
            }
        }
        return true;
    }
    void retuneObserved(const SpectralEvidence* evidence) noexcept {
        if(!evidence||evidence->count<=0||evidence->maximumMagnitude<=0)return;
        struct Observation {float f=0,weight=0;};
        std::array<std::array<Observation,harmonics>,noteCount> observed{};
        const int peakCount=std::clamp(evidence->count,0,int(evidence->peaks.size()));
        for(int i=0;i<peakCount;++i){const auto& peak=evidence->peaks[size_t(i)];
            if(!std::isfinite(peak.frequency)||!std::isfinite(peak.magnitude)||peak.frequency<=0||peak.magnitude<evidence->maximumMagnitude*.008f)continue;
            for(int h=1;h<=harmonics;++h){
                const double f=peak.frequency/h,pitch=69+12*std::log2(f/440.);
                const int midi=int(std::lround(pitch));
                // Exactly the already explicit standard-six-string prior.
                if(midi<40||midi>88)continue;
                const int note=midi-lowNote;
                auto& target=observed[size_t(note)][size_t(h-1)];
                if(peak.magnitude>target.weight)target={float(f),peak.magnitude};
            }
        }
        struct Candidate {int note=-1;double f=0,score=0;};
        std::array<Candidate,noteCount> candidates{};int candidateCount=0;
        for(int n=0;n<noteCount;++n){
            std::array<Observation,harmonics> values{};int used=0;double total=0;
            for(const auto& x:observed[size_t(n)])if(x.weight>0){values[size_t(used++)]=x;total+=x.weight;}
            if(used<2)continue;
            std::sort(values.begin(),values.begin()+used,[](const Observation&a,const Observation&b){return a.f<b.f;});
            double cumulative=0,f=values[size_t(used-1)].f;
            for(int i=0;i<used;++i){cumulative+=values[size_t(i)].weight;if(cumulative>=total*.5){f=values[size_t(i)].f;break;}}
            const double score=total+(evidence->active[size_t(n)]?evidence->maximumMagnitude*8:0);
            candidates[size_t(candidateCount++)]={n,f,score};
        }
        const int selected=std::min(candidateCount,12);
        std::partial_sort(candidates.begin(),candidates.begin()+selected,candidates.begin()+candidateCount,[](const Candidate&a,const Candidate&b){return a.score>b.score;});
        for(int i=0;i<selected;++i){const auto& c=candidates[size_t(i)];const auto& current=models[size_t(c.note)];
            const double f=evidence->active[size_t(c.note)]?current.frequency*.75+c.f*.25:c.f;
            // A numerical cache tolerance, not an admission/output threshold.
            if(std::abs(1200*std::log2(f/current.frequency))>=1.)rebuildModel(c.note,f);
        }
    }
};'''
assert needle in s;s=s.replace(needle,replace)
s=s.replace('''        const double f=440*std::pow(2.,double(note+lowNote-69)/12.);
        for(int c=0;c<columns;++c){''','''        const double f=440*std::pow(2.,double(note+lowNote-69)/12.);
        Data::cacheGeometry(m,f,analysisRate,samples);
        for(int c=0;c<columns;++c){''')
needle='''    if(energy<1e-12)return;
    std::array<std::array<double,columns>,noteCount> projection{};'''
replace='''    if(energy<1e-12)return;
    data->retuneObserved(evidence);
    std::array<std::array<double,columns>,noteCount> projection{};'''
assert needle in s;s=s.replace(needle,replace)
P.write_text(s)
