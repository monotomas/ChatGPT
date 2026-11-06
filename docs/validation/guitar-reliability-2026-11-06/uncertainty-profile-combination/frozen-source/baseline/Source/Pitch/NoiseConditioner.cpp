// SPDX-License-Identifier: AGPL-3.0-or-later
#include "NoiseConditioner.h"
#include <algorithm>
#include <cmath>

namespace flysnapper::pitch {
namespace {
constexpr double pi=3.1415926535897932384626433832795;
double clean(double x) noexcept {return std::isfinite(x)?std::clamp(x,-16.,16.):0.;}
struct Projection {double a=0,b=0,energy=0,cMean=0,sMean=0;};
Projection project(const double* frame,int count,double omega,int first=0) noexcept {
    double c=std::cos(omega*first),s=std::sin(omega*first);
    const double dc=std::cos(omega),ds=std::sin(omega);
    double cc=0,ss=0,cs=0,xc=0,xs=0,cSum=0,sSum=0,xSum=0;
    for(int i=0;i<count;++i){const double x=frame[i];cc+=c*c;ss+=s*s;cs+=c*s;xc+=x*c;xs+=x*s;cSum+=c;sSum+=s;xSum+=x;
        const double next=c*dc-s*ds;s=s*dc+c*ds;c=next;}
    cc-=cSum*cSum/count;ss-=sSum*sSum/count;cs-=cSum*sSum/count;
    xc-=xSum*cSum/count;xs-=xSum*sSum/count;
    const double determinant=cc*ss-cs*cs;
    if(determinant<=1e-12)return {};
    Projection p;p.a=(ss*xc-cs*xs)/determinant;p.b=(cc*xs-cs*xc)/determinant;p.cMean=cSum/count;p.sMean=sSum/count;
    p.energy=std::max(0.,p.a*xc+p.b*xs);return p;
}
void subtract(double* frame,int count,double omega,const Projection& p,int first=0) noexcept {
    double c=std::cos(omega*first),s=std::sin(omega*first);const double dc=std::cos(omega),ds=std::sin(omega);
    for(int i=0;i<count;++i){frame[i]-=p.a*(c-p.cMean)+p.b*(s-p.sMean);const double next=c*dc-s*ds;s=s*dc+c*ds;c=next;}
}
void subtractRampedLine(double* frame,int count,double omega) noexcept {
    const int half=count/2;
    const auto left=project(frame,half,omega),right=project(frame+half,count-half,omega,half);
    const double firstCentre=(half-1)*.5,secondCentre=half+(count-half-1)*.5;
    const double slopeA=(right.a-left.a)/(secondCentre-firstCentre),slopeB=(right.b-left.b)/(secondCentre-firstCentre);
    std::array<double,NoiseConditioner::maxFrame> prediction{};
    double c=1,s=0,mean=0;const double dc=std::cos(omega),ds=std::sin(omega);
    for(int i=0;i<count;++i){const double relative=i-firstCentre;
        const double value=(left.a+slopeA*relative)*c+(left.b+slopeB*relative)*s;
        prediction[size_t(i)]=value;mean+=value;const double next=c*dc-s*ds;s=s*dc+c*ds;c=next;}
    mean/=count;
    for(int i=0;i<count;++i)frame[i]-=prediction[size_t(i)]-mean;
}
double energy(const double* frame,int count) noexcept {double sum=0;for(int i=0;i<count;++i)sum+=frame[i]*frame[i];return sum;}
double periodicCoherence(const double* frame,int count,double period) noexcept {
    // A finite sinusoid bank cannot cover every audible electrical overtone.
    // Compare complete cycles at the known mains period as a second, bounded
    // explanation. This is only used inside the narrow mains-frequency screen;
    // periodicity on its own is not evidence that a signal is unwanted.
    const int first=int(std::ceil(period))+1,n=count-first;
    if(n<64||n<period*.8)return 0;
    double xSum=0,ySum=0,xx=0,yy=0,xy=0;
    for(int i=first;i<count;++i){
        const double at=i-period;const int k=int(at);const double t=at-k;
        const double a=frame[k-1],b=frame[k],c=frame[k+1],d=frame[k+2];
        const double y=b+.5*t*(c-a+t*(2*a-5*b+4*c-d+t*(3*(b-c)+d-a))),x=frame[i];
        xSum+=x;ySum+=y;xx+=x*x;yy+=y*y;xy+=x*y;
    }
    xx-=xSum*xSum/n;yy-=ySum*ySum/n;xy-=xSum*ySum/n;
    return xx>1e-24&&yy>1e-24?std::clamp(xy/std::sqrt(xx*yy),0.,1.):0.;
}
}

NoiseConditioner::NoiseConditioner(double sampleRate){prepare(sampleRate);}
void NoiseConditioner::prepare(double sampleRate){
    rate=std::isfinite(sampleRate)&&sampleRate>=4000&&sampleRate<=384000?sampleRate:12000;
    adapt=2./(rate*.20);
    fastCoefficient=1-std::exp(-1/(rate*.003));slowCoefficient=1-std::exp(-1/(rate*.060));
    noiseRise=1-std::exp(-1/(rate*.30));noiseFall=1-std::exp(-1/(rate*.030));
    reset();
}
void NoiseConditioner::setOscillatorSteps() noexcept {
    for(int family=0;family<2;++family)for(int h=1;h<=harmonics;++h){auto& line=oscillators[size_t(family*harmonics+h-1)];
        const double omega=2*pi*familyFrequency[size_t(family)]*h/rate;
        line.dc=std::cos(omega);line.ds=std::sin(omega);}
}
void NoiseConditioner::reset() noexcept {
    oscillators={};familyFrequency={{50,60}};fastPower=slowPower=residualFast=residualSlow=noisePower=inputNoisePower=humPower=0;
    freezeSamples=attackCooldown=idleSamples=frequencySamples=renormalise=0;sampleClock=frequencySnapshot=0;setOscillatorSteps();
}
float NoiseConditioner::processSample(float sample,bool protectedSignal,bool musicalExcitation) noexcept {
    const double x=clean(sample),power=x*x;
    // inputNoisePower is a recent RAW peak-power envelope, independent of
    // note protection. One full low-string crest interval survives its8-ms
    // amplitude release. A stale canceller must not manufacture an oscillator
    // after the source has stopped while that oscillator keeps itself held.
    inputNoisePower=std::max(power,inputNoisePower*std::max(0.,1.-2./(rate*.008)));
    ++sampleClock;if(attackCooldown>0)--attackCooldown;
    fastPower+=fastCoefficient*(power-fastPower);slowPower+=slowCoefficient*(power-slowPower);
    // Catch the first physical excitation before a slower pitch decision can
    // protect it. A startup hum also pauses learning briefly; mains evidence
    // below handles its admission without requiring a manual calibration.
    if(musicalExcitation||(attackCooldown==0&&fastPower>slowPower*2.8&&fastPower>1e-20)) {
        freezeSamples=std::max(freezeSamples,int(rate*.080));
        attackCooldown=int(rate*.250);
    } else if(freezeSamples>0)--freezeSamples;
    if(humPower>1e-24&&inputNoisePower<humPower*.0625){
        for(auto& line:oscillators)line.a=line.b=line.previousA=line.previousB=0;
        humPower=0;
    }
    double model=0;for(const auto& line:oscillators)model+=line.a*line.c+line.b*line.s;
    const double maturity=std::min(1.,idleSamples/(rate*.12));
    const double removed=model*maturity,result=x-removed;
    residualFast+=fastCoefficient*(result*result-residualFast);
    residualSlow+=slowCoefficient*(result*result-residualSlow);
    // A quiet new string can be far below a loud hum's raw envelope. Detect
    // novelty in the cleaned residual as well, before pitch acquisition can
    // explicitly protect it from being absorbed by the line learner.
    if(attackCooldown==0&&maturity>=1&&residualFast>residualSlow*2.8
        &&residualFast>std::max(noisePower*4,1e-20)){
        freezeSamples=std::max(freezeSamples,int(rate*.080));attackCooldown=int(rate*.250);
    }
    humPower+=slowCoefficient*(removed*removed-humPower);
    const bool learn=!protectedSignal&&!musicalExcitation&&freezeSamples==0;
    if(learn){
        idleSamples=std::min(idleSamples+1,int(rate*60));
        // A joint residual drives every sinusoid. Independent cancellers would
        // double-remove the shared300-Hz line of the50/60-Hz families.
        const double error=x-model;
        for(auto& line:oscillators){line.a+=adapt*error*line.c;line.b+=adapt*error*line.s;}
        noisePower+=(result*result>noisePower?noiseRise:noiseFall)*(result*result-noisePower);
        if(++frequencySamples>=int(rate*.050)){updateFrequency();frequencySamples=0;}
    }
    for(auto& line:oscillators){const double next=line.c*line.dc-line.s*line.ds;line.s=line.s*line.dc+line.c*line.ds;line.c=next;}
    if(++renormalise==2048){renormalise=0;for(auto& line:oscillators){const double magnitude=std::hypot(line.c,line.s);
        if(magnitude>0){line.c/=magnitude;line.s/=magnitude;}}}
    return float(result);
}
void NoiseConditioner::updateFrequency() noexcept {
    const double elapsed=(sampleClock-frequencySnapshot)/rate;
    for(int family=0;family<2;++family){double weighted=0,weight=0,maximum=0;
        for(int h=1;h<=4;++h){const auto& l=oscillators[size_t(family*harmonics+h-1)];maximum=std::max(maximum,l.a*l.a+l.b*l.b);}
        for(int h=1;h<=4;++h){auto& l=oscillators[size_t(family*harmonics+h-1)];const double a=l.a*l.a+l.b*l.b,b=l.previousA*l.previousA+l.previousB*l.previousB;
            if(idleSamples>rate*.30&&a>maximum*.04&&a>1e-24&&b>a*.25){
                const double angle=std::atan2(l.previousA*l.b-l.previousB*l.a,l.previousA*l.a+l.previousB*l.b);
                const double error=-angle/(2*pi*std::max(.001,elapsed)*h);
                if(std::abs(error)<.8){weighted+=a*error;weight+=a;}
            }
            l.previousA=l.a;l.previousB=l.b;
        }
        if(weight>1e-24){const double nominal=family==0?50.:60.;
            familyFrequency[size_t(family)]=std::clamp(familyFrequency[size_t(family)]+std::clamp(weighted/weight*.12,-.008,.008),nominal-.40,nominal+.40);}
    }
    frequencySnapshot=sampleClock;
    setOscillatorSteps();
}
float NoiseConditioner::backgroundRms() const noexcept {return float(std::sqrt(std::max(0.,noisePower)));}
float NoiseConditioner::learnedHumRms() const noexcept {return float(std::sqrt(std::max(0.,humPower)));}
double NoiseConditioner::mainsFrequency(int family) const noexcept {return familyFrequency[size_t(std::clamp(family,0,1))];}

NoiseConditioner::Evidence NoiseConditioner::assess(const float* input,int samples,float frequency,
                                                   float candidateConfidence,bool held,bool excitation) const noexcept {
    Evidence result;result.backgroundRms=backgroundRms();result.humRms=learnedHumRms();
    result.learnedBackground=idleSamples>=rate*.25;
    if(!input||samples<32)return result;
    const int count=std::min(samples,maxFrame);input+=samples-count;
    std::array<double,maxFrame> frame{},residual{};double mean=0;
    for(int i=0;i<count;++i){frame[size_t(i)]=clean(input[i]);mean+=frame[size_t(i)];}mean/=count;
    for(int i=0;i<count;++i)frame[size_t(i)]-=mean;
    residual=frame;const double total=energy(frame.data(),count);result.rms=float(std::sqrt(total/count));
    if(total<1e-24)return result;
    result.backgroundSnrDb=float(10*std::log10(std::max(total/count-noisePower,1e-30)/std::max(noisePower,1e-30)));
    int mainsFamily=0,mainsOrder=0;
    if(std::isfinite(frequency)&&frequency>20&&frequency<rate*.45){
        double coherence=0,weight=0;
        for(int h=1;h<=12&&h*frequency<rate*.45;++h){
            const double omega=2*pi*h*frequency/rate;
            const auto p=project(residual.data(),count,omega);subtract(residual.data(),count,omega,p);
        }
        // Non-integer windows make harmonic columns slightly non-orthogonal.
        // Two bounded residual-refinement passes prevent that predictable
        // projection leakage from being mislabeled as background noise.
        for(int pass=0;pass<2;++pass)for(int h=1;h<=12&&h*frequency<rate*.45;++h){
            const double omega=2*pi*h*frequency/rate;
            const auto p=project(residual.data(),count,omega);subtract(residual.data(),count,omega,p);
        }
        // Estimate the TWO half-frame harmonic models jointly as well.
        // A raw projection of one partial through another partial's leakage
        // produces false phase changes in short non-integer windows.
        auto phaseResidual=frame;
        std::array<Projection,12> first{},second{};
        const int half=count/2;
        for(int pass=0;pass<3;++pass)for(int h=1;h<=12&&h*frequency<rate*.45;++h){
            const double omega=2*pi*h*frequency/rate;
            const auto a=project(phaseResidual.data(),half,omega),b=project(phaseResidual.data()+half,count-half,omega,half);
            first[size_t(h-1)].a+=a.a;first[size_t(h-1)].b+=a.b;
            second[size_t(h-1)].a+=b.a;second[size_t(h-1)].b+=b.b;
            subtract(phaseResidual.data(),half,omega,a);subtract(phaseResidual.data()+half,count-half,omega,b,half);
        }
        for(int h=1;h<=12&&h*frequency<rate*.45;++h){const auto& a=first[size_t(h-1)];const auto& b=second[size_t(h-1)];
            const double pa=a.a*a.a+a.b*a.b,pb=b.a*b.a+b.b*b.b;
            const double evidence=std::sqrt(pa*pb);
            if(pa>1e-24&&pb>1e-24&&evidence*count>total*.006){
                const double agreement=std::clamp((a.a*b.a+a.b*b.b)/evidence,0.,1.);
                coherence+=agreement*evidence;weight+=evidence;
                if(agreement>.85)++result.coherentPartials;
            }
        }
        result.phaseCoherence=weight>0?float(coherence/weight):0;
        const double rest=energy(residual.data(),count);
        result.harmonicFraction=float(std::clamp(1-rest/total,0.,1.));
        result.residualRms=float(std::sqrt(rest/count));
        const double noise=std::max(rest/count,noisePower);
        result.harmonicSnrDb=float(10*std::log10(std::max(total-rest,1e-30)/std::max(rest,1e-30)));
        result.snrDb=float(10*std::log10(std::max(total-rest,1e-30)/std::max(noise*count,1e-30)));
        for(int family=0;family<2;++family){
            const double base=familyFrequency[size_t(family)],h=std::round(frequency/base);
            // Cancellation-envelope changes can bias a short candidate by a
            // few tenths of a hertz. This screen still excludes the standard
            // neighbouring guitar/bass notes (e.g. G1 at48.999Hz); the actual
            // stationary-line fit below, not proximity alone, decides evidence.
            if(h>=1&&h<=harmonics&&std::abs(frequency-base*h)<=.35*h){result.mainsCandidate=true;mainsFamily=family;mainsOrder=int(h);}
        }
    }
    if(result.mainsCandidate){
        // High-order buzz must not become a new note when the lower mains
        // partials have already cancelled. Squared complete-cycle coherence is
        // the explained energy of a gain-normalised, one-cycle predictor.
        result.mainsPeriodicity=float(periodicCoherence(frame.data(),count,rate/familyFrequency[size_t(mainsFamily)]));
        result.mainsFraction=result.mainsPeriodicity*result.mainsPeriodicity;
        const bool periodExplains=result.mainsFraction>.94f
            &&(result.phaseCoherence>.96f||result.mainsPeriodicity>.995f);
        if(!periodExplains){
            residual=frame;
            // A deterministic fit to both complete line families catches cold
            // mains before learning or when fewer than two cycles are present.
            // Three bounded passes account for short-frame cross-correlation.
            for(int pass=0;pass<3;++pass)for(int order=0;order<2;++order)for(int h=1;h<=harmonics;++h){
                const int family=(mainsFamily+order)%2;
                const double omega=2*pi*familyFrequency[size_t(family)]*h/rate;
                const auto p=project(residual.data(),count,omega);subtract(residual.data(),count,omega,p);
            }
            result.mainsFraction=std::max(result.mainsFraction,float(std::clamp(1-energy(residual.data(),count)/total,0.,1.)));
            if(result.mainsFraction<.94f&&result.mainsFraction>.75f&&result.phaseCoherence>.93f){
                // Switching/ramping electrical interference is still interference.
                // A raw RMS increase must not masquerade as musical excitation.
                residual=frame;
                for(int pass=0;pass<3;++pass)for(int h=1;h<=harmonics;++h)
                    subtractRampedLine(residual.data(),count,2*pi*familyFrequency[size_t(mainsFamily)]*h/rate);
                result.mainsFraction=std::max(result.mainsFraction,float(std::clamp(1-energy(residual.data(),count)/total,0.,1.)));
            }
        }
        // A known idle white-noise floor does not become a new string when
        // the protected line model drifts slightly. Explain that already
        // observed remainder without relaxing the cold-input purity rule.
        const double unexplained=total*(1-result.mainsFraction)/count;
        const bool knownFloorExplains=result.learnedBackground&&humPower>noisePower*9
            &&result.mainsFraction>.65f&&unexplained<=noisePower*4;
        // Many musical pitches lie very close to a high mains overtone:
        // F4≈349.23Hz and E5≈659.26Hz are ordinary guitar notes. Their own
        // harmonic series must not be rejected just because350/660Hz appear
        // in the interference dictionary. A high-order hypothesis needs the
        // actual low mains carrier, or previously established same-line noise.
        bool familyEvidence=mainsOrder<=2;
        if(!familyEvidence){
            const double base=familyFrequency[size_t(mainsFamily)];
            const auto low=project(frame.data(),count,2*pi*base/rate);
            const auto& fundamental=oscillators[size_t(mainsFamily*harmonics)];
            const auto& same=oscillators[size_t(mainsFamily*harmonics+mainsOrder-1)];
            double familyPower=0;for(int h=0;h<harmonics;++h){const auto& line=oscillators[size_t(mainsFamily*harmonics+h)];familyPower+=line.a*line.a+line.b*line.b;}
            const bool prior=(fundamental.a*fundamental.a+fundamental.b*fundamental.b)>familyPower*.08
                &&(same.a*same.a+same.b*same.b)*.5>total/count*.10;
            // A high oscillator spans many cycles, so its estimated carrier
            // is much more precise than a short50-Hz candidate. Preserve
            // ordinary tempered notes a few cents from350/660/700Hz even
            // when a louder mains model already exists behind them.
            familyEvidence=std::abs(frequency-base*mainsOrder)<=.04*mainsOrder
                &&(low.energy>total*.08||prior);
        }
        result.mainsExplained=familyEvidence&&(result.mainsFraction>.94f||knownFloorExplains)
            &&(result.phaseCoherence>.96f||result.mainsPeriodicity>.995f);
        result.mainsOnly=!held&&!excitation&&result.mainsExplained;
    }
    candidateConfidence=std::isfinite(candidateConfidence)?std::clamp(candidateConfidence,0.f,1.f):0;
    result.clearlyUnpitched=!held&&candidateConfidence<.93f&&result.harmonicFraction<.45f&&result.phaseCoherence<.75f;
    return result;
}
}
