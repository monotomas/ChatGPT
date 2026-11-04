// SPDX-License-Identifier: AGPL-3.0-or-later
#include "HarmonicContinuity.h"
#include <algorithm>
#include <cmath>

namespace flysnapper::pitch {
namespace {
constexpr double pi=3.1415926535897932384626433832795;
constexpr int dimension=1+2*HarmonicContinuity::maximumPartials;
double safe(double value) noexcept {return std::isfinite(value)?std::clamp(value,-16.,16.):0.;}
double wrap(double phase) noexcept {return std::remainder(phase,2*pi);}
// Validate a proposed coherent amplitude change against a different, newest
// observation. A transient edge in one sinusoid can leak into every fitted
// harmonic of a longer window. Those coefficients are not independent new
// excitations. At an established F0 a whole latest cycle has a well-conditioned
// rectangular harmonic basis, even though it cannot establish an unknown F0.
bool recentPowers(const float* input,int count,const double* carrierCos,const double* carrierSin,int partialCount,
                  std::array<double,HarmonicContinuity::maximumPartials>& power) noexcept {
    if(count<2*partialCount+3)return false;
    const int size=1+2*partialCount;
    double system[dimension][dimension+1]{};
    for(int i=0;i<count;++i){
        const double c=carrierCos[size_t(i)],s=carrierSin[size_t(i)];
        double basis[dimension]{1},hc=c,hs=s;
        for(int h=0;h<partialCount;++h){basis[1+2*h]=hc;basis[2+2*h]=hs;const double next=hc*c-hs*s;hs=hs*c+hc*s;hc=next;}
        const double x=safe(input[i]);
        for(int row=0;row<size;++row){
            system[row][size]+=basis[row]*x;
            for(int column=0;column<=row;++column)system[row][column]+=basis[row]*basis[column];
        }
    }
    for(int row=0;row<size;++row)for(int column=row+1;column<size;++column)system[row][column]=system[column][row];
    for(int column=0;column<size;++column){
        int pivot=column;for(int row=column+1;row<size;++row)if(std::abs(system[row][column])>std::abs(system[pivot][column]))pivot=row;
        if(std::abs(system[pivot][column])<1e-10)return false;
        if(pivot!=column)for(int k=column;k<=size;++k)std::swap(system[column][k],system[pivot][k]);
        const double scale=system[column][column];for(int k=column;k<=size;++k)system[column][k]/=scale;
        for(int row=0;row<size;++row)if(row!=column){const double value=system[row][column];for(int k=column;k<=size;++k)system[row][k]-=value*system[column][k];}
    }
    for(int h=0;h<partialCount;++h){const double a=system[1+2*h][size],b=system[2+2*h][size];power[size_t(h)]=.5*(a*a+b*b);}
    return true;
}
bool oldPhaseResidual(const float* input,int count,const double* carrierCos,const double* carrierSin,int partialCount,
                      const std::array<double,HarmonicContinuity::maximumPartials>& phases,
                      const std::array<bool,HarmonicContinuity::maximumPartials>& trusted,
                      double& residual) noexcept {
    // Unknown phase bands retain their two unconstrained quadratures. Known
    // bands have one NONNEGATIVE amplitude along their predicted axes. One
    // shared polarity sign is tried for the whole source, so a cable polarity
    // flip is harmless but independent pi flips of partials are not hidden.
    std::array<int,HarmonicContinuity::maximumPartials> first{};
    int size=1;for(int h=0;h<partialCount;++h){first[size_t(h)]=size;size+=trusted[size_t(h)]?1:2;}
    double system[dimension][dimension+1]{};
    const double wc=std::cos(2*pi/(count-1)),ws=std::sin(2*pi/(count-1));
    double windowC=1,windowS=0,totalWeight=0;
    std::array<double,HarmonicContinuity::maximumPartials> pc{},ps{};
    for(int h=0;h<partialCount;++h){pc[size_t(h)]=std::cos(phases[size_t(h)]);ps[size_t(h)]=std::sin(phases[size_t(h)]);}
    for(int i=0;i<count;++i){const double c=carrierCos[i],s=carrierSin[i];double basis[dimension]{1},hc=c,hs=s;
        for(int h=0;h<partialCount;++h){const int k=first[size_t(h)];
            if(trusted[size_t(h)])basis[k]=hc*pc[size_t(h)]-hs*ps[size_t(h)];
            else{basis[k]=hc;basis[k+1]=hs;}
            const double next=hc*c-hs*s;hs=hs*c+hc*s;hc=next;}
        const double weight=.5-.5*windowC,x=safe(input[i]);totalWeight+=weight;
        for(int row=0;row<size;++row){system[row][size]+=weight*basis[row]*x;for(int column=0;column<=row;++column)system[row][column]+=weight*basis[row]*basis[column];}
        const double nw=windowC*wc-windowS*ws;windowS=windowS*wc+windowC*ws;windowC=nw;
    }
    for(int row=0;row<size;++row)for(int column=row+1;column<size;++column)system[row][column]=system[column][row];
    std::array<bool,dimension> nonnegative{};
    for(int h=0;h<partialCount;++h)if(trusted[size_t(h)])nonnegative[size_t(first[size_t(h)])]=true;
    double maximumRhs=0;for(int i=0;i<size;++i)maximumRhs=std::max(maximumRhs,std::abs(system[i][size]));
    const double tolerance=std::max(1e-24,maximumRhs*1e-8);
    double bestResidual=1e100;
    for(int polarity:{1,-1}){
      std::array<double,dimension> coefficient{},sign{};
      for(int i=0;i<size;++i)sign[size_t(i)]=nonnegative[size_t(i)]?polarity:1;
      bool converged=false;
      for(int iteration=0;iteration<64;++iteration){
        for(int i=0;i<size;++i){
            if(system[i][i]<1e-10)return false;
            double value=system[i][size]*sign[size_t(i)];
            for(int j=0;j<size;++j)if(j!=i)value-=system[i][j]*sign[size_t(i)]*sign[size_t(j)]*coefficient[size_t(j)];
            value/=system[i][i];coefficient[size_t(i)]=nonnegative[size_t(i)]?std::max(0.,value):value;
        }
        double maximumViolation=0;
        for(int i=0;i<size;++i){double gradient=-system[i][size]*sign[size_t(i)];
            for(int j=0;j<size;++j)gradient+=system[i][j]*sign[size_t(i)]*sign[size_t(j)]*coefficient[size_t(j)];
            const double violation=nonnegative[size_t(i)]&&coefficient[size_t(i)]==0?std::max(0.,-gradient):std::abs(gradient);
            maximumViolation=std::max(maximumViolation,violation);
        }
        if(maximumViolation<=tolerance){converged=true;break;}
      }
      // If either physical old-phase hypothesis is unresolved, do not claim
      // evidence for a new excitation from its potentially suboptimal fit.
      if(!converged)return false;
      for(int i=0;i<size;++i)coefficient[size_t(i)]*=sign[size_t(i)];
      windowC=1;windowS=0;
      double candidateResidual=0;
      for(int i=0;i<count;++i){const double c=carrierCos[i],s=carrierSin[i];double prediction=coefficient[0],hc=c,hs=s;
        for(int h=0;h<partialCount;++h){const int k=first[size_t(h)];
            if(trusted[size_t(h)])prediction+=coefficient[size_t(k)]*(hc*pc[size_t(h)]-hs*ps[size_t(h)]);
            else prediction+=coefficient[size_t(k)]*hc+coefficient[size_t(k+1)]*hs;
            const double next=hc*c-hs*s;hs=hs*c+hc*s;hc=next;}
        const double error=safe(input[i])-prediction;candidateResidual+=(.5-.5*windowC)*error*error;
        const double nw=windowC*wc-windowS*ws;windowS=windowS*wc+windowC*ws;windowC=nw;
      }
      bestResidual=std::min(bestResidual,candidateResidual/std::max(totalWeight,1e-30));
    }
    residual=std::max(0.,bestResidual);return true;
}
}
void HarmonicContinuity::prepare(double sampleRate) noexcept {
    rate=std::isfinite(sampleRate)&&sampleRate>=4000&&sampleRate<=384000?sampleRate:12000;
    reset();
}
void HarmonicContinuity::reset() noexcept {
    previousCentre=previousFrequency=residualBaseline=previousFrequencySample=previousCentreFrequency=0;
    previousPhase={};previousPower={};baselinePower={};departure={};detuning={};referenceAge={};previousTrusted={};initialized=false;referenceFrozen=false;frequencyHistory={};frequencyHistoryCount=0;
}
bool HarmonicContinuity::insertFrequency(double sample,double frequency) noexcept {
    if(frequencyHistoryCount==0){
        frequencyHistory[0]={sample-2*maximumSamples,frequency,0};frequencyHistoryCount=1;
    }
    if(sample<frequencyHistory[0].sample)return false;
    int at=0;while(at<frequencyHistoryCount&&frequencyHistory[size_t(at)].sample<sample)++at;
    if(at<frequencyHistoryCount&&frequencyHistory[size_t(at)].sample==sample)frequencyHistory[size_t(at)].frequency=frequency;
    else{
        if(frequencyHistoryCount==int(frequencyHistory.size())){
            for(int i=1;i<frequencyHistoryCount;++i)frequencyHistory[size_t(i-1)]=frequencyHistory[size_t(i)];
            --frequencyHistoryCount;--at;
            if(at<0)return false;
        }
        for(int i=frequencyHistoryCount;i>at;--i)frequencyHistory[size_t(i)]=frequencyHistory[size_t(i-1)];
        frequencyHistory[size_t(at)]={sample,frequency,0};++frequencyHistoryCount;
    }
    for(int i=1;i<frequencyHistoryCount;++i){const auto& a=frequencyHistory[size_t(i-1)];auto& b=frequencyHistory[size_t(i)];
        b.phase=a.phase+2*pi/rate*(a.frequency+b.frequency)*.5*(b.sample-a.sample);}
    return true;
}
double HarmonicContinuity::carrierPhase(double sample) const noexcept {
    if(frequencyHistoryCount<1)return 0;
    int at=frequencyHistoryCount-1;
    for(int i=0;i<frequencyHistoryCount-1;++i)if(sample<frequencyHistory[size_t(i+1)].sample){at=i;break;}
    const auto& point=frequencyHistory[size_t(at)];double slope=0;
    if(sample<frequencyHistory[0].sample)at=0;
    else if(at+1<frequencyHistoryCount){const auto& next=frequencyHistory[size_t(at+1)];slope=(next.frequency-point.frequency)/(next.sample-point.sample);}
    else if(at>0){const auto& previous=frequencyHistory[size_t(at-1)];slope=(point.frequency-previous.frequency)/(point.sample-previous.sample);}
    const double elapsed=sample-point.sample;
    return point.phase+2*pi/rate*(point.frequency*elapsed+.5*slope*elapsed*elapsed);
}
double HarmonicContinuity::carrierFrequency(double sample) const noexcept {
    if(frequencyHistoryCount<1)return 0;
    int at=frequencyHistoryCount-1;
    for(int i=0;i<frequencyHistoryCount-1;++i)if(sample<frequencyHistory[size_t(i+1)].sample){at=i;break;}
    const auto& point=frequencyHistory[size_t(at)];double slope=0;
    if(sample<frequencyHistory[0].sample)at=0;
    else if(at+1<frequencyHistoryCount){const auto& next=frequencyHistory[size_t(at+1)];slope=(next.frequency-point.frequency)/(next.sample-point.sample);}
    else if(at>0){const auto& previous=frequencyHistory[size_t(at-1)];slope=(point.frequency-previous.frequency)/(point.sample-previous.sample);}
    return point.frequency+slope*(sample-point.sample);
}
HarmonicContinuity::Evidence HarmonicContinuity::observe(const float* input,int samples,float frequency,
                                                       std::int64_t lastSample) noexcept {
    return observeAt(input,samples,frequency,double(lastSample),lastSample);
}
HarmonicContinuity::Evidence HarmonicContinuity::observeAt(const float* input,int samples,float frequency,
                                                        double frequencySample,std::int64_t lastSample) noexcept {
    Evidence result;
    result.referenceFrozen=referenceFrozen;
    if(!input||samples<32||!std::isfinite(frequency)||!std::isfinite(frequencySample)||frequency<20||frequency>=rate*.45)return result;
    const double period=rate/frequency;
    const int count=std::min(std::min(samples,maximumSamples),std::max(64,int(std::ceil(period*2.5))));
    if(count<period*1.8)return result;
    input+=samples-count;result.observationSamples=count;
    const int partialCount=std::min(maximumPartials,int(std::ceil(rate*.45/frequency))-1);
    if(partialCount<1)return result;
    const int size=1+2*partialCount;
    double system[dimension][dimension+1]{};
    const double centre=double(lastSample)-(count-1)*.5;
    if(frequencySample>lastSample+.5||!insertFrequency(frequencySample,frequency))return result;
    const double endFrequency=carrierFrequency(double(lastSample));
    if(endFrequency<20||endFrequency>=rate*.45)return result;
    std::array<double,maximumSamples> carrierCos{},carrierSin{};
    const double centrePhase=carrierPhase(centre);
    for(int i=0;i<count;++i){const double angle=carrierPhase(double(lastSample-count+1+i))-centrePhase;carrierCos[size_t(i)]=std::cos(angle);carrierSin[size_t(i)]=std::sin(angle);}
    const double windowOmega=2*pi/(count-1),wc=std::cos(windowOmega),ws=std::sin(windowOmega);
    double windowC=1,windowS=0;
    double totalWeight=0,squaredWeight=0,weightedPower=0,weightedSum=0;
    for(int i=0;i<count;++i){
        const double c=carrierCos[size_t(i)],s=carrierSin[size_t(i)];
        double basis[dimension]{1},hc=c,hs=s;
        for(int h=0;h<partialCount;++h){basis[1+2*h]=hc;basis[2+2*h]=hs;const double next=hc*c-hs*s;hs=hs*c+hc*s;hc=next;}
        const double weight=.5-.5*windowC,x=safe(input[i]);totalWeight+=weight;squaredWeight+=weight*weight;
        weightedPower+=weight*x*x;weightedSum+=weight*x;
        for(int row=0;row<size;++row){
            system[row][size]+=weight*basis[row]*x;
            for(int column=0;column<=row;++column)system[row][column]+=weight*basis[row]*basis[column];
        }
        const double nextWindow=windowC*wc-windowS*ws;windowS=windowS*wc+windowC*ws;windowC=nextWindow;
    }
    if(totalWeight<=0)return result;
    for(int row=0;row<size;++row)for(int column=row+1;column<size;++column)system[row][column]=system[column][row];
    for(int column=0;column<size;++column){
        int pivot=column;for(int row=column+1;row<size;++row)if(std::abs(system[row][column])>std::abs(system[pivot][column]))pivot=row;
        if(std::abs(system[pivot][column])<1e-10)return result;
        if(pivot!=column)for(int k=column;k<=size;++k)std::swap(system[column][k],system[pivot][k]);
        const double scale=system[column][column];for(int k=column;k<=size;++k)system[column][k]/=scale;
        for(int row=0;row<size;++row)if(row!=column){const double value=system[row][column];for(int k=column;k<=size;++k)system[row][k]-=value*system[column][k];}
    }
    std::array<double,maximumPartials> phase{},power{};double maximumPower=0;
    for(int h=0;h<partialCount;++h){const double a=system[1+2*h][size],b=system[2+2*h][size];
        phase[size_t(h)]=std::atan2(-b,a);power[size_t(h)]=.5*(a*a+b*b);maximumPower=std::max(maximumPower,power[size_t(h)]);
    }
    windowC=1;windowS=0;double residual=0;
    for(int i=0;i<count;++i){const double c=carrierCos[size_t(i)],s=carrierSin[size_t(i)];double prediction=system[0][size],hc=c,hs=s;
        for(int h=0;h<partialCount;++h){prediction+=system[1+2*h][size]*hc+system[2+2*h][size]*hs;const double next=hc*c-hs*s;hs=hs*c+hc*s;hc=next;}
        const double error=safe(input[i])-prediction;residual+=(.5-.5*windowC)*error*error;
        const double nextWindow=windowC*wc-windowS*ws;windowS=windowS*wc+windowC*ws;windowC=nextWindow;
    }
    const double signal=std::max(0.,weightedPower/totalWeight-std::pow(weightedSum/totalWeight,2));
    residual=std::max(0.,residual/totalWeight);const double foreground=std::max(0.,signal-residual);
    result.signalPower=float(signal);result.foregroundPower=float(foreground);result.residualPower=float(residual);
    result.harmonicFraction=float(foreground/std::max(signal,1e-30));
    const double effectiveSamples=totalWeight*totalWeight/std::max(squaredWeight,1e-30);
    const double projectionNoise=std::max(1e-24,2*residual/effectiveSamples);
    const double elapsed=initialized?(centre-previousCentre)/rate:0;
    result.frequencyComparable=initialized&&elapsed>0&&elapsed<.080
        &&std::abs(12*std::log2(frequency/previousFrequency))<3;
    // Integrate the actual received F0 sequence over the two phase centres.
    // No future F0 samples or output timestamp shifts are used.
    const double expected=initialized?centrePhase-carrierPhase(previousCentre):0;
    const double baselineCoefficient=elapsed>0?1-std::exp(-elapsed/.060):1;
    double weightSum=0,stableWeight=0,addedForeground=0,baselineForeground=0;
    std::array<double,maximumPartials> offsets{};int offsetCount=0;
    std::array<double,maximumPartials> predictedPhases{};
    std::array<bool,maximumPartials> havePrediction{};
    for(int h=0;h<maximumPartials;++h){auto& part=result.partials[size_t(h)];
        const double p=power[size_t(h)],oldBaseline=initialized?baselinePower[size_t(h)]:p;
        part.power=float(p);part.baselinePower=float(oldBaseline);
        part.projectionSnrDb=float(10*std::log10(std::max(p,1e-30)/projectionNoise));
        // Confidence belongs to this partial's projected noise, not the
        // loudest bin. An unrelated fundamental must not erase strong-SNR
        // upper harmonics merely by changing a global amplitude ratio.
        part.observed=h<partialCount&&p>projectionNoise*4;
        result.observedPartials+=part.observed;
        part.energyRising=initialized&&part.observed&&p>oldBaseline*1.35&&p-oldBaseline>projectionNoise*6;
        part.phaseTrusted=result.frequencyComparable&&part.observed&&previousTrusted[size_t(h)];
        const double predicted=result.frequencyComparable
            ?wrap(previousPhase[size_t(h)]+(h+1)*expected+2*pi*detuning[size_t(h)]*elapsed):phase[size_t(h)];
        predictedPhases[size_t(h)]=predicted;
        havePrediction[size_t(h)]=result.frequencyComparable&&previousTrusted[size_t(h)];
        if(result.frequencyComparable)referenceAge[size_t(h)]+=elapsed;
        if(part.phaseTrusted){
            const double innovation=wrap(phase[size_t(h)]-predicted);
            const double weight=std::clamp(std::min(p,previousPower[size_t(h)])/(projectionNoise*16),.25,1.);
            part.phaseInnovation=float(innovation);part.phaseWeight=float(weight);
            departure[size_t(h)]=referenceFrozen?innovation:wrap(departure[size_t(h)]*std::exp(-elapsed/.040)+innovation);
            part.phaseDeparture=float(departure[size_t(h)]);
            result.departurePartials+=std::abs(part.phaseDeparture)>.65f;
            part.frequencyOffsetHz=float(innovation/(2*pi*(h+1)*std::max(elapsed,referenceAge[size_t(h)])));
            part.continuing=std::abs(innovation)<.35;part.changed=std::abs(innovation)>.65;
            ++result.phaseSupport;result.continuingPartials+=part.continuing;result.changedPartials+=part.changed;
            weightSum+=weight;stableWeight+=weight*std::max(0.,std::cos(innovation));
            offsets[size_t(offsetCount++)]=part.frequencyOffsetHz;
            // Real stiff-string partials need not be exact integer multiples.
            // Learn each small stable detuning independently; a contaminated
            // fundamental must never rotate every upper partial's reference.
            if(!referenceFrozen&&std::abs(innovation)<.20&&!part.energyRising){
                const double target=std::clamp(detuning[size_t(h)]+innovation/(2*pi*std::max(elapsed,referenceAge[size_t(h)])),-frequency*(h+1)*.015,frequency*(h+1)*.015);
                detuning[size_t(h)]+=(1-std::exp(-elapsed/.100))*(target-detuning[size_t(h)]);
            }
        }else{
            departure[size_t(h)]=0;
            if(!result.frequencyComparable){detuning[size_t(h)]=0;referenceAge[size_t(h)]=0;}
        }
        result.energyRisePartials+=part.energyRising;
        if(part.observed)addedForeground+=std::max(0.,p-oldBaseline);
        baselineForeground+=oldBaseline;
        // Update a continuing partial, but let a transient phase outlier leave
        // the established source's reference intact for the next observation.
        if(!initialized)baselinePower[size_t(h)]=p;
        else if(!referenceFrozen&&part.phaseTrusted&&part.continuing)baselinePower[size_t(h)]+=baselineCoefficient*(p-baselinePower[size_t(h)]);
        // Advance an established oscillator through an uncertain observation;
        // do not replace its physical phase reference with a noisy projection.
        // Each partial keeps its own actual age since the last trusted update.
        // A persistent new phase remains visible until the caller accepts a
        // new excitation and resets this observer. Silence supplies no proof.
        if(referenceFrozen&&result.frequencyComparable&&previousTrusted[size_t(h)])previousPhase[size_t(h)]=predicted;
        else{
            previousPhase[size_t(h)]=phase[size_t(h)];previousPower[size_t(h)]=p;
            previousTrusted[size_t(h)]=part.observed;referenceAge[size_t(h)]=0;
        }
    }
    result.phaseContinuity=weightSum>0?float(stableWeight/weightSum):0;
    // Compare the free-phase fit with an amplitude-refitted model along
    // established phase axes. Corruption in
    // a weak band is not convincing excitation when the unmodeled waveform
    // residual is still much larger. This is diagnostic likelihood evidence,
    // not an automatic attack decision or a permission to discard residuals.
    double constrainedResidual=residual;
    result.phaseFitComparable=oldPhaseResidual(input,count,carrierCos.data(),carrierSin.data(),partialCount,predictedPhases,havePrediction,constrainedResidual);
    result.phaseFitGainPower=float(std::max(0.,constrainedResidual-residual));
    result.phaseFitGainToResidualDb=float(10*std::log10(std::max(double(result.phaseFitGainPower),1e-30)/std::max(residual,1e-24)));
    const double addedResidual=initialized?std::max(0.,residual-residualBaseline):0;
    result.foregroundDeltaPower=float(addedForeground);result.residualDeltaPower=float(addedResidual);
    result.baselineForegroundPower=float(baselineForeground);result.baselineResidualPower=float(residualBaseline);
    result.foregroundNovelty=float(addedForeground/std::max(baselineForeground,projectionNoise));
    result.residualNovelty=float(addedResidual/std::max(residualBaseline,projectionNoise));
    result.noveltyFraction=float(addedForeground/std::max(addedForeground+addedResidual,projectionNoise));
    // A broadband pulse or one contaminated harmonic may raise weak fitted
    // bins. Require compatible gain on several independently continuing,
    // previously established partials before offering energy-only support.
    std::array<double,maximumPartials> recentPower{};
    const int recentCount=std::min(count,std::max(2*partialCount+3,int(std::ceil(period))));
    const bool recentValid=recentPowers(input+count-recentCount,recentCount,carrierCos.data()+count-recentCount,carrierSin.data()+count-recentCount,partialCount,recentPower);
    for(int h=0;h<maximumPartials;++h)result.partials[size_t(h)].recentPower=float(recentPower[size_t(h)]);
    int energyEligible=0;double maximumBaseline=0;
    for(const auto& part:result.partials)maximumBaseline=std::max(maximumBaseline,double(part.baselinePower));
    for(int reference=0;reference<maximumPartials;++reference){const auto& source=result.partials[size_t(reference)];
        if(!source.energyRising||!source.phaseTrusted||!source.continuing||std::abs(source.phaseDeparture)>.35f||source.baselinePower<maximumBaseline*.01)continue;
        if(!recentValid||source.recentPower<source.baselinePower*1.35f)continue;
        ++energyEligible;
        const double gain=10*std::log10(source.power/std::max(source.baselinePower,1e-30f));int agreeing=0;
        const double recentGain=10*std::log10(source.recentPower/std::max(source.baselinePower,1e-30f));
        if(std::abs(recentGain-gain)>2.)continue;
        for(const auto& target:result.partials){
            if(!target.energyRising||!target.phaseTrusted||!target.continuing||std::abs(target.phaseDeparture)>.35f||target.baselinePower<maximumBaseline*.01)continue;
            if(target.recentPower<target.baselinePower*1.35f)continue;
            const double other=10*std::log10(target.power/std::max(target.baselinePower,1e-30f));
            const double otherRecent=10*std::log10(target.recentPower/std::max(target.baselinePower,1e-30f));
            if(std::abs(otherRecent-other)>2.)continue;
            agreeing+=std::abs(other-gain)<2.;
        }
        if(agreeing>result.coherentEnergyRisePartials){result.coherentEnergyRisePartials=agreeing;result.commonEnergyGainDb=float(gain);}
    }
    result.energyGainConsensus=energyEligible>0?float(result.coherentEnergyRisePartials/double(energyEligible)):0;
    if(offsetCount>=2){
        for(int i=1;i<offsetCount;++i){const double value=offsets[size_t(i)];int at=i;while(at>0&&offsets[size_t(at-1)]>value){offsets[size_t(at)]=offsets[size_t(at-1)];--at;}offsets[size_t(at)]=value;}
        const double median=offsets[size_t(offsetCount/2)];
        int agreeing=0;for(int i=0;i<offsetCount;++i)agreeing+=std::abs(offsets[size_t(i)]-median)<1.;
        result.frequencyOffsetHz=float(median);result.frequencyConsensus=float(agreeing/double(offsetCount));
    }
    // Find a COMMON phase displacement without making every feature depend
    // on the same fundamental. This is a diagnostic fit, not a note decision.
    if(result.phaseSupport>=2){double best=-1,bestPhase=0;
        for(int step=0;step<128;++step){const double reset=-pi+2*pi*step/128;double agreement=0;
            for(int h=0;h<maximumPartials;++h){const auto& part=result.partials[size_t(h)];if(part.phaseTrusted)agreement+=part.phaseWeight*std::cos(part.phaseDeparture-(h+1)*reset);}
            if(agreement>best){best=agreement;bestPhase=reset;}}
        result.commonResetRadians=float(bestPhase);result.resetCoherence=float(std::max(0.,best/weightSum));
    }
    if(!referenceFrozen||!initialized)residualBaseline=initialized?residualBaseline+baselineCoefficient*(residual-residualBaseline):residual;
    previousCentre=centre;previousFrequency=frequency;previousFrequencySample=frequencySample;
    previousCentreFrequency=carrierFrequency(centre);initialized=true;
    return result;
}
}
