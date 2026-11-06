// SPDX-License-Identifier: AGPL-3.0-or-later
#include "EarlyEstimator.h"
#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>

namespace flysnapper::pitch {
namespace {
constexpr double pi=3.1415926535897932384626433832795;
constexpr double regularisation=.02;
bool invert(std::array<double,12*12>& matrix) {
    std::array<double,12*24> augmented{};
    for(int r=0;r<12;++r)for(int c=0;c<12;++c){
        augmented[size_t(r*24+c)]=matrix[size_t(r*12+c)];
        augmented[size_t(r*24+c+12)]=(r==c)?1.:0.;
    }
    for(int pivot=0;pivot<12;++pivot){
        int selected=pivot;
        for(int r=pivot+1;r<12;++r)
            if(std::abs(augmented[size_t(r*24+pivot)])>std::abs(augmented[size_t(selected*24+pivot)]))selected=r;
        if(std::abs(augmented[size_t(selected*24+pivot)])<1e-12)return false;
        if(selected!=pivot)for(int c=0;c<24;++c)std::swap(augmented[size_t(selected*24+c)],augmented[size_t(pivot*24+c)]);
        const double divisor=augmented[size_t(pivot*24+pivot)];
        for(int c=0;c<24;++c)augmented[size_t(pivot*24+c)]/=divisor;
        for(int r=0;r<12;++r)if(r!=pivot){
            const double multiplier=augmented[size_t(r*24+pivot)];
            for(int c=0;c<24;++c)augmented[size_t(r*24+c)]-=multiplier*augmented[size_t(pivot*24+c)];
        }
    }
    for(int r=0;r<12;++r)for(int c=0;c<12;++c)matrix[size_t(r*12+c)]=augmented[size_t(r*24+c+12)];
    return true;
}
}
void EarlyEstimator::prepare(double sampleRate,int observationSamples){
    if(!std::isfinite(sampleRate)||sampleRate<8000||sampleRate>192000||observationSamples<32||observationSamples>maxSamples)
        throw std::invalid_argument("EarlyEstimator requires 8..192kHz and32..512 observation samples");
    rate=sampleRate;length=observationSamples;
    models.clear();models.resize(noteCount);
    for(int index=0;index<noteCount;++index){
        auto& model=models[size_t(index)];
        const double frequency=440.*std::pow(2.,double(index+lowNote-69)/12.);
        for(int harmonic=1;harmonic<=maxHarmonics;++harmonic){
            const float weight=float(1./std::pow(double(harmonic),.7));
            for(int quadrature=0;quadrature<2;++quadrature){
                const int column=(harmonic-1)+quadrature*maxHarmonics;
                model.weights[size_t(column)]=weight;
                double mean=0;
                if(frequency*harmonic>=rate*.45)continue;
                for(int sample=0;sample<length;++sample){
                    const double phase=2*pi*frequency*harmonic*sample/rate;
                    const float value=float(quadrature?std::cos(phase):std::sin(phase));
                    model.basis[size_t(column*maxSamples+sample)]=value;mean+=value;
                }
                mean/=length;
                for(int sample=0;sample<length;++sample)
                    model.basis[size_t(column*maxSamples+sample)]=float((model.basis[size_t(column*maxSamples+sample)]-mean)*weight);
            }
        }
        for(int r=0;r<columns;++r)for(int c=0;c<columns;++c){
            double sum=0;
            for(int sample=0;sample<length;++sample)sum+=double(model.basis[size_t(r*maxSamples+sample)])*model.basis[size_t(c*maxSamples+sample)];
            if(r==c)sum+=regularisation*length;
            model.inverse[size_t(r*columns+c)]=sum;
        }
        if(!invert(model.inverse))throw std::runtime_error("EarlyEstimator regression table failed");
    }
}
EarlyEstimator::Result EarlyEstimator::estimate(const float* input,int samples,int maxVoices) const noexcept {
    Result result;
    if(!input||length==0||samples<length||models.size()!=noteCount||maxVoices<1)return result;
    std::array<double,maxSamples> frame{};
    double mean=0,energy=0;
    const auto* start=input+samples-length;
    for(int i=0;i<length;++i){frame[size_t(i)]=std::isfinite(start[i])?start[i]:0;mean+=frame[size_t(i)];}
    mean/=length;
    for(int i=0;i<length;++i){frame[size_t(i)]-=mean;energy+=frame[size_t(i)]*frame[size_t(i)];}
    result.rms=float(std::sqrt(energy/length));
    if(result.rms<.0005f)return result;
    double best=-1,runner=-1,fundamental=0;
    for(int index=0;index<noteCount;++index){
        const auto& model=models[size_t(index)];
        std::array<double,columns> projection{},coefficients{};
        for(int c=0;c<columns;++c)for(int i=0;i<length;++i)
            projection[size_t(c)]+=frame[size_t(i)]*model.basis[size_t(c*maxSamples+i)];
        for(int r=0;r<columns;++r)for(int c=0;c<columns;++c)
            coefficients[size_t(r)]+=model.inverse[size_t(r*columns+c)]*projection[size_t(c)];
        double explained=0,totalAmplitude=0,fundamentalAmplitude=0;
        for(int c=0;c<columns;++c){
            explained+=projection[size_t(c)]*coefficients[size_t(c)];
            const double amplitude=coefficients[size_t(c)]*model.weights[size_t(c)];
            totalAmplitude+=amplitude*amplitude;
            if(c==0||c==maxHarmonics)fundamentalAmplitude+=amplitude*amplitude;
        }
        const double score=std::clamp(explained/energy,0.,1.);
        if(score>best){runner=best;best=score;result.bestNote=index+lowNote;fundamental=totalAmplitude>0?fundamentalAmplitude/totalAmplitude:0;}
        else if(score>runner)runner=score;
    }
    result.fit=float(best);result.margin=float(std::max(0.,best-runner));result.fundamentalFraction=float(fundamental);
    result.confidence=std::clamp((result.fit-.8f)/.2f,0.f,1.f)*std::clamp(result.margin/.06f,0.f,1.f);
    result.accepted=maxVoices==1&&result.fit>=.93f&&result.margin>=.025f&&result.fundamentalFraction>=.02f;
    if(result.accepted){result.notes[0]=result.bestNote;result.count=1;}
    return result;
}
}
