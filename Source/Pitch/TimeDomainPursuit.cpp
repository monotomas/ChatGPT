// SPDX-License-Identifier: AGPL-3.0-or-later
#include "TimeDomainPursuit.h"
#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <vector>
#include <cstdint>
#if defined(__SSE2__) || defined(_M_X64)
#include <emmintrin.h>
#endif

namespace flysnapper::pitch {
namespace {
constexpr int harmonics=8,columns=2*harmonics,components=TimeDomainPursuit::noteCount*columns;
constexpr double pi=3.1415926535897932384626433832795,ridge=.006;
double dot(const float* a,const float* b,int samples) noexcept {
    int i=0;double value=0;
#if defined(__SSE2__) || defined(_M_X64)
    __m128 sum=_mm_setzero_ps();
    for(;i+4<=samples;i+=4)sum=_mm_add_ps(sum,_mm_mul_ps(_mm_loadu_ps(a+i),_mm_loadu_ps(b+i)));
    float lanes[4];_mm_storeu_ps(lanes,sum);
    value=double(lanes[0])+lanes[1]+lanes[2]+lanes[3];
#endif
    for(;i<samples;++i)value+=double(a[i])*b[i];
    return value;
}
struct Pair {double sine,cosine;};
Pair sums(double phase,int samples) noexcept {
    phase=std::remainder(phase,2*pi);
    if(std::abs(phase)<1e-10)return {phase*samples*(samples-1)*.5,double(samples)};
    const double factor=std::sin(samples*phase*.5)/std::sin(phase*.5);
    return {factor*std::sin((samples-1)*phase*.5),factor*std::cos((samples-1)*phase*.5)};
}
void inverse(std::array<double,columns*columns>& matrix) {
    std::array<double,columns*columns*2> augmented{};
    for(int r=0;r<columns;++r)for(int c=0;c<columns;++c){
        augmented[size_t(r*columns*2+c)]=matrix[size_t(r*columns+c)];
        augmented[size_t(r*columns*2+c+columns)]=r==c?1.:0.;
    }
    for(int p=0;p<columns;++p){
        int best=p;
        for(int r=p+1;r<columns;++r)if(std::abs(augmented[size_t(r*columns*2+p)])>std::abs(augmented[size_t(best*columns*2+p)]))best=r;
        if(std::abs(augmented[size_t(best*columns*2+p)])<1e-12)throw std::runtime_error("Singular harmonic regression table");
        if(best!=p)for(int c=0;c<columns*2;++c)std::swap(augmented[size_t(best*columns*2+c)],augmented[size_t(p*columns*2+c)]);
        const double divisor=augmented[size_t(p*columns*2+p)];
        for(int c=0;c<columns*2;++c)augmented[size_t(p*columns*2+c)]/=divisor;
        for(int r=0;r<columns;++r)if(r!=p){
            const double multiplier=augmented[size_t(r*columns*2+p)];
            for(int c=0;c<columns*2;++c)augmented[size_t(r*columns*2+c)]-=multiplier*augmented[size_t(p*columns*2+c)];
        }
    }
    for(int r=0;r<columns;++r)for(int c=0;c<columns;++c)matrix[size_t(r*columns+c)]=augmented[size_t(r*columns*2+c+columns)];
}
}
struct TimeDomainPursuit::Data {
    struct Model {
        std::array<float,columns*maxSamples> basis{};
        std::array<float,columns*predictionSamples> continuation{};
        std::array<double,columns*columns> inv{};
        std::array<double,columns> phase{},mean{},weight{};
        std::array<bool,columns> valid{};
    };
    std::array<Model,noteCount> models{};
    std::vector<float> gram;
    int samples=512;
    double rate=12000;
    std::array<float,predictionSamples> prediction{};bool predictionValid=false;
    float cross(int noteA,int colA,int noteB,int colB) const noexcept {
        return gram[size_t((noteA*columns+colA)*components+noteB*columns+colB)];
    }
};
TimeDomainPursuit::TimeDomainPursuit()=default;
TimeDomainPursuit::~TimeDomainPursuit()=default;
TimeDomainPursuit::TimeDomainPursuit(TimeDomainPursuit&&) noexcept=default;
TimeDomainPursuit& TimeDomainPursuit::operator=(TimeDomainPursuit&&) noexcept=default;
void TimeDomainPursuit::prepare(double analysisRate,int samples){
    if(!std::isfinite(analysisRate)||analysisRate<8000||analysisRate>48000||samples<64||samples>maxSamples)
        throw std::invalid_argument("TimeDomainPursuit requires8..48kHz and64..512samples");
    auto next=std::make_unique<Data>();next->samples=samples;next->rate=analysisRate;
    next->gram.resize(size_t(components)*components);
    for(int note=0;note<noteCount;++note){
        auto& m=next->models[size_t(note)];
        const double f=440*std::pow(2.,double(note+lowNote-69)/12.);
        for(int c=0;c<columns;++c){
            const int h=c%harmonics+1;const bool cosine=c>=harmonics;
            m.phase[size_t(c)]=2*pi*f*h/analysisRate;m.weight[size_t(c)]=1/std::sqrt(double(h));
            m.valid[size_t(c)]=f*h<analysisRate*.45;
            if(!m.valid[size_t(c)])continue;
            const auto sum=sums(m.phase[size_t(c)],samples);
            m.mean[size_t(c)]=(cosine?sum.cosine:sum.sine)/samples;
            for(int i=0;i<samples;++i){
                const double angle=m.phase[size_t(c)]*i;
                m.basis[size_t(c*maxSamples+i)]=float(((cosine?std::cos(angle):std::sin(angle))-m.mean[size_t(c)])*m.weight[size_t(c)]);
            }
            for(int i=0;i<predictionSamples;++i) {
                const double angle=m.phase[size_t(c)]*(samples+i);
                m.continuation[size_t(c*predictionSamples+i)]=float(((cosine?std::cos(angle):std::sin(angle))-m.mean[size_t(c)])*m.weight[size_t(c)]);
            }
        }
    }
    // Analytic inner products avoid a large O(notes^2*partials^2*samples)
    // preparation loop. They are fixed before realtime use, never recomputed.
    for(int a=0;a<components;++a)for(int b=0;b<=a;++b){
        const int na=a/columns,ca=a%columns,nb=b/columns,cb=b%columns;
        const auto& ma=next->models[size_t(na)];const auto& mb=next->models[size_t(nb)];
        double value=0;
        if(ma.valid[size_t(ca)]&&mb.valid[size_t(cb)]){
            const auto minus=sums(ma.phase[size_t(ca)]-mb.phase[size_t(cb)],samples);
            const auto plus=sums(ma.phase[size_t(ca)]+mb.phase[size_t(cb)],samples);
            const bool ac=ca>=harmonics,bc=cb>=harmonics;
            if(ac&&bc)value=.5*(minus.cosine+plus.cosine);
            else if(!ac&&!bc)value=.5*(minus.cosine-plus.cosine);
            else if(!ac&&bc)value=.5*(plus.sine+minus.sine);
            else value=.5*(plus.sine-minus.sine);
            value=(value-samples*ma.mean[size_t(ca)]*mb.mean[size_t(cb)])*ma.weight[size_t(ca)]*mb.weight[size_t(cb)];
        }
        next->gram[size_t(a*components+b)]=next->gram[size_t(b*components+a)]=float(value);
    }
    for(int note=0;note<noteCount;++note){
        auto& m=next->models[size_t(note)];
        for(int r=0;r<columns;++r)for(int c=0;c<columns;++c)
            m.inv[size_t(r*columns+c)]=next->cross(note,r,note,c)+(r==c?ridge*samples:0);
        inverse(m.inv);
    }
    data=std::move(next);
}
bool TimeDomainPursuit::copyPrediction(std::array<float,predictionSamples>& out) const noexcept {
    if(!data||!data->predictionValid)return false;
    out=data->prediction;return true;
}
bool TimeDomainPursuit::hasIndependentPeaks(int note,const SpectralEvidence& evidence,
                                          const std::array<bool,noteCount>& explained,bool requireNovelty) noexcept {
    if(note<0||note>=noteCount||evidence.count<2||evidence.maximumMagnitude<=0)return false;
    const float frequency=evidence.frequencies[size_t(note)];
    if(!std::isfinite(frequency)||frequency<=0)return false;
    const int peakCount=std::clamp(evidence.count,0,int(evidence.peaks.size()));
    int supported=0;bool fundamental=false;
    for(int harmonic=1;harmonic<=8;++harmonic) {
        const float target=frequency*harmonic;if(target>=evidence.maximumHz)break;
        bool shared=false;
        for(int other=0;other<noteCount&&!shared;++other)if(other!=note&&explained[size_t(other)]) {
            const float f=evidence.frequencies[size_t(other)];
            if(!std::isfinite(f)||f<=0)continue;
            const int nearest=int(std::lround(target/f));
            if(nearest>=1&&f*nearest<evidence.maximumHz&&std::abs(target-f*nearest)<evidence.resolutionHz)shared=true;
        }
        if(shared){if(harmonic==1)return false;continue;}
        bool found=false;
        for(int p=0;p<peakCount;++p) {
            const auto& peak=evidence.peaks[size_t(p)];
            if(peak.magnitude<evidence.maximumMagnitude*(harmonic==1?.015f:.008f))continue;
            if(requireNovelty&&peak.novelty<.25f)continue;
            if(std::abs(peak.frequency-target)<target*.022f+evidence.resolutionHz*.05f){found=true;break;}
        }
        if(found){++supported;if(harmonic==1)fundamental=true;}
        else if(harmonic==1)return false;
    }
    return fundamental&&supported>=2;
}
namespace {
bool independentOfAnchor(int note,const TimeDomainPursuit::SpectralEvidence& evidence,int minimum) noexcept {
    if(note<0||note>=TimeDomainPursuit::noteCount||evidence.anchorNote<0||evidence.anchorNote>=TimeDomainPursuit::noteCount)return false;
    const float f=evidence.frequencies[size_t(note)],hint=evidence.frequencies[size_t(evidence.anchorNote)];
    if(!std::isfinite(f)||!std::isfinite(hint)||f<=0||hint<=0||evidence.maximumMagnitude<=0)return false;
    const int peakCount=std::clamp(evidence.count,0,int(evidence.peaks.size()));
    int count=0;
    for(int h=1;h<=8&&f*h<evidence.maximumHz;++h){
        float target=f*h;int nearest=int(std::lround(target/hint));
        if(nearest>=1&&std::abs(target-hint*nearest)<evidence.resolutionHz)continue;
        for(int p=0;p<peakCount;++p){const auto& peak=evidence.peaks[size_t(p)];
            if(peak.magnitude>=evidence.maximumMagnitude*.008f&&std::abs(peak.frequency-target)<target*.022f+evidence.resolutionHz*.05f){++count;break;}}
    }
    return count>=minimum;
}
}
void TimeDomainPursuit::analyse(const float* wave,int count,std::array<float,noteCount>& strength,
                              std::array<float,noteCount>& confidence,int maxVoices,float sensitivity,const SpectralEvidence* evidence) noexcept {
    strength.fill(0);confidence.fill(0);
    if(data)data->predictionValid=false;
    if(!data||!wave||count<data->samples)return;
    maxVoices=std::clamp(maxVoices,1,6);sensitivity=std::clamp(sensitivity,0.f,1.f);
    const int anchor=evidence&&evidence->anchorNote>=0&&evidence->anchorNote<noteCount?evidence->anchorNote:-1;
    const int samples=data->samples;
    std::array<float,maxSamples> frame{};
    double mean=0,energy=0;
    wave+=count-samples;
    for(int i=0;i<samples;++i){frame[size_t(i)]=std::isfinite(wave[i])?std::clamp(wave[i],-16.f,16.f):0;mean+=frame[size_t(i)];}
    mean/=samples;
    for(int i=0;i<samples;++i){frame[size_t(i)]-=float(mean);energy+=double(frame[size_t(i)])*frame[size_t(i)];}
    if(energy<1e-12)return;
    std::array<std::array<double,columns>,noteCount> projection{};
    for(int note=0;note<noteCount;++note){
        const auto& m=data->models[size_t(note)];
        for(int c=0;c<columns;++c){
            const auto* basis=m.basis.data()+c*maxSamples;
            projection[size_t(note)][size_t(c)]=dot(basis,frame.data(),samples);
        }
    }
    std::array<int,6> selected{};selected.fill(-1);
    std::array<std::array<double,columns>,6> coefficients{};
    int used=0;double firstScore=0;
    std::array<double,noteCount> octaveEvidence{};
    constexpr int maxJoint=6*columns;
    std::array<float,maxJoint*maxJoint> joint{};
    // A lower note's freely fitted even harmonics can absorb an additional
    // octave voice. Fit a generic log-linear decay to the odd partials, then
    // require broad even-partial excess before proposing the octave. This is
    // an instrument-envelope prior, not a chord/scale rule. Evidence survives
    // subsequent joint reallocation within this frame, never across frames.
    auto refreshOctaveEvidence=[&]() noexcept {
        for(int j=0;j<used;++j){
            const int note=selected[size_t(j)];
            if(note+12>=noteCount)continue;
            std::array<double,harmonics> amp{};
            const auto& model=data->models[size_t(note)];
            for(int h=0;h<harmonics;++h){
                const double a=coefficients[size_t(j)][size_t(h)],b=coefficients[size_t(j)][size_t(h+harmonics)];
                amp[size_t(h)]=std::sqrt(a*a+b*b)*model.weight[size_t(h)];
            }
            double sx=0,sy=0,sxx=0,sxy=0;
            for(int h:{0,2,4,6}){
                const double x=std::log(double(h+1)),y=std::log(std::max(amp[size_t(h)],1e-9));
                sx+=x;sy+=y;sxx+=x*x;sxy+=x*y;
            }
            const double slope=std::clamp((4*sxy-sx*sy)/(4*sxx-sx*sx),-3.0,.5);
            const double intercept=(sy-slope*sx)/4;
            int strong=0;double excess=0;
            for(int h:{1,3,5,7}){
                const double predicted=std::exp(intercept+slope*std::log(double(h+1)));
                const double ratio=amp[size_t(h)]/std::max(predicted,1e-9);
                if(ratio>2.0)++strong;
                excess+=std::max(0.,std::log(std::max(ratio,1e-9)));
            }
            if(strong>=3)octaveEvidence[size_t(note+12)]=std::max(octaveEvidence[size_t(note+12)],excess);
        }
    };
    auto computeActiveUpperSupport=[&](int note) noexcept {
        if(!evidence||!evidence->active[size_t(note)])return false;
        const double frequency=evidence->frequencies[size_t(note)];int found=0;
        if(!std::isfinite(frequency)||frequency<=0||!std::isfinite(evidence->maximumMagnitude)||evidence->maximumMagnitude<=0)return false;
        const int peakCount=std::clamp(evidence->count,0,int(evidence->peaks.size()));
        bool lowerSelected=false;for(int j=0;j<used;++j)lowerSelected|=selected[size_t(j)]==note-12;
        if(!lowerSelected)return false;
        const double base=frequency*.5;
        if(base*16>=evidence->maximumHz)return false;
        std::array<double,16> physical{};int oddFound=0;
        for(int h=1;h<=16;++h){
            const double target=base*h;double value=0;
            for(int q=0;q<peakCount;++q){const auto& peak=evidence->peaks[size_t(q)];
                if(std::abs(peak.frequency-target)<target*.022f+evidence->resolutionHz*.05f)value=std::max(value,double(peak.magnitude));
            }
            physical[size_t(h-1)]=std::max(value,double(evidence->maximumMagnitude)*.004);
            if(h%2&&value>=evidence->maximumMagnitude*.008)++oddFound;
        }
        if(oddFound<5)return false;
        double sx=0,sy=0,sxx=0,sxy=0;
        for(int h=1;h<=15;h+=2){double x=std::log(double(h)),y=std::log(physical[size_t(h-1)]);sx+=x;sy+=y;sxx+=x*x;sxy+=x*y;}
        const double slope=std::clamp((8*sxy-sx*sy)/(8*sxx-sx*sx),-3.,.5),intercept=(sy-slope*sx)/8;
        double excess=0;int strong=0;
        for(int h=10;h<=16;h+=2){double ratio=physical[size_t(h-1)]/std::exp(intercept+slope*std::log(double(h)));excess+=std::log(std::max(ratio,1e-9));if(ratio>2)++strong;}
        if(strong<2||excess<4*std::log(1.8))return false;
        for(int h=3;h<=8;++h){
            const double target=frequency*h;if(target>=evidence->maximumHz)break;
            bool shared=false;
            for(int j=0;j<used&&!shared;++j){
                const double other=evidence->frequencies[size_t(selected[size_t(j)])];
                if(!std::isfinite(other)||other<=0)continue;
                const int k=int(std::lround(target/other));
                if(k>=1&&k<=8&&std::abs(target-other*k)<evidence->resolutionHz)shared=true;
            }
            if(shared)continue;
            for(int q=0;q<peakCount;++q){const auto& peak=evidence->peaks[size_t(q)];
                if(peak.magnitude>=evidence->maximumMagnitude*.008f&&std::abs(peak.frequency-target)<target*.022f+evidence->resolutionHz*.05f){++found;break;}
            }
        }
        return found>=2;
    };
    // The selected models stay fixed while candidates are compared. Cache
    // this selected-model-dependent result only within that one iteration.
    std::array<int8_t,noteCount> activeSupport{};activeSupport.fill(-1);
    auto activeUpperSupport=[&](int note) noexcept {
        auto& cached=activeSupport[size_t(note)];
        if(cached<0)cached=computeActiveUpperSupport(note)?1:0;
        return cached!=0;
    };
    const double cutoff=.06+.12*(1-sensitivity);
    for(int iteration=0;iteration<maxVoices;++iteration){
        activeSupport.fill(-1);
        int best=-1;double bestScore=0;bool bestWeak=false;
        std::array<double,columns> bestCoefficients{};
        for(int note=0;note<noteCount;++note){
            bool duplicate=false;for(int j=0;j<used;++j)duplicate|=selected[size_t(j)]==note;if(duplicate)continue;
            std::array<double,columns> correlation=projection[size_t(note)],beta{};
            for(int r=0;r<columns;++r)for(int j=0;j<used;++j)for(int c=0;c<columns;++c)
                correlation[size_t(r)]-=data->cross(note,r,selected[size_t(j)],c)*coefficients[size_t(j)][size_t(c)];
            const auto& model=data->models[size_t(note)];
            for(int r=0;r<columns;++r)for(int c=0;c<columns;++c)beta[size_t(r)]+=model.inv[size_t(r*columns+c)]*correlation[size_t(c)];
            double score=0,totalAmplitude=0,fundamental=0;
            for(int c=0;c<columns;++c){
                score+=correlation[size_t(c)]*beta[size_t(c)];
                const double a=beta[size_t(c)]*model.weight[size_t(c)];totalAmplitude+=a*a;
                if(c==0||c==harmonics)fundamental+=a*a;
            }
            if(totalAmplitude<=0||(fundamental<totalAmplitude*.05&&anchor!=note&&!activeUpperSupport(note)))continue;
            if(anchor>=0&&anchor!=note&&octaveEvidence[size_t(note)]<=0&&!activeUpperSupport(note)
                &&!independentOfAnchor(note,*evidence,note<anchor?3:2))continue;
            bool weak=false;
            if(iteration>0&&score<firstScore*cutoff) {
                if(score<firstScore*.006||score<energy*.006||!evidence)continue;
                auto explained=evidence->active;
                for(int j=0;j<used;++j)explained[size_t(selected[size_t(j)])]=true;
                if(!activeUpperSupport(note)&&!hasIndependentPeaks(note,*evidence,explained,false))continue;
                weak=true;
            }
            if(score>bestScore){bestScore=score;best=note;bestCoefficients=beta;bestWeak=weak;}
        }
        if(iteration==0)firstScore=bestScore;
        if(best<0||bestScore<firstScore*(bestWeak?.006:cutoff)||bestScore<energy*.006){
            refreshOctaveEvidence();
            int extra=-1;double strongest=0;
            for(int note=0;note<noteCount;++note){
                bool duplicate=false;for(int j=0;j<used;++j)duplicate|=selected[size_t(j)]==note;
                if(!duplicate&&octaveEvidence[size_t(note)]>strongest){extra=note;strongest=octaveEvidence[size_t(note)];}
            }
            if(extra<0)break;
            best=extra;bestCoefficients={};
        }
        selected[size_t(used)]=best;coefficients[size_t(used)]=bestCoefficients;++used;
        // Nearly shared octave/fifth columns make a small fixed number of
        // coordinate sweeps allocation-dependent despite a small residual.
        // Solve the positive-definite joint ridge system directly. Appending
        // sixteen columns preserves the already factored leading block; only
        // the new rows are factorised. Maximum dimension is exactly 96, with
        // stack storage and no heap activity, waits, or unbounded loops.
        const int dimension=used*columns,previousDimension=(used-1)*columns;
        std::array<double,maxJoint> rhs{},solution{};
        for(int i=0;i<dimension;++i)rhs[size_t(i)]=projection[size_t(selected[size_t(i/columns)])][size_t(i%columns)];
        for(int i=previousDimension;i<dimension;++i){
            const int ni=selected[size_t(i/columns)],ci=i%columns;
            for(int j=0;j<=i;++j){
                double value=data->cross(ni,ci,selected[size_t(j/columns)],j%columns);
                if(i==j)value+=ridge*samples;
                value-=dot(joint.data()+i*maxJoint,joint.data()+j*maxJoint,j);
                joint[size_t(i*maxJoint+j)]=float(i==j?std::sqrt(std::max(value,1e-10)):value/joint[size_t(j*maxJoint+j)]);
            }
        }
        for(int i=0;i<dimension;++i){
            double value=rhs[size_t(i)];
            for(int j=0;j<i;++j)value-=joint[size_t(i*maxJoint+j)]*solution[size_t(j)];
            solution[size_t(i)]=value/joint[size_t(i*maxJoint+i)];
        }
        for(int i=dimension-1;i>=0;--i){
            double value=solution[size_t(i)];
            for(int j=i+1;j<dimension;++j)value-=joint[size_t(j*maxJoint+i)]*solution[size_t(j)];
            solution[size_t(i)]=value/joint[size_t(i*maxJoint+i)];
        }
        for(int j=0;j<used;++j)for(int c=0;c<columns;++c)coefficients[size_t(j)][size_t(c)]=solution[size_t(j*columns+c)];
        refreshOctaveEvidence();
        // Additional note hypotheses may reduce envelope complexity even if waveform residual is small.
    }
    for(int j=0;j<used;++j){
        const int note=selected[size_t(j)];const auto& model=data->models[size_t(note)];
        double magnitude=0,fundamental=0,modelEnergy=0;
        for(int c=0;c<columns;++c){
            const double amplitude=coefficients[size_t(j)][size_t(c)]*model.weight[size_t(c)];magnitude+=amplitude*amplitude;
            if(c==0||c==harmonics)fundamental+=amplitude*amplitude;
            for(int r=0;r<columns;++r)modelEnergy+=coefficients[size_t(j)][size_t(c)]*data->cross(note,c,note,r)*coefficients[size_t(j)][size_t(r)];
        }
        // A strongly supported octave may share its fundamental with an
        // existing lower note's third or even harmonic. In that case require
        // independent upper partials instead of discarding an already fitted
        // voice merely because ridge allocation favoured the lower column.
        bool octaveSupported=false;
        if(octaveEvidence[size_t(note)]>0&&modelEnergy>energy*.015){
            double largest=0,independentMagnitude=0;int independentStrong=0;
            std::array<double,harmonics> amplitudes{};
            for(int h=0;h<harmonics;++h){
                double a=coefficients[size_t(j)][size_t(h)],b=coefficients[size_t(j)][size_t(h+harmonics)];
                amplitudes[size_t(h)]=std::sqrt(a*a+b*b)*model.weight[size_t(h)];largest=std::max(largest,amplitudes[size_t(h)]);
            }
            for(int h=4;h<harmonics;++h){
                if(!model.valid[size_t(h)])continue;
                bool shared=false;
                for(int other=0;other<used&&!shared;++other)if(other!=j){
                    const auto& m=data->models[size_t(selected[size_t(other)])];
                    for(int k=0;k<harmonics;++k)if(m.valid[size_t(k)]&&std::abs(model.phase[size_t(h)]-m.phase[size_t(k)])*samples<1.0){shared=true;break;}
                }
                if(!shared){
                    const double a=amplitudes[size_t(h)];independentMagnitude+=a*a;
                    if(a>largest*.10)++independentStrong;
                }
            }
            octaveSupported=independentStrong>=2&&independentMagnitude>magnitude*.02;
        }
        if(fundamental<magnitude*.035&&!octaveSupported&&(anchor!=note))continue;
        strength[size_t(note)]=float(std::sqrt(std::max(0.,magnitude)*.5));
        confidence[size_t(note)]=float(std::clamp(modelEnergy/energy,0.,1.));
    }
    data->prediction.fill(float(mean));
    for(int j=0;j<used;++j) {
        const auto& model=data->models[size_t(selected[size_t(j)])];
        for(int c=0;c<columns;++c) {
            const float coefficient=float(coefficients[size_t(j)][size_t(c)]);
            for(int i=0;i<predictionSamples;++i)data->prediction[size_t(i)]+=coefficient*model.continuation[size_t(c*predictionSamples+i)];
        }
    }
    // This is analytic continuation of the past-only fitted oscillator model.
    // It reads no future audio. The caller compares it with samples only when
    // they actually arrive, providing an onset-innovation signal that is not
    // confused by beating already represented by the current voices.
    data->predictionValid=used>0;

}
}
