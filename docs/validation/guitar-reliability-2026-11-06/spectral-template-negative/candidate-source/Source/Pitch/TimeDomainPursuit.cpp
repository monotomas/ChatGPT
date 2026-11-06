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
    static constexpr int bins=1025,profiles=12;
    std::vector<float> envelopeTemplates;
    std::array<double,noteCount*profiles> envelopeNorm{},envelopePower{};
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
    data=std::move(next);
}
bool TimeDomainPursuit::copyPrediction(std::array<float,predictionSamples>& out) const noexcept {
    if(!data||!data->predictionValid)return false;
    out=data->prediction;return true;
}
namespace {
// Actual detector peaks are strictly frequency-ordered. Other evidence uses
// the original bounded linear search unless the caller asserts that order.
bool observedPeak(const TimeDomainPursuit::SpectralEvidence& evidence,float target,float tolerance,
                  float minimumMagnitude,float minimumNovelty=-1) noexcept {
    const int count=std::clamp(evidence.count,0,int(evidence.peaks.size()));
    auto qualifies=[&](int i) noexcept {const auto& p=evidence.peaks[size_t(i)];return p.magnitude>=minimumMagnitude&&(minimumNovelty<0||p.novelty>=minimumNovelty)&&std::abs(p.frequency-target)<tolerance;};
    if(!evidence.sortedPeaks){for(int i=0;i<count;++i)if(qualifies(i))return true;return false;}
    int first=0,last=count;
    while(first<last){const int middle=(first+last)/2;if(evidence.peaks[size_t(middle)].frequency<target)first=middle+1;else last=middle;}
    for(int i=first;i<count&&std::abs(evidence.peaks[size_t(i)].frequency-target)<tolerance;++i)if(qualifies(i))return true;
    for(int i=first-1;i>=0&&std::abs(evidence.peaks[size_t(i)].frequency-target)<tolerance;--i)if(qualifies(i))return true;
    return false;
}
}
bool TimeDomainPursuit::hasIndependentPeaks(int note,const SpectralEvidence& evidence,
                                          const std::array<bool,noteCount>& explained,bool requireNovelty) noexcept {
    if(note<0||note>=noteCount||evidence.count<2||evidence.maximumMagnitude<=0)return false;
    const float frequency=evidence.frequencies[size_t(note)];
    if(!std::isfinite(frequency)||frequency<=0)return false;
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
        const bool found=observedPeak(evidence,target,target*.022f+evidence.resolutionHz*.05f,
            evidence.maximumMagnitude*(harmonic==1?.015f:.008f),requireNovelty?.25f:-1);
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
    int count=0;
    for(int h=1;h<=8&&f*h<evidence.maximumHz;++h){
        float target=f*h;int nearest=int(std::lround(target/hint));
        if(nearest>=1&&std::abs(target-hint*nearest)<evidence.resolutionHz)continue;
        if(observedPeak(evidence,target,target*.022f+evidence.resolutionHz*.05f,evidence.maximumMagnitude*.008f))++count;
    }
    return count>=minimum;
}
}

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
