// SPDX-License-Identifier: AGPL-3.0-or-later
#include "InputDynamics.h"
#include <algorithm>
#include <cmath>

namespace flysnapper::pitch {
namespace {
constexpr double tiny=1e-30;
float db(double amplitude) noexcept {return float(std::clamp(20*std::log10(std::max(amplitude,1e-6)),-120.,48.));}
double gain(double decibels) noexcept {return std::pow(10.,decibels/20.);}
float range(float x,float lo,float hi,float fallback) noexcept {return std::isfinite(x)?std::clamp(x,lo,hi):fallback;}
double coefficient(double seconds,double rate) noexcept {return 1-std::exp(-1/(std::max(seconds,1e-7)*rate));}
}

void InputDynamics::Histogram::add(float level) noexcept {
    const int bin=std::clamp(int(std::floor(std::isfinite(level)?level+120:0)),0,histogramBins-1);
    ++counts[size_t(bin)];++total;
}
float InputDynamics::Histogram::quantile(float fraction) const noexcept {
    if(!total)return -120;
    const uint32_t wanted=std::max(1u,uint32_t(std::ceil(std::clamp(fraction,0.f,1.f)*total)));
    uint32_t sum=0;
    for(int i=0;i<histogramBins;++i){sum+=counts[size_t(i)];if(sum>=wanted)return i==0?-120.f:float(i-120)+.5f;}
    return 24;
}
uint32_t InputDynamics::Histogram::above(float threshold) const noexcept {
    const int first=std::clamp(int(std::ceil(threshold+120-.5f)),0,histogramBins);
    uint32_t sum=0;for(int i=first;i<histogramBins;++i)sum+=counts[size_t(i)];return sum;
}
float InputDynamics::Histogram::quantileAbove(float threshold,float fraction) const noexcept {
    const int first=std::clamp(int(std::ceil(threshold+120-.5f)),0,histogramBins);
    const auto count=above(threshold);if(!count)return -120;
    const uint32_t wanted=std::max(1u,uint32_t(std::ceil(std::clamp(fraction,0.f,1.f)*count)));
    uint32_t sum=0;
    for(int i=first;i<histogramBins;++i){sum+=counts[size_t(i)];if(sum>=wanted)return i==0?-120.f:float(i-120)+.5f;}
    return 24;
}
InputDynamics::Settings InputDynamics::sanitise(const Settings& s) noexcept {
    Settings v=s;
    v.inputGainDb=range(s.inputGainDb,-24,24,0);
    v.gateThresholdDb=range(s.gateThresholdDb,-96,-12,-60);
    v.gateHysteresisDb=range(s.gateHysteresisDb,0,24,8);
    v.gateAttackMs=range(s.gateAttackMs,.05f,10,.25f);
    v.gateHoldMs=range(s.gateHoldMs,0,250,35);
    v.gateReleaseMs=range(s.gateReleaseMs,10,2000,160);
    v.expanderRatio=range(s.expanderRatio,1,12,4);
    v.compressorThresholdDb=range(s.compressorThresholdDb,-60,0,-24);
    v.compressorRatio=range(s.compressorRatio,1,12,3);
    v.compressorAttackMs=range(s.compressorAttackMs,.1f,50,3);
    v.compressorReleaseMs=range(s.compressorReleaseMs,10,1000,120);
    v.makeupGainDb=range(s.makeupGainDb,0,24,0);
    v.limiterCeilingDb=range(s.limiterCeilingDb,-12,0,-3);
    return v;
}
InputDynamics::InputDynamics(double sampleRate){prepare(sampleRate);}
void InputDynamics::prepare(double sampleRate) noexcept {
    rate=std::isfinite(sampleRate)&&sampleRate>=8000&&sampleRate<=384000?sampleRate:48000;
    blockSamples=std::max(1,int(std::lround(rate*.010)));
    noiseSampleCount=uint64_t(std::llround(rate*noiseSeconds));
    totalSampleCount=uint64_t(std::llround(rate*totalSeconds));
    calibrationLowpass=1-std::exp(-2*3.14159265358979323846*1400/rate);
    updateCoefficients();reset();
}
void InputDynamics::reset() noexcept {
    detectorEnvelope=compressorEnvelope=rawPower=outputPower=0;
    gateGain=0;compressorGain=limiterGain=1;lastTotalGain=1;
    holdRemaining=0;open=admission=limited=false;
    calibrationPhase=CalibrationPhase::Idle;calibrationResult={};
    noiseRms={};noisePeaks={};playingRms={};playingPeaks={};decayRates={};
    calibrationSamples=clippedSamples=invalidSamples=0;blockCount=0;blockPower=blockPeak=0;
    decayEnvelopePower=0;previousDecayDb=-120;decayReady=false;
    calibrationHistory={};calibrationLow1=calibrationLow2=calibrationPhaseStep=0;
    calibrationWrite=calibrationFilled=calibrationSinceCheck=0;tonalWindows=0;
}
void InputDynamics::updateCoefficients() noexcept {
    inputGain=gain(config.inputGainDb);makeupGain=gain(config.makeupGainDb);
    openLevel=gain(config.gateThresholdDb);closeLevel=gain(config.gateThresholdDb-config.gateHysteresisDb);
    ceiling=gain(config.limiterCeilingDb);
    compressorLevel=gain(config.compressorThresholdDb);
    envelopeRelease=std::exp(-1/(rate*.015));
    // These two controls specify time to traverse60dB, rather than leaving
    // an exponentially decaying gate almost open for many release periods.
    gateAttack=1-std::exp(-std::log(1000.)/(rate*config.gateAttackMs*.001));
    gateRelease=1-std::exp(-std::log(1000.)/(rate*config.gateReleaseMs*.001));
    compAttack=coefficient(config.compressorAttackMs*.001,rate);
    compRelease=coefficient(config.compressorReleaseMs*.001,rate);
    limiterRelease=coefficient(.050,rate);rmsCoefficient=coefficient(.010,rate);
    holdSamples=std::max(0,int(std::lround(config.gateHoldMs*.001*rate)));
}
void InputDynamics::configure(const Settings& s) noexcept {
    const auto next=sanitise(s);
    // A trim change invalidates the measured scale. The processor also cancels
    // on a manual knob edit so a later proposal cannot overwrite that edit.
    if(next.inputGainDb!=config.inputGainDb&&isCalibrating())cancelCalibration();
    const bool switched=next.enabled!=config.enabled;
    config=next;updateCoefficients();
    if(switched){gateGain=0;compressorGain=limiterGain=1;holdRemaining=0;open=admission=limited=false;}
}
float InputDynamics::processSample(float input) noexcept {
    const bool invalid=!std::isfinite(input);
    const double raw=invalid?0.:std::clamp(double(input),-16.,16.);
    const double x=raw*inputGain,absolute=std::abs(x);
    rawPower+=rmsCoefficient*(raw*raw-rawPower);
    if(rawPower<tiny)rawPower=0;
    if(isCalibrating())observeCalibration(float(x),!invalid&&std::abs(input)>=.9995f,invalid);
    detectorEnvelope=std::max(absolute,detectorEnvelope*envelopeRelease);
    if(detectorEnvelope<1e-15)detectorEnvelope=0;
    if(!config.enabled){
        outputPower+=rmsCoefficient*(raw*raw-outputPower);lastTotalGain=1;
        return float(raw);
    }
    if(detectorEnvelope>=openLevel){open=true;holdRemaining=holdSamples;}
    else if(open){
        if(detectorEnvelope>=closeLevel)holdRemaining=holdSamples;
        else if(holdRemaining>0)--holdRemaining;
        else open=false;
    }
    admission=open&&detectorEnvelope>=openLevel;
    double gateTarget=0;
    if(open){
        gateTarget=1;
        if(detectorEnvelope<closeLevel&&config.expanderRatio>1)
            gateTarget=std::pow(std::max(0.,detectorEnvelope/closeLevel),config.expanderRatio-1);
    }
    gateGain+=(gateTarget>gateGain?gateAttack:gateRelease)*(gateTarget-gateGain);
    if(gateGain<1e-8)gateGain=0;
    const double gated=x*gateGain;
    const double compInput=std::abs(gated);
    compressorEnvelope+=(compInput>compressorEnvelope?compAttack:compRelease)*(compInput-compressorEnvelope);
    const double target=compressorEnvelope>compressorLevel
        ?std::pow(compressorEnvelope/compressorLevel,1./config.compressorRatio-1):1;
    compressorGain+=(target<compressorGain?compAttack:compRelease)*(target-compressorGain);
    const double beforeLimit=gated*compressorGain*makeupGain;
    limiterGain+=limiterRelease*(1-limiterGain);
    const double peak=std::abs(beforeLimit);
    if(peak*limiterGain>ceiling&&peak>0)limiterGain=ceiling/peak;
    limited=limiterGain<.999;
    const double output=std::clamp(beforeLimit*limiterGain,-ceiling,ceiling);
    lastTotalGain=inputGain*gateGain*compressorGain*makeupGain*limiterGain;
    outputPower+=rmsCoefficient*(output*output-outputPower);
    if(outputPower<tiny)outputPower=0;
    return float(output);
}
InputDynamics::Meter InputDynamics::meter() const noexcept {
    Meter m;m.rawRmsDb=db(std::sqrt(std::max(0.,rawPower)));
    m.inputEnvelopeDb=db(detectorEnvelope);m.processedDb=db(std::sqrt(std::max(0.,outputPower)));
    m.gainDb=db(lastTotalGain);m.gateOpen=!config.enabled||open;
    m.admissionOpen=!config.enabled||admission;m.limiting=config.enabled&&limited;return m;
}
bool InputDynamics::isCalibrating() const noexcept {return calibrationPhase==CalibrationPhase::Noise||calibrationPhase==CalibrationPhase::Playing;}
void InputDynamics::startCalibration() noexcept {
    calibrationPhase=CalibrationPhase::Noise;calibrationResult={};
    noiseRms={};noisePeaks={};playingRms={};playingPeaks={};decayRates={};
    calibrationSamples=clippedSamples=invalidSamples=0;blockCount=0;blockPower=blockPeak=0;
    decayEnvelopePower=0;previousDecayDb=-120;decayReady=false;
    calibrationHistory={};calibrationLow1=calibrationLow2=calibrationPhaseStep=0;
    calibrationWrite=calibrationFilled=calibrationSinceCheck=0;tonalWindows=0;
}
void InputDynamics::cancelCalibration() noexcept {
    if(isCalibrating()){calibrationPhase=CalibrationPhase::Cancelled;calibrationResult={};}
}
InputDynamics::CalibrationStatus InputDynamics::calibrationStatus() const noexcept {
    CalibrationStatus s;s.phase=calibrationPhase;s.result=calibrationResult;
    if(calibrationPhase!=CalibrationPhase::Idle){
        s.progress=float(std::min(1.,double(calibrationSamples)/std::max(uint64_t(1),totalSampleCount)));
        s.remainingSeconds=isCalibrating()?float((totalSampleCount-calibrationSamples)/rate):0;
    }
    return s;
}
void InputDynamics::finishCalibrationBlock(bool noise) noexcept {
    if(!blockCount)return;
    const double power=blockPower/blockCount;
    const float level=db(std::sqrt(power)),peak=db(blockPeak);
    if(noise){noiseRms.add(level);noisePeaks.add(peak);}
    else{
        playingRms.add(level);
        const float activeFloor=std::max(-110.f,noiseRms.quantile(.95f)+8);
        if(level>=activeFloor)playingPeaks.add(peak);
        // A moderate falling40-ms energy envelope describes audible decay.
        // Ignore abrupt muting, near-floor noise and upward excitation. This
        // only chooses a starting release; it cannot infer musical intention.
        const double elapsed=blockCount/rate;
        const double smooth=1-std::exp(-elapsed/.040);
        decayEnvelopePower+=smooth*(power-decayEnvelopePower);
        const double smoothedDb=db(std::sqrt(std::max(0.,decayEnvelopePower)));
        if(decayReady&&level>activeFloor+3){
            const double slope=(previousDecayDb-smoothedDb)/elapsed;
            if(slope>=3&&slope<=160)++decayRates[size_t(std::clamp(int(std::lround(slope)),0,160))];
        }
        previousDecayDb=smoothedDb;decayReady=true;
    }
    blockCount=0;blockPower=blockPeak=0;
}
void InputDynamics::observeCalibration(float input,bool clipped,bool invalid) noexcept {
    const bool noise=calibrationSamples<noiseSampleCount;
    blockPower+=double(input)*input;blockPeak=std::max(blockPeak,std::abs(double(input)));++blockCount;
    ++calibrationSamples;if(clipped)++clippedSamples;if(invalid)++invalidSamples;
    const bool phaseBoundary=calibrationSamples==noiseSampleCount;
    const bool complete=calibrationSamples>=totalSampleCount;
    if(blockCount>=blockSamples||phaseBoundary||complete)finishCalibrationBlock(noise);
    if(phaseBoundary)calibrationPhase=CalibrationPhase::Playing;
    else if(!noise)observeCalibrationPeriodicity(input);
    if(complete)finishCalibration();
}
void InputDynamics::observeCalibrationPeriodicity(float input) noexcept {
    // Calibration-only coarse periodicity prevents a raised broadband-noise
    // level alone from being accepted as a playing sample. This is not a new
    // pitch estimator or a proof that a periodic sound is intentional music.
    // Two causal lowpasses precede a4-kHz observation; no future samples enter.
    calibrationLow1+=calibrationLowpass*(input-calibrationLow1);
    calibrationLow2+=calibrationLowpass*(calibrationLow1-calibrationLow2);
    calibrationPhaseStep+=4000/rate;
    if(calibrationPhaseStep<1)return;
    calibrationPhaseStep-=1;
    calibrationHistory[size_t(calibrationWrite)]=float(calibrationLow2);
    calibrationWrite=(calibrationWrite+1)%int(calibrationHistory.size());
    calibrationFilled=std::min(calibrationFilled+1,int(calibrationHistory.size()));
    if(++calibrationSinceCheck<400||calibrationFilled<int(calibrationHistory.size()))return;
    calibrationSinceCheck=0;
    std::array<double,512> frame{};double mean=0,total=0;
    for(size_t i=0;i<frame.size();++i){frame[i]=calibrationHistory[(size_t(calibrationWrite)+i)%frame.size()];mean+=frame[i];}
    mean/=frame.size();for(auto& x:frame){x-=mean;total+=x*x;}
    if(db(std::sqrt(total/frame.size()))<std::max(-110.f,noiseRms.quantile(.95f)+8))return;
    std::array<double,116> correlation{};
    for(int lag=3;lag<int(correlation.size());++lag){
        double xy=0,xx=0,yy=0;
        for(int i=lag;i<int(frame.size());++i){const double x=frame[size_t(i)],y=frame[size_t(i-lag)];xy+=x*y;xx+=x*x;yy+=y*y;}
        correlation[size_t(lag)]=xx>tiny&&yy>tiny?xy/std::sqrt(xx*yy):0;
    }
    for(int lag=4;lag<115;++lag)if(correlation[size_t(lag)]>.60
        &&correlation[size_t(lag)]>correlation[size_t(lag-1)]
        &&correlation[size_t(lag)]>=correlation[size_t(lag+1)]){++tonalWindows;break;}
}
void InputDynamics::finishCalibration() noexcept {
    CalibrationResult r;r.settings=config;r.settings.enabled=true;
    r.noiseFloorDb=noiseRms.quantile(.95f);r.noisePeakDb=noisePeaks.quantile(.95f);
    r.clippedFraction=float(double(clippedSamples)/std::max(uint64_t(1),calibrationSamples));
    const float activeFloor=std::max(-110.f,r.noiseFloorDb+8);
    const uint32_t active=playingRms.above(activeFloor);
    r.usableSeconds=float(active*.010);
    r.tonalSeconds=tonalWindows*.1f;
    r.softSignalDb=playingRms.quantileAbove(activeFloor,.20f);
    r.strongSignalDb=playingRms.quantileAbove(activeFloor,.90f);
    const float softPeak=playingPeaks.quantile(.20f);
    uint32_t decayTotal=0;for(auto n:decayRates)decayTotal+=n;
    if(decayTotal>=20){uint32_t count=0;for(size_t i=0;i<decayRates.size();++i){count+=decayRates[i];if(count>=(decayTotal+1)/2){r.decayDbPerSecond=float(i);break;}}}
    if(invalidSamples>std::max(uint64_t(3),calibrationSamples/1000))r.failure=CalibrationFailure::InvalidInput;
    else if(r.clippedFraction>.001f)r.failure=CalibrationFailure::Clipping;
    else if(active<100||playingPeaks.total<100||r.tonalSeconds<.5f)r.failure=CalibrationFailure::NoPlaying;
    else if(r.strongSignalDb-r.noiseFloorDb<12||softPeak-r.noisePeakDb<6||r.noisePeakDb+3>-12)
        r.failure=CalibrationFailure::InsufficientSeparation;
    else{
        r.settings.gateThresholdDb=std::clamp(std::max(r.noisePeakDb+3,std::min(r.noisePeakDb+6,softPeak-3)),-96.f,-12.f);
        r.settings.gateHysteresisDb=std::clamp(r.settings.gateThresholdDb-r.noiseFloorDb-2,4.f,12.f);
        if(r.decayDbPerSecond>0){
            r.settings.gateHoldMs=std::clamp(400.f/r.decayDbPerSecond,20.f,65.f);
            r.settings.gateReleaseMs=std::clamp(6000.f/r.decayDbPerSecond,80.f,350.f);
        }else{r.settings.gateHoldMs=35;r.settings.gateReleaseMs=160;}
        r.settings.compressorThresholdDb=std::clamp(r.strongSignalDb-9,-48.f,-6.f);
        const float dynamicRange=r.strongSignalDb-r.softSignalDb;
        r.settings.compressorRatio=dynamicRange>18?3.f:dynamicRange>10?2.5f:2.f;
        r.settings=sanitise(r.settings);r.success=true;
    }
    calibrationResult=r;calibrationPhase=r.success?CalibrationPhase::Complete:CalibrationPhase::Failed;
}
}
