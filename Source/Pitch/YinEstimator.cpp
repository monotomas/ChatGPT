// SPDX-License-Identifier: AGPL-3.0-or-later
#include "YinEstimator.h"
#include <algorithm>
#include <cmath>
#include <stdexcept>
#if defined(__SSE2__) || defined(_M_X64)
#include <emmintrin.h>
#endif
namespace flysnapper::pitch {
namespace {
constexpr double pi=3.1415926535897932384626433832795;
float squaredDifference(const float* a,const float* b,int count) noexcept {
    int i=0;double total=0;
#if defined(__SSE2__) || defined(_M_X64)
    __m128 sum=_mm_setzero_ps();
    for(;i+4<=count;i+=4){const auto delta=_mm_sub_ps(_mm_loadu_ps(a+i),_mm_loadu_ps(b+i));sum=_mm_add_ps(sum,_mm_mul_ps(delta,delta));}
    float lanes[4];_mm_storeu_ps(lanes,sum);total=double(lanes[0])+lanes[1]+lanes[2]+lanes[3];
#endif
    for(;i<count;++i){const double delta=double(a[i])-b[i];total+=delta*delta;}
    return float(total);
}
}
void YinEstimator::prepare(double analysisRate,int lowestNote,int highestNote){
    if(!std::isfinite(analysisRate)||analysisRate<8000||analysisRate>48000||lowestNote<0||highestNote>127||lowestNote>=highestNote)
        throw std::invalid_argument("YinEstimator requires8..48kHz and a valid MIDI range");
    rate=analysisRate;lowNote=lowestNote;highNote=highestNote;
    lowHz=float(440*std::pow(2.,double(lowNote-69)/12.));highHz=float(440*std::pow(2.,double(highNote-69)/12.));
}
YinEstimator::Result YinEstimator::estimate(const float* wave,int samples,float threshold,bool harmonicDisambiguation) noexcept {
    Result result;
    if(!wave||samples<32)return result;
    const int count=std::min(samples,maxSamples),comparison=count/2;
    const int minimumLag=std::max(2,int(std::floor(rate/highHz)));
    const int maximumLag=std::min(comparison,int(std::ceil(rate/lowHz)));
    if(maximumLag<minimumLag+2)return result;
    threshold=std::isfinite(threshold)?std::clamp(threshold,.02f,.25f):.08f;
    wave+=samples-count;
    double mean=0,energy=0;
    for(int i=0;i<count;++i){frame[size_t(i)]=std::isfinite(wave[i])?std::clamp(wave[i],-16.f,16.f):0;mean+=frame[size_t(i)];}
    mean/=count;
    for(int i=0;i<count;++i){frame[size_t(i)]-=float(mean);energy+=double(frame[size_t(i)])*frame[size_t(i)];}
    result.rms=float(std::sqrt(energy/count));
    if(energy<1e-12)return result;
    difference[0]=0;normalised[0]=1;
    double cumulative=0;
    for(int lag=1;lag<=maximumLag;++lag){
        difference[size_t(lag)]=squaredDifference(frame.data(),frame.data()+lag,comparison);
        cumulative+=difference[size_t(lag)];
        normalised[size_t(lag)]=cumulative>1e-12?float(difference[size_t(lag)]*lag/cumulative):1;
    }
    int chosen=-1;float delta=0,minimum=1;
    for(int lag=minimumLag;lag<maximumLag;++lag){
        const float a=normalised[size_t(lag-1)],b=normalised[size_t(lag)],c=normalised[size_t(lag+1)];
        if(b<=a&&b<c){
            const float denominator=a-2*b+c;
            const float interpolated=std::abs(denominator)>1e-12f?std::clamp(.5f*(a-c)/denominator,-.5f,.5f):0;
            const float value=std::max(0.f,b-.25f*(a-c)*interpolated);
            // Threshold the interpolated minimum as well as its frequency.
            // Otherwise high notes whose periods fall between sample lags can
            // incorrectly wait for an integer-aligned double period.
            if(value<threshold){chosen=lag;delta=interpolated;minimum=value;break;}
        }
    }
    if(chosen<0)return result;
    double frequency=rate/(chosen+delta);
    result.periodicity=std::clamp(1-minimum,0.f,1.f);result.confidence=result.periodicity;
    if(harmonicDisambiguation){
        std::array<double,12> powers{};double total=0;
        // A guitar attack can contain one body-resonance outlier along with a
        // much stronger harmonic series. Pure CMNDF can then report their low
        // common divisor. Prefer a dominant series only when at least two of
        // its partials carry >85% of measured harmonic power collectively.
        // This is a generic timbre heuristic, not proof for arbitrary audio.
        for(int harmonic=1;harmonic<=12;++harmonic){
            if(frequency*harmonic>=rate*.45)continue;
            const double angle=2*pi*frequency*harmonic/rate,stepC=std::cos(angle),stepS=std::sin(angle);
            double oscillatorC=1,oscillatorS=0,re=0,im=0;
            for(int i=0;i<count;++i){
                re+=frame[size_t(i)]*oscillatorC;im+=frame[size_t(i)]*oscillatorS;
                const double nextC=oscillatorC*stepC-oscillatorS*stepS;
                oscillatorS=oscillatorS*stepC+oscillatorC*stepS;oscillatorC=nextC;
            }
            powers[size_t(harmonic-1)]=re*re+im*im;total+=powers[size_t(harmonic-1)];
        }
        int preferred=1;double preferredRatio=0;
        if(total>1e-12)for(int divisor=2;divisor<=4;++divisor){
            double supportedPower=0;int support=0;
            for(int harmonic=divisor;harmonic<=12;harmonic+=divisor){
                supportedPower+=powers[size_t(harmonic-1)];
                if(powers[size_t(harmonic-1)]>total*.025)++support;
            }
            const double ratio=supportedPower/total;
            if(ratio>.85&&support>=2&&ratio>preferredRatio){preferred=divisor;preferredRatio=ratio;}
        }
        if(preferred>1){
            frequency*=preferred;result.harmonicDivisor=preferred;result.harmonicPurity=float(preferredRatio);
            result.confidence=std::min(result.confidence,result.harmonicPurity);
        }
    }
    const double pitch=69+12*std::log2(frequency/440.0);
    const int note=int(std::lround(pitch));
    result.frequency=float(frequency);result.pitch=float(pitch);
    if(note<lowNote||note>highNote||std::abs(pitch-note)>.35)return result;
    result.note=note;result.valid=true;
    return result;
}
}
