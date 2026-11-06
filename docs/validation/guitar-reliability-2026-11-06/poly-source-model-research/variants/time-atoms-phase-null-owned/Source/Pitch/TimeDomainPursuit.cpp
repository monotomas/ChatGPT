// SPDX-License-Identifier: AGPL-3.0-or-later
#include "TimeDomainPursuit.h"
#include <algorithm>
#ifdef FLYSNAPPER_RESEARCH_TRACE_NULL
#include <iostream>
#include <iomanip>
#endif
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
    struct PhaseState {std::array<double,8> phase{},omega{},trend{},departure{},power{};std::array<bool,8> valid{};int warmup=0;int64_t sample=0,lastEvent=-1000000;};
    std::array<PhaseState,noteCount> sourcePhase{};
    std::array<float,noteCount> sourceExcitation{},heldStrength{};
    int64_t observationClock=0,lastObservationClock=0;
    std::array<bool,noteCount> previousHeld{};
    std::array<std::array<double,columns>,noteCount> nullPreviousCoefficient{};
    std::array<std::array<double,8>,noteCount> nullPreviousOmega{};
    std::array<bool,noteCount> nullPreviousHeld{};
    int64_t nullPreviousSample=0;
    double nullPrecision=0;
    unsigned diagnosticEpoch=0;
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
void TimeDomainPursuit::resetTracking() noexcept {if(!data)return;
    data->sourcePhase={};data->sourceExcitation={};data->heldStrength={};data->previousHeld={};data->observationClock=data->lastObservationClock=0;
    data->nullPreviousCoefficient={};data->nullPreviousOmega={};data->nullPreviousHeld={};data->nullPreviousSample=0;data->nullPrecision=0;
    data->prediction={};data->predictionValid=false;++data->diagnosticEpoch;
}
void TimeDomainPursuit::setObservationClock(int64_t sample) noexcept {if(data)data->observationClock=sample;}
void TimeDomainPursuit::copyExcitation(std::array<float,noteCount>& out) const noexcept {out.fill(0);if(data)out=data->sourceExcitation;}
void TimeDomainPursuit::copyHeldStrength(std::array<float,noteCount>& out) const noexcept {out.fill(0);if(data)out=data->heldStrength;}
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
    if(data){data->predictionValid=false;data->heldStrength.fill(0);data->sourceExcitation.fill(0);}
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
    const double cutoff=0.02;
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
    // Acquisition/output keep the soft envelope hypothesis. The separate
    // waveform predictor can jointly explain all already selected harmonics
    // without manufacturing a new note from a benign envelope mismatch.
    std::array<int,6> predictionSelected=selected;int predictionUsed=used;
    if(evidence){int held=0;std::array<int,6> active{};active.fill(-1);
        for(int note=0;note<noteCount;++note)if(evidence->active[size_t(note)]&&held<6)active[size_t(held++)]=note;
        if(held>0){predictionSelected=active;predictionUsed=held;}}
    std::array<std::array<double,columns>,6> predictionCoefficient{};
    constexpr int maxJoint=6*columns;const int dimension=predictionUsed*columns;
    std::array<float,maxJoint*maxJoint> factor{};
    std::array<double,maxJoint> rhs{},solution{};
    for(int i=0;i<dimension;++i){const int ni=predictionSelected[size_t(i/columns)],ci=i%columns;rhs[size_t(i)]=projection[size_t(ni)][size_t(ci)];
        for(int j=0;j<=i;++j){double value=data->cross(ni,ci,predictionSelected[size_t(j/columns)],j%columns)+(i==j?ridge*samples:0.);
            value-=dot(factor.data()+i*maxJoint,factor.data()+j*maxJoint,j);
            factor[size_t(i*maxJoint+j)]=float(i==j?std::sqrt(std::max(value,1e-10)):value/factor[size_t(j*maxJoint+j)]);}}
    for(int i=0;i<dimension;++i){double value=rhs[size_t(i)];for(int j=0;j<i;++j)value-=factor[size_t(i*maxJoint+j)]*solution[size_t(j)];solution[size_t(i)]=value/factor[size_t(i*maxJoint+i)];}
    for(int i=dimension-1;i>=0;--i){double value=solution[size_t(i)];for(int j=i+1;j<dimension;++j)value-=factor[size_t(j*maxJoint+i)]*solution[size_t(j)];solution[size_t(i)]=value/factor[size_t(i*maxJoint+i)];}
    for(int j=0;j<predictionUsed;++j)for(int c=0;c<columns;++c)predictionCoefficient[size_t(j)][size_t(c)]=solution[size_t(j*columns+c)];
    if(evidence)for(int j=0;j<predictionUsed;++j){const int n=predictionSelected[size_t(j)];if(!evidence->active[size_t(n)])continue;const auto &m=data->models[size_t(n)];double power=0;
        for(int c=0;c<columns;++c){const double a=predictionCoefficient[size_t(j)][size_t(c)]*m.weight[size_t(c)];power+=a*a;}data->heldStrength[size_t(n)]=float(std::sqrt(power*.5));}
    // The state below belongs to held physical-source hypotheses. Instant
    // candidate-order churn is excluded by the separate held-set joint fit.
    const int64_t clock=data->observationClock>0?data->observationClock:data->lastObservationClock+predictionSamples;
    if(clock<=data->lastObservationClock)data->sourcePhase={};
    std::array<bool,noteCount> held{};if(evidence)held=evidence->active;
    if(held!=data->previousHeld)data->sourcePhase={};
    data->previousHeld=held;
    data->lastObservationClock=clock;data->sourceExcitation.fill(0);
    double explained=0,regularised=0;
    for(int i=0;i<dimension;++i){explained+=solution[size_t(i)]*rhs[size_t(i)];regularised+=solution[size_t(i)]*solution[size_t(i)];}
    const double residual=std::max(0.,energy-explained-ridge*samples*regularised);
    const double variance=residual/std::max(samples-dimension,1);
    const double freeRelativePrecision=std::max(residual/std::max(energy,1e-30),1e-8);
    bool nullExplained=false;double nullRelativeResidual=-1,gain0=0,gain1=0,gain2=0;
    const bool stableNull=held==data->nullPreviousHeld&&data->nullPreviousSample>0&&clock>data->nullPreviousSample
        &&clock-data->nullPreviousSample<=predictionSamples*4&&predictionUsed>0;
    if(!stableNull)data->nullPrecision=freeRelativePrecision;
    else{
        std::array<std::array<double,columns>,6> prior{};const double dt=double(clock-data->nullPreviousSample),center=(samples-1)*.5;
        for(int j=0;j<predictionUsed;++j){const int n=predictionSelected[size_t(j)];const auto &model=data->models[size_t(n)];
            for(int h=0;h<8;++h){const double old=data->nullPreviousOmega[size_t(n)][size_t(h)],now=model.phase[size_t(h)],rotation=(old-now)*center+.5*(old+now)*dt,c=std::cos(rotation),si=std::sin(rotation);
                const double a=data->nullPreviousCoefficient[size_t(n)][size_t(h)],z=data->nullPreviousCoefficient[size_t(n)][size_t(h+8)];prior[size_t(j)][size_t(h)]=a*c-z*si;prior[size_t(j)][size_t(h+8)]=z*c+a*si;}}
        std::array<float,maxSamples> priorWave{};double matrix[4][5]{};
        for(int i=0;i<samples;++i){double w=0;for(int j=0;j<predictionUsed;++j){const auto &model=data->models[size_t(predictionSelected[size_t(j)])];for(int c=0;c<columns;++c)w+=prior[size_t(j)][size_t(c)]*model.basis[size_t(c*maxSamples+i)];}
            priorWave[size_t(i)]=float(w);const double u=(i-center)/samples,row[4]{w,u*w,u*u*w,1.};
            for(int a=0;a<4;++a){matrix[a][4]+=row[a]*frame[size_t(i)];for(int c=0;c<4;++c)matrix[a][c]+=row[a]*row[c];}}
        bool solved=true;for(int pivot=0;pivot<4;++pivot){int best=pivot;for(int row=pivot+1;row<4;++row)if(std::abs(matrix[row][pivot])>std::abs(matrix[best][pivot]))best=row;
            if(std::abs(matrix[best][pivot])<1e-16){solved=false;break;}if(best!=pivot)for(int c=pivot;c<5;++c)std::swap(matrix[pivot][c],matrix[best][c]);
            const double divisor=matrix[pivot][pivot];for(int c=pivot;c<5;++c)matrix[pivot][c]/=divisor;
            for(int row=0;row<4;++row)if(row!=pivot){const double scale=matrix[row][pivot];for(int c=pivot;c<5;++c)matrix[row][c]-=scale*matrix[pivot][c];}}
        gain0=matrix[0][4];gain1=matrix[1][4];gain2=matrix[2][4];
        if(solved&&gain0>1e-8){double nullError=0;
            for(int i=0;i<samples;++i){const double u=(i-center)/samples,gain=std::clamp(gain0+gain1*u+gain2*u*u,gain0*.10,gain0*10.);
                const double error=frame[size_t(i)]-gain*priorWave[size_t(i)]-matrix[3][4];nullError+=error*error;}
            nullRelativeResidual=nullError/std::max(energy,1e-30);
            // Existing factor-four innovation margin; learn only from frames
            // already explained by the old held wave plus a positive gain.
            nullExplained=nullRelativeResidual<=4*data->nullPrecision;
            if(nullExplained)data->nullPrecision+=(1-std::exp(-dt/(data->rate*.25)))*(freeRelativePrecision-data->nullPrecision);
        }
    }
#ifdef FLYSNAPPER_RESEARCH_TRACE_NULL
    std::cerr<<std::setprecision(9)<<"NULL,"<<data->diagnosticEpoch<<','<<clock<<','<<nullRelativeResidual<<','<<data->nullPrecision<<','<<freeRelativePrecision<<','<<nullExplained<<','<<gain0<<','<<gain1<<','<<gain2<<'\n';
#endif

    std::array<double,maxJoint*maxJoint> inverseLower{};
    std::array<double,maxJoint> conditionalVariance{};
    for(int row=0;row<dimension;++row)for(int col=0;col<=row;++col){double value=row==col?1.:0.;
        for(int k=col;k<row;++k)value-=factor[size_t(row*maxJoint+k)]*inverseLower[size_t(k*maxJoint+col)];
        inverseLower[size_t(row*maxJoint+col)]=value/factor[size_t(row*maxJoint+row)];}
    for(int col=0;col<dimension;++col)for(int row=col;row<dimension;++row){const double x=inverseLower[size_t(row*maxJoint+col)];conditionalVariance[size_t(col)]+=x*x;}

    for(int j=0;j<predictionUsed;++j){
        const int note=predictionSelected[size_t(j)];if(!held[size_t(note)])continue;
        const auto &model=data->models[size_t(note)];auto &state=data->sourcePhase[size_t(note)];
        const int64_t elapsed=clock-state.sample;
        if(elapsed<=0||elapsed>predictionSamples*4){state={};state.lastEvent=-1000000;}
        const double dt=state.sample>0?double(clock-state.sample):0;
        std::array<double,8> power{},angle{};double maximum=0;
        for(int h=0;h<8;++h){const double a=predictionCoefficient[size_t(j)][size_t(h)],c=predictionCoefficient[size_t(j)][size_t(h+8)];power[size_t(h)]=(a*a+c*c)*model.weight[size_t(h)]*model.weight[size_t(h)];maximum=std::max(maximum,power[size_t(h)]);angle[size_t(h)]=std::remainder(std::atan2(c,a)+model.phase[size_t(h)]*(samples-1)*.5,2*pi);}
        int supported=0,changed=0;double largest=0;
        for(int h=0;h<8;++h){double coherence=0;
            for(int other=0;other<predictionUsed;++other)if(other!=j){const auto &m=data->models[size_t(predictionSelected[size_t(other)])];
                for(int k=0;k<8;++k)if(m.valid[size_t(k)]){const double delta=model.phase[size_t(h)]-m.phase[size_t(k)],denominator=samples*std::sin(delta*.5);const double value=std::abs(denominator)<1e-10?1.:std::abs(std::sin(samples*delta*.5)/denominator);coherence=std::max(coherence,value);}}
            const int si=j*columns+h,ci=j*columns+h+8;
            const double conditioning=std::max(conditionalVariance[size_t(si)]*data->cross(note,h,note,h),conditionalVariance[size_t(ci)]*data->cross(note,h+8,note,h+8));
            const double noisePower=variance*(conditionalVariance[size_t(si)]+conditionalVariance[size_t(ci)])*model.weight[size_t(h)]*model.weight[size_t(h)];
            const double phaseUncertainty=std::sqrt(noisePower/std::max(power[size_t(h)],1e-30));
            const bool reliable=model.valid[size_t(h)]&&coherence<.80&&conditioning<8&&power[size_t(h)]>std::max(maximum*.003,noisePower*9.)&&power[size_t(h)]>1e-16;
            if(reliable&&state.valid[size_t(h)]&&dt>0&&dt<=predictionSamples*4){
                const double expected=.5*(model.phase[size_t(h)]+state.omega[size_t(h)])*dt;
                const double difference=std::remainder(angle[size_t(h)]-state.phase[size_t(h)]-expected,2*pi);
                if(state.warmup<4||clock-state.lastEvent<data->rate*.075||nullExplained){state.trend[size_t(h)]=.5*state.trend[size_t(h)]+.5*difference/dt;state.departure[size_t(h)]=0;}
                else{const double innovation=std::remainder(difference-state.trend[size_t(h)]*dt,2*pi);state.departure[size_t(h)]=state.departure[size_t(h)]*std::exp(-dt/(data->rate*.040))+innovation;
                    const double distance=std::abs(state.departure[size_t(h)]);largest=std::max(largest,distance);++supported;if(distance>.30+3*phaseUncertainty)++changed;
                    if(distance<.15)state.trend[size_t(h)]=.9*state.trend[size_t(h)]+.1*difference/dt;}
            }else{state.trend[size_t(h)]=0;state.departure[size_t(h)]=0;}
            state.phase[size_t(h)]=angle[size_t(h)];state.omega[size_t(h)]=model.phase[size_t(h)];state.power[size_t(h)]=power[size_t(h)];state.valid[size_t(h)]=reliable;
        }
        if(!nullExplained&&supported>=2&&changed>=2&&clock-state.lastEvent>data->rate*.075){state.lastEvent=clock;state.departure={};}
        if(clock-state.lastEvent<data->rate*.060)data->sourceExcitation[size_t(note)]=1;
        state.warmup=std::min(state.warmup+1,8);state.sample=clock;
    }
    data->nullPreviousHeld=held;data->nullPreviousSample=clock;
    for(int j=0;j<predictionUsed;++j){const int n=predictionSelected[size_t(j)];data->nullPreviousCoefficient[size_t(n)]=predictionCoefficient[size_t(j)];
        for(int h=0;h<8;++h)data->nullPreviousOmega[size_t(n)][size_t(h)]=data->models[size_t(n)].phase[size_t(h)];}
    data->prediction.fill(float(mean));
    for(int j=0;j<predictionUsed;++j){const auto &model=data->models[size_t(predictionSelected[size_t(j)])];for(int c=0;c<columns;++c)for(int i=0;i<predictionSamples;++i)data->prediction[size_t(i)]+=float(predictionCoefficient[size_t(j)][size_t(c)])*model.continuation[size_t(c*predictionSamples+i)];}
    data->predictionValid=predictionUsed>0;
    (void)evidence;
}
}
