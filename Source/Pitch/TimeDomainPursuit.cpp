// SPDX-License-Identifier: AGPL-3.0-or-later
#include "TimeDomainPursuit.h"
#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <vector>
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
        std::array<double,columns*columns> inv{};
        std::array<double,columns> phase{},mean{},weight{};
        std::array<bool,columns> valid{};
    };
    std::array<Model,noteCount> models{};
    std::vector<float> gram;
    int samples=512;
    double rate=12000;
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
void TimeDomainPursuit::analyse(const float* wave,int count,std::array<float,noteCount>& strength,
                              std::array<float,noteCount>& confidence,int maxVoices,float sensitivity) noexcept {
    strength.fill(0);confidence.fill(0);
    if(!data||!wave||count<data->samples)return;
    maxVoices=std::clamp(maxVoices,1,6);sensitivity=std::clamp(sensitivity,0.f,1.f);
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
    int used=0;double firstScore=0,residualEnergy=energy;
    const double cutoff=.06+.12*(1-sensitivity);
    for(int iteration=0;iteration<maxVoices;++iteration){
        int best=-1;double bestScore=0;
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
            if(totalAmplitude<=0||fundamental<totalAmplitude*.05)continue;
            if(score>bestScore){bestScore=score;best=note;bestCoefficients=beta;}
        }
        if(best<0)break;
        if(iteration==0)firstScore=bestScore;
        if(bestScore<firstScore*cutoff||bestScore<energy*.006)break;
        selected[size_t(used)]=best;coefficients[size_t(used)]=bestCoefficients;++used;
        // Block coordinate descent solves the joint ridge regression. Shared
        // harmonic columns are jointly refitted instead of subtracting a fixed
        // 1/h envelope. Number of sweeps/voices/columns is strictly bounded.
        for(int sweep=0;sweep<12;++sweep)for(int j=0;j<used;++j){
            const int note=selected[size_t(j)];auto correlation=projection[size_t(note)];
            for(int r=0;r<columns;++r)for(int other=0;other<used;++other)if(other!=j)for(int c=0;c<columns;++c)
                correlation[size_t(r)]-=data->cross(note,r,selected[size_t(other)],c)*coefficients[size_t(other)][size_t(c)];
            std::array<double,columns> next{};
            const auto& model=data->models[size_t(note)];
            for(int r=0;r<columns;++r)for(int c=0;c<columns;++c)next[size_t(r)]+=model.inv[size_t(r*columns+c)]*correlation[size_t(c)];
            coefficients[size_t(j)]=next;
        }
        double fit=0;
        for(int j=0;j<used;++j)for(int r=0;r<columns;++r){
            fit+=2*projection[size_t(selected[size_t(j)])][size_t(r)]*coefficients[size_t(j)][size_t(r)];
            for(int other=0;other<used;++other)for(int c=0;c<columns;++c)
                fit-=coefficients[size_t(j)][size_t(r)]*data->cross(selected[size_t(j)],r,selected[size_t(other)],c)*coefficients[size_t(other)][size_t(c)];
        }
        residualEnergy=std::max(0.,energy-fit);
        if(residualEnergy<energy*.008)break;
    }
    for(int j=0;j<used;++j){
        const int note=selected[size_t(j)];const auto& model=data->models[size_t(note)];
        double magnitude=0,fundamental=0,modelEnergy=0;
        for(int c=0;c<columns;++c){
            const double amplitude=coefficients[size_t(j)][size_t(c)]*model.weight[size_t(c)];magnitude+=amplitude*amplitude;
            if(c==0||c==harmonics)fundamental+=amplitude*amplitude;
            for(int r=0;r<columns;++r)modelEnergy+=coefficients[size_t(j)][size_t(c)]*data->cross(note,c,note,r)*coefficients[size_t(j)][size_t(r)];
        }
        if(fundamental<magnitude*.035)continue;
        strength[size_t(note)]=float(std::sqrt(std::max(0.,magnitude)*.5));
        confidence[size_t(note)]=float(std::clamp(modelEnergy/energy,0.,1.));
    }
}
}
