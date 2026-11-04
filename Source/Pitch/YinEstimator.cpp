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
    // Search the complete rounding bins of the endpoint notes. A tone forty
    // cents below lowNote still rounds to lowNote and needs a longer lag.
    // The final integer-note check retains the requested output bounds.
    lowHz=float(440*std::pow(2.,(double(lowNote)-69.5)/12.));highHz=float(440*std::pow(2.,(double(highNote)-68.5)/12.));
}
YinEstimator::Result YinEstimator::estimate(const float* wave,int samples,float threshold,bool harmonicDisambiguation) noexcept {
    Result result;
    if(!wave||samples<32)return result;
    const int count=std::min(samples,maxSamples),comparison=count/2;
    result.observedSamples=count;
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
            float value=lag<16?std::max(0.f,b-.25f*(a-c)*interpolated):b;
            float refined=interpolated;
            if(lag<16){
                double averageDifference=0;for(int k=1;k<=lag;++k)averageDifference+=difference[size_t(k)];
                averageDifference/=lag;
                if(averageDifference>1e-12){
                    const auto evaluate=[&](float period) noexcept {
                        const int integer=int(std::floor(period));const double u=period-integer;
                        const double w0=-u*(u-1)*(u-2)/6,w1=(u+1)*(u-1)*(u-2)/2;
                        const double w2=-(u+1)*u*(u-2)/2,w3=(u+1)*u*(u-1)/6;
                        // A cubic delay needs two following samples. On a
                        // very short frame, never read stale scratch entries
                        // beyond count; normalise the available comparison.
                        double sum=0;const int usable=std::min(comparison,count-integer-2);
                        if(usable<=0)return 1.f;
                        for(int i=0;i<usable;++i){const int p=i+integer;
                            const double shifted=w0*frame[size_t(p-1)]+w1*frame[size_t(p)]+w2*frame[size_t(p+1)]+w3*frame[size_t(p+2)];
                            const double error=frame[size_t(i)]-shifted;sum+=error*error;
                        }
                        return float(sum*comparison/(usable*averageDifference));
                    };
                    float bestPeriod=float(lag)+refined,bestValue=evaluate(bestPeriod);
                    for(int step=-4;step<=4;++step){const float period=float(lag)+step*.125f;const float error=evaluate(period);
                        if(error<bestValue){bestPeriod=period;bestValue=error;}}
                    const float leftPeriod=std::max(float(lag)-.5f,bestPeriod-.0625f),rightPeriod=std::min(float(lag)+.5f,bestPeriod+.0625f);
                    if(rightPeriod>leftPeriod){
                        const float left=evaluate(leftPeriod),right=evaluate(rightPeriod),curvature=left-2*bestValue+right;
                        if(curvature>1e-12f&&bestPeriod>leftPeriod&&bestPeriod<rightPeriod){
                            const float displacement=std::clamp(.5f*(left-right)/curvature,-1.f,1.f)*.0625f;
                            const float period=std::clamp(bestPeriod+displacement,float(lag)-.5f,float(lag)+.5f),error=evaluate(period);
                            if(error<bestValue){bestValue=error;bestPeriod=period;}
                        }
                    }
                    value=bestValue;refined=bestPeriod-lag;
                }
            }
            // Threshold the interpolated minimum as well as its frequency.
            // Otherwise high notes whose periods fall between sample lags can
            // incorrectly wait for an integer-aligned double period.
            if(value<threshold){chosen=lag;delta=refined;minimum=value;break;}
        }
    }
    if(chosen<0)return result;
    double frequency=rate/(chosen+delta);
    // A strong even series can make its half-period pass the YIN threshold
    // while weaker, coherent odd partials still establish the lower root.
    // Require a substantially better observed double period and a resolved
    // fundamental plus three odd partials before preferring that root.
    if(harmonicDisambiguation&&2*(chosen+delta)<maximumLag-1){
        const int expected=int(std::lround(2*(chosen+delta)));
        int longer=-1;float longerMinimum=1,longerDelta=0;
        for(int lag=std::max(minimumLag+1,expected-2);lag<=std::min(maximumLag-1,expected+2);++lag){
            const float a=normalised[size_t(lag-1)],b=normalised[size_t(lag)],c=normalised[size_t(lag+1)];
            if(b<=a&&b<c&&b<longerMinimum){const float denominator=a-2*b+c;longer=lag;longerMinimum=b;longerDelta=std::abs(denominator)>1e-12f?std::clamp(.5f*(a-c)/denominator,-.5f,.5f):0;}
        }
        if(longer>=0){
            double average=0;for(int k=1;k<=longer;++k)average+=difference[size_t(k)];average/=longer;
            const double period=longer+longerDelta;const int integer=int(std::floor(period));const double u=period-integer;
            const double w0=-u*(u-1)*(u-2)/6,w1=(u+1)*(u-1)*(u-2)/2,w2=-(u+1)*u*(u-2)/2,w3=(u+1)*u*(u-1)/6;
            const int usable=std::min(comparison,count-integer-2);double sum=0;
            for(int i=0;i<usable;++i){const int p=i+integer;const double shifted=w0*frame[size_t(p-1)]+w1*frame[size_t(p)]+w2*frame[size_t(p+1)]+w3*frame[size_t(p+2)];const double error=frame[size_t(i)]-shifted;sum+=error*error;}
            longerMinimum=average>1e-12&&usable>0?float(sum*comparison/(usable*average)):1.f;
        }
        if(longer>=0&&longerMinimum<threshold&&longerMinimum<minimum*.5f){
            const double lowerFrequency=rate/(longer+longerDelta);
            std::array<float,maxSamples> windowed{};
            const double wa=2*pi/(count-1),wC=std::cos(wa),wS=std::sin(wa);double wc=1,ws=0;
            for(int i=0;i<count;++i){windowed[size_t(i)]=float(frame[size_t(i)]*(.5-.5*wc));const double next=wc*wC-ws*wS;ws=ws*wC+wc*wS;wc=next;}
            const auto physicalPower=[&](double hz) noexcept {
                if(hz<=0||hz>=rate*.45)return 0.;
                const double angle=2*pi*hz/rate,dc=std::cos(angle),ds=std::sin(angle);double c=1,s=0,re=0,im=0;
                for(int i=0;i<count;++i){re+=windowed[size_t(i)]*c;im+=windowed[size_t(i)]*s;const double next=c*dc-s*ds;s=s*dc+c*ds;c=next;}
                return re*re+im*im;
            };
            const double root=physicalPower(lowerFrequency),dominant=physicalPower(frequency),spacing=rate/count;
            if(root>=dominant*.0016&&root>physicalPower(lowerFrequency-spacing)&&root>physicalPower(lowerFrequency+spacing)){
                std::array<double,12> partial{};double maximum=0;
                for(int h=1;h<=12;++h){partial[size_t(h-1)]=physicalPower(lowerFrequency*h);maximum=std::max(maximum,partial[size_t(h-1)]);}
                int coherent=0;
                for(int h=1;h<=12;h+=2)if(partial[size_t(h-1)]>=maximum*.0016){const double center=partial[size_t(h-1)];if(center>physicalPower(lowerFrequency*h-spacing)&&center>physicalPower(lowerFrequency*h+spacing))++coherent;}
                if(coherent>=3){frequency=lowerFrequency;chosen=longer;delta=longerDelta;minimum=longerMinimum;}
            }
        }
    }
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
            // Dominant even harmonics alone do not establish a new octave.
            // Resolved off-series partials support the original candidate;
            // broad spectral leakage and one isolated body peak do not.
            std::array<float,maxSamples> windowed{};
            const double windowAngle=2*pi/(count-1),windowC=std::cos(windowAngle),windowS=std::sin(windowAngle);
            double wc=1,ws=0;
            for(int i=0;i<count;++i){windowed[size_t(i)]=float(frame[size_t(i)]*(.5-.5*wc));const double next=wc*windowC-ws*windowS;ws=ws*windowC+wc*windowS;wc=next;}
            const auto physicalPower=[&](double hz) noexcept {
                if(hz<=0||hz>=rate*.45)return 0.;
                const double angle=2*pi*hz/rate,dc=std::cos(angle),ds=std::sin(angle);
                double c=1,s=0,re=0,im=0;
                for(int i=0;i<count;++i){re+=windowed[size_t(i)]*c;im+=windowed[size_t(i)]*s;const double next=c*dc-s*ds;s=s*dc+c*ds;c=next;}
                return re*re+im*im;
            };
            std::array<double,12> physical{};double maximum=0;
            for(int h=1;h<=12;++h){physical[size_t(h-1)]=physicalPower(frequency*h);maximum=std::max(maximum,physical[size_t(h-1)]);}
            int coherent=0;const double spacing=rate/count;
            for(int h=1;h<=12;++h)if(h%preferred&&physical[size_t(h-1)]>=maximum*.0016){
                const double center=physical[size_t(h-1)];
                if(center>physicalPower(frequency*h-spacing)&&center>physicalPower(frequency*h+spacing))++coherent;
            }
            if(coherent>=3)preferred=1;
        }
        if(preferred>1){
            frequency*=preferred;result.harmonicDivisor=preferred;result.harmonicPurity=float(preferredRatio);
            result.confidence=std::min(result.confidence,result.harmonicPurity);
        }
    }
    const double pitch=69+12*std::log2(frequency/440.0);
    const int note=int(std::lround(pitch));
    result.frequency=float(frequency);result.pitch=float(pitch);
    if(note<lowNote||note>highNote)return result;
    result.note=note;result.valid=true;
    return result;
}
YinEstimator::Result YinEstimator::track(const float* wave,int samples,float expectedPitch,
                                       float searchSemitones,float minimumCorrelation) noexcept {
    Result result;
    if(!wave||samples<32||!std::isfinite(expectedPitch)||expectedPitch<lowNote-.5f||expectedPitch>highNote+.5f)return result;
    searchSemitones=std::isfinite(searchSemitones)?std::clamp(searchSemitones,.1f,2.0f):.8f;
    minimumCorrelation=std::isfinite(minimumCorrelation)?std::clamp(minimumCorrelation,.5f,.9999f):.9f;
    const double expectedPeriod=rate/(440.0*std::pow(2.,(expectedPitch-69.)/12.));
    const int count=std::min({samples,maxSamples,std::max(96,int(std::ceil(expectedPeriod*3.25)))});
    result.observedSamples=count;
    const double minimumPeriod=std::max(2.0,expectedPeriod*std::pow(2.,-searchSemitones/12.));
    const double maximumPeriod=std::min(double(count)-18.,expectedPeriod*std::pow(2.,searchSemitones/12.));
    if(maximumPeriod<=minimumPeriod)return result;
    const int comparison=count-int(std::ceil(maximumPeriod))-2;
    if(comparison<16||comparison<expectedPeriod*.7)return result;
    wave+=samples-count;
    double energy=0,mean=0;
    for(int i=0;i<count;++i){frame[size_t(i)]=std::isfinite(wave[i])?std::clamp(wave[i],-16.f,16.f):0;mean+=frame[size_t(i)];}
    mean/=count;
    for(int i=0;i<count;++i){frame[size_t(i)]-=float(mean);energy+=double(frame[size_t(i)])*frame[size_t(i)];}
    if(energy<1e-13)return result;
    result.rms=float(std::sqrt(energy/count));
    // A DC-blocker's exponential tail has nearly perfect gain-normalised
    // correlation at every lag, but is not an oscillating string. Require
    // several observed polarity reversals and actual recent-cycle energy.
    int crossings=0,polarity=0;
    for(int i=0;i<count;++i){const float value=frame[size_t(i)];const int next=value>result.rms*.03f?1:value<-result.rms*.03f?-1:polarity;
        if(polarity&&next!=polarity)++crossings;
        polarity=next;
    }
    if(crossings<4)return result;
    const int cycle=std::clamp(int(std::lround(expectedPeriod)),3,count/2);
    const auto cyclePower=[&](int start) noexcept {double sum=0,squared=0;for(int i=0;i<cycle;++i){const double x=frame[size_t(start+i)];sum+=x;squared+=x*x;}return std::max(0.,squared-sum*sum/cycle);};
    const double recentPower=cyclePower(count-cycle),priorPower=cyclePower(count-2*cycle);
    if(recentPower<priorPower*.0625||recentPower*count<energy*cycle*.04)return result;
    const auto correlation=[&](double period) noexcept {
        const int integer=int(std::floor(period));const double u=period-integer;
        const double w0=-u*(u-1)*(u-2)/6,w1=(u+1)*(u-1)*(u-2)/2,w2=-(u+1)*u*(u-2)/2,w3=(u+1)*u*(u-1)/6;
        double a=0,b=0,aa=0,bb=0,ab=0;
        for(int i=0;i<comparison;++i){const int p=i+integer;
            const double x=frame[size_t(i)],y=w0*frame[size_t(p-1)]+w1*frame[size_t(p)]+w2*frame[size_t(p+1)]+w3*frame[size_t(p+2)];
            a+=x;b+=y;aa+=x*x;bb+=y*y;ab+=x*y;
        }
        aa-=a*a/comparison;bb-=b*b/comparison;ab-=a*b/comparison;
        return aa>1e-15&&bb>1e-15?std::clamp(ab/std::sqrt(aa*bb),-1.,1.):-1.;
    };
    double bestPeriod=std::clamp(expectedPeriod,minimumPeriod,maximumPeriod),best=correlation(bestPeriod);
    for(int lag=int(std::ceil(minimumPeriod));lag<=int(std::floor(maximumPeriod));++lag){const double value=correlation(lag);if(value>best){best=value;bestPeriod=lag;}}
    // A narrow bounded golden-section search refines the strongest integer
    // maximum. Cubic interpolation keeps high-register pitch-wheel values
    // independent of the analysis-rate sample grid.
    double left=std::max(minimumPeriod,bestPeriod-1),right=std::min(maximumPeriod,bestPeriod+1);
    constexpr double fraction=.6180339887498948482;
    double a=right-fraction*(right-left),b=left+fraction*(right-left),ca=correlation(a),cb=correlation(b);
    for(int step=0;step<10;++step){
        if(ca>cb){right=b;b=a;cb=ca;a=right-fraction*(right-left);ca=correlation(a);}
        else{left=a;a=b;ca=cb;b=left+fraction*(right-left);cb=correlation(b);}
    }
    if(ca>best){best=ca;bestPeriod=a;}if(cb>best){best=cb;bestPeriod=b;}
    const double frequency=rate/bestPeriod,pitch=69+12*std::log2(frequency/440.0);
    const int note=int(std::lround(pitch));
    result.frequency=float(frequency);result.pitch=float(pitch);result.note=note;
    result.confidence=result.periodicity=float(std::max(0.,best));
    result.valid=best>=minimumCorrelation&&note>=lowNote&&note<=highNote;
    return result;
}
}
