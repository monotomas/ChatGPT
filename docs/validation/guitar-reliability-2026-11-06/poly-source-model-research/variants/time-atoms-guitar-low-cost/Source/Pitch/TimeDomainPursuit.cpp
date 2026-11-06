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
    std::array<std::array<float,8>,27> profiles{};
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
    int profile=0;
    for(double slope:{.7,1.2,1.8})for(double odd:{.15,1.,4.})for(double pluck:{0.,.22,.37}){
        for(int h=1;h<=8;++h)next->profiles[size_t(profile)][size_t(h-1)]=float((h%2?odd:1.)/std::pow(double(h),slope)*(pluck==0?1.:std::abs(std::sin(pi*h*pluck))));
        ++profile;
    }

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
void TimeDomainPursuit::analyse(const float* wave,int count,std::array<float,noteCount>& strength,
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
    // Private explicit standard-six-string prior: each model must be
    // jointly assignable to a distinct string, before it can explain audio.
    auto guitarAssignable=[&](int note) noexcept {
        std::array<int,7> ordered{};
        for(int j=0;j<used;++j)ordered[size_t(j)]=selected[size_t(j)]+lowNote;
        ordered[size_t(used)]=note+lowNote;
        std::sort(ordered.begin(),ordered.begin()+used+1);
        constexpr std::array<int,6> tuning{{40,45,50,55,59,64}};
        int string=0;
        for(int j=0;j<=used;++j){
            while(string<6&&(ordered[size_t(j)]<tuning[size_t(string)]||ordered[size_t(j)]>tuning[size_t(string)]+24))++string;
            if(string>=6)return false;++string;
        }
        return true;
    };
    const double cutoff=0.006;
    auto crossShape=[&](int a,const std::array<double,columns>& x,int b,const std::array<double,columns>& y) noexcept {
        double result=0;for(int i=0;i<columns;++i)for(int j=0;j<columns;++j)result+=x[size_t(i)]*data->cross(a,i,b,j)*y[size_t(j)];return result;
    };
    for(int iteration=0;iteration<maxVoices;++iteration){
        int best=-1;double bestScore=0;
        std::array<double,columns> bestShape{};
        for(int note=0;note<noteCount;++note){
            if(!guitarAssignable(note))continue;
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
