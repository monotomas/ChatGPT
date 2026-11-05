// SPDX-License-Identifier: AGPL-3.0-or-later
#include "Pitch/InputDynamics.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <new>
#include <stdexcept>

namespace {
bool trackAllocations=false;
unsigned allocations=0,checks=0;
constexpr double pi=3.1415926535897932384626433832795;
using Dynamics=flysnapper::pitch::InputDynamics;
void check(bool ok,const char* message){++checks;if(!ok)throw std::runtime_error(message);}
struct Random {uint32_t value=0x6db94a31;double next() noexcept {value=value*1664525u+1013904223u;return 2*(double(value)/4294967296.)-1;}};
double db(double amplitude){return 20*std::log10(std::max(amplitude,1e-12));}
Dynamics::Settings transparent(){Dynamics::Settings s;s.enabled=true;s.gateThresholdDb=-96;s.gateHoldMs=0;s.compressorThresholdDb=0;s.compressorRatio=1;s.limiterCeilingDb=0;return s;}

void bypassAndSanitisation(){
    Dynamics d(48000);
    for(float x:{-.75f,-.125f,0.f,.125f,.75f})check(d.processSample(x)==x,"Disabled dynamics altered finite input");
    check(d.processSample(std::numeric_limits<float>::quiet_NaN())==0,"NaN did not become silence");
    check(d.processSample(std::numeric_limits<float>::infinity())==0,"Infinity did not become silence");
    check(d.allowsNewNotes(),"Disabled dynamics blocked new-note admission");
    auto s=transparent();s.inputGainDb=std::numeric_limits<float>::infinity();s.gateThresholdDb=std::numeric_limits<float>::quiet_NaN();
    s.gateAttackMs=-200;s.gateReleaseMs=0;s.compressorRatio=100;s.limiterCeilingDb=100;
    d.configure(s);const auto v=d.settings();
    check(v.inputGainDb==0&&v.gateThresholdDb==-60,"Nonfinite settings lacked finite defaults");
    check(v.gateAttackMs==.05f&&v.gateReleaseMs==10&&v.compressorRatio==12&&v.limiterCeilingDb==0,"Settings were not range limited");
    for(int i=0;i<2000;++i)check(std::isfinite(d.processSample(float(std::sin(i*.1)*.2))),"Sanitised dynamics produced nonfinite output");
    d.prepare(0);check(d.sampleRate()==48000,"Invalid sample rate fallback failed");
}
void gateRejectsNoiseAndOpensPromptly(){
    for(double rate:{12000.,44100.,48000.,96000.}){
        Dynamics d(rate);auto s=transparent();s.gateThresholdDb=-40;s.gateHysteresisDb=6;s.gateHoldMs=25;s.gateReleaseMs=120;s.makeupGainDb=24;d.configure(s);
        Random random;double noiseEnergy=0;bool admitted=false;
        for(int i=0;i<int(rate*.3);++i){const double y=d.processSample(float(random.next()*.001));noiseEnergy+=y*y;admitted=admitted||d.allowsNewNotes();}
        check(noiseEnergy==0&&!admitted,"Below-threshold noise opened before post-gate gain");
        int first=-1;
        for(int i=0;i<int(rate*.020);++i){const double y=d.processSample(float(.1*std::sin(2*pi*82.406889*i/rate)));if(first<0&&std::abs(y)>.001)first=i;}
        check(first>=0&&first/rate<.0015,"Gate imposed excessive strong low-string onset delay");
        check(d.allowsNewNotes(),"Strong played tone did not enable admission");
        check(d.meter().gateOpen,"Strong played tone did not latch gate");
        std::cout<<"gate_onset_ms,"<<rate<<','<<1000*first/rate<<'\n';
    }
}
void hysteresisHoldAndRelease(){
    for(double rate:{12000.,48000.}){
        Dynamics d(rate);auto s=transparent();s.gateThresholdDb=-40;s.gateHysteresisDb=6;s.gateHoldMs=30;s.gateReleaseMs=120;d.configure(s);
        for(int i=0;i<int(rate*(1000./48000));++i)d.processSample(.02f);
        for(int i=0;i<int(rate*.250);++i)d.processSample(.007f);
        check(d.meter().gateOpen,"Between-threshold sustain lost the open latch");
        check(!d.allowsNewNotes(),"Hysteresis/hold incorrectly permitted fresh notes below opening threshold");
        for(int i=0;i<int(rate*.250);++i)d.processSample(.0005f);
        check(!d.meter().gateOpen&&!d.allowsNewNotes(),"Gate did not close after hold");
        check(std::abs(d.processSample(.0005f))<1e-8,"Closed gate retained an audible/noise floor");
        d.configure(transparent());
        for(int i=0;i<int(rate*(2000./48000));++i)d.processSample(.2f);
        for(int i=0;i<100;++i)check(d.processSample(0)==0,"Dynamics synthesised a release tail after actual silence");
    }
}
void quietRingingAndRawVelocity(){
    for(double rate:{12000.,48000.}){
        for(double frequency:{41.203444,82.406889,164.813778,659.255114}){
            Dynamics d(rate);auto s=transparent();s.gateThresholdDb=-65;s.gateHysteresisDb=8;s.gateHoldMs=35;s.gateReleaseMs=180;d.configure(s);
            double in=0,out=0;bool dropped=false;
            for(int i=0;i<int(rate*3);++i){const double t=i/rate,x=.05*std::exp(-t/1.5)*std::sin(2*pi*frequency*t);const double y=d.processSample(float(x));
                if(i>rate*.010){in+=x*x;out+=y*y;if(i>rate*.100&&!d.meter().gateOpen)dropped=true;}}
            check(!dropped,"Credible above-floor ringing repeatedly closed the dynamics gate");
            check(out/in>.999,"Gate attenuated an above-floor ringing source");
        }
        Dynamics a(rate),b(rate);auto settings=transparent();settings.inputGainDb=12;settings.compressorThresholdDb=-30;settings.compressorRatio=6;b.configure(settings);
        for(int i=0;i<int(rate*.5);++i){const float x=float(.02*std::sin(2*pi*220*i/rate));a.processSample(x);b.processSample(x);}
        check(std::abs(a.meter().rawRmsDb-b.meter().rawRmsDb)<.0001,"Dynamics changed raw-level velocity evidence");
        check(std::abs(a.meter().processedDb-b.meter().processedDb)>1,"Velocity control did not exercise actual processing");
    }
}
void compressionAndCeiling(){
    for(double rate:{12000.,48000.}){
        Dynamics d(rate);auto s=transparent();s.compressorThresholdDb=-30;s.compressorRatio=4;d.configure(s);
        std::array<double,2> energy{};
        for(int part=0;part<2;++part)for(int i=0;i<int(rate);++i){const double amplitude=part?.3:.03;const double y=d.processSample(float(amplitude*std::sin(2*pi*440*i/rate)));if(i>rate*.75)energy[size_t(part)]+=y*y;}
        const double span=db(std::sqrt(energy[1]/energy[0]));
        check(span>3&&span<10,"Compressor did not reduce the20dB source span predictably");
        s=transparent();s.inputGainDb=24;s.makeupGainDb=24;s.limiterCeilingDb=-3;d.configure(s);
        const double ceiling=std::pow(10.,-3./20.);bool limited=false;
        for(int i=0;i<int(rate*.5);++i){const double x=i%777==0?12.:.8*std::sin(2*pi*330*i/rate);const double y=d.processSample(float(x));
            check(std::isfinite(y)&&std::abs(y)<=ceiling+1e-6,"No-lookahead ceiling overshot");limited=limited||d.meter().limiting;}
        check(limited,"Limiter control did not actually limit");
    }
}
enum class Sample { Guitar,Silence,Noise,RaisedNoise,PlayedDuringQuiet,Clipped };
Dynamics::CalibrationResult calibrate(Dynamics& d,Sample kind,double gainFactor=1,bool incidentClip=false){
    d.startCalibration();const double rate=d.sampleRate();const int count=int(std::llround(rate*20));Random random;
    for(int i=0;i<count;++i){const double time=i/rate;const double noise=.0006*random.next()+.0004*std::sin(2*pi*50*time);double x=noise;
        if(kind==Sample::Silence)x=0;
        else if(kind==Sample::PlayedDuringQuiet)x=.03*std::sin(2*pi*110*time);
        else if(kind==Sample::RaisedNoise)x=random.next()*(time<3?.0006:.03);
        else if((kind==Sample::Guitar||kind==Sample::Clipped)&&time>=3){
            const double local=std::fmod(time-3,2.3),amplitude=.15*(1-std::exp(-local/.001))*std::exp(-local/.75);
            x+=amplitude*(std::sin(2*pi*110*local)+.3*std::sin(4*pi*110*local+.2)+.15*std::sin(6*pi*110*local+.6));
            if(kind==Sample::Clipped)x=std::clamp(x*20,-1.,1.);
        }
        if(incidentClip&&i==int(rate*5))x=1;
        d.processSample(float(x*gainFactor));
        if(i==int(std::llround(rate*3))-1)check(d.phase()==Dynamics::CalibrationPhase::Playing,"Calibration did not enter playing exactly after3seconds");
    }
    const auto status=d.calibrationStatus();check(status.progress==1&&status.remainingSeconds==0,"Calibration did not finish at20seconds");
    return status.result;
}
void calibrationSuccessAndGainScaling(){
    std::array<Dynamics::CalibrationResult,3> results{};int at=0;
    for(double commonGain:{.1,1.,4.}){
        Dynamics d(8000);auto s=transparent();s.enabled=false;s.gateThresholdDb=-60;d.configure(s);
        const auto r=calibrate(d,Sample::Guitar,commonGain);results[size_t(at++)]=r;
        check(r.success&&r.failure==Dynamics::CalibrationFailure::None,"Usable quiet+guitar calibration failed");
        check(r.tonalSeconds>=.5&&r.usableSeconds>=1,"Successful calibration had no actual active/periodic coverage");
        check(d.settings().gateThresholdDb==-60&&!d.settings().enabled,"Calibration applied settings before owner accepted proposal");
        check(r.settings.enabled&&r.settings.gateThresholdDb>r.noisePeakDb+2.5f,"Proposal did not leave a peak-noise margin");
        check(r.settings.gateThresholdDb<r.softSignalDb+3,"Proposal placed opening above soft musical level");
        check(r.settings.gateHysteresisDb>=4&&r.settings.gateHysteresisDb<=12,"Proposal lacked useful gate hysteresis");
        check(r.settings.gateReleaseMs>=80&&r.settings.gateReleaseMs<=350,"Learned release escaped its documented range");
        auto manual=r.settings;manual.gateThresholdDb=-47;d.configure(manual);for(int i=0;i<1000;++i)d.processSample(0);
        check(d.settings().gateThresholdDb==-47,"Completed proposal later overwrote manual control");
        std::cout<<"calibration,"<<commonGain<<",noise,"<<r.noiseFloorDb<<",gate,"<<r.settings.gateThresholdDb<<",release,"<<r.settings.gateReleaseMs<<",tonal_seconds,"<<r.tonalSeconds<<'\n';
    }
    check(std::abs((results[0].settings.gateThresholdDb-results[1].settings.gateThresholdDb)+20)<1.1,"Common gain did not shift learned gate by20dB");
    check(std::abs((results[2].settings.gateThresholdDb-results[1].settings.gateThresholdDb)-db(4))<1.1,"Common gain did not shift learned gate consistently");
    // The full-rate phase clock and sample-rate lifecycle are independent of
    // the4-kHz calibration observation history.
    for(double rate:{44100.,96000.}){Dynamics d(rate);const auto r=calibrate(d,Sample::Guitar);check(r.success,"Native-rate guitar calibration failed");}
}
void unusableCalibrationAndLifecycle(){
    for(auto sample:{Sample::Silence,Sample::Noise,Sample::RaisedNoise,Sample::PlayedDuringQuiet}){
        Dynamics d(8000);const auto before=d.settings();const auto r=calibrate(d,sample);
        check(!r.success&&r.failure==Dynamics::CalibrationFailure::NoPlaying,"Silence/noise/no-quiet evidence was accepted as a playing sample");
        check(d.calibrationStatus().phase==Dynamics::CalibrationPhase::Failed,"Unusable calibration was not marked failed");
        check(d.settings().gateThresholdDb==before.gateThresholdDb,"Failed calibration changed settings");
    }
    Dynamics clipped(8000);const auto bad=calibrate(clipped,Sample::Clipped);
    check(!bad.success&&bad.failure==Dynamics::CalibrationFailure::Clipping,"Substantial recorded clipping was silently accepted");
    Dynamics occasional(8000);check(calibrate(occasional,Sample::Guitar,1,true).success,"A single incidental clipped sample invalidated otherwise usable calibration");
    Dynamics d(48000);d.startCalibration();for(int i=0;i<4800;++i)d.processSample(0);
    d.cancelCalibration();check(d.calibrationStatus().phase==Dynamics::CalibrationPhase::Cancelled&&!d.isCalibrating(),"Cancel did not stop calibration");
    d.startCalibration();check(d.calibrationStatus().phase==Dynamics::CalibrationPhase::Noise&&d.calibrationStatus().progress==0,"Restart retained old partial observations");
    auto s=d.settings();s.inputGainDb=6;d.configure(s);check(d.calibrationStatus().phase==Dynamics::CalibrationPhase::Cancelled,"A trim change retained obsolete calibration scale");
    d.startCalibration();d.prepare(96000);check(d.calibrationStatus().phase==Dynamics::CalibrationPhase::Idle,"Sample-rate change retained a partial calibration");
    d.startCalibration();for(int i=0;i<1920000;++i)d.processSample(std::numeric_limits<float>::quiet_NaN());
    check(d.calibrationStatus().result.failure==Dynamics::CalibrationFailure::InvalidInput,"Invalid sample stream was silently calibrated");
}
void realtimeAllocationGuard(){
    for(double rate:{8000.,12000.}){
        Dynamics d(rate);auto s=transparent();d.configure(s);d.startCalibration();
        allocations=0;trackAllocations=true;
        for(int i=0;i<int(rate*20)+1;++i){d.processSample(float(std::sin(i*.1)*.02));if(i%64==0){(void)d.meter();(void)d.calibrationStatus();}}
        d.cancelCalibration();d.startCalibration();d.reset();
        trackAllocations=false;
        check(allocations==0,"Audio/calibration processing allocated C++ memory");
    }
}
}
void* operator new(std::size_t bytes){if(trackAllocations)++allocations;if(void* p=std::malloc(bytes?bytes:1))return p;throw std::bad_alloc();}
void* operator new[](std::size_t bytes){return ::operator new(bytes);}
void operator delete(void* p) noexcept {std::free(p);}
void operator delete[](void* p) noexcept {std::free(p);}
void operator delete(void* p,std::size_t) noexcept {std::free(p);}
void operator delete[](void* p,std::size_t) noexcept {std::free(p);}
int main(){
    try{bypassAndSanitisation();gateRejectsNoiseAndOpensPromptly();hysteresisHoldAndRelease();quietRingingAndRawVelocity();compressionAndCeiling();calibrationSuccessAndGainScaling();unusableCalibrationAndLifecycle();realtimeAllocationGuard();std::cout<<"Input dynamics checks passed: "<<checks<<'\n';return 0;}
    catch(const std::exception& error){trackAllocations=false;std::cerr<<"Input dynamics failure: "<<error.what()<<'\n';return 1;}
}
